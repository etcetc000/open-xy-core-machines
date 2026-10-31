#include "optimized/exact_mix.hpp"
#include "optimized/byte_mix.hpp"
#include <cassert>
#include <cstdint>
static int16_t reference(int16_t a,int16_t b,uint16_t balance) {
  const int64_t n=int64_t(a)*(65535-balance)+int64_t(b)*balance;
  return static_cast<int16_t>(n<0?-((-n+65535)/65536):n/65536);
}
int main() {
  const int16_t values[]={-32768,-32767,-1,0,1,32766,32767};
  const uint16_t balances[]={0,1,127,255,256,32767,32768,32769,65534,65535};
  for(int a=-32768;a<=32767;++a)for(auto b:values)for(auto balance:balances)
    assert(macroosc::mix_exact(static_cast<int16_t>(a),b,balance)==reference(a,b,balance));
  unsigned seed=12345;
  for(unsigned i=0;i<1000000;++i) {
    seed=seed*1664525u+1013904223u;const int16_t a=seed>>16;
    seed=seed*1664525u+1013904223u;const int16_t b=seed>>16;
    seed=seed*1664525u+1013904223u;const uint16_t balance=seed>>16;
    assert(macroosc::mix_exact(a,b,balance)==reference(a,b,balance));
  }
  // Every byte pair at the rounding/balance anchors, then every balance
  // for byte extrema, zero and sign boundaries. Test both physical scales
  // against the independent wide-integer floor reference.
  for(int a=-128;a<=127;++a)for(int b=-128;b<=127;++b)for(auto balance:balances) {
    const int32_t expected=reference(a,b,balance);
    assert(macroosc::mix_byte<8>(a,b,balance)==expected*256);
    assert(macroosc::mix_byte<9>(a,b,balance)==expected*512);
  }
  const int8_t bytes[]={-128,-127,-1,0,1,126,127};
  for(auto a:bytes)for(auto b:bytes)for(unsigned balance=0;balance<65536;++balance) {
    const int32_t expected=reference(a,b,balance);
    assert(macroosc::mix_byte<8>(a,b,balance)==expected*256);
    assert(macroosc::mix_byte<9>(a,b,balance)==expected*512);
  }
}
