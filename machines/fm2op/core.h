#pragma once
#include <opxy/audio.hpp>

namespace fm2op {
using opxy::i32;
using opxy::u32;

// Only sound state. The SDK owns firmware headers, slot padding and allocation.
struct Voice {
    u32 carrier_phase;
    u32 modulator_phase;
    i32 last_modulator;
    i32 previous_modulator;
    i32 brightness;
    u32 initialized;
    i32 ratio;
    i32 amount;
    i32 feedback;
    i32 decay;
};
// 2op voices share nothing.
struct Shared {};
}
