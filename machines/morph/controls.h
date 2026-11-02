#pragma once
#include <opxy/types.hpp>
// Morph's own controls and display words, shared by control.cpp and display.cpp.
namespace morph {
const opxy::u32 maximum = 0x7fffffffu;
// Q16 morph position: 0 is snapshot A, one is B.
const opxy::u32 one = 1u << 16;

// A and B: "live" (what the tracks play now) or one of the ten snapshots.
const opxy::u32 live = 0, choices = 11;
// An enum control's stored word (the middle of its choice of eleven) as a
// choice: live, then snapshots 1..9 and 0 (slot choice - 1). 32-bit: (v / 8) * 11 < 2^32.
inline opxy::u32 choice(opxy::i32 value) {
    if (value < 0) return 0;
    const opxy::u32 c = ((static_cast<opxy::u32>(value) >> 3) * choices) >> 28;
    return c >= choices ? choices - 1 : c;
}
// 32-bit arithmetic only: the device build links no 64-bit helpers.
// A 0..maximum control as a Q16 fraction 0..one (rounded).
inline opxy::u32 fraction(opxy::i32 value) {
    const opxy::u32 v = value < 0 ? 0 : static_cast<opxy::u32>(value);
    return (v + (1u << 14)) >> 15;
}
// a + (b - a) * x for non-negative parameter words, x a Q16 fraction 0..one;
// never outside a..b.
inline opxy::i32 mix(opxy::i32 a, opxy::i32 b, opxy::u32 x) {
    if (x >= one) return b;
    const opxy::i32 d = b - a;  // |d| < 2^31: both are non-negative words
    const opxy::i32 high = (d >> 16) * static_cast<opxy::i32>(x);
    const opxy::i32 low = static_cast<opxy::i32>((static_cast<opxy::u32>(d) & 0xffff) * x >> 16);
    return a + high + low;
}
// The glide control's fraction as microseconds, about 30.5 per Q16 step.
inline opxy::u32 glide_time(opxy::u32 x) { return x * 30 + (x * 33 >> 6); }
// How far, Q16, a glide of `glide_us` moves in `elapsed_us`.
inline opxy::u32 glide_step(opxy::u32 elapsed_us, opxy::u32 glide_us) {
    if (elapsed_us >= glide_us) return one;
    return (elapsed_us << 8) / ((glide_us >> 8) + 1);
}

// Display words (Overlay::display), packed by control.cpp, read by display.cpp.
// 0: A's and B's choice (bits 0-3, 4-7; 0 live, else slot + 1) and the taken slots (8-17).
// 1: parameters overlaid (bits 0-15) and parameters left out for capacity (16-31).
// 2: the morph position as heard, Q16.
// 3: status bits, then the tracks left alone because their engine changed (8-15).
const opxy::u32 status_muted = 1, status_no_a = 2, status_no_b = 4;
}
