#include "optimized/flute_math.hpp"
#include "vendor/braids/resources.cc"
#include <cassert>
#include <cstdint>

static int32_t floor12(int64_t n) {
  return static_cast<int32_t>(n<0?-((-n+4095)/4096):n/4096);
}
int main() {
  static_assert(static_cast<unsigned>(0.99*4096)==4055,"Original DC coefficient changed");
  static_assert(sizeof(braids::lut_blowing_envelope)/sizeof(uint16_t)==197,
                "Original blowing envelope changed");
  for (auto envelope : braids::lut_blowing_envelope) {
    assert(envelope <= 21299);
    for (int noise = -16800; noise <= 16799; ++noise) {
      const int64_t product = int64_t(noise) * (2 * int(envelope));
      const int expected = product < 0 ? -int((-product + 32767) / 32768)
                                       : int(product / 32768);
      assert(macroosc::flute_envelope_noise(noise, envelope) == expected);
    }
  }
  // Byte interpolation emits a signed byte scaled by512. Exhaust every
  // possible result, including the negative full-scale output clamp.
  for (int byte = -128; byte <= 127; ++byte) {
    const int mixed = byte * 512;
    int old_index = mixed;
    if (old_index < 0) old_index = 0;
    if (old_index > 65535) old_index = 65535;
    const int new_index = mixed < 0 ? 0 : mixed;
    assert((old_index >> 8) == (new_index >> 8) && (new_index >> 8) <= 254);
    int old_out = mixed >> 1;
    if (old_out > 32767) old_out = 32767;
    if (old_out < -32767) old_out = -32767;
    const int new_out = (mixed >> 1) < -32767 ? -32767 : (mixed >> 1);
    assert(old_out == new_out);
  }
  for(auto f:braids::lut_flute_body_filter)assert(f>=30 && f<=2867);
  const int states[]={-65536,-65535,-1,0,1,65024,65536};
  for(auto f:braids::lut_flute_body_filter)for(auto s:states)for(int b=-128;b<=127;++b) {
    const int bore=b*512;
    assert(macroosc::flute_body(s,bore,-int32_t(f)*524288)==floor12(-int64_t(f)*bore+int64_t(4096-f)*s));
  }
  for(int s=-262144;s<=262144;++s)
    assert(macroosc::flute_dc(s)==floor12(int64_t(4055)*s));
  const unsigned intensities[]={53,54,255,256,1024,2047,2048,2100};
  for(int s=-32768;s<=32767;++s)for(auto intensity:intensities) {
    const uint32_t word=static_cast<uint32_t>(s)<<16;
    assert(macroosc::flute_noise(word,intensity*8)==floor12(int64_t(s)*intensity));
  }
  unsigned rng=73;
  for(unsigned i=0;i<1000000;++i) {
    rng=rng*1664525u+1013904223u;const int state=int(rng%131073)-65536;
    rng=rng*1664525u+1013904223u;const int bore=int(rng%131073)-65536;
    const int f=braids::lut_flute_body_filter[i%128];
    assert(macroosc::flute_body(state,bore,-f*524288)==floor12(-int64_t(f)*bore+int64_t(4096-f)*state));
  }
}
