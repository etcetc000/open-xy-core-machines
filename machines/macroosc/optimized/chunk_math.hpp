#pragma once
#include <opxy/types.hpp>
namespace macroosc { namespace chunk_math {
opxy::u32 phase_delta(opxy::u32 numerator, unsigned size);
// Parameter interpolation always divides the same numerator. The adapter's
// three chunk sizes therefore have exact constant quotients.
inline opxy::u32 interpolation_increment(unsigned size){
  if(size==24)return 1365;
  if(size==8)return 4095;
  if(size==16)return 2047;
  return 32767u/size;
}
// Exact quotients for every supported analog chunk length. Keeping this
// lookup separate avoids changing the already measured parameter-0 paths.
// The unsigned range check preserves the general division outside 1..24.
inline opxy::u32 parameter1_increment(unsigned size){
  static const opxy::u16 values[24]={32767,16383,10922,8191,6553,5461,4681,4095,
    3640,3276,2978,2730,2520,2340,2184,2047,1927,1820,1724,1638,1560,1489,1424,1365};
  if(size-1u<24)return values[size-1];
  return 32767u/size;
}
}}
