#include "optimized/normalization.hpp"
#include <cassert>

static unsigned reference(opxy::u32 value) {
  unsigned shifts = 0;
  while (!(value & 0x80000000u)) { value <<= 1; ++shifts; }
  return shifts;
}
static void check(opxy::u32 value) {
  if (value) assert(macroosc::normalization_shift(value) == reference(value));
}
int main() {
  for (unsigned bit = 0; bit < 32; ++bit) {
    const opxy::u32 value = 1u << bit;
    check(value - 1); check(value); check(value + 1);
  }
  opxy::u32 rng = 0x54a79b31u;
  for (unsigned i = 0; i < 1000000; ++i) {
    rng = rng * 1664525u + 1013904223u;
    check(rng);
  }
}
