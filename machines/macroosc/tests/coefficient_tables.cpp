#include "optimized/packed_tables.hpp"
#include "optimized/packed_tables.cpp"
#include "vendor/braids/resources.cc"
#include <cassert>
#include <cstdio>
static int32_t Floor16(int64_t n) {
  return static_cast<int32_t>(n<0?-((-n+65535)/65536):n/65536);
}
int main() {
  const uint16_t* originals[]={braids::lut_svf_cutoff,braids::lut_svf_damp,
    braids::lut_svf_scale,braids::lut_resonator_coefficient,
    braids::lut_resonator_scale,braids::lut_bell};
  const uint32_t* packed[]={macroosc::svf_cutoff_pairs,macroosc::svf_damp_pairs,
    macroosc::svf_scale_pairs,macroosc::resonator_coefficient_pairs,
    macroosc::resonator_scale_pairs,macroosc::bell_pairs};
  const unsigned segments[]={256,256,256,128,128,256};
  unsigned checked=0;
  // Every significant phase bit. The low eight input bits are unused.
  for(unsigned t=0;t<6;++t)for(uint32_t pos=0;pos<segments[t]*65536u;++pos) {
    const unsigned i=pos>>16;
    const int32_t a=originals[t][i],b=originals[t][i+1];
    const uint16_t expected=static_cast<uint16_t>(a+Floor16(int64_t(b-a)*(pos&65535)));
    const uint32_t phase=pos<<8;
    assert(macroosc::paired_u824(packed[t],phase)==expected);
    assert(macroosc::paired_fractional_u824(packed[t],phase)==expected);
    ++checked;
  }
  assert(checked==83886080u);
  std::puts("Unsigned coefficient interpolation:83886080 phases exact in both forms");
}
