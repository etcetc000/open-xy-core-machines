"""Use the shared exact wave table; omit the zero-weight second waveform."""
import re


def transform(source):
    match = re.search(r'void DigitalOscillator::RenderWaveParaphonic\([^{}]*\{.*?\n\}', source, re.S)
    if not match:
        raise ValueError('Missing chord renderer')
    body = match[0].replace('void DigitalOscillator::RenderWaveParaphonic(',
        'template<bool Pure> void DigitalOscillator::RenderWaveParaphonicPaired(')
    old = '''  const uint8_t* wave_1 = wt_waves + mini_wave_line[parameter_[0] >> 10] * 129;
  const uint8_t* wave_2 = wt_waves + mini_wave_line[(parameter_[0] >> 10) + 1] * 129;'''
    if body.count(old) != 1:
        raise ValueError('Chord wave selection changed')
    body = body.replace(old, '''  const uint32_t* wave_a = macroosc::wave_compact + mini_wave_line[parameter_[0] >> 10] * 128;
  const uint32_t* wave_b = macroosc::wave_compact + mini_wave_line[(parameter_[0] >> 10) + 1] * 128;''')
    begin = body.index('  while (size) {')
    body = body[:begin] + '''  // At an exact wave position the second waveform has zero weight. All
  // intermediate positions retain both interpolations and their rounding.
  if (Pure) {
    for (unsigned frame = 0; frame < size; ++frame) {
      phase_0 += phase_increment_0;
      phase_1 += phase_increment[0];
      phase_2 += phase_increment[1];
      phase_3 += phase_increment[2];
      int32_t sample = macroosc::wave_sample(wave_a, phase_0 >> 1);
      sample += macroosc::wave_sample(wave_a, phase_1 >> 1);
      sample += macroosc::wave_sample(wave_a, phase_2 >> 1);
      sample += macroosc::wave_sample(wave_a, phase_3 >> 1);
      buffer[frame] = sample >> 2;
    }
    state_.saw.phase[0] = phase_0;
    state_.saw.phase[1] = phase_1;
    state_.saw.phase[2] = phase_2;
    state_.saw.phase[3] = phase_3;
  } else {
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
  }
}

void DigitalOscillator::RenderWaveParaphonic(const uint8_t* sync, int16_t* buffer, size_t size) {
  if ((parameter_[0] & 1023) == 0) RenderWaveParaphonicPaired<true>(sync, buffer, size);
  else RenderWaveParaphonicPaired<false>(sync, buffer, size);
}
'''
    return source[:match.start()] + body + source[match.end():]
