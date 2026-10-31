#include "optimized/packed_tables.hpp"
#include "optimized/packed_tables.cpp"
#include "vendor/braids/resources.cc"
#include <opxy/q31.hpp>
#include <cstdio>

static int32_t floor_shift(int64_t value, unsigned bits) {
  const int64_t scale = int64_t(1) << bits;
  return static_cast<int32_t>(value < 0 ? -((-value + scale - 1) / scale) : value / scale);
}

int main() {
  // Harmonics' amplitude is normalized to 0..32767 and smoothed within it.
  const int32_t gains[] = {0, 1, 255, 256, 16383, 16384, 32766, 32767};
  for (int32_t sample = -32768; sample <= 32767; ++sample) {
    for (unsigned gain = 0; gain < sizeof(gains)/sizeof(gains[0]); ++gain) {
      if (opxy::q31::multiply(sample * 65536, gains[gain]) !=
          floor_shift(int64_t(sample) * gains[gain], 15)) {
        std::puts("harmonic amplitude scaling differs"); return 1;
      }
    }
  }
  // Exhaust all significant phase bits. The lowest eight are unused in 8.24.
  for (uint32_t position = 0; position < (1u << 24); ++position) {
    const uint32_t index = position >> 16;
    const int32_t a = braids::wav_sine[index], b = braids::wav_sine[index + 1];
    const int16_t expected = static_cast<int16_t>(a + floor_shift(int64_t(b-a) * (position & 65535), 16));
    if (macroosc::paired824(macroosc::sine_pairs, position << 8) != expected ||
        macroosc::sine_signed_half(position << 8) != expected) {
      std::puts("packed sine interpolation differs"); return 1;
    }
  }
  const int16_t* originals[] = {braids::ws_sine_fold, braids::ws_tri_fold, braids::ws_moderate_overdrive, braids::ws_violent_overdrive};
  const uint32_t* packed[] = {macroosc::sine_fold_pairs, macroosc::tri_fold_pairs, macroosc::moderate_pairs, macroosc::violent_pairs};
  for (unsigned table = 0; table < 4; ++table) {
    for (uint32_t position = 0; position < 65536; ++position) {
      const unsigned index = position >> 8;
      const int32_t a = originals[table][index], b = originals[table][index + 1];
      const int16_t expected = static_cast<int16_t>(a + floor_shift(int64_t(b-a) * (position & 255), 8));
      if (macroosc::paired88(packed[table], static_cast<uint16_t>(position)) != expected) {
        std::puts("packed curve interpolation differs"); return 1;
      }
    }
  }
  return 0;
}
