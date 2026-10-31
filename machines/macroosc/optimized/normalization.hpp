#pragma once
#include <opxy/types.hpp>

namespace macroosc {
// The caller handles zero. Shifting a nonzero unsigned word right once makes
// it nonnegative; its redundant sign bits then equal the original leading
// zero count, including value 1 (shifted to zero, with 31 redundant sign bits).
// This compiler built-in emits a native sign-bit count on the target.
inline unsigned normalization_shift(opxy::u32 value) {
  return __builtin_clrsb(static_cast<opxy::i32>(value >> 1));
}
}
