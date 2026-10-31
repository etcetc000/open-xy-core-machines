#include <cstdio>
#include <cstdlib>
#include <vector>
#include "sample.cpp"

static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
    using namespace opxy;
    unsigned cases = 0;
    const u32 ticks[] = {1, 47, 96, 301, 12345};
    for (u32 channels = 1; channels <= 2; ++channels) {
        std::vector<i16> pcm(50000 * channels);
        for (u32 f = 0; f < 50000; ++f)
            for (u32 c = 0; c < channels; ++c) pcm[f * channels + c] = i16((f * 7 + c * 997) % 30000 - 15000);
        const SampleView sample{pcm.data(), 50000, 44100, channels};
        std::vector<u32> storage(timestretch::analysis_words(sample));
        timestretch::Preparation job{};
        require(timestretch::prepare_begin(job, sample, storage.data(), u32(storage.size())), "prepare begin");
        u32 cursor = 0;
        while (timestretch::prepare_step(job, 4096, cursor) == prepare_running) {}
        SampleAnalysis analysis{};
        require(timestretch::prepared(job, analysis), "prepared analysis");
        for (unsigned mode = 0; mode < 2; ++mode)
        for (unsigned once = 0; once < 2; ++once)
        for (unsigned reverse = 0; reverse < 2; ++reverse)
        for (unsigned bpm = 0; bpm < 2; ++bpm)
        for (unsigned running = 0; running < 2; ++running)
        for (u32 tick : ticks) {
            timestretch::Voice voice{};
            timestretch::initialize(voice, sample);
            timestretch::start(voice);
            i32 output[128]{};
            const timestretch::Controls controls{bool(once), bool(mode), bpm ? 1200u : 0u, 2, 1200};
            SampleBlock block{output, nullptr, nullptr, Transport{1373, tick, bool(running)},
                SampleRegion{1000, 45100, sample_unity_level, bool(reverse), 0},
                mode ? nullptr : &analysis, timestretch::encode_controls(controls), channels, 0, 0};
            if (mode) {
                // Analysis may arrive several blocks after the note/region setup.
                for (unsigned pending = 0; pending < 3; ++pending) {
                    timestretch::render_sample(voice, block);
                    for (unsigned i = 0; i < 64 * channels; ++i) require(output[i] == 0, "pending analysis silence");
                    block.transport.ticks96 += 7;
                }
                block.analysis = &analysis;
            }
            for (unsigned trigger = 0; trigger < 3; ++trigger) {
                if (trigger) {
                    block.transport.ticks96 += 29;
                    block.region.begin += 113;
                    block.region.end -= 97;
                    timestretch::start(voice);
                }
                timestretch::render_sample(voice, block);
                const u32 first = reverse ? block.region.end - 1 : block.region.begin;
                for (u32 c = 0; c < channels; ++c)
                    require(output[c] == i32(pcm[first * channels + c]) * 32768, "first audible frame matches selected marker");
                const u32 prior = voice.position;
                timestretch::render_sample(voice, block);
                require(voice.position > prior, "subsequent block continues rather than restarting");
                ++cases;
            }
        }
    }
    // At unity tempo, loop traversal must remain exactly one source frame per output frame.
    i16 pcm[256]{};
    timestretch::Voice voice{};
    timestretch::initialize(voice, SampleView{pcm, 256, 44100, 1});
    timestretch::region(voice, 20, 220);
    timestretch::set_source_bpm_x10(voice, 1200);
    timestretch::start(voice);
    i32 output[64], parameters[2] = {0x40000000, 0};
    for (u32 block = 1; block <= 100; ++block) {
        timestretch::render(voice, output, parameters, nullptr, Transport{1200, 47 + block, true});
        require(timestretch::frame(voice) == (block * 64) % 200, "loop speed and wrap stay exact");
    }
    std::printf("PASS: %u retriggers, delayed analysis, mono/stereo, forward/reverse, both playback and tempo modes; exact loop traversal\n", cases);
}
