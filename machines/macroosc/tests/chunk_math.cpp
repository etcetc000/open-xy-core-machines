#include "optimized/chunk_math.hpp"
#include "optimized/chunk_math.cpp"
#include <cassert>
int main() {
  for (unsigned size = 1; size <= 65536; ++size) {
    assert(macroosc::chunk_math::interpolation_increment(size) == 32767u / size);
    assert(macroosc::chunk_math::parameter1_increment(size) == 32767u / size);
  }
  assert(macroosc::chunk_math::interpolation_increment(0xffffffffu) == 0);
  assert(macroosc::chunk_math::parameter1_increment(0xffffffffu) == 0);
  const unsigned sizes[] = {1, 3, 8, 16, 24, 32, 64, 0xffffffffu};
  for (unsigned q = 0; q < 65536; ++q) {
    for (unsigned r = 0; r < 24; ++r) {
      const unsigned n = q * 24 + r;
      assert(macroosc::chunk_math::phase_delta(n, 24) == n / 24);
      assert(macroosc::chunk_math::phase_delta(0xffffffffu - n, 24) ==
             (0xffffffffu - n) / 24);
    }
  }
  unsigned rng = 0x27acf159u;
  for (unsigned i = 0; i < 1000000; ++i) {
    rng = rng * 1664525u + 1013904223u;
    for (unsigned size : sizes) {
      assert(macroosc::chunk_math::phase_delta(rng, size) == rng / size);
    }
  }
}
