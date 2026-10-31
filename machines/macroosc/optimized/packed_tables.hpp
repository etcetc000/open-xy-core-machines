#pragma once
#include <stdint.h>
#include <opxy/q31.hpp>

namespace macroosc {
// Unsigned bases with signed slopes. The original unsigned product's wrap
// contributes only multiples of65536 after shifting, discarded by uint16.
// Both forms preserve the original result; use the measured cheaper form.
inline __attribute__((always_inline)) uint16_t paired_fractional_u824(const uint32_t* table,uint32_t phase) {
  const uint32_t pair=table[phase>>24];
  const uint32_t base=static_cast<uint16_t>(pair);
  const int32_t delta=static_cast<int32_t>(pair)>>16;
  return base+opxy::q31::multiply(delta*32768,static_cast<int32_t>((phase>>8)&65535));
}

inline __attribute__((always_inline)) uint16_t paired_u824(const uint32_t* table,uint32_t phase) {
  const uint32_t pair=table[phase>>24];
  const uint32_t base=static_cast<uint16_t>(pair);
  const int32_t delta=static_cast<int32_t>(pair)>>16;
  return base+((static_cast<uint32_t>(delta)*((phase>>8)&65535))>>16);
}

extern const uint32_t sine_pairs[256];
extern const uint32_t sine_half_pairs[512];
// Each original interval has two entries with the same signed slope. The
// lower half is based at a; the upper half at b. Casting the fraction to
// int16 subtracts 65536 in the upper half, so b+d*(f-65536)/65536 equals
// a+d*f/65536 exactly, including floor rounding. Both multiply operands
// now fit signed16. The low eight phase bits remain unused.
inline __attribute__((always_inline)) int16_t sine_signed_half(uint32_t phase) {
  const uint32_t pair=sine_half_pairs[phase>>23];
  const int16_t delta=pair>>16;
  const int16_t fraction=phase>>8;
  return static_cast<int16_t>(pair)+((delta*fraction)>>16);
}

extern const uint32_t svf_cutoff_pairs[256];
extern const uint32_t svf_damp_pairs[256];
extern const uint32_t svf_scale_pairs[256];
extern const uint32_t resonator_coefficient_pairs[128];
extern const uint32_t resonator_scale_pairs[128];
extern const uint32_t bell_pairs[256];

extern const uint32_t moderate_pairs[256];
extern const uint32_t violent_pairs[256];

extern const uint32_t sine_fold_pairs[256];
extern const uint32_t tri_fold_pairs[256];

// One aligned load supplies both the original sample and its exact slope.
// Generated slopes fit int16_t; interpolation retains the original rounding.
inline __attribute__((always_inline)) int16_t paired824(const uint32_t* table, uint32_t phase) {
  const uint32_t pair = table[phase >> 24];
  const int32_t base = static_cast<int16_t>(pair);
  const int32_t delta = static_cast<int32_t>(pair) >> 16;
  return base + ((delta * static_cast<int32_t>((phase >> 8) & 0xffff)) >> 16);
}

inline __attribute__((always_inline)) int16_t paired88(const uint32_t* table, uint16_t index) {
  const uint32_t pair = table[index >> 8];
  const int32_t base = static_cast<int16_t>(pair);
  const int32_t delta = static_cast<int32_t>(pair) >> 16;
  return base + ((delta * static_cast<int32_t>(index & 0xff)) >> 8);
}
}
