// Continuous FM ratio, interpolating definition.json's harmonic anchors.
#pragma once
namespace macroosc {
static const unsigned char kRatioHalves[] = {1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16};
const unsigned kRatios = sizeof(kRatioHalves) / sizeof(kRatioHalves[0]);
inline unsigned RatioQ16FromParameter(int q31) {
  const unsigned position = (q31 < 0 ? 0 : static_cast<unsigned>(q31) >> 16) * (kRatios - 1);
  const unsigned index = position >> 15;
  return (unsigned(kRatioHalves[index]) << 15) +
      (kRatioHalves[index + 1] - kRatioHalves[index]) * (position & 32767);
}
// floor(increment * ratio / 65536), modulo the phase word, without 64-bit
// helpers. The only low-word product is at most 65535 squared.
inline unsigned RatioIncrement(unsigned increment, unsigned ratio) {
  return increment * (ratio >> 16) + (increment >> 16) * (ratio & 65535) +
      ((increment & 65535) * (ratio & 65535) >> 16);
}
}  // namespace macroosc
