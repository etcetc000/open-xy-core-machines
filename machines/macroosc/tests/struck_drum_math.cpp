#include "optimized/struck_drum_math.hpp"
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

static void check_level(int base, int target, int fade) {
  const int expected = base + (((target - base) * fade) >> 15);
  const int actual = macroosc::struck_drum_amplitude(
      base, target, static_cast<opxy::i16>(fade - 32768));
  assert(actual == expected && actual >= 0 && actual <= 16986);
}

int main() {
  const int offsets[] = {0, 0, 1041, 1747, 1846, 3072};
  // Exhaust the complete range after DigitalOscillator::Render's clamp,
  // including every normalization and upper-clamp boundary.
  for (int pitch = 0; pitch <= 17920; ++pitch) {
    unsigned actual[6];
    macroosc::struck_drum_increments(actual, pitch);
    for (unsigned i = 0; i < 6; ++i)
      assert(actual[i] == (original_increment(pitch + offsets[i]) << 1));
  }
  // Every possible amplitude against both extreme targets, at every fade
  // step of every positive even chunk through 24 samples (the adapter max).
  for (unsigned size = 2; size <= 24; size += 2) {
    const int step = 65536 / size;
    for (int base = 0; base <= 16986; ++base) {
      for (unsigned n = 0; n <= size / 2; ++n) {
        check_level(base, 0, n * step);
        check_level(base, 16986, n * step);
      }
    }
  }
  // Interior amplitude pairs and fades, beyond the adapter's discrete grid.
  unsigned seed = 29;
  for (unsigned n = 0; n < 1048576; ++n) {
    seed = seed * 1664525u + 1013904223u;
    const int base = seed % 16987;
    seed = seed * 1664525u + 1013904223u;
    const int target = seed % 16987;
    seed = seed * 1664525u + 1013904223u;
    check_level(base, target, seed % 32769);
  }
}
