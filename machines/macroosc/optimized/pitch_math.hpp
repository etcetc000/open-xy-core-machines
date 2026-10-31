#pragma once
#include <opxy/types.hpp>

namespace macroosc {
// Fold a nonpositive table-relative pitch into its top octave. Both vendor
// callers clamp an int16 pitch at or below 128*128 first, so ref is in
// [-49152, 0]. This is exactly ceil(-ref/1536), without a division or loop.
inline unsigned octave_shifts(opxy::i32 ref) {
  // Narrow the proven 0..98 intermediate so the native compiler can use a
  // single signed 16-bit multiply instead of a general 32-bit multiply.
  const opxy::i16 thirds = static_cast<opxy::i16>(
      (static_cast<unsigned>(-ref) + 1535u) >> 9);
  // thirds <= 98; floor(thirds * 171 / 512) == floor(thirds / 3).
  return (thirds * 171) >> 9;
}
}
