#include "optimized/swarm_pitch.hpp"
#include "optimized/pitch_table.cpp"
#include "vendor/braids/resources.cc"
#include <cassert>

static unsigned original_increment(int note) {
  if (note >= 16384) note = 16383;
  int index = note - 16384;
  unsigned shifts = 0;
  while (index < 0) { index += 1536; ++shifts; }
  const unsigned a = braids::lut_oscillator_increments[index >> 4];
  const unsigned b = braids::lut_oscillator_increments[(index >> 4) + 1];
  return (a + ((static_cast<int>(b - a) * (index & 15)) >> 4)) >> shifts;
}

static void check(int note, unsigned fraction) {
  const unsigned a = original_increment(note);
  const unsigned b = original_increment(note + 1);
  const unsigned product = (b - a) * fraction;
  assert(macroosc::swarm_increment(note, fraction) ==
         a + (static_cast<int>(product) >> 16));
}

static void check_group(int pitch, int parameter) {
  unsigned actual[7];
  macroosc::swarm_increments(actual, pitch, parameter);
  int detune = parameter + 1024;
  detune = (detune * detune) >> 9;
  for (int i = 0; i < 7; ++i) {
    const int spread = detune * (i - 3);
    const int note = pitch + (spread >> 16);
    const unsigned a = original_increment(note);
    const unsigned b = original_increment(note + 1);
    const unsigned product = (b - a) * static_cast<unsigned>(spread & 65535);
    assert(actual[i] == a + (static_cast<int>(product) >> 16));
  }
}

int main() {
  // Adapter pitch 0..16383 and all seven offsets bound notes to -103..16485.
  unsigned seed = 17;
  for (int note = -103; note <= 16485; ++note) {
    check(note, 0); check(note, 1); check(note, 32768); check(note, 65535);
    for (unsigned i = 0; i < 64; ++i) {
      seed = seed * 1664525u + 1013904223u;
      check(note, seed & 65535);
    }
  }
  // Exhaust every fraction on either side of normalization/clamp boundaries.
  for (int octave = 0; octave <= 11; ++octave) {
    const int boundary = 16384 - octave * 1536;
    for (int delta = -2; delta <= 1; ++delta) {
      for (unsigned fraction = 0; fraction < 65536; ++fraction)
        check(boundary + delta, fraction);
    }
  }
  // All pitches and a control grid, then all controls around octave/clamp
  // boundaries. The private qualification additionally exhausts the complete
  // 16384 x 32768 domain; keep this ordinary test fast for machine submissions.
  for (int pitch = 0; pitch <= 16383; ++pitch) {
    for (int parameter = 0; parameter < 32768; parameter += 257)
      check_group(pitch, parameter);
    check_group(pitch, 32767);
  }
  const int offsets[] = {-104, -103, -2, -1, 0, 1, 2, 102, 103, 104};
  for (int octave = 0; octave <= 11; ++octave) {
    for (int delta : offsets) {
      const int pitch = 16384 - octave * 1536 + delta;
      if (pitch < 0 || pitch > 16383) continue;
      for (int parameter = 0; parameter < 32768; ++parameter)
        check_group(pitch, parameter);
    }
  }
}
