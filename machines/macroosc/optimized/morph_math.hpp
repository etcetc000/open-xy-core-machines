#pragma once
#include "packed_tables.hpp"

namespace macroosc {
// The original table is constant at25078 from entry113 through its end.
// This also subsumes the old upper cutoff clamp at32767. The lower range
// retains the original packed interpolation and its floor rounding.
inline __attribute__((always_inline)) int32_t morph_cutoff(int32_t cutoff) {
  if (cutoff < 0) cutoff = 0;
  return cutoff >= 14464 ? 25078 : paired_u824(svf_cutoff_pairs, cutoff << 17);
}

// f is in[38,25078]; multiplier=f*65536 fits signed32. Sample/state
// difference is bounded by65535, so the original integer product cannot
// overflow. q31 gives the same floor(delta*f/32768) with one operation.
// The convex recurrence cannot exceed32767; retain the lower clamp because
// a -32768 input can produce -32768 before the original -32767 clamp.
inline __attribute__((always_inline)) int32_t morph_filter_step(
    int32_t state, int32_t sample, int32_t multiplier) {
  state += opxy::q31::multiply(sample - state, multiplier);
  if (state < -32767) state = -32767;
  return state;
}
}
