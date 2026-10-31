#include "optimized/division_table.cpp"
#include "optimized/exact_divide.hpp"
#include <cassert>
#include <stdint.h>
#include <initializer_list>
int main(){
 uint32_t rng=0x5292ac7f;
 for(uint32_t d=1;d<=65535;++d){
   const uint32_t quotients[]={0,1,2,127,255,32767,65535,65536,65537,0xffffffffu/d};
   for(uint32_t q:quotients){
     uint64_t n=uint64_t(q)*d;
     if(n<=0xffffffffu){assert(macroosc::transition_quotient(n,d)==n/d);if(n)assert(macroosc::transition_quotient(n-1,d)==(n-1)/d);if(n<0xffffffffu)assert(macroosc::transition_quotient(n+1,d)==(n+1)/d);}
   }
   for(unsigned j=0;j<64;++j){rng=rng*1664525u+1013904223u;assert(macroosc::transition_quotient(rng,d)==rng/d);}
 }
 // Larger divisors include the sync master's phase increment shifted by 7.
 for(unsigned bit=1;bit<31;++bit){
   const uint32_t center=uint32_t(1)<<bit;
   for(uint32_t d: {center-1,center,center+1}){
     const uint32_t qs[]={0,1,127,65535,65536,0xffffffffu/d};
     for(uint32_t q:qs){
       uint64_t n=uint64_t(q)*d;
       if(n<=0xffffffffu){assert(macroosc::transition_quotient(n,d)==n/d);if(n)assert(macroosc::transition_quotient(n-1,d)==(n-1)/d);if(n<0xffffffffu)assert(macroosc::transition_quotient(n+1,d)==(n+1)/d);}
     }
   }
 }
 for(unsigned i=0;i<1000000;++i){
   rng=rng*1664525u+1013904223u;const uint32_t d=(rng&0x7fffffffu)|1u;
   rng=rng*1664525u+1013904223u;
   assert(macroosc::transition_quotient(rng,d)==rng/d);
 }
}
