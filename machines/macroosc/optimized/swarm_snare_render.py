"""Exact adjacent-pitch lookup and independent Snare filter passes."""


def transform(source):
    start = source.index('void DigitalOscillator::RenderSawSwarm(')
    end = source.index('\nvoid DigitalOscillator::RenderComb(', start)
    body = source[start:end]
    old = '''    int32_t increment_a = ComputePhaseIncrement(pitch_ + detune_integral);
    int32_t increment_b = ComputePhaseIncrement(pitch_ + detune_integral + 1);
    increments[i] = increment_a + \\
        (((increment_b - increment_a) * detune_fractional) >> 16);'''
    if body.count(old) != 1:
        raise ValueError('Saw Swarm adjacent pitch setup changed')
    body = body.replace(old, '''    increments[i] = macroosc::swarm_increment(
        pitch_ + detune_integral, detune_fractional);''')
    source = '#include "optimized/swarm_pitch.hpp"\n' + source[:start] + body + source[end:]

    start = source.index('void DigitalOscillator::RenderSnare(')
    end = source.index('\nvoid DigitalOscillator::RenderCymbal(', start)
    body = source[start:end]
    begin = body.index('  while (size) {')
    if body.count('ProcessNoPunch<SVF_MODE_BP>') != 3:
        raise ValueError('Snare prepared filters changed')
    body = body[:begin] + '''  // Each path owns its pulse/filter state. Only the last path uses RNG.
  // Staging reduces live working state without reordering any path's history.
  // Adapter chunks are even and at most 24 samples; Snare runs at half rate.
  int32_t summed[12];
  const unsigned count = size / 2;
  // Zero state with no delayed trigger is an absorbing pulse state. The
  // filters still ring, so process both filters for every original sample.
  if (pulse_[0].silent() && pulse_[1].silent() && pulse_[2].silent()) {
    for (unsigned i = 0; i < count; ++i) {
      summed[i] = (svf_[0].ProcessNoPunchLocal<SVF_MODE_BP>(0) * g_1) >> 15;
    }
    for (unsigned i = 0; i < count; ++i) {
      summed[i] += (svf_[1].ProcessNoPunchLocal<SVF_MODE_BP>(0) * g_2) >> 15;
    }
  } else {
  for (unsigned i = 0; i < count; ++i) {
    int32_t excitation_1 = pulse_[0].Process() + pulse_[1].Process();
    excitation_1 += !pulse_[1].done() ? 2621 : 0;
    summed[i] = (svf_[0].ProcessNoPunchLocal<SVF_MODE_BP>(excitation_1) + (excitation_1 >> 4)) * g_1 >> 15;
  }
  for (unsigned i = 0; i < count; ++i) {
    int32_t excitation_2 = pulse_[2].Process();
    excitation_2 += !pulse_[2].done() ? 13107 : 0;
    summed[i] += (svf_[1].ProcessNoPunchLocal<SVF_MODE_BP>(excitation_2) + (excitation_2 >> 4)) * g_2 >> 15;
  }
  }
  for (unsigned i = 0; i < count; ++i) {
    const int32_t noise_sample = Random::GetSample() * pulse_[3].Process() >> 15;
    int32_t sd = summed[i] + svf_[2].ProcessNoPunchLocal<SVF_MODE_BP>(noise_sample);
    CLIP(sd);
    *buffer++ = sd;
    *buffer++ = sd;
  }
}
'''
    return source[:start] + body + source[end:]
