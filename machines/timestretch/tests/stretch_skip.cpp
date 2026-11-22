// The timestretch block loop (stretch_stereo: runs, and skipped keyframe-window
// steps while no splice can be due) against the same loop taken one frame at a
// time with no run and no skip, word for word and state for state:
// - each frame steps a playhead's window when its countdown runs out (a crossing
//   or a wrap), storing that playhead's position; a splice is checked only
//   then, against the stored positions; then the clock and both playheads
//   advance, the incoming grain and the fade as in a splice.
// At the edges that skipping meets:
// - playheads that land exactly on keyframes (whole, quarter and eighth-frame
//   rates) while skips run to a block's end;
// - a change of tempo, pitch, or both (together or blocks apart) at a block
//   boundary right after such skips, with the grain held near the drift budget;
// - forward and reverse, whole and offset regions, regions starting on a key,
//   loops and play-once, tempo clocks with and without a remainder;
// - steady tones, whose analysis keys every 2, 3 or 4 frames, so a playhead
//   moving a whole number of frames can sit on a key frame after frame.
// A window is compared by what it does (right edge, lap, next key) once a
// window that ran out at a block's last frame has been stepped, as the next
// frame would step it.
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "sample.cpp"

using namespace opxy;
using namespace timestretch;
static unsigned where_trial, where_block;
static int where_frame = -1;
static void require(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (trial %u, block %u, frame %d)\n", message, where_trial, where_block, where_frame);
        std::exit(1);
    }
}
static u32 seed = 1;
static u32 next() { seed = seed * 1664525u + 1013904223u; return seed ^ (seed >> 13); }
static u32 pick(u32 n) { return n ? next() % n : 0; }
// Drum-like material, as render_paths.cpp: the analysis keys every few frames.
static std::vector<i16> material(u32 frames) {
    std::vector<i16> pcm(frames * 2);
    u32 s = 12345;
    for (u32 f = 0; f < frames; ++f) {
        s = s * 1103515245u + 12345u;
        const i32 noise = i32(s >> 16) - 32768;
        const u32 since = f % 1500;
        const i32 envelope = since < 1100 ? 32767 - i32(since * 29) : 900;
        const i16 v = i16((noise * envelope) >> 15);
        pcm[f * 2] = v;
        pcm[f * 2 + 1] = i16(v * 3 / 4 + (since & 255) - 128);
    }
    return pcm;
}
// Steady tones of 4, 6 and 8 frames a cycle: the analysis keys every 2, 3 and 4 frames.
static std::vector<i16> tones(u32 frames) {
    std::vector<i16> pcm(frames * 2);
    for (u32 f = 0; f < frames; ++f) {
        const u32 period = f < frames / 3 ? 4u : f < 2 * (frames / 3) ? 6u : 8u;
        const i16 v = (f % period) < period / 2 ? 20000 : -20000;
        pcm[f * 2] = pcm[f * 2 + 1] = v;
    }
    return pcm;
}
// What a window does: the playhead it holds, stepped as the next frame would.
static void effective(stretch::Reference h, const stretch::Source& s, u32& right, u32& lap, u32& anchor) {
    if (window_room(h) < 0) stretch::step_reference(h, s);
    right = h.window.left + h.window.width;
    lap = h.window.lap;
    anchor = h.window.next_anchor;
}
// One block, one frame at a time: the reference.
static void frame_by_frame(i32* out, stretch::Granule& g, const stretch::Source& s, const StretchBlock& b, LoopState& state) {
    u32 position = state.position, remainder = state.time_remainder;
    bool finished = state.finished;
    Head ideal, actual;
    ideal.load(g.ideal);
    actual.load(g.actual);
    bool due = stretch::splice_due(g, s);
    for (int i = 0; i < kBlock; ++i) {
        if (finished) { out[2 * i] = out[2 * i + 1] = 0; continue; }
        remainder += b.step.remainder;
        const bool carry = remainder >= b.step.divisor;
        if (carry) remainder -= b.step.divisor;
        const u32 advance = b.advances[carry ? 1 : 0];
        if ((ideal.room | actual.room) < 0) {
            if (ideal.room < 0) { ideal.store(g.ideal); stretch::step_reference(g.ideal, s); ideal.load(g.ideal); }
            if (actual.room < 0) { actual.store(g.actual); stretch::step_reference(g.actual, s); actual.load(g.actual); }
            due = stretch::splice_due(g, s);
        }
        if (due) {
            ideal.store(g.ideal);
            actual.store(g.actual);
            stretch::splice_begin(g, s, advance, b.pitch_step, b.tuning);
            due = false;
        }
        ideal.advance(advance, b.loop);
        i32 l, r;
        stretch::read_frame<true>(s, actual.position, l, r);
        actual.advance(b.pitch_step, b.loop);
        if (g.splicing) {
            i32 nl, nr;
            stretch::read_frame<true>(s, g.temp.position, nl, nr);
            const u32 w = stretch::smooth(g.fade >> 16);
            l = stretch::mix(l, nl, w);
            r = stretch::mix(r, nr, w);
            stretch::advance(g.temp, s, b.pitch_step);
            g.fade += g.fade_increment;
            if (g.fade >= 0x40000000u) {
                g.actual = g.temp;
                g.splicing = false;
                ideal.store(g.ideal);
                actual.load(g.actual);
                due = stretch::splice_due(g, s);
            }
        }
        out[2 * i] = l * 32768;
        out[2 * i + 1] = r * 32768;
        const u32 moved = position + advance;
        const bool wrapped = moved >= b.loop;
        position = wrapped ? moved - b.loop : moved;
        finished = (b.play_once && wrapped) || (carry ? b.crossings[1] : b.crossings[0]);
    }
    ideal.store(g.ideal);
    actual.store(g.actual);
    state.position = position;
    state.time_remainder = remainder;
    state.finished = finished;
}

