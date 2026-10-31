#pragma once
#include "pitch_math.hpp"
#include "pitch_table.hpp"

namespace macroosc {
// Adjacent pitches share normalization except at an octave boundary. Preserve
// the original wrapped 32-bit interpolation product, including at high pitch.
inline __attribute__((always_inline)) opxy::u32 swarm_increment(
    opxy::i32 note, opxy::u32 fractional) {
  constexpr int top = 16384;
  constexpr int octave = 1536;
  if (note >= top) note = top - 1;
  int index = note - top;
  const unsigned shifts = octave_shifts(index);
  index += shifts * octave;
  const opxy::u32 a = pitch_increments[index] >> shifts;
  opxy::u32 b = a;
  if (note < top - 1 && fractional) {
    b = index + 1 == octave ? pitch_increments[0] >> (shifts - 1)
        : pitch_increments[index + 1] >> shifts;
  }
  const opxy::u32 product = (b - a) * fractional;
  return a + (static_cast<opxy::i32>(product) >> 16);
}

// All seven detuned pitches usually occupy one normalized octave. A single
// range check lets that case share normalization. Crossing an octave or the
// upper clamp uses the original adjacent-pitch helper, including its wrapped
// interpolation product. Valid adapter pitch is 0..16383, parameter 0..32767.
inline __attribute__((always_inline)) void swarm_increments(
    opxy::u32* increments, opxy::i32 pitch, opxy::i32 parameter) {
  opxy::i32 detune = parameter + 1024;
  detune = (detune * detune) >> 9;
  const opxy::i32 ref = pitch - 16384;
  opxy::i32 base_index = 0;
  unsigned shifts = 0;
  bool common = false;
  if (ref < 0) {
    shifts = octave_shifts(ref);
    base_index = ref + shifts * 1536;
    const opxy::i32 widest = detune * 3;
    common = base_index + ((-widest) >> 16) >= 0 &&
        base_index + (widest >> 16) + 1 < 1536;
  }
  if (common) {
    for (opxy::i16 i = 0; i < 7; ++i) {
      const opxy::i32 spread = detune * (i - 3);
      const opxy::i32 index = base_index + (spread >> 16);
      const opxy::u32 left = pitch_increments[index] >> shifts;
      const opxy::u32 right = pitch_increments[index + 1] >> shifts;
      const opxy::u32 product = (right - left) * static_cast<opxy::u32>(spread & 65535);
      increments[i] = left + (static_cast<opxy::i32>(product) >> 16);
    }
  } else {
    for (opxy::i16 i = 0; i < 7; ++i) {
      opxy::i32 saw_detune = detune * (i - 3);
      opxy::i32 detune_integral = saw_detune >> 16;
      opxy::i32 detune_fractional = saw_detune & 65535;
      increments[i] = swarm_increment(pitch + detune_integral, detune_fractional);
    }
  }
}
}
