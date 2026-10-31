#include "optimized/morph_math.hpp"
#include "optimized/packed_tables.cpp"
#include "vendor/braids/resources.cc"
#include <cassert>
#include <cstdint>

static int original_cutoff(int cutoff) {
  if (cutoff < 0) cutoff = 0;
  if (cutoff > 32767) cutoff = 32767;
  const unsigned phase = static_cast<unsigned>(cutoff) << 17;
  const unsigned index = phase >> 24;
  const int a = braids::lut_svf_cutoff[index];
  const int b = braids::lut_svf_cutoff[index + 1];
  return a + ((int64_t(b - a) * ((phase >> 8) & 65535)) >> 16);
}

static void check_step(int state, int sample, int f) {
  const int64_t product = int64_t(sample - state) * f;
  const int delta = product < 0 ? -int((-product + 32767) / 32768)
                                : int(product / 32768);
  int expected = state + delta;
  if (expected > 32767) expected = 32767;
  if (expected < -32767) expected = -32767;
  const int actual = macroosc::morph_filter_step(state, sample, f * 65536);
  assert(actual == expected && actual >= -32767 && actual <= 32767);
}

int main() {
  for (auto f : braids::lut_svf_cutoff) assert(f >= 38 && f <= 25078);
  for (int cutoff = -32768; cutoff < 65536; ++cutoff)
    assert(macroosc::morph_cutoff(cutoff) == original_cutoff(cutoff));
  // Every possible input/state difference, at every original coefficient
  // anchor. Choose valid sample/state pairs including both extrema.
  for (auto f : braids::lut_svf_cutoff) {
    for (int delta = -65535; delta <= 65534; ++delta) {
      const int state = delta < 0 ? 32767 : -32767;
      check_step(state, state + delta, f);
    }
  }
  // Every coefficient in the entire possible range, including values
  // between table anchors, with input and state boundary combinations.
  const int states[] = {-32767, -32766, -1, 0, 1, 32766, 32767};
  const int samples[] = {-32768, -32767, -1, 0, 1, 32766, 32767};
  for (int f = 38; f <= 25078; ++f)
    for (int state : states) for (int sample : samples) check_step(state, sample, f);
  unsigned seed = 41;
  for (unsigned i = 0; i < 1048576; ++i) {
    seed = seed * 1664525u + 1013904223u;
    const int state = int(seed % 65535) - 32767;
    seed = seed * 1664525u + 1013904223u;
    const int sample = int(seed & 65535) - 32768;
    seed = seed * 1664525u + 1013904223u;
    check_step(state, sample, 38 + seed % 25041);
  }
}
