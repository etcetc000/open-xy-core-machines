#pragma once
#include <opxy/types.hpp>

// Where the Pick control plucks the string, shared by the sound and the screen.
namespace karplus {
// Pick's sweep: from 2% of the string (at the bridge: thin, twangy) to 50%
// (the middle: round and hollow), evenly in tone. Position p = 0.02 * 25^c for
// the control c, in 17 steps (Q15 fractions of the string).
const opxy::u16 kPickPositions[17] = {655,  801,  980,  1198, 1465, 1792,  2191,  2680, 3277,
                                      4007, 4900, 5992, 7327, 8960, 10957, 13398, 16384};
// A pickup at p passes the fundamental at 2 sin(pi p): the gain (Q12, at most 4)
// that brings it back to level.
const opxy::u16 kPickupLevels[17] = {16384, 16384, 16384, 16384, 14625, 11979, 9820, 8060, 6627,
                                     5464,  4524,  3769,  3170,  2705,  2360,  2135, 2048};

// A table value at control (Q31 0..1), interpolated between its 17 entries.
inline opxy::u32 along(const opxy::u16 (&table)[17], opxy::i32 control) {
    const opxy::u32 c = control < 0 ? 0u : static_cast<opxy::u32>(control) >> 15;  // 0..65535
    const opxy::u32 at = c >> 12, part = c & 4095;
    if (at >= 16) return table[16];
    return (table[at] * (4096 - part) + table[at + 1] * part) >> 12;
}
}
