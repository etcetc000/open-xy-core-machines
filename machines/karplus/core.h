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
// Where a string reads and how its fractional delay filters. Setting it takes
// divisions, so it is kept while the pitch and controls hold.
struct Tap {
    opxy::u32 delay;        // whole samples read back
    opxy::i32 coefficient;  // Q15, (1 - d) / (1 + d) for the remaining d samples
    opxy::u32 pickup;       // samples between the string and the pickup's second reading
};
// Low-passed noise for the pluck.
struct Noise {
    opxy::u32 seed;
    opxy::i32 smooth;  // the low-pass's state
};
// The pluck as it is laid into the first line, a few hundred samples a block,
// so a low note's long line costs no block more than a sounding string does.
struct Pluck {
    opxy::u32 length;     // one period at the note's start, in samples
    opxy::u32 done;       // samples laid so far
    opxy::u32 peak;       // the pick point, samples from the bridge
    opxy::u32 up, down;   // the pulled shape's slope either side of the peak, Q8
    opxy::u32 shape;      // the pulled shape at the next sample, Q8, weighed by what the noise leaves
    opxy::i32 burst;      // the noise's share, Q15
    opxy::i32 smoothing;  // the noise's low-pass weight, Q15
    Noise ahead;
};
// Two strings, the second tuned apart by Spread; at Spread 0 they are one.
struct Voice {
    // No part of a new voice needs clearing: initialize, start and a note's
    // pluck set everything the strings read (SDKs that clear only a declared
    // prefix skip the 8 KB lines; others clear the whole voice as before).
    static constexpr unsigned zeroed_bytes = 0;
    String strings[2];
    opxy::u32 write;    // next write position, shared by both lines
    opxy::u32 seed;     // excitation noise
    opxy::u32 excited;  // 0 at a note's start, 1 while its pluck is laid, 2 once it sounds
    opxy::u32 age;      // frames since the strings began to sound, up to kLength
    Pluck pluck;
    // The pitch and controls the taps were set for, and what they set.
    opxy::u32 increment;
    opxy::i32 controls[3];
    Tap taps[2];
    opxy::i32 level, damp;
};
// State shared by every voice; empty when voices share nothing.
struct Shared {};
}
