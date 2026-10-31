// Check the optimized byte-wave helpers against the original wrapping formula.
#include "optimized/stmlib/utils/dsp.h"
#include <cstdint>
#include <cstdio>

static int32_t original_product_high(int32_t a, uint32_t b) {
  const uint32_t bits = static_cast<uint32_t>(a) * b;
  const int64_t signed_value = bits <= 0x7fffffffu ? static_cast<int64_t>(bits)
      : static_cast<int64_t>(bits) - (int64_t(1) << 32);
  return signed_value < 0 ? -static_cast<int32_t>((-signed_value + 65535) / 65536)
      : static_cast<int32_t>(signed_value / 65536);
}
static int16_t reference(uint8_t a, uint8_t b, uint32_t fraction) {
  return static_cast<int16_t>((static_cast<int32_t>(a) << 8) - 32768 +
      original_product_high(static_cast<int32_t>(b) - a, fraction));
}
int main() {
  const uint32_t fractions[] = {0, 1, 255, 256, 65535, 65536, 0x7fffff,
      0x800000, 0xffff00, 0xfffffe, 0xffffff};
  uint8_t a[2], b[2];
  for (unsigned x = 0; x < 256; ++x) {
    for (unsigned y = 0; y < 256; ++y) {
      a[0] = static_cast<uint8_t>(x); a[1] = static_cast<uint8_t>(y);
      for (unsigned i = 0; i < sizeof(fractions)/sizeof(fractions[0]); ++i) {
        if (stmlib::MacroInterpolateByte(a, fractions[i]) != reference(a[0], a[1], fractions[i])) {
          std::puts("byte interpolation differs from original wrapping arithmetic"); return 1;
        }
      }
    }
  }
  uint32_t rng = 0x2b9d1357u;
  for (unsigned i = 0; i < 1000000; ++i) {
    rng = rng * 1664525u + 1013904223u;
    a[0] = rng; a[1] = rng >> 8; b[0] = rng >> 16; b[1] = rng >> 24;
    rng = rng * 1664525u + 1013904223u;
    const uint32_t fraction = rng & 0xffffff;
    const uint16_t balance = static_cast<uint16_t>(rng ^ (rng >> 16));
    const int32_t left = reference(a[0], a[1], fraction);
    const int32_t right = reference(b[0], b[1], fraction);
    const int16_t expected = static_cast<int16_t>(left + original_product_high(right-left, balance));
    if (stmlib::MacroCrossfadeByte(a, b, fraction, balance) != expected) {
      std::puts("byte crossfade differs from original wrapping arithmetic"); return 1;
    }
  }
  return 0;
}
