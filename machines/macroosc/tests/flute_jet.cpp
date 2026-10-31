#include "optimized/flute_jet.hpp"
#include "optimized/resources.cpp"
#define braids original_braids
#include "vendor/braids/resources.cc"
#undef braids
#include <cassert>
#include <cstdint>
#include <cstdio>

static unsigned checks=0;
static void Check(int a,int b,int balance) {
  if(balance<0 || balance>65535)return;
  const int64_t weighted=int64_t(a)*(65535-balance)+int64_t(b)*balance;
  const int64_t mixed=weighted/65536-(weighted<0 && weighted%65536!=0);
  assert(mixed>=-128 && mixed<=127);
  const unsigned index=macroosc::flute_jet_index(a,b,balance);
  assert(index==static_cast<uint8_t>(mixed));
  int original_index=static_cast<int>(mixed*512);
  if(original_index<0)original_index=0;
  if(original_index>65535)original_index=65535;
  assert(braids::lut_blowing_jet[index]==original_braids::lut_blowing_jet[original_index>>8]);
  ++checks;
}
int main() {
  static_assert(sizeof(braids::lut_blowing_jet)==sizeof(original_braids::lut_blowing_jet),"Jet table extent changed");
  for(unsigned i=0;i<256;++i) {
    const int value=i<128?int(i):int(i)-256;
    const int original_index=value<0?0:value*2;
    assert(braids::lut_blowing_jet[i]==original_braids::lut_blowing_jet[original_index]);
  }
  assert(braids::lut_blowing_jet[256]==original_braids::lut_blowing_jet[256]);
  // Every byte pair, endpoints, and both sides of every quotient boundary.
  // These are the only places its affine weighted sum can change floor value.
  for(int a=-128;a<=127;++a)for(int b=-128;b<=127;++b) {
    Check(a,b,0);Check(a,b,65535);
    if(a==b)continue;
    const int low=(a<b?a:b)-1,high=(a>b?a:b)+1;
    for(int q=low;q<=high;++q) {
      const int center=static_cast<int>((int64_t(q)*65536-int64_t(a)*65535)/(b-a));
      Check(a,b,center-1);Check(a,b,center);Check(a,b,center+1);
    }
  }
  std::printf("%u exact jet boundary checks pass\n",checks);
}
