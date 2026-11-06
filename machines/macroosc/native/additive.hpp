#pragma once
#include <opxy/native.hpp>
#include <opxy/types.hpp>

namespace macroosc { namespace native {
using namespace opxy;

// Macro Osc's native routine: twelve harmonics of one phase, each an
// interpolated sine weighted by its amplitude, the amplitudes gliding toward
// their targets. A firmware build runs native/additive.s (opxy/native.hpp);
// elsewhere, and for targets outside the routine's range, the portable
// reference below runs. The reference defines the routine: both give the same
// samples.
//
// The sine table holds 512 words: a signed 16-bit value in the low half and
// the signed slope in the high half, read at phase >> 23 with the signed
// 16-bit fraction phase >> 8. Amplitudes and targets are packed two to a word,
// harmonic 2j + 1 in the low half of word j and harmonic 2j + 2 in the high.
struct Additive {
    u32 phase;           // advanced by the call, before each output pair
    u32 increment;
    i16 previous;        // the last output sample, updated
    u16 unused;
    u32* amplitude;      // 6 words, 0..32767 each, updated
    const u32* target;   // 6 words, 0..32767 each
    const u32* sine;
};

namespace additive_detail {
inline i32 lane(u32 word, unsigned k) { return static_cast<i16>(k & 1 ? word >> 16 : word); }
inline i16 sine(const u32* table, u32 phase) {
    const u32 pair = table[phase >> 23];
    const i32 slope = static_cast<i16>(pair >> 16), fraction = static_cast<i16>(phase >> 8);
    return static_cast<i16>(static_cast<i16>(pair) + ((slope * fraction) >> 16));
}
}

// The definition: per output pair, the phase steps; out sums, over harmonic k
// = 1..12 at phase k * phase, floor(sine * a_k / 2^15), and then each a_k
// steps by (target_k - a_k) >> 8. The pair is (out + previous) >> 1, then out,
// and out becomes previous.
inline void additive_reference(Additive& s, i16* out, unsigned pairs) {
    using namespace additive_detail;
    i32 a[12];
    for (unsigned k = 0; k < 12; ++k) a[k] = lane(s.amplitude[k >> 1], k);
    while (pairs--) {
        s.phase += s.increment;
        i32 sum = 0;
        u32 phase = s.phase;
        for (unsigned k = 0; k < 12; ++k, phase += s.phase) {
            const i32 product = sine(s.sine, phase) * a[k];
            sum += product >= 0 ? product >> 15 : -((-product + 32767) >> 15);
            a[k] += (lane(s.target[k >> 1], k) - a[k]) >> 8;
        }
        *out++ = static_cast<i16>((sum + s.previous) >> 1);
        *out++ = static_cast<i16>(sum);
        s.previous = static_cast<i16>(sum);
    }
    for (unsigned j = 0; j < 6; ++j)
        s.amplitude[j] = static_cast<u16>(a[2 * j]) | static_cast<u32>(static_cast<u16>(a[2 * j + 1])) << 16;
}

// The device routine's range: every target in 0..32767 and their sum at most
// 32767, with the amplitudes the same (gliding toward such targets keeps them
// so). Calls outside it take the reference.
inline bool additive_in_range(const Additive& s) {
    u32 targets = 0, amplitudes = 0;
    for (unsigned j = 0; j < 6; ++j) {
        if ((s.target[j] | s.amplitude[j]) & 0x80008000u) return false;
        targets += (s.target[j] & 0xffff) + (s.target[j] >> 16);
        amplitudes += (s.amplitude[j] & 0xffff) + (s.amplitude[j] >> 16);
    }
    return targets <= 32767 && amplitudes <= 32767;
}

// native/additive.s on the device, and its definition: the reference.
extern "C" void macroosc_native_additive(Additive* s, i16* out, unsigned pairs);
inline void additive_reference_entry(Additive* s, i16* out, unsigned pairs) { additive_reference(*s, out, pairs); }

inline void additive(Additive& s, i16* out, unsigned pairs) {
    if (additive_in_range(s)) OPXY_NATIVE(macroosc_native_additive, additive_reference_entry)(&s, out, pairs);
    else additive_reference(s, out, pairs);
}
}} // namespace macroosc::native
