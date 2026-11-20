// The SDK sample routine (opxy/sample_kernel.hpp) against this machine's own
// second pass as it was before the routine replaced it: the repitch loop's
// reads, interpolation and pad level for every layout (mono or stereo source,
// forward or reverse) and level class (unity, below 2^28, any), and the
// timestretch run's reads (FrameReader at unity). Random positions with the
// edges forced: frame 0, the last frame, past it, the end of the region,
// whole frames; regions of 1 to 4,096 frames; shifts 11 to 30.
// Then the crossfade (opxy::sample_crossfade) against the former per-frame
// splice path: two FrameReader reads and the mix, weights 0 to 16384.
#include <cstdio>
#include <cstdlib>
#include "sample.cpp"

using namespace opxy;
static void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
namespace before {
inline u32 when(bool c) { return 0u - static_cast<u32>(c); }
inline i32 scale_sample(i32 s, u16 low, u16 high) {
    const u32 negative = static_cast<u32>(s) >> 31;
    const u16 m = static_cast<u16>(s < 0 ? -s : s);
    const u32 upper = u32(m) * u32(high);
    const u32 r = (upper << 4) + ((u32(m) * u32(low)) >> 12);
    const u32 saturated = (0u - static_cast<u32>(upper >= 0x08000000u)) | (0u - (r >> 31));
    const u32 magnitude = (r & ~saturated) | ((0x7fffffffu + negative) & saturated);
    return static_cast<i32>((magnitude ^ (0u - negative)) + negative);
}
inline i32 scale_small(i32 s, i16 low, i16 high) {
    const i16 v = static_cast<i16>(s);
    const i32 bias = (s >> 31) & 4095;
    return i32(v) * i32(high) * 8 + ((i32(v) * i32(low) + bias) >> 12);
}
// The repitch loop's second pass (repitch_stereo before the SDK routine).
template <bool Stereo, int Level>
void repitch(i32* output, const u32* positions, int count, const i16* origin, u32 shift, u32 frames, int stride_signed,
             u32 level) {
    const u16 low = static_cast<u16>(level), high = static_cast<u16>(level >> 16);
    const i16 low15 = static_cast<i16>(level & 32767u), high15 = static_cast<i16>(level >> 15);
    const u32 last = (frames - 1u) << shift, up = 32u - shift;
    const u32 flip = when(stride_signed < 0);
    const u32 width = Stereo ? 1u : 0u;
    const u32 stride = static_cast<u32>(stride_signed);
    for (int i = 0; i < count; ++i) {
        const u32 position = positions[i];
        const u32 offset = ((position >> shift) ^ flip) - flip;
        const u32 following = (offset + (stride >> width)) & ~when(position >= last);
        const i16* const at = origin + static_cast<i32>(offset << width);
        const i16* const after = origin + static_cast<i32>(following << width);
        const i32 fraction = static_cast<i32>((position << up) >> 18);
        const i16 rest = static_cast<i16>(16384 - fraction), part = static_cast<i16>(fraction);
        const i32 left = (i32(at[0]) * rest + i32(after[0]) * part) >> 14;
        const i32 right = Stereo ? (i32(at[1]) * rest + i32(after[1]) * part) >> 14 : left;
        output[2 * i] = Level == 2 ? scale_sample(left, low, high) : Level == 1 ? scale_small(left, low15, high15) : left * 32768;
        output[2 * i + 1] = Level == 2 ? scale_sample(right, low, high) :
                            Level == 1 ? scale_small(right, low15, high15) : right * 32768;
    }
}
// The timestretch run's reads (FrameReader::read times 32768).
template <bool Stereo>
void stretch(i32* output, const u32* positions, int count, const i16* origin, u32 shift, u32 frames, bool reverse) {
    const u32 flip = when(reverse), width = Stereo ? 1u : 0u, last = (frames - 1u) << shift, up = 32u - shift;
    for (int i = 0; i < count; ++i) {
        const u32 position = positions[i];
        const u32 offset = ((position >> shift) ^ flip) - flip;
        const u32 following = (offset + (flip | 1u)) & ~when(position >= last);
        const i16* const at = origin + static_cast<i32>(offset << width);
        const i16* const after = origin + static_cast<i32>(following << width);
        const i32 fraction = static_cast<i32>((position << up) >> 18);
        const i16 rest = static_cast<i16>(16384 - fraction), part = static_cast<i16>(fraction);
        const i32 left = (i32(at[0]) * rest + i32(after[0]) * part) >> 14;
        const i32 right = Stereo ? (i32(at[1]) * rest + i32(after[1]) * part) >> 14 : left;
        output[2 * i] = left * 32768;
        output[2 * i + 1] = right * 32768;
    }
}
}
static u32 seed = 12345;
static u32 rnd() { seed = seed * 1664525u + 1013904223u; return seed ^ (seed >> 15); }
static u32 pick(u32 n) { return rnd() % n; }

