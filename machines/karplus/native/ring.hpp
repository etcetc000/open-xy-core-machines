#pragma once
#include <opxy/native.hpp>
#include <opxy/types.hpp>
#include "lay.hpp"

// One string's frames through (part of) a block (engine.cpp's run): a
// firmware build runs native/ring.s (opxy/native.hpp); elsewhere, and for
// arguments outside the routine's range, the portable reference below runs.
// The reference defines the routine: both give the same samples.
namespace karplus {
// Frames of a block, reading and writing round the lines (kLayLength samples
// each, native/lay.hpp) from line positions.
struct Ringing {
    const opxy::i16* reads;    // the line read one period back
    const opxy::i16* hears;    // the line heard a pick's travel back
    opxy::i16* writes;         // the line the string writes
    opxy::u32 from, heard, to; // their positions at the first frame, below kLayLength
    opxy::i32* frame;          // the output
    opxy::u32 count;           // frames, at least 1
    opxy::i32 input, output, previous;  // the allpass's last input and output, the loop filter's last input (in and out)
    opxy::i32 coefficient;     // the allpass's, Q15
    opxy::i32 damp;            // the loop filter's weight on the previous sample, Q16
    opxy::i32 level;           // the pickup's gain, Q12
    opxy::u32 add;             // 0: set the frames; else add to them
};

// value / 2^shift, rounding toward zero (as division does).
template <unsigned shift> inline opxy::i32 toward_zero(opxy::i32 value) {
    return (value + ((value >> 31) & ((1 << shift) - 1))) >> shift;
}

// Loop gain per pass, Q15: 0.999.
const opxy::i32 kGain = 32735;

// Each frame reads one period back through the fractional-delay allpass and
// the loop filter, writes the result (less a little energy, clamped to 15
// bits) and hears it at the pickup, less what it wrote a pick's travel ago.
template <bool add> inline void ring_frames(Ringing* r) {
    opxy::u32 from = r->from, heard = r->heard, to = r->to;
    opxy::i32* frame = r->frame;
    opxy::i32 input = r->input, output = r->output, previous = r->previous;
    const opxy::i32 coefficient = r->coefficient, damp = r->damp, level = r->level;
    for (opxy::u32 count = r->count; count; --count) {
        const opxy::i32 x = r->reads[from];
        const opxy::i32 sample = input + toward_zero<15>(coefficient * (x - output));
        input = x;
        output = sample;
        const opxy::i32 filtered = sample + toward_zero<16>((previous - sample) * damp);
        previous = sample;
        const opxy::i32 ringing = clamp15(toward_zero<15>(filtered * kGain));
        r->writes[to] = static_cast<opxy::i16>(ringing);
        const opxy::i32 heard_now = clamp15((ringing - r->hears[heard]) * level >> 12) << 15;
        *frame = add ? *frame + heard_now : heard_now;
        ++frame;
        from = (from + 1) & kLayMask;
        heard = (heard + 1) & kLayMask;
        to = (to + 1) & kLayMask;
    }
    r->input = input;
    r->output = output;
    r->previous = previous;
}
inline void ring_reference(Ringing* r) {
    if (r->add) ring_frames<true>(r);
    else ring_frames<false>(r);
}
}

extern "C" void karplus_native_ring(karplus::Ringing* r);

namespace karplus {
// The routine's range: the allpass's output and the loop filter's input are
// one value (as the engine keeps them), within +-32704, which an allpass of
// coefficient -10894..0 keeps there for 15-bit lines; a 15-bit last input; a
// loop filter weight of at most a half; a pickup gain of at most 4. The lines
// must hold 15-bit samples, as the engine writes them.
inline bool ring_in_range(const Ringing* r) {
    return r->count >= 1 && r->from <= kLayMask && r->heard <= kLayMask && r->to <= kLayMask &&
           r->output == r->previous && r->output >= -32704 && r->output <= 32704 && r->input >= -16384 &&
           r->input <= 16383 && r->coefficient >= -10894 && r->coefficient <= 0 && r->damp >= 0 && r->damp <= 32768 &&
           r->level >= 0 && r->level <= 16384;
}
inline void ring(Ringing* r) {
    if (ring_in_range(r)) OPXY_NATIVE(karplus_native_ring, ring_reference)(r);
    else ring_reference(r);
}
}