static unsigned trials, blocks, splices, on_key;
static void sweep(const std::vector<i16>& pcm, u32 frames, unsigned count) {
    const SampleView view{pcm.data(), frames, 44100, 2};
    std::vector<u32> words(analysis_words(view));
    Preparation job{};
    require(prepare_begin(job, view, words.data(), u32(words.size())), "prepare begin");
    u32 at = 0;
    while (prepare_step(job, 4096, at) == prepare_running) {}
    SampleAnalysis analysis{};
    require(prepared(job, analysis), "prepared analysis");
    stretch::Analysis left_view{}, right_view{};
    require(from_sdk(analysis, left_view, right_view), "analysis views");
    for (unsigned t = 0; t < count; ++t) {
        where_trial = t;
        const bool reverse = pick(3) == 0;
        u32 begin = 0, length = frames;
        const u32 shape = pick(3);
        if (shape == 1) { begin = pick(frames / 2); length = frames - begin; }
        if (shape == 2) {
            begin = stretch::next_key(left_view, pick(frames / 2));
            if (begin >= frames - 2) begin = 0;
            length = frames - begin;
        }
        const u32 shift = fraction_shift(length), loop = length << shift, unit = 1u << shift;
        stretch::Source s{view, begin, length, shift, loop, reverse, &left_view};
        s.bind_pcm();
        if (right_view.activity > left_view.activity) s.analysis = &right_view;
        const auto rate = [&]() -> u32 {
            const u32 e = pick(4);
            if (e == 0) return (1 + pick(8)) * unit;
            if (e == 1) return (1 + pick(32)) * (unit / 4);
            if (e == 2) return (1 + pick(64)) * (unit / 8);
            return unit / 8 + pick(8 * unit);
        };
        u32 start = pick(length) * unit;
        if (pick(2)) {
            const u32 k = stretch::next_key(*s.analysis, begin + pick(length));
            if (k > begin && k < begin + length) { start = (reverse ? begin + length - 1 - k : k - begin) << shift; ++on_key; }
        }
        const u32 apart = pick(4) ? 900 + pick(900) : pick(1801);
        if (static_cast<unsigned long long>(apart) * unit >= loop) continue;
        stretch::Granule a, b;
        stretch::spawn(a, s, start);
        const bool lead = pick(2);
        const unsigned long long moved = lead ? static_cast<unsigned long long>(start) + static_cast<unsigned long long>(apart) * unit
                                              : static_cast<unsigned long long>(start) + 8ull * loop - static_cast<unsigned long long>(apart) * unit;
        a.actual.lap = static_cast<u32>(moved / loop) - (lead ? 0u : 8u);
        a.actual.position = static_cast<u32>(moved % loop);
        a.actual.window = stretch::cursor(s, a.actual.position, a.actual.lap);
        b = a;
        const bool once = pick(6) == 0;
        const u32 rB = rate(), drift = pick(3) ? 0u : pick(unit / 4 + 1);
        const u32 tB = rB, pB = pick(2) ? rB + drift : rB - (drift < rB ? drift : 0u);
        u32 divB = 1, remB = 0;
        if (pick(3) == 0) { divB = 2 + pick(1000); remB = pick(divB); }
        const u32 nB = 1 + pick(6), kind = pick(4), later = 1 + pick(3);
        const u32 tC = rate(), pC = rate();
        u32 divC = 1, remC = 0;
        if (pick(3) == 0) { divC = 2 + pick(1000); remC = pick(divC); }
        LoopState sa{start, 0, false}, sb{start, 0, false};
        ++trials;
        for (u32 n = 0; n < nB + 30; ++n) {
            where_block = n;
            where_frame = -1;
            u32 tempo = tB, pitch = pB, divisor = divB, remainder = remB;
            if (n >= nB) {
                if (kind == 0 || kind == 2 || (kind == 3 && n - nB >= later)) { tempo = tC; divisor = divC; remainder = remC; }
                if (kind != 0) pitch = pC;
            }
            sa.time_remainder %= divisor;
            sb.time_remainder %= divisor;
            const u32 extra = remainder ? 1u + pick(4) : 0u;
            const StretchBlock block{TimeStep{tempo, remainder, divisor, tempo >> shift}, {tempo, tempo + extra},
                                     {false, false}, loop, pitch, 1024u, once};
            i32 oa[2 * kBlock], ob[2 * kBlock];
            stretch_stereo<true>(oa, a, s, block, sa);
            frame_by_frame(ob, b, s, block, sb);
            for (int i = 0; i < 2 * kBlock; ++i) {
                where_frame = i / 2;
                require(oa[i] == ob[i], "block words equal the frame-by-frame loop's");
            }
            where_frame = -1;
            require(sa.position == sb.position && sa.time_remainder == sb.time_remainder && sa.finished == sb.finished,
                    "clock equals the frame-by-frame loop's");
            require(a.ideal.position == b.ideal.position && a.ideal.lap == b.ideal.lap &&
                    a.actual.position == b.actual.position && a.actual.lap == b.actual.lap &&
                    a.splicing == b.splicing && a.fade == b.fade, "playheads equal the frame-by-frame loop's");
            if (a.splicing) require(a.temp.position == b.temp.position && a.temp.lap == b.temp.lap, "incoming grain equals the frame-by-frame loop's");
            u32 ra, la, na, rb, lb, nb;
            effective(a.ideal, s, ra, la, na);
            effective(b.ideal, s, rb, lb, nb);
            require(ra == rb && la == lb && na == nb, "ideal window equals the frame-by-frame loop's");
            effective(a.actual, s, ra, la, na);
            effective(b.actual, s, rb, lb, nb);
            require(ra == rb && la == lb && na == nb, "actual window equals the frame-by-frame loop's");
            splices += a.splicing;
            ++blocks;
        }
    }
}

int main() {
    const u32 frames = 6000;
    sweep(material(frames), frames, 6000);
    require(trials > 4000 && splices > 1000 && on_key > 1000, "the sweep reaches splices and keyframe starts");
    sweep(tones(frames), frames, 6000);
    require(trials > 8000, "the tone sweep runs");
    std::printf("PASS: %u trials, %u blocks (%u splicing, %u starting on a keyframe): block loop equals the frame-by-frame loop\n",
                trials, blocks, splices, on_key);
    return 0;
}
