#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include "sample.cpp"

static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
    using namespace opxy;
    unsigned cases = 0;
    double maximum_error = 0, maximum_twenty_second_error = 0;
    for (u32 channels : {1u, 2u}) {
        const u32 total = 600000;
        std::vector<i16> pcm(total * channels);
        for (u32 f = 0; f < total; ++f)
            for (u32 channel = 0; channel < channels; ++channel)
                pcm[f * channels + channel] = i16(int((f * 17 + channel * 7001) % 30001) - 15000);
        for (u32 rate : {8000u, 44100u, 48000u, 192000u}) {
            const SampleView sample{pcm.data(), total, rate, channels};
            std::vector<u32> storage(timestretch::analysis_words(sample));
            timestretch::Preparation job{};
            require(timestretch::prepare_begin(job, sample, storage.data(), u32(storage.size())), "prepare begin");
            u32 cursor = 0;
            while (timestretch::prepare_step(job, 4096, cursor) == prepare_running) {}
            SampleAnalysis analysis{};
            require(timestretch::prepared(job, analysis), "prepared analysis");
            for (u32 source_bpm : {0u, 990u, 1200u})
            for (u32 target_bpm : {500u, 990u, 1373u})
            for (u32 bars : {0u, 2u, 5u})
            for (bool stretching : {false, true})
            for (i32 transpose : {-36, 0, 7, 36})
            for (i32 saved_pitch : {-1200, 0, 370}) {
                // Non-neutral trims/direction are deliberately set: the tap
                // audition must still begin at its frozen outer source bound.
                i32 live[8] = {0x48000000, 0, 0x7fffffff, 0,
                    0x7fffffff, 0x40000000, 0x40000000, 0x7fffffff};
                const SampleRegion region{1234, total - 4321, sample_unity_level, true, saved_pitch};
                const PadSettings settings = timestretch::encode_controls(
                    timestretch::Controls{false, stretching, source_bpm, bars, 1200});
                const SampleSlice input{total, rate, target_bpm, region, settings, live, transpose};
                SampleSliceRate clock{};
                timestretch::slice_clock(input, clock);
                require(clock.denominator && clock.tuning, "clock is valid");
                const double advance = double(clock.numerator) / clock.denominator * clock.tuning / 4096.0;
                const unsigned max_blocks = unsigned((region.end - region.begin) * 0.7 / (advance * 64));
                const unsigned blocks = max_blocks < 64 ? max_blocks : 64;
                require(blocks, "case has at least one pre-end block");
                timestretch::Voice voice{};
                timestretch::initialize(voice, sample);
                timestretch::start(voice);
                i32 output[128]{};
                SampleBlock block{output, live, nullptr, Transport{target_bpm, 12345, false},
                    region, &analysis, settings, channels, transpose, 48, true};
                for (unsigned b = 0; b < blocks; ++b) {
                    timestretch::render_sample(voice, block);
                    require(!voice.finished, "clock predicts before source end");
                    if (!b) {
                        const timestretch::PhaseAdvance a = timestretch::phase_advance(
                            voice.time_step, voice.shift, voice.tuning, stretching, 0);
                        const timestretch::PhaseAdvance z = timestretch::phase_advance(
                            voice.time_step, voice.shift, voice.tuning, stretching, 1);
                        const double phase_unit = double(u32(1) << voice.shift);
                        const double plain_rate = a.whole + a.fraction / phase_unit;
                        const double carry_rate = z.whole + z.fraction / phase_unit;
                        const double exact_rate = plain_rate + (carry_rate - plain_rate) *
                            voice.time_step.remainder / voice.time_step.divisor;
                        const double twenty_second_error = std::fabs(advance - exact_rate) * 882000;
                        if (twenty_second_error > maximum_twenty_second_error)
                            maximum_twenty_second_error = twenty_second_error;
                        require(twenty_second_error + std::fabs(carry_rate - plain_rate) < 1,
                            "clock drift including carry jitter stays below one source frame over20 seconds");
                        require(voice.region_begin == region.begin && voice.region_end == region.end,
                            "frozen source bounds ignore live trims");
                        require(!voice.reverse && voice.play_once, "source is forward once");
                        for (u32 channel = 0; channel < channels; ++channel)
                            require(output[channel] == i32(pcm[region.begin * channels + channel]) * 32768,
                                "first source frame matches original marker");
                    }
                    const double expected = (b + 1) * 64 * advance;
                    const double actual = double(voice.position) / double(u32(1) << voice.shift);
                    const double error = std::fabs(actual - expected);
                    // The reported clock folds in the actual quantized pitch
                    // advances, including their exact tempo carry frequency.
                    const double bound = 1.0;
                    if (error > bound) {
                        std::fprintf(stderr, "rate=%u source=%u target=%u bars=%u stretch=%u transpose=%d saved=%d block=%u error=%f bound=%f\n",
                            rate, source_bpm, target_bpm, bars, stretching, transpose, saved_pitch, b, error, bound);
                        require(false, "render position follows clock within phase quantization");
                    }
                    if (error > maximum_error) maximum_error = error;
                }
                ++cases;
            }
        }
    }
    // Reproduce the user's exact neutral-pitch 99 -> 50 BPM example.
    std::vector<i16> pcm(441000, 12000);
    SampleView sample{pcm.data(), u32(pcm.size()), 44100, 1};
    SampleRegion region{0, sample.frames, sample_unity_level, false, 0};
    PadSettings settings = timestretch::encode_controls(timestretch::Controls{false, false, 990, 2, 990});
    SampleSlice input{sample.frames, sample.rate, 500, region, settings, nullptr, 0};
    SampleSliceRate clock{};
    timestretch::slice_clock(input, clock);
    require(clock.numerator == 44100u * 500 && clock.denominator == 44100u * 990 && clock.tuning == 4096,
        "99 BPM source at 50 BPM master advances exactly 50/99");
    timestretch::Voice voice{};
    timestretch::initialize(voice, sample);
    timestretch::start(voice);
    i32 output[64]{};
    SampleBlock block{output, nullptr, nullptr, Transport{500, 0, false}, region, nullptr, settings, 1, 0, 0, true};
    const u32 blocks = 1378;
    for (u32 b = 0; b < blocks; ++b) timestretch::render_sample(voice, block);
    const double expected = double(blocks * 64) * 50 / 99;
    require(std::fabs(double(timestretch::frame(voice)) - expected) < 1, "neutral 99/50 clock matches actual audio playhead");
    const u32 original_clock = blocks * 64;
    require(original_clock > timestretch::frame(voice) + 43000, "old original-rate clock reproduces large drift");
    std::printf("99 -> 50 BPM: old tap=%u source frames; new tap=%.6f; rendered=%u after %u output frames\n",
        original_clock, expected, timestretch::frame(voice), blocks * 64);
    // A complete source pass must end in silence instead of wrapping.
    while (!voice.finished) timestretch::render_sample(voice, block);
    timestretch::render_sample(voice, block);
    for (i32 value : output) require(value == 0, "finished audition remains silent");
    // A malformed/empty source gives no clock; no division by zero escapes.
    input.region.end = input.region.begin;
    timestretch::slice_clock(input, clock);
    require(!clock.numerator && !clock.denominator && !clock.tuning, "invalid source clock declined");
    std::printf("PASS: %u rendered clock cases; mono/stereo; BPM/bars; repitch/stretch; four rates; saved/live/track pitch; maximum phase rounding error %.6f source frames\n",
        cases, maximum_error);
    std::printf("Maximum normalized clock rounding error over20 seconds: %.9f source frames\n", maximum_twenty_second_error);
    for (u32 frames : {2u, 7u, 1048575u})
    for (u32 rate : {8000u, 192000u})
    for (u32 source : {0u, 200u, 3000u})
    for (u32 target : {200u, 3000u})
    for (i32 direction : {-1, 1}) {
        i32 live[8] = {direction < 0 ? 0 : 0x7fffffff, 0x40000000, 0x40000000,
            0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x7fffffff};
        input = SampleSlice{frames, rate, target,
            SampleRegion{0, frames, sample_unity_level, false, direction * 1200},
            timestretch::encode_controls(timestretch::Controls{false, false, source, 5, 1200}),
            live, direction * 36};
        timestretch::slice_clock(input, clock);
        require(clock.denominator && clock.tuning == 4096, "extreme clock is bounded and normalized");
        const u32 shift = timestretch::fraction_shift(frames);
        const u32 loop = frames << shift;
        const u32 span = timestretch::duration(32, target);
        const timestretch::TimeStep step = source ? timestretch::source_advance(frames, shift, rate, source, target) :
            timestretch::TimeStep{loop / span, loop % span, span, (loop / span) >> shift};
        const u32 tuning = timestretch::pitch_ratio_cents(direction * 6000);
        const timestretch::PhaseAdvance a = timestretch::phase_advance(step, shift, tuning, false, 0);
        const timestretch::PhaseAdvance z = timestretch::phase_advance(step, shift, tuning, false, 1);
        const double unit = double(u32(1) << shift);
        const double plain = a.whole + a.fraction / unit;
        const double carried = z.whole + z.fraction / unit;
        const double actual = plain + (carried - plain) * step.remainder / step.divisor;
        const double predicted = double(clock.numerator) / clock.denominator;
        require(std::fabs(actual - predicted) * 882000 + std::fabs(carried - plain) < 1,
            "extreme clock follows actual phase under one frame over20 seconds");
        require(bool(clock.numerator) == bool(actual), "zero clock only when actual render advance is zero");
    }
    std::printf("PASS: 72 extreme short/long, 8/192kHz, 20/300BPM, +/-60-semitone clocks\n");
}
