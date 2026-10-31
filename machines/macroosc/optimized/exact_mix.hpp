#pragma once
#include <stdint.h>
namespace macroosc {
// a*(65535-balance)+b*balance = a*65536+(b-a)*balance-a.
// Explicit unsigned wrapping preserves the int16 result even when the
// residual exceeds int32. Retain the original floor rounding and -a term.
inline __attribute__((always_inline)) int16_t mix_exact(int16_t a,int16_t b,uint16_t balance) {
  const uint32_t residual=static_cast<uint32_t>(static_cast<int32_t>(b)-a)*balance-static_cast<uint32_t>(a);
  return a+(static_cast<int32_t>(residual)>>16);
}
}
