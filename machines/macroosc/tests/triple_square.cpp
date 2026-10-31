#include "optimized/division_table.cpp"
#include "optimized/analog_oscillator.cpp"
#include "optimized/chunk_math.cpp"
#include "optimized/packed_tables.cpp"
#include "optimized/pitch_table.cpp"
#include "optimized/resources.cpp"
#include <cassert>

// The unspecialized renderer is the independent reference. Exercise switches
// from variable PWM without a shape Init, every short chunk length, larger
// fallback chunks, pitch clamps and subsequent ordinary renders to expose
// any difference in retained phase/increment/BLEP state.
int main() {
  braids::AnalogOscillator reference, candidate;
  std::memset(static_cast<void*>(&reference), 0, sizeof reference);
  std::memset(static_cast<void*>(&candidate), 0, sizeof candidate);
  reference.Init(); candidate.Init();
  uint8_t sync[64] = {};
  int16_t expected[64], actual[64];
  uint32_t seed = 0x75a5d139u;
  const int pitches[] = {-4096, 0, 1, 1536, 7680, 10240, 15359, 16383, 22000};
  unsigned compared = 0;
  for (unsigned block = 0; block < 98304; ++block) {
    seed = seed * 1664525u + 1013904223u;
    const unsigned count = block % 31 == 0 ? 64 : 1 + block % 24;
    const int pitch = block % 3 == 0 ? pitches[(block / 3) % 9]
                                    : static_cast<int>((seed >> 16) & 16383);
    const bool ordinary = block % 13 < 3;
    const auto shape = block % 101 == 0 ? braids::OSC_SHAPE_SAW
                                       : braids::OSC_SHAPE_SQUARE;
    const int parameter = ordinary ? seed & 32767 : 0;
    reference.set_shape(shape); candidate.set_shape(shape);
    reference.set_pitch(pitch); candidate.set_pitch(pitch);
    reference.set_parameter(parameter); candidate.set_parameter(parameter);
    reference.Render(sync, expected, nullptr, count);
    if (ordinary) {
      candidate.Render(sync, actual, nullptr, count);
    } else {
      const bool first = block & 1;
      for (unsigned i = 0; i < count; ++i) {
        const int16_t initial = first ? 0 : static_cast<int16_t>(seed + i * 171u);
        actual[i] = initial;
        expected[i] = static_cast<int16_t>(initial + ((expected[i] * 21) >> 6));
      }
      if (first) candidate.RenderTripleFirst(sync, actual, nullptr, count);
      else candidate.RenderTripleMix(sync, actual, nullptr, count);
    }
    for (unsigned i = 0; i < count; ++i) assert(expected[i] == actual[i]);
    compared += count;
  }
  assert(compared == 1392162u);
}
