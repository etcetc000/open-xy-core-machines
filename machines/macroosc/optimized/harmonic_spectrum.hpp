#pragma once
#include "braids/digital_oscillator.h"
#include "exact_divide.hpp"

namespace braids {
// Width is in4..3973 and squared distances are below2^24. Prepare the
// same lower-bound reciprocal used by transition_quotient once for all24
// distance divisions. Residual correction preserves the exact quotient.
struct PreparedHarmonicWidth {
  uint32_t denominator,quotient_shift;
  int32_t reciprocal;
  explicit PreparedHarmonicWidth(uint32_t d):denominator(d) {
    const unsigned shift=__builtin_clrsb(static_cast<int32_t>(d));
    const int32_t normalized=d<<shift;
    quotient_shift=29-shift;
    reciprocal=macroosc::reciprocal_seed[(normalized>>21)-512];
    for(unsigned i=0;i<2;++i) reciprocal+=2*opxy::q31::multiply(reciprocal,
       0x40000000-opxy::q31::multiply(normalized,reciprocal))-2;
  }
  uint32_t Divide(uint32_t n) const {
    uint32_t q=static_cast<uint32_t>(opxy::q31::multiply(static_cast<int32_t>(n>>1),reciprocal))>>quotient_shift;
    uint32_t residual=n-q*denominator;
    while(residual>=denominator) {++q;residual-=denominator;}
    return q;
  }
};
static __attribute__((noinline)) void PrepareHarmonicSpectrum(int32_t* target,int32_t timbre,int32_t color) {
    int32_t peak = (kNumAdditiveHarmonics * timbre) >> 7;
    int32_t second_peak = (peak >> 1) + kNumAdditiveHarmonics * 128;
    int32_t second_peak_amount = color * color >> 15;

    int32_t sqrtsqrt_width = color < 16384
        ? color >> 6 : 511 - (color >> 6);
    int32_t sqrt_width = sqrtsqrt_width * sqrtsqrt_width >> 10;
    int32_t width = sqrt_width * sqrt_width + 4;
    const PreparedHarmonicWidth divider(width);
    int32_t total = 0;
    for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
      int32_t x = i << 8;
      int32_t d, g;

      d = (x - peak);
      g = macroosc::transition_quotient(32768 * 128,128 + divider.Divide(d * d));

      d = (x - second_peak);
      g += macroosc::transition_quotient(second_peak_amount * 128,128 + divider.Divide(d * d));
      total += g;
      target[i] = g;
    }

    int32_t attenuation = macroosc::transition_quotient(2147483647,total);
    for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
      target[i] = target[i] * attenuation >> 16;
    }
}

} // namespace braids
