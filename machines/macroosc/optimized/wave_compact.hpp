#pragma once
#include <stdint.h>
#include <opxy/q31.hpp>
namespace macroosc {
// One aligned word carries a signed16 base and exact signed16 slope.
extern const uint32_t wave_compact[32768];
extern const uint8_t wave_line_indices[65];
inline __attribute__((always_inline)) int16_t wave_sample(const uint32_t* wave,uint32_t phase) {
  const uint32_t pair=wave[phase>>24];
  const int32_t base=static_cast<int16_t>(pair);
  const int32_t delta=static_cast<int32_t>(pair)>>16;
  return base+opxy::q31::multiply(delta*32768,static_cast<int32_t>(phase&0xffffff));
}
inline __attribute__((always_inline)) int16_t wave_crossfade(const uint32_t* a,const uint32_t* b,uint32_t phase,uint16_t balance) {
  const int32_t left=wave_sample(a,phase),right=wave_sample(b,phase);
  return left+opxy::q31::multiply((right-left)*32768,static_cast<int32_t>(balance));
}
// Only the low16 bits of the interpolation survive the final int16 result.
// Wrapping the product modulo2^32 before shifting therefore changes the
// unbounded signed quotient by multiples of65536, discarded by that result.
// This form is cheaper in Wave Map's four-wave loop; the fractional-multiply
// form above remains faster for Wave Line and the other measured call sites.
inline __attribute__((always_inline)) int16_t wave_sample_wrapped(const uint32_t* wave,uint32_t phase) {
  const uint32_t pair=wave[phase>>24];
  const int32_t base=static_cast<int16_t>(pair);
  const int32_t delta=static_cast<int32_t>(pair)>>16;
  return base+((static_cast<uint32_t>(delta)*(phase&0xffffff))>>16);
}
inline __attribute__((always_inline)) int16_t wave_crossfade_wrapped(const uint32_t* a,const uint32_t* b,uint32_t phase,uint16_t balance) {
  const int32_t left=wave_sample_wrapped(a,phase),right=wave_sample_wrapped(b,phase);
  return left+opxy::q31::multiply((right-left)*32768,static_cast<int32_t>(balance));
}
// Quantized phases have no fractional part: use the exact table base.
inline __attribute__((always_inline)) int16_t wave_grid_sample(const uint32_t* wave,uint32_t phase) {
 return static_cast<int16_t>(wave[phase>>24]);
}
inline __attribute__((always_inline)) int16_t wave_grid_crossfade(const uint32_t* a,const uint32_t* b,uint32_t phase,uint16_t balance) {
 const int32_t left=wave_grid_sample(a,phase),right=wave_grid_sample(b,phase);
 return left+opxy::q31::multiply((right-left)*32768,static_cast<int32_t>(balance));
}
inline __attribute__((always_inline)) int16_t wave_sample_crossfade(int16_t a,int16_t b,uint16_t balance) {
 const int32_t left=a,right=b;
 return left+opxy::q31::multiply((right-left)*32768,static_cast<int32_t>(balance));
}
}
