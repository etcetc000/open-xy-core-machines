#include "braids/digital_oscillator.h"
#include <cstddef>

// Harmonics deliberately uses the previous-sample slot of the larger union
// member. In-place amplitude writes are safe only while that slot remains
// disjoint from the harmonic amplitudes, cache key and spectrum targets.
static_assert(offsetof(braids::AdditiveState, previous_sample) >= sizeof(braids::HarmonicsState),
              "Harmonic state overlaps the previous-sample slot");
int main() {}
