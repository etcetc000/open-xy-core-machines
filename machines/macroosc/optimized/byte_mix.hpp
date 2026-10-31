#pragma once
#include <stdint.h>

namespace macroosc {
// These delay lines contain signed bytes. The residual is bounded by
// +/-16711553, so signed32 arithmetic cannot overflow. Retain the -a
// correction from a*(65535-balance)+b*balance and the original floor.
template<unsigned Shift>
inline __attribute__((always_inline)) int32_t mix_byte(
    int8_t a, int8_t b, uint16_t balance) {
  static_assert(Shift == 8 || Shift == 9, "Byte-delay output scale");
  const int32_t result = a + (((int32_t(b) - a) * int32_t(balance) - a) >> 16);
  // Multiplication also defines the negative-input case in portable C++.
  return result * (1 << Shift);
}
}
