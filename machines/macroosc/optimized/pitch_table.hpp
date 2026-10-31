#pragma once
#include <opxy/types.hpp>

namespace macroosc {
// All 1536 integer pitches in the top octave, including the original
// interpolation truncation. Callers retain their original octave scaling.
extern const opxy::u32 pitch_increments[1536];
}
