#pragma once
#include <opxy/audio.hpp>
#include <opxy/lease_pool.hpp>
#include "braids/macro_oscillator.h"

namespace macroosc {
using DelayLines = braids::DigitalOscillator::DelayLines;
struct Voice {
  opxy::u32 rng_state;
  opxy::u32 strike_pending;
  opxy::u32 delay_slot;
  opxy::u32 fm_phase;  // FM modulator phase, reset on each note
  opxy::u32 hold_phase;  // Rate: Q16 progress to the next held sample
  opxy::i32 held;        // Rate: the sample held, after bit reduction
  opxy::i32 drift_walk;  // Drift: the random walk, -32767..32767; kept across notes
  braids::MacroOscillator osc;
};
struct Shared {
  opxy::LeasePool<DelayLines, 16> delays;
};
// The SDK supplies a stable nonzero owner ID and preallocated instance context.
void initialize(Voice&, Shared&, opxy::u32 owner);
void start(Voice&, Shared&, opxy::u32 owner);
void destroy(Voice&, Shared&, opxy::u32 owner);
void render(Voice&, Shared&, opxy::u32 owner, opxy::Output, opxy::Parameters, opxy::Pitch);
}  // namespace macroosc