int main() {
    enum { kFrames = 6000 };
    static i16 pcm[kFrames * 2];
    for (i16& v : pcm) {
        const u32 k = pick(16);
        v = k == 0 ? i16(32767) : k == 1 ? i16(-32768) : static_cast<i16>(rnd());
    }
    const u32 levels[] = {sample_unity_level, 0u, 1u, 0x0fffffffu, 0x10000000u, 0xffffffffu, 0x08000001u, 0x07ffffffu};
    const u32 shifts[] = {11, 11, 12, 13, 14, 15, 16, 18, 20, 24, 27, 30};
    static i32 expected[132], got[132];
    static u32 positions[64];
    unsigned classes[3] = {0, 0, 0}, stretch_cases = 0;
    for (unsigned k = 0; k < 200000; ++k) {
        SampleTaps t;
        t.channels = 1 + pick(2);
        t.reverse = pick(2);
        t.shift = shifts[pick(12)];
        const u32 max_frames = t.shift >= 19 ? (1u << (31 - t.shift)) : 4096u;
        u32 frames = 1 + pick(max_frames);
        if (pick(8) == 0) frames = 1 + pick(3);
        const u32 base = 2 + pick(kFrames - frames - 4);
        t.origin = pcm + (t.reverse ? base + frames - 1 : base) * t.channels;
        t.last = (frames - 1u) << t.shift;
        const u32 lv = pick(10);
        t.level = lv < 8 ? levels[lv] : lv == 8 ? rnd() & 0x0fffffffu : rnd() | 0x10000000u;
        const u32 count = pick(4) == 0 ? 64 : pick(65);
        const u32 span = frames << t.shift;
        for (u32 i = 0; i < count; ++i) {
            const u32 e = pick(12);
            positions[i] = e == 0 ? 0 : e == 1 ? t.last : e == 2 ? span - 1 : e == 3 ? (t.last ? t.last - 1 : 0) :
                           e == 4 ? (pick(frames) << t.shift) : rnd() % span;
        }
        for (u32 i = 0; i < 132; ++i) expected[i] = got[i] = 0x5a5a5a5a;
        const bool stretching = pick(4) == 0;
        if (stretching) {
            t.level = sample_unity_level;
            if (t.channels == 2) before::stretch<true>(expected, positions, int(count), t.origin, t.shift, frames, t.reverse);
            else before::stretch<false>(expected, positions, int(count), t.origin, t.shift, frames, t.reverse);
            ++stretch_cases;
        } else {
            const int stride = (t.reverse ? -1 : 1) * int(t.channels);
            const int level = t.level == sample_unity_level ? 0 : t.level < 0x10000000u ? 1 : 2;
            ++classes[level];
            if (t.channels == 2) {
                if (level == 0) before::repitch<true, 0>(expected, positions, int(count), t.origin, t.shift, frames, stride, t.level);
                else if (level == 1) before::repitch<true, 1>(expected, positions, int(count), t.origin, t.shift, frames, stride, t.level);
                else before::repitch<true, 2>(expected, positions, int(count), t.origin, t.shift, frames, stride, t.level);
            } else {
                if (level == 0) before::repitch<false, 0>(expected, positions, int(count), t.origin, t.shift, frames, stride, t.level);
                else if (level == 1) before::repitch<false, 1>(expected, positions, int(count), t.origin, t.shift, frames, stride, t.level);
                else before::repitch<false, 2>(expected, positions, int(count), t.origin, t.shift, frames, stride, t.level);
            }
        }
        sample_interpolate(got, positions, count, t);
        for (u32 i = 0; i < 132; ++i) require(got[i] == expected[i], "the SDK routine's word equals the machine's former loop's");
    }
    std::printf("PASS: SDK sample routine = former second pass, 200000 blocks (%u unity, %u below 2^28, %u any, %u stretch)\n",
                classes[0], classes[1], classes[2], stretch_cases);
    u32 fades = 0;
    for (u32 k = 0; k < 200000; ++k) {
        SampleTaps t;
        t.channels = 1 + pick(2);
        t.reverse = pick(2);
        const u32 shifts[] = {11, 11, 12, 13, 14, 15, 16, 18, 20, 24, 27, 30};
        t.shift = shifts[pick(12)];
        const u32 max_frames = t.shift >= 19 ? (1u << (31 - t.shift)) : 4096u;
        u32 frames = 1 + pick(max_frames);
        if (pick(8) == 0) frames = 1 + pick(3);
        const u32 base = 2 + pick(kFrames - frames - 4);
        t.origin = pcm + (t.reverse ? base + frames - 1 : base) * t.channels;
        t.last = (frames - 1u) << t.shift;
        t.level = sample_unity_level;
        const u32 count = pick(4) == 0 ? 64 : pick(65);
        const u32 span = frames << t.shift;
        u32 from[64], to[64];
        u16 weights[64];
        for (u32 i = 0; i < count; ++i) {
            for (u32 h = 0; h < 2; ++h) {
                const u32 e = pick(12);
                (h ? to : from)[i] = e == 0 ? 0 : e == 1 ? t.last : e == 2 ? span - 1 : e == 3 ? (t.last ? t.last - 1 : 0) :
                                     e == 4 ? (pick(frames) << t.shift) : rnd() % span;
            }
            const u32 wk = pick(8);
            weights[i] = static_cast<u16>(wk == 0 ? 0 : wk == 1 ? 16384 : wk == 2 ? 1 : wk == 3 ? 16383 : pick(16385));
            if (pick(4) == 0) { to[i] = from[i]; weights[i] = 0; }  // a frame outside a splice
        }
        i32 a[132], b[132], expected[132], got[132];
        if (t.channels == 2) {
            before::stretch<true>(a, from, int(count), t.origin, t.shift, frames, t.reverse);
            before::stretch<true>(b, to, int(count), t.origin, t.shift, frames, t.reverse);
        } else {
            before::stretch<false>(a, from, int(count), t.origin, t.shift, frames, t.reverse);
            before::stretch<false>(b, to, int(count), t.origin, t.shift, frames, t.reverse);
        }
        for (u32 i = 0; i < 132; ++i) expected[i] = got[i] = 0x5a5a5a5a;
        for (u32 i = 0; i < count; ++i) {
            // The former splice frame: read, read, stretch::mix as (a * (16384 - w) + b * w) >> 14, times 32768.
            i32 left = a[2 * i] >> 15, right = a[2 * i + 1] >> 15;
            const i32 next_left = b[2 * i] >> 15, next_right = b[2 * i + 1] >> 15;
            const u32 weight = weights[i];
            const i32 keep = static_cast<i32>(16384u - weight), take = static_cast<i32>(weight);
            left = (left * keep + next_left * take) >> 14;
            right = (right * keep + next_right * take) >> 14;
            expected[2 * i] = left * 32768;
            expected[2 * i + 1] = right * 32768;
        }
        const SampleFade fade{from, to, weights};
        sample_crossfade(got, fade, count, t);
        for (u32 i = 0; i < 132; ++i) require(got[i] == expected[i], "the SDK crossfade's word equals the former splice frame's");
        ++fades;
    }
    std::printf("PASS: SDK crossfade = former splice frames, %u blocks\n", fades);
}
