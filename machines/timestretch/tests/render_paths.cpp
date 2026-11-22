// The block loops against each other and against first principles:
// - a voice with stereo output (the specialised repitch and timestretch loops,
//   the reused repitch set-up) against one with mono output (the general loop,
//   always the full set-up): on a mono source both give the same words; on a
//   stereo source the general loop's word is the mean of the two channels. With
//   tempo, level, mode, retrigger and pitch changes mid-note, loop wraps,
//   reverse and play-once;
// - a scaled or boosted pad level against the unity render, word by word.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "sample.cpp"

using namespace opxy;
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
// Drum-like material: decaying noise bursts over a quiet bed, so the analysis
// keys every few frames (same-word key crossings, frequent splices).
static std::vector<i16> material(u32 frames, u32 channels) {
    std::vector<i16> pcm(frames * channels);
    u32 seed = 12345;
    for (u32 f = 0; f < frames; ++f) {
        seed = seed * 1103515245u + 12345u;
        const i32 noise = i32(seed >> 16) - 32768;
        const u32 since = f % 5511;
        const i32 envelope = since < 4000 ? 32767 - i32(since * 8) : 1200;
        const i16 v = i16((noise * envelope) >> 15);
        pcm[f * channels] = v;
        if (channels == 2) pcm[f * channels + 1] = i16(v * 3 / 4 + (since & 255) - 128);
    }
    return pcm;
}
struct Prepared {
    std::vector<i16> pcm;
    std::vector<u32> words;
    SampleView view;
    SampleAnalysis analysis;
};
static void prepare(Prepared& p, u32 frames, u32 channels) {
    p.pcm = material(frames, channels);
    p.view = SampleView{p.pcm.data(), frames, 44100, channels};
    p.words.resize(timestretch::analysis_words(p.view));
    timestretch::Preparation job{};
    require(timestretch::prepare_begin(job, p.view, p.words.data(), u32(p.words.size())), "prepare begin");
    u32 cursor = 0;
    while (timestretch::prepare_step(job, 4096, cursor) == prepare_running) {}
    require(timestretch::prepared(job, p.analysis), "prepared analysis");
}
// gain_rendered's definition: sign(s) * floor(|s| * level / 4096), saturated.
static i32 scaled(i32 s, u32 level) {
    const unsigned long long m = (unsigned long long)(s < 0 ? -s : s) * level / 4096u;
    if (m >= 0x80000000ull) return s < 0 ? i32(-2147483647 - 1) : 2147483647;
    return s < 0 ? -i32(m) : i32(m);
}

