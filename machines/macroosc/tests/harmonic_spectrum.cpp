#include "optimized/division_table.cpp"
#include "optimized/harmonic_spectrum.hpp"
#include <cassert>
#include <cstdio>

// Independent original integer formulas: neither reciprocal helper nor
// transformed renderer participates in this reference.
static void Reference(int32_t* target,int32_t timbre,int32_t color) {
  const int32_t peak=12*timbre>>7,second_peak=(peak>>1)+12*128;
  const int32_t amount=color*color>>15;
  const int32_t fourth=color<16384?color>>6:511-(color>>6);
  const int32_t root=fourth*fourth>>10,width=root*root+4;
  int32_t total=0;
  for(unsigned i=0;i<12;++i) {
    int32_t d=int32_t(i<<8)-peak;
    int32_t g=32768*128/(128+d*d/width);
    d=int32_t(i<<8)-second_peak;
    g+=amount*128/(128+d*d/width);
    total+=g;target[i]=g;
  }
  assert(total>0);
  const int32_t attenuation=2147483647/total;
  for(unsigned i=0;i<12;++i)target[i]=target[i]*attenuation>>16;
}
static void Check(unsigned timbre,unsigned color) {
  int32_t expected[12],actual[12];
  Reference(expected,timbre,color);
  braids::PrepareHarmonicSpectrum(actual,timbre,color);
  for(unsigned i=0;i<12;++i)assert(actual[i]==expected[i]);
}
int main() {
  static_assert(braids::kNumAdditiveHarmonics==12,"Spectrum reference partial count");
  // Every possible prepared width and every possible absolute distance.
  for(unsigned root=0;root<=63;++root) {
    const unsigned width=root*root+4;
    const braids::PreparedHarmonicWidth divider(width);
    for(unsigned d=0;d<=3072;++d)assert(divider.Divide(d*d)==d*d/width);
  }
  const unsigned edges[]={0,1,16383,16384,32766,32767};
  // Every frequency-mask divisor: compare the cutoff count to the original
  // per-partial multiplication predicate, including all crossing boundaries.
  for(unsigned d=0;d<=65535;++d) {
    const unsigned keep=d*12>16384?macroosc::transition_quotient(16384,d):12;
    assert(keep<=12);
    for(unsigned i=0;i<12;++i)assert((i<keep)==(d*(i+1)<=16384));
  }
  for(unsigned p=0;p<32768;++p)for(unsigned edge:edges) {
    Check(p,edge);Check(edge,p);
  }
  uint32_t rng=0x78c1a035;
  for(unsigned i=0;i<65536;++i) {
    rng=rng*1664525u+1013904223u;Check(rng&32767,(rng>>16)&32767);
    const int16_t a=static_cast<int16_t>(i),b=static_cast<int16_t>(rng);
    const uint32_t old_key=0x80008000u|(uint32_t(a&32767)<<16)|(b&32767);
    const uint32_t new_key=0x80008000u|(uint32_t(a)<<16)|uint16_t(b);
    assert(old_key==new_key);
  }
  std::puts("Harmonic spectrum:458752 control pairs and all width/distance combinations exact");
}
