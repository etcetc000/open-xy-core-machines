#pragma once
#include <opxy/effect.hpp>

namespace pingpong {
// One second per side: two lines of Q31 samples, as stock's delay keeps.
const opxy::u32 kLine = 44100;
const opxy::u32 kBufferWords = 2 * kLine;  // definition.json: bufferBytes 352800
struct State {
    opxy::u32 write;              // the next position in each line
    opxy::u32 written;            // positions written since initialize, up to kLine
    opxy::i32 delay;              // smoothed, Q8 samples
    opxy::i32 feedback, tone;     // smoothed, Q31
    opxy::i32 low_left, low_right;  // the tone filters on the repeats
};
void initialize(State& state);
void process(State& state, opxy::Buffer buffer, opxy::Channel left, opxy::Channel right,
             opxy::Parameters parameters);
}
