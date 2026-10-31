#pragma once
#include <stdint.h>
#include <opxy/q31.hpp>
namespace macroosc {
// Bore samples are signed bytes scaled by512: [-65536,65024]. The stable
// body filter stays within [-65536,65536], with coefficients30..2867.
// Combining the terms therefore cannot overflow; integer state passes
// through the original floor operation exactly.
inline __attribute__((always_inline)) int32_t flute_body(int32_t state,int32_t bore,int32_t multiplier) {
  return state+opxy::q31::multiply(multiplier,bore+state);
}
inline __attribute__((always_inline)) int32_t flute_dc(int32_t state) {
  return opxy::q31::multiply(4055*524288,state);
}
inline __attribute__((always_inline)) int32_t flute_noise(uint32_t word,int32_t multiplier) {
  return opxy::q31::multiply(static_cast<int32_t>(word&0xffff0000u),multiplier);
}
// Noise is in[-16800,16799] and the original envelope table in[0,21299].
// noise*(2*envelope)>>15 equals noise*envelope>>14, with both factors
// signed16 and no overflow. Do not narrow the combined breath pressure.
inline __attribute__((always_inline)) int32_t flute_envelope_noise(
    int16_t noise, int16_t envelope) {
  return (noise * envelope) >> 14;
}
}
