#pragma once
#include <opxy/effect.hpp>

namespace ringmod {
struct State {
    opxy::u32 phase;
    opxy::i32 increment;
    opxy::i32 shape;
    opxy::i32 low_cut;
    opxy::i32 low_left;
    opxy::i32 low_right;
    opxy::u32 started;
};
void initialize(State&);
void process(State&, opxy::Channel left, opxy::Channel right, opxy::Parameters parameters);
} // namespace ringmod
