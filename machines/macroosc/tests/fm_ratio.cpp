#include <stdint.h>
#include <cstdio>
#include "fm.h"
static uint64_t count=0;
static bool check(uint32_t increment,uint32_t ratio) {
 ++count;
 const uint32_t expected=static_cast<uint32_t>((static_cast<uint64_t>(increment)*ratio)>>16);
 if(macroosc::RatioIncrement(increment,ratio)!=expected) {
  std::printf("Mismatch %u %u\n",increment,ratio);return false;
 }
 return true;
}
int main() {
 const uint32_t high[]={0,1,32767,32768,65535};
 const uint32_t ratios[]={0,1,65535,65536,65537,131071,524282,524288,0x7fffffffu,0xffffffffu};
 for(uint32_t h:high)for(uint32_t low=0;low<65536;++low)for(uint32_t r:ratios)
  if(!check((h<<16)|low,r))return 1;
 const uint32_t inc[]={0,1,32767,32768,65535,65536,0x7fff0000u,0x7fffffffu,0x80000000u,0xffffffffu};
 for(uint32_t f=0;f<65536;++f)for(uint32_t i:inc)
  if(!check(i,(7u<<16)|f))return 2;
 uint32_t state=0x12345678u;
 for(unsigned n=0;n<1048576;++n) {
  state=state*1664525u+1013904223u;const uint32_t i=state;
  state=state*1664525u+1013904223u;if(!check(i,state))return 3;
 }
 std::printf("PASS %llu exact full-domain ratio cases\n",static_cast<unsigned long long>(count));
}
