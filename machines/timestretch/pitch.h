// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <opxy/types.hpp>
namespace timestretch {
static inline opxy::u32 pitch_ratio(int semitones) {
    static const opxy::u32 ratios[] = {
        2048,2170,2299,2435,2580,2734,2896,3069,3251,3444,3649,3866,
        4096,4340,4598,4871,5161,5468,5793,6137,6502,6889,7298,7732,8192
    };
    return ratios[(semitones < -12 || semitones > 12 ? 0 : semitones) + 12];
}
// Linear interpolation between semitone ratios, exact at integer pitches.
// Beyond +/-24 semitones (to the +/-60 limit) whole octaves shift exactly.
static inline opxy::u32 pitch_ratio_cents(int cents) {
    static const opxy::u32 ratios[] = {
        1024,1085,1149,1218,1290,1367,1448,1534,1625,1722,1825,1933,
        2048,2170,2299,2435,2580,2734,2896,3069,3251,3444,3649,3866,
        4096,4340,4598,4871,5161,5468,5793,6137,6502,6889,7298,7732,
        8192,8679,9195,9742,10321,10935,11585,12274,13004,13777,14596,15464,16384
    };
    if (cents < -6000) cents = -6000;
    if (cents > 6000) cents = 6000;
    unsigned up = 0, down = 0;
    while (cents > 2400) { cents -= 1200; ++up; }
    while (cents < -2400) { cents += 1200; ++down; }
    const unsigned shifted = unsigned(cents + 2400);
    const unsigned note = shifted / 100, fraction = shifted % 100;
    const opxy::u32 ratio = note == 48 ? ratios[48] :
        ratios[note] + ((ratios[note + 1] - ratios[note]) * fraction + 50) / 100;
    return (ratio << up) >> down;
}
// Scale a fractional phase by Q12 tuning, returning whole-frame carry
// separately. At +60 semitones a short loop's scaled phase exceeds u32.
inline opxy::u32 scale_pitch_fraction(opxy::u32 value, opxy::u32 ratio,
                                      opxy::u32 shift, opxy::u32& whole) {
    if (ratio <= 16384u || shift <= 26) {
        const opxy::u32 scaled = (value >> 12) * ratio + (((value & 4095) * ratio) >> 12);
        whole = scaled >> shift;
        return scaled & ((1u << shift) - 1);
    }
    const opxy::u32 a = value & 65535u, b = value >> 16;
    const opxy::u32 c = ratio & 65535u, d = ratio >> 16;
    const opxy::u32 low = a * c;
    const opxy::u32 carry = b * c + (low >> 16);
    const opxy::u32 middle = (carry & 65535u) + a * d;
    const opxy::u32 high = b * d + (carry >> 16) + (middle >> 16);
    const opxy::u32 bottom = (middle << 16) | (low & 65535u);
    whole = shift >= 20 ? high >> (shift - 20) :
        (high << (20 - shift)) | (bottom >> (shift + 12));
    return ((bottom >> 12) | (high << 20)) & ((1u << shift) - 1);
}
// Q12 multiply with a full 32-bit input and no wide runtime dependency.
// Callers bound the mathematical result to u32; exact unity is unchanged.
inline opxy::u32 scale_pitch(opxy::u32 value, opxy::u32 ratio) {
    return (value >> 12) * ratio + (((value & 4095) * ratio) >> 12);
}
}
