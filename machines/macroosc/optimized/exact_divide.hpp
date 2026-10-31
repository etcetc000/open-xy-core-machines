#pragma once
#include <opxy/q31.hpp>
#include <stdint.h>
#include "division_table.hpp"

namespace macroosc {
// Exact quotient for the transition-time numerators and positive divisors
// below 2^31. The zero-divisor result mirrors the native helper, though the
// original C++ division is undefined there. No oscillator rate is changed.
//
// Let S=2^31, D=d*2^shift and T=S*S/(2*D). The table starts with R<=T.
// Each update is 2R-2DR^2/S^2 plus rounding error strictly less than zero:
// subtracting two compensates for the two truncated fractional multiplies.
// Thus R stays a lower bound. The quotient estimate cannot exceed n/d,
// its product with d cannot overflow, and residual correction is exact.
inline uint32_t transition_quotient(uint32_t numerator, uint32_t denominator) {
  if (numerator < denominator) return 0;
  if (denominator <= 1) return denominator ? numerator : 0xffffffffu;
  const unsigned shift = __builtin_clrsb(static_cast<int32_t>(denominator));
  const int32_t normalized = denominator << shift;
  int32_t reciprocal = reciprocal_seed[(normalized >> 21) - 512];
  reciprocal += 2 * opxy::q31::multiply(reciprocal,
      0x40000000 - opxy::q31::multiply(normalized, reciprocal)) - 2;
  // One refinement suffices for the usual transition quotient below 65536.
  // Refine again for larger quotients to bound the correction loop's work.
  if ((numerator >> 16) >= denominator) {
    reciprocal += 2 * opxy::q31::multiply(reciprocal,
        0x40000000 - opxy::q31::multiply(normalized, reciprocal)) - 2;
  }
  uint32_t quotient = static_cast<uint32_t>(opxy::q31::multiply(
      static_cast<int32_t>(numerator >> 1), reciprocal)) >> (29 - shift);
  uint32_t residual = numerator - quotient * denominator;
  while (residual >= denominator) { ++quotient; residual -= denominator; }
  return quotient;
}

// Classic Saw benefits from a call that limits register pressure. Other
// selected loops inline the same operation. Both paths are tested.
__attribute__((noinline)) inline uint32_t transition_quotient_call(uint32_t n, uint32_t d) {
  return transition_quotient(n, d);
}
}