int main() {
    unsigned blocks = 0, reused = 0, splicing = 0, finishes = 0, switches = 0;
    const u32 levels[] = {sample_unity_level, 0x04000000u /* -6 dB */, 0x0fffffffu, 0x20000000u /* +12 dB */};
    for (u32 channels = 1; channels <= 2; ++channels) {
        Prepared p;
        prepare(p, 24000, channels);
        for (unsigned stretch = 0; stretch < 2; ++stretch)
        for (unsigned once = 0; once < 2; ++once)
        for (unsigned reverse = 0; reverse < 2; ++reverse)
        for (u32 level : levels)
        for (int cents = 0; cents <= 700; cents += 700) {
            timestretch::Voice wide{}, narrow{};
            timestretch::initialize(wide, p.view);
            timestretch::initialize(narrow, p.view);
            timestretch::start(wide);
            timestretch::start(narrow);
            i32 parameters[8];
            for (i32& v : parameters) v = 0x40000000;
            parameters[7] = 0x7fffffff;
            i32 out_wide[128], out_narrow[64];
            timestretch::Controls controls{bool(once), bool(stretch), 1200, 2, 1200};
            u32 ticks = 0, bpm = 1440, region_level = level;
            // Each phase runs long enough for the grain to drift past the
            // drift budget (1764 frames) and splice.
            for (unsigned b = 0; b < 144; ++b) {
                if (b == 36) bpm = 1000;                                  // tempo: invalidates the reused set-up
                if (b == 54) region_level = level == sample_unity_level ? 0x04000000u : sample_unity_level;
                if (b == 72) { controls.stretch = !controls.stretch; ++switches; } // mode switch
                if (b == 90) { timestretch::start(wide); timestretch::start(narrow); }
                if (b == 108) { controls.stretch = !controls.stretch; ++switches; } // and back
                const PadSettings settings = timestretch::encode_controls(controls);
                const u32 end = once ? 1500 : 1700;  // loops wrap; a play-once note ends
                const SampleRegion region{500, end, region_level, bool(reverse), cents, end, end};
                SampleBlock block{out_wide, parameters, nullptr, Transport{bpm, ticks, true}, region,
                                  &p.analysis, settings, 2, 0, 0, false};
                const bool cached = wide.repitch_ready && !wide.sync_pending && !wide.finished &&
                                    timestretch::same_inputs(block, wide.repitch.inputs);
                timestretch::render_sample(wide, block);
                block.output = out_narrow;
                block.output_channels = 1;
                timestretch::render_sample(narrow, block);
                for (unsigned i = 0; i < 64; ++i) {
                    if (channels == 1) {
                        require(out_wide[2 * i] == out_narrow[i], "stereo-output left word equals the general loop's");
                        require(out_wide[2 * i + 1] == out_narrow[i], "stereo-output right word equals the general loop's");
                    } else if (region_level == sample_unity_level) {
                        require(((out_wide[2 * i] >> 15) + (out_wide[2 * i + 1] >> 15)) >> 1 == out_narrow[i] >> 15,
                                "stereo-output channels' mean equals the general loop's word");
                    }
                }
                require(wide.position == narrow.position && wide.finished == narrow.finished &&
                        wide.time_remainder == narrow.time_remainder, "voice state equals the general loop's");
                if (wide.stretching && wide.grains_ready) {
                    require(wide.grain.splicing == narrow.grain.splicing && wide.grain.fade == narrow.grain.fade &&
                            wide.grain.actual.position == narrow.grain.actual.position &&
                            wide.grain.actual.window.left == narrow.grain.actual.window.left &&
                            wide.grain.actual.window.width == narrow.grain.actual.window.width &&
                            wide.grain.ideal.window.left == narrow.grain.ideal.window.left &&
                            wide.grain.ideal.window.width == narrow.grain.ideal.window.width,
                            "grain equals the general loop's");
                    splicing += wide.grain.splicing;
                }
                reused += cached;
                finishes += wide.finished;
                ++blocks;
                ticks += 2;
            }
        }
        // A scaled or boosted level against the unity render, in both modes.
        for (unsigned stretch = 0; stretch < 2; ++stretch)
        for (unsigned l = 1; l < 4; ++l) {
            timestretch::Voice unity{}, gained{};
            timestretch::initialize(unity, p.view);
            timestretch::initialize(gained, p.view);
            timestretch::start(unity);
            timestretch::start(gained);
            const PadSettings settings = timestretch::encode_controls(timestretch::Controls{false, bool(stretch), 1200, 2, 1200});
            i32 a[128], b[128];
            for (unsigned k = 0; k < 24; ++k) {
                SampleBlock block{a, nullptr, nullptr, Transport{1300, 2 * k, true},
                                  SampleRegion{0, 24000, sample_unity_level, false, 300, 24000, 24000},
                                  &p.analysis, settings, 2, 0, 0, false};
                timestretch::render_sample(unity, block);
                block.output = b;
                block.region.level = levels[l];
                timestretch::render_sample(gained, block);
                for (unsigned i = 0; i < 128; ++i)
                    require(b[i] == scaled(a[i] >> 15, levels[l]), "level scales the unity render exactly");
            }
        }
    }
    require(reused > 300, "repitch blocks reuse their set-up");
    require(splicing > 32, "timestretch blocks splice inside runs");
    require(finishes > 0, "play-once voices finish");
    std::printf("PASS: %u block pairs (%u reused set-ups, %u splicing, %u finished, %u mode switches); levels exact\n",
                blocks, reused, splicing, finishes, switches);
    return 0;
}
