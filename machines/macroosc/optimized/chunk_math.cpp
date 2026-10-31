#include "chunk_math.hpp"
#include <opxy/q31.hpp>

namespace macroosc { namespace chunk_math {
// Keep this out of the oscillator translation units: inlining the branches
// increases register pressure in several sample loops. The call replaces the
// existing general-purpose division call without expanding those loops.
opxy::u32 phase_delta(opxy::u32 numerator, unsigned size) {
  if (size == 24) {
    // floor(n / 24) = floor(floor(n / 8) / 3). For x <= 2^29-1,
    // floor(x * ceil(2^31/3) / 2^31) is exactly floor(x / 3).
    return opxy::q31::multiply(static_cast<opxy::i32>(numerator >> 3), 715827883);
  }
  if (size == 8) return numerator >> 3;
  if (size == 16) return numerator >> 4;
  return numerator / size;
}
}}
