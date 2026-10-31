#include "optimized/division_table.cpp"
#include "optimized/analog_oscillator.cpp"
#include "optimized/chunk_math.cpp"
#include "optimized/packed_tables.cpp"
#include "optimized/pitch_table.cpp"
#include "optimized/resources.cpp"
#include <cassert>

// The general renderer retains the original transition divisions. Compare
// every changed no-sync waveform and both direct Triple mixing operations
// against it, including parameter/pitch ramps and shape-change initialization.
int main() {
  const braids::AnalogOscillatorShape shapes[] = {
    braids::OSC_SHAPE_CSAW, braids::OSC_SHAPE_SQUARE,
    braids::OSC_SHAPE_SAW, braids::OSC_SHAPE_VARIABLE_SAW
  };
  const braids::AnalogOscillatorShape triple_shapes[] = {
    braids::OSC_SHAPE_SQUARE, braids::OSC_SHAPE_SAW,
    braids::OSC_SHAPE_TRIANGLE, braids::OSC_SHAPE_SINE
  };
  for (int sample = -32768; sample <= 32767; ++sample)
    assert(opxy::q31::multiply(sample, 704643072) == ((sample * 21) >> 6));
  uint32_t seed = 0x57acf132u;
  for (unsigned triple = 0; triple < 2; ++triple) {
    braids::AnalogOscillator reference, candidate;
    // Match the adapter's zeroed storage before the vendor Init, which
    // intentionally leaves some previous-state members untouched.
    std::memset(static_cast<void*>(&reference), 0, sizeof reference);
    std::memset(static_cast<void*>(&candidate), 0, sizeof candidate);
    reference.Init(); candidate.Init();
    uint8_t silent[24] = {};
    int16_t expected[24], actual[24];
    for (unsigned block = 0; block < 24576; ++block) {
      seed = seed * 1664525u + 1013904223u;
      const unsigned count = 8 * (1 + block % 3);
      const auto shape = triple ? triple_shapes[(block / 17) % 4] : shapes[(block / 17) % 4];
      const int pitch = static_cast<int>((seed >> 16) & 16383) + (block % 11 == 0 ? 8191 : 0);
      const int parameter = triple ? 0 : seed & 32767;
      const int auxiliary = (seed >> 8) & 32767;
      reference.set_shape(shape); candidate.set_shape(shape);
      reference.set_pitch(pitch); candidate.set_pitch(pitch);
      reference.set_parameter(parameter); candidate.set_parameter(parameter);
      reference.set_aux_parameter(auxiliary); candidate.set_aux_parameter(auxiliary);
      reference.Render(silent, expected, nullptr, count);
      if (!triple) {
        candidate.RenderNoSync(silent, actual, nullptr, count);
      } else {
        const bool first = block % 3 == 0;
        for (unsigned i = 0; i < count; ++i) {
          const int16_t initial = first ? 0 : static_cast<int16_t>(seed + i * 171u);
          actual[i] = initial;
          expected[i] = static_cast<int16_t>(initial + ((expected[i] * 21) >> 6));
        }
        if (shape == braids::OSC_SHAPE_TRIANGLE || shape == braids::OSC_SHAPE_SINE) {
          if (first) candidate.RenderTripleSimpleFirst(silent, actual, nullptr, count);
          else candidate.RenderTripleSimpleMix(silent, actual, nullptr, count);
        } else {
          if (first) candidate.RenderTripleFirst(silent, actual, nullptr, count);
          else candidate.RenderTripleMix(silent, actual, nullptr, count);
        }
      }
      for (unsigned i = 0; i < count; ++i) assert(expected[i] == actual[i]);
    }
  }
}
