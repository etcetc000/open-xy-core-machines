#include "optimized/pitch_math.hpp"
#include "optimized/pitch_table.hpp"
#include "optimized/pitch_table.cpp"
#include "vendor/braids/resources.cc"
#include <cassert>

int main() {
  // Every possible clamped int16 input to either vendor pitch-table lookup.
  for (int ref = -49152; ref <= 0; ++ref) {
    int folded = ref;
    unsigned shifts = 0;
    while (folded < 0) { folded += 1536; ++shifts; }
    const unsigned actual = macroosc::octave_shifts(ref);
    assert(actual == shifts);
    assert(ref + static_cast<int>(actual) * 1536 == folded);
  }
  for (unsigned pitch = 0; pitch < 1536; ++pitch) {
    const unsigned a = braids::lut_oscillator_increments[pitch >> 4];
    const unsigned b = braids::lut_oscillator_increments[(pitch >> 4) + 1];
    const unsigned original = a + (static_cast<int>(b - a) * (pitch & 15) >> 4);
    assert(macroosc::pitch_increments[pitch] == original);
    // Cover every defined right shift used by the original int16-pitch path.
    for (unsigned shifts = 0; shifts < 32; ++shifts) {
      assert((macroosc::pitch_increments[pitch] >> shifts) == (original >> shifts));
    }
  }
}
