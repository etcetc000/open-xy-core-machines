// Continuous FM ratio, interpolating definition.json's harmonic anchors.
#pragma once
#include <opxy/q31.hpp>
namespace macroosc {
static const unsigned char kRatioHalves[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16};
const unsigned kRatios = sizeof(kRatioHalves) / sizeof(kRatioHalves[0]);
inline unsigned RatioQ16FromParameter(int q31) {
  const unsigned position = (q31 < 0 ? 0 : static_cast<unsigned>(q31) >> 16) * (kRatios - 1);
  const unsigned index = position >> 15;
  return (unsigned(kRatioHalves[index]) << 15) +
      (kRatioHalves[index + 1] - kRatioHalves[index]) * (position & 32767);
}
// floor(increment * ratio / 65536), modulo the phase word. Split the
// integer/fractional ratio and the increment's top bit. Multiplying the
// low31 increment by the fraction scaled by2^15 gives the exact fractional
// quotient through q31. The top-bit term is already an integer, so adding
// it preserves floor rounding. This remains exact for all uint32 inputs;
// the audio caller's increment clamp lets its compiler omit the top term.
inline unsigned RatioIncrement(unsigned increment, unsigned ratio) {
  const unsigned fraction=(ratio&65535u)<<15;
  return increment*(ratio>>16)+static_cast<unsigned>(opxy::q31::multiply(
      static_cast<opxy::i32>(increment&0x7fffffffu),static_cast<opxy::i32>(fraction)))+
      ((increment>>31)*fraction);
}
}  // namespace macroosc
