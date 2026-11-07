"""Wave Chords: the crossfaded four-stream loop as the native wave_quad routine.

Between exact wave positions every stream reads both waves at phase >> 1 and
crossfades them by a nonzero 16-bit balance; the sample is the four values'
sum >> 2. macroosc::native::wave_quad defines exactly that sum, so the phases,
reads, crossfade rounding and final shift are unchanged.
"""

OLD = '''  } else {
    // The adapter bounds calls to24 frames. Two streams per pass reduce
    // live working state, preserving the sum before the final division.
    int32_t partial_sum[24];
    for (unsigned frame = 0; frame < size; ++frame) {
      phase_0 += phase_increment_0;
      phase_1 += phase_increment[0];
      partial_sum[frame] = macroosc::wave_crossfade(wave_a, wave_b, phase_0 >> 1, wave_xfade)
          + macroosc::wave_crossfade(wave_a, wave_b, phase_1 >> 1, wave_xfade);
    }
    state_.saw.phase[0] = phase_0;
    state_.saw.phase[1] = phase_1;
    for (unsigned frame = 0; frame < size; ++frame) {
      phase_2 += phase_increment[1];
      phase_3 += phase_increment[2];
      const int32_t sample = partial_sum[frame]
          + macroosc::wave_crossfade(wave_a, wave_b, phase_2 >> 1, wave_xfade)
          + macroosc::wave_crossfade(wave_a, wave_b, phase_3 >> 1, wave_xfade);
      buffer[frame] = sample >> 2;
    }
    state_.saw.phase[2] = phase_2;
    state_.saw.phase[3] = phase_3;
  }'''

NEW = '''  } else {
    // The native four-stream crossfade: the same per-stream reads, crossfades
    // and sum >> 2 as wave_crossfade, in one pass.
    macroosc::native::WaveQuad quad;
    quad.a = wave_a;
    quad.b = wave_b;
    quad.phase[0] = phase_0;
    quad.phase[1] = phase_1;
    quad.phase[2] = phase_2;
    quad.phase[3] = phase_3;
    quad.increment[0] = phase_increment_0;
    quad.increment[1] = phase_increment[0];
    quad.increment[2] = phase_increment[1];
    quad.increment[3] = phase_increment[2];
    quad.balance = wave_xfade;
    macroosc::native::wave_quad(quad, buffer, size);
    for (unsigned i = 0; i < 4; ++i) state_.saw.phase[i] = quad.phase[i];
  }'''


def transform(source):
    if source.count(OLD) != 1:
        raise ValueError('Wave Chords crossfade loop changed')
    return '#include "native/wave_quad.hpp"\n' + source.replace(OLD, NEW)
