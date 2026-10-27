#pragma once
#include <opxy/audio.hpp>

namespace karplus {
// A delay line holds one period of a string: 2048 samples reach about 21.5 Hz
// at 44.1 kHz, below the OP-XY's lowest note.
const unsigned kLength = 2048, kMask = kLength - 1;
struct String {
    opxy::i16 line[kLength];  // Q15 samples
    opxy::i32 previous;       // the loop filter's last input
    opxy::i32 input, output;  // the fractional-delay allpass's last input and output
};
// Two strings, the second tuned apart by Spread; at Spread 0 they are one.
struct Voice {
    String strings[2];
    opxy::u32 write;    // next write position, shared by both lines
    opxy::u32 seed;     // excitation noise
    opxy::u32 excited;  // 0 until the note's first block fills the lines
};
// State shared by every voice; empty when voices share nothing.
struct Shared {};
}
