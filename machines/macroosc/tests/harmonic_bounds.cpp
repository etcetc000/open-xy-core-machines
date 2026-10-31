#include "optimized/division_table.cpp"
#include "optimized/harmonic_spectrum.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>

// Range checks complement the algebraic proof; they do not replace it.
// Independent 64-bit floor division checks the rounded gain at each sample.
static int64_t Floor(int64_t x,int64_t divisor) {
  return x/divisor-(x<0 && x%divisor!=0);
}
int main() {
  int32_t amplitude[12]={},target[12]={};
  uint32_t rng=0x77b0a3d1;
  unsigned contributions=0;
  const int16_t sine_edges[]={-32768,-32767,-1,0,1,32766,32767};
  // Arbitrary signed16 samples, independently changing per partial, cover a
  // strictly wider input set than any actual collection of harmonic phases.
  for(unsigned step=0;step<1048576;++step) {
    rng=rng*1664525u+1013904223u;
    if((step&31)==0) {
      braids::PrepareHarmonicSpectrum(target,rng&32767,(rng>>16)&32767);
      const unsigned keep=(rng>>8)%13;
      for(unsigned i=keep;i<12;++i)target[i]=0;
    }
    if(step%7919==0)for(unsigned i=0;i<12;++i)amplitude[i]=0;
    int32_t before=0,after=0,targets=0,out=0;
    for(unsigned i=0;i<12;++i) {
      assert(target[i]>=0 && target[i]<=32767);
      assert(amplitude[i]>=0 && amplitude[i]<=32767);
      before+=amplitude[i];targets+=target[i];
      rng=rng*1664525u+1013904223u;
      const int16_t sine=step%8<7?sine_edges[step%8]:static_cast<int16_t>(rng>>16);
      const int32_t contribution=opxy::q31::multiply(static_cast<int32_t>(sine)*65536,amplitude[i]);
      assert(contribution==Floor(int64_t(sine)*amplitude[i],32768));
      assert(contribution>=-amplitude[i] && contribution<=amplitude[i]);
      out+=contribution;
      const int32_t next=amplitude[i]+((target[i]-amplitude[i])>>8);
      assert(next==Floor(255LL*amplitude[i]+target[i],256));
      amplitude[i]=next;after+=next;
      ++contributions;
    }
    assert(before<=32767 && targets<=32767 && after<=32767);
    assert(out>=-32767 && out<=32767);
  }
  assert(contributions==12582912);
  std::puts("1048576 amplitude transitions and12582912 gain bounds pass");
}
