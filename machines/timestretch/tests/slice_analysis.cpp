#include "sample.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void require(bool ok, const char* reason) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}

int main() {
    using namespace opxy;
    unsigned cases = 0;
    for (u32 channels : {1u, 2u}) {
        std::vector<i16> pcm(5000 * channels);
        for (u32 frame = 0; frame < 5000; ++frame)
            for (u32 channel = 0; channel < channels; ++channel)
                pcm[frame * channels + channel] = i16(frame * 3 + channel * 300);
        const SampleView sample{pcm.data(), 5000, 44100, channels};
        std::vector<u32> storage(timestretch::analysis_words(sample));
        timestretch::Preparation job{};
        require(timestretch::prepare_begin(job, sample, storage.data(), u32(storage.size())), "prepare begin");
        u32 cursor = 0;
        while (timestretch::prepare_step(job, 4096, cursor) == prepare_running) {}
        SampleAnalysis analysis{};
        require(timestretch::prepared(job, analysis), "prepared analysis");
        for (u32 target : {500u, 990u, 1800u})
        for (u32 source : {0u, 990u}) {
            const SampleRegion region{100, 4100, sample_unity_level, false, 0};
            const PadSettings settings = timestretch::encode_controls(
                timestretch::Controls{false, true, source, 0, 990});
            const SampleSlice input{sample.frames, sample.rate, target, region, settings, nullptr, 0};
            SampleSliceRate clock{};
            timestretch::slice_clock(input, clock);
            const double advance = double(clock.numerator) / clock.denominator * clock.tuning / 4096.0;
            i32 output[128]{}, reference_output[128]{};
            SampleBlock block{output, nullptr, nullptr, Transport{target, 0, false},
                region, nullptr, settings, channels, 0, 0, true};
            SampleBlock reference_block = block;
            reference_block.analysis = &analysis;
            reference_block.output = reference_output;
            timestretch::Voice delayed{}, reference{};
            timestretch::initialize(delayed, sample);
            timestretch::initialize(reference, sample);
            timestretch::start(delayed);
            timestretch::start(reference);
            for (u32 pending = 1; pending <= 5; ++pending) {
                timestretch::render_sample(delayed, block);
                timestretch::render_sample(reference, reference_block);
                for (u32 i = 0; i < 64 * channels; ++i) require(output[i] == 0, "pending analysis remains silent");
                require(delayed.position == reference.position && delayed.time_remainder == reference.time_remainder,
                        "loading audition keeps the prepared audition's clock");
                require(std::fabs(double(timestretch::frame(delayed)) - pending * 64 * advance) < 1,
                        "tap clock advances during loading");
            }
            const double before = double(delayed.position) / double(u32(1) << delayed.shift);
            block.analysis = &analysis;
            timestretch::render_sample(delayed, block);
            timestretch::render_sample(reference, reference_block);
            require(delayed.position == reference.position && delayed.time_remainder == reference.time_remainder,
                    "analysis arrival does not reset source time");
            require(delayed.grains_ready && delayed.grain.ideal.position == delayed.position,
                    "new grains start on the elapsed source clock");
            for (u32 channel = 0; channel < channels; ++channel) {
                const double expected = (region.begin + before) * 3 + channel * 300;
                require(std::fabs(output[channel] / 32768.0 - expected) < 1.01,
                        "first prepared sound reads the elapsed source frame");
            }

            // A source that finishes before analysis arrives must not restart
            // late. A fresh note still starts a complete new audition.
            timestretch::start(delayed);
            block.analysis = nullptr;
            unsigned blocks = 0;
            while (!delayed.finished && blocks++ < 10000) timestretch::render_sample(delayed, block);
            require(delayed.finished, "silent audition finishes on time");
            const u32 stopped_position = delayed.position;
            block.analysis = &analysis;
            timestretch::render_sample(delayed, block);
            require(delayed.finished && delayed.position == stopped_position, "late analysis cannot restart a completed audition");
            for (u32 i = 0; i < 64 * channels; ++i) require(output[i] == 0, "completed audition stays silent");
            timestretch::start(delayed);
            timestretch::render_sample(delayed, block);
            require(!delayed.finished && output[0] == i32(pcm[region.begin * channels]) * 32768,
                    "a new audition starts at its own source marker");
            ++cases;
        }
    }
    std::printf("PASS: %u loading/ready/expired slice clocks; mono/stereo and BPM/bar timing\n", cases);
}
