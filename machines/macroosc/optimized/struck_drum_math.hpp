#pragma once
#include "pitch_math.hpp"
#include "pitch_table.hpp"

namespace macroosc {
inline __attribute__((always_inline)) opxy::u32 struck_drum_increment(int pitch) {
  if (pitch >= 16384) pitch = 16383;
  const int ref = pitch - 16384;
  const unsigned shift = octave_shifts(ref);
  return pitch_increments[ref + shift * 1536] >> shift;
}

// Render() clamps pitch to 0..17920. Below 13312 all six partials are
// unclamped, so they can share one octave normalization. Shift the original
// table word separately for each octave: shifting an already-rounded root
// increment would lose bits. The upper range retains individual clamping.
inline __attribute__((always_inline)) void struck_drum_increments(
    opxy::u32* increments, int pitch) {
  if (pitch >= 0 && pitch < 13312) {
    const int ref = pitch - 16384;
    const unsigned shift = octave_shifts(ref);
    const int index = ref + shift * 1536;
    const opxy::u32 value = pitch_increments[index];
    increments[0] = (value >> shift) << 1;
    increments[1] = increments[0];
    increments[5] = (value >> (shift - 2)) << 1;
    {
      int partial_index = index + 1041;
      unsigned partial_shift = shift;
      if (partial_index >= 1536) { partial_index -= 1536; --partial_shift; }
      increments[2] = (pitch_increments[partial_index] >> partial_shift) << 1;
    }
    {
      int partial_index = index + 211;
      unsigned partial_shift = shift - 1;
      if (partial_index >= 1536) { partial_index -= 1536; --partial_shift; }
      increments[3] = (pitch_increments[partial_index] >> partial_shift) << 1;
    }
    {
      int partial_index = index + 310;
      unsigned partial_shift = shift - 1;
      if (partial_index >= 1536) { partial_index -= 1536; --partial_shift; }
      increments[4] = (pitch_increments[partial_index] >> partial_shift) << 1;
    }
  } else {
    increments[0] = struck_drum_increment(pitch) << 1;
    increments[1] = increments[0];
    const opxy::i16 offsets[] = {0, 0, 1041, 1747, 1846, 3072};
    for (unsigned i = 2; i < 6; ++i) {
      const opxy::i16 partial_pitch = pitch + offsets[i];
      increments[i] = struck_drum_increment(partial_pitch) << 1;
    }
  }
}

// Drum amplitudes start at zero, strike to at most 16986, then only decay.
// For an even render size, signed_fade is original_fade - 32768 in
// [-32768,0]. Therefore target + floor((target-base)*signed_fade/32768)
// exactly equals base + floor((target-base)*original_fade/32768).
// Both factors fit signed 16 bits; the product and result do not overflow.
inline __attribute__((always_inline)) opxy::i16 struck_drum_amplitude(
    opxy::i32 base, opxy::i32 target, opxy::i16 signed_fade) {
  const opxy::i16 delta = target - base;
  return target + ((delta * signed_fade) >> 15);
}
}
