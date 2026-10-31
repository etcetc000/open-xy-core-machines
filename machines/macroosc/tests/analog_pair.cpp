#include "optimized/division_table.cpp"
#include "optimized/analog_oscillator.cpp"
#include "optimized/chunk_math.cpp"
#include "optimized/packed_tables.cpp"
#include "optimized/pitch_table.cpp"
#include "optimized/resources.cpp"
#include <cassert>

// Compare paired preparation with two independent generic renders. Change
// shapes independently so Init may reset one pitch to C4 while leaving the
// other untouched. Follow some pairs with ordinary renders to expose any
// difference in retained phase, increment, parameter or clamped pitch state.
int main() {
  braids::AnalogOscillator reference[2], candidate[2];
  // Match the adapter's intentionally zeroed storage before Braids Init().
  std::memset(static_cast<void*>(reference), 0, sizeof(reference));
  std::memset(static_cast<void*>(candidate), 0, sizeof(candidate));
  for (unsigned i = 0; i < 2; ++i) { reference[i].Init(); candidate[i].Init(); }
  uint8_t silent[24] = {};
  int16_t expected[2][24], actual[2][24];
  const int pitches[] = {-3072, -1, 0, 1, 7679, 7680, 16383, 16384, 16385, 24575};
  unsigned seed = 71;
  for (unsigned block = 0; block < 24576; ++block) {
    seed = seed * 1664525u + 1013904223u;
    const unsigned count = 1 + block % 24;
    const int first = pitches[(block / 24) % 10];
    const int second = block % 3 ? first : pitches[(block / 240) % 10];
    for (unsigned i = 0; i < 2; ++i) {
      const auto shape = static_cast<braids::AnalogOscillatorShape>((block / (17 + i * 6)) % 9);
      const int pitch = i ? second : first;
      const int parameter = static_cast<int>((seed >> (i * 16)) & 32767);
      reference[i].set_shape(shape); candidate[i].set_shape(shape);
      reference[i].set_pitch(pitch); candidate[i].set_pitch(pitch);
      reference[i].set_parameter(parameter); candidate[i].set_parameter(parameter);
      reference[i].set_aux_parameter(parameter); candidate[i].set_aux_parameter(parameter);
      reference[i].Render(silent, expected[i], nullptr, count);
    }
    candidate[0].RenderPairNoSync(candidate[1], actual[0], actual[1], count);
    for (unsigned i = 0; i < 2; ++i) {
      assert(reference[i].phase_increment() == candidate[i].phase_increment());
      for (unsigned sample = 0; sample < count; ++sample)
        assert(expected[i][sample] == actual[i][sample]);
      if (block % 5 == 0) {
        reference[i].Render(silent, expected[i], nullptr, count);
        candidate[i].RenderNoSync(nullptr, actual[i], nullptr, count);
        for (unsigned sample = 0; sample < count; ++sample)
          assert(expected[i][sample] == actual[i][sample]);
      }
    }
  }
}
