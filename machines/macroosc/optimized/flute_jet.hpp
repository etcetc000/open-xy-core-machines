// Exact byte-delay jet lookup; derived from the MIT-licensed Braids renderer.
#pragma once
#include <stdint.h>
namespace macroosc {
// The original weighted byte mix is floor((a*(65535-b)+c*b)/65536).
// It remains a signed byte. The generated table folds its negative clamp
// and doubled lookup index into the unsigned-byte representation.
inline __attribute__((always_inline)) unsigned flute_jet_index(int8_t a,int8_t c,uint16_t b) {
  const int32_t mixed=a+(((int32_t(c)-a)*int32_t(b)-a)>>16);
  return static_cast<uint8_t>(mixed);
}
}
