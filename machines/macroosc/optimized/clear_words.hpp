#pragma once
#include <stddef.h>
#include <opxy/types.hpp>

namespace macroosc {
// Zero `bytes` (a multiple of four) from a word-aligned address, a word at a
// time: the freestanding memset stores a byte at a time. The delay lines this
// clears start a word-aligned DelayPool slot at word-multiple offsets.
typedef opxy::u32 __attribute__((may_alias)) AliasedWord;
inline void clear_words(void* destination, size_t bytes) {
  AliasedWord* words = static_cast<AliasedWord*>(destination);
  for (size_t i = 0; i < bytes / 4; ++i) words[i] = 0;
}
}  // namespace macroosc
