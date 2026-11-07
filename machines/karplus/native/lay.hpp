#pragma once
#include <opxy/native.hpp>
#include <opxy/types.hpp>

// Laying a pluck into the lines (engine.cpp's lay): a firmware build runs
// native/lay.s (opxy/native.hpp); elsewhere, and for arguments outside the
// routine's range, the portable reference below runs. The reference defines
// the routine: both give the same samples.
namespace karplus {
// A stretch of the pluck that wraps around neither line.
struct Laying {
    opxy::i16* laid;           // into the first line: the pluck
    opxy::i16* kept;           // into the second line: the noise as drawn
    const opxy::i16* before;   // past the pick point, the noise a pick's travel back; else null
    opxy::u32 count;           // samples, at least 1
    opxy::u32 seed;            // the noise's generator (in and out)
    opxy::i32 smooth;          // the noise's low-pass state (in and out)
    opxy::i32 shape;           // the pulled shape at the next sample, Q8 (in and out)
    opxy::i32 step;            // its change per sample, Q8
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
// (burst, Q15) of it, or past the pick point of it less the noise a pick's
// travel back (clamped to 15 bits), clamped to 15 bits; then the shape moves
// by step.
inline void lay_reference(Laying* l) {
    opxy::i16* laid = l->laid;
    opxy::i16* kept = l->kept;
    const opxy::i16* before = l->before;
    opxy::u32 seed = l->seed;
    opxy::i32 smooth = l->smooth, shape = l->shape;
    for (opxy::u32 k = l->count; k; --k) {
        seed = seed * 1664525u + 1013904223u;
        const opxy::i32 noise = static_cast<opxy::i32>(seed >> 17) - 16384;
        smooth += (noise - smooth) * l->smoothing >> 15;
        *kept++ = static_cast<opxy::i16>(smooth);
        const opxy::i32 share = before ? clamp15(smooth - *before++) : smooth;
        *laid++ = static_cast<opxy::i16>(clamp15((shape >> 8) + (share * l->burst >> 15)));
        shape += l->step;
    }
    l->seed = seed;
    l->smooth = smooth;
    l->shape = shape;
}
}

extern "C" void karplus_native_lay(karplus::Laying* l);

namespace karplus {
// The routine's range: a low-pass state and weight that keep the noise within
// 15 bits, a share that fits 16, and a shape whose whole samples fit 16 bits
// throughout. Past the pick point `before` must hold noise this stretch or an
// earlier one kept (15 bits), as the engine's own lines do.
inline bool lay_in_range(const Laying* l) {
    const opxy::i32 last = l->shape + static_cast<opxy::i32>(l->count - 1) * l->step;
    return l->count >= 1 && l->count <= 2048 && l->smooth >= -16384 && l->smooth <= 16383 && l->smoothing >= 1 &&
           l->smoothing <= 32767 && l->burst >= 0 && l->burst <= 32767 && l->step >= -65536 && l->step <= 65536 &&
           (l->shape >> 8) >= -32768 && (l->shape >> 8) <= 32767 && (last >> 8) >= -32768 && (last >> 8) <= 32767;
}
inline void lay_stretch(Laying* l) {
    if (lay_in_range(l)) OPXY_NATIVE(karplus_native_lay, lay_reference)(l);
    else lay_reference(l);
}
}
