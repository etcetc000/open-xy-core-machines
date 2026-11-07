#pragma once
#include <opxy/native.hpp>
#include <opxy/types.hpp>

// Laying a block's share of a pluck into the lines (engine.cpp's lay): a
// firmware build runs native/lay.s (opxy/native.hpp); elsewhere, and for
// arguments outside the routine's range, the portable reference below runs.
// The reference defines the routine: both give the same samples.
namespace karplus {
const unsigned kLayLength = 2048, kLayMask = kLayLength - 1;  // the lines' length (core.h's kLength)

// The samples run on from line position `at`, wrapping round both lines:
// first `pulled` samples before the pick point, then `combed` after it.
struct Laying {
    opxy::i16* first;          // the first line: the pluck
    opxy::i16* noisy;          // the second line: the noise as drawn
    opxy::u32 at;              // where the first sample goes, below kLayLength
    opxy::u32 pulled, combed;  // samples before and after the pick point
    opxy::u32 peak;            // the pick's travel: past it, the noise this many samples back is combed out
    opxy::u32 seed;            // the noise's generator (in and out)
    opxy::i32 smooth;          // the noise's low-pass state (in and out)
    opxy::i32 shape;           // the pulled shape at the next sample, Q8 (in and out)
    opxy::i32 up, down;        // its slope before and after the pick point, Q8
    opxy::i32 burst;           // the noise's share, Q15
    opxy::i32 smoothing;       // the noise's low-pass weight, Q15
};

inline opxy::i32 clamp15(opxy::i32 value) {
    value = value > 16383 ? 16383 : value;
    return value < -16384 ? -16384 : value;
}

// Each sample draws the next noise (seed * 1664525 + 1013904223, its top 15
// bits less 16384), low-passes it (smooth += (noise - smooth) * smoothing >> 15),
// keeps it, and lays the shape (its Q8 value >> 8) plus the noise's share
// (burst, Q15) of it, or past the pick point of it less the noise kept `peak`
// samples back (clamped to 15 bits), clamped to 15 bits; then the shape rises
// by up, or past the pick point falls by down.
template <bool combing> inline void lay_samples(Laying* l, opxy::u32& at, opxy::u32 count) {
    opxy::u32 seed = l->seed;
    opxy::i32 smooth = l->smooth, shape = l->shape;
    const opxy::i32 burst = l->burst, smoothing = l->smoothing, step = combing ? -l->down : l->up;
    for (; count; --count) {
        seed = seed * 1664525u + 1013904223u;
        const opxy::i32 noise = static_cast<opxy::i32>(seed >> 17) - 16384;
        smooth += (noise - smooth) * smoothing >> 15;
        l->noisy[at] = static_cast<opxy::i16>(smooth);
        const opxy::i32 share = combing ? clamp15(smooth - l->noisy[(at - l->peak) & kLayMask]) : smooth;
        l->first[at] = static_cast<opxy::i16>(clamp15((shape >> 8) + (share * burst >> 15)));
        shape += step;
        at = (at + 1) & kLayMask;
    }
    l->seed = seed;
    l->smooth = smooth;
    l->shape = shape;
}
inline void lay_reference(Laying* l) {
    opxy::u32 at = l->at;
    lay_samples<false>(l, at, l->pulled);
    lay_samples<true>(l, at, l->combed);
}
}

extern "C" void karplus_native_lay(karplus::Laying* l);

namespace karplus {
// The routine's range: 1 to 256 samples from a line position; a pick's
// travel within the line; a low-pass state and weight that keep the noise
// within 15 bits, and a share that fits 16; slopes within 2^22 and a shape
// whose whole samples (shape >> 8) fit 16 bits at every sample (it moves
// monotonically either side of the pick point, so the ends of each part tell).
// Past the pick point the noise `peak` samples back, in the second line, must
// be noise this pluck kept (15 bits), as it is in the engine.
inline bool lay_whole(opxy::i32 shape) { return (shape >> 8) >= -32768 && (shape >> 8) <= 32767; }
inline bool lay_in_range(const Laying* l) {
    const opxy::u32 count = l->pulled + l->combed;
    if (count < 1 || count > 256 || l->pulled > 256 || l->at > kLayMask || l->peak < 1 || l->peak > kLayMask ||
        l->up < -(1 << 22) || l->up > (1 << 22) || l->down < -(1 << 22) || l->down > (1 << 22) || !lay_whole(l->shape) ||
        l->smooth < -16384 || l->smooth > 16383 || l->smoothing < 1 || l->smoothing > 32767 || l->burst < 0 ||
        l->burst > 32767)
        return false;
    // Each within 2^31: no overflow.
    const opxy::i32 turn = l->shape + static_cast<opxy::i32>(l->pulled) * l->up;  // the first sample past the pick point
    if (l->pulled && !lay_whole(turn - l->up)) return false;
    return !l->combed || (lay_whole(turn) && lay_whole(turn - static_cast<opxy::i32>(l->combed - 1) * l->down));
}
inline void lay_block(Laying* l) {
    if (lay_in_range(l)) OPXY_NATIVE(karplus_native_lay, lay_reference)(l);
    else lay_reference(l);
}
}
