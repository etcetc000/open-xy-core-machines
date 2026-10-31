"""Stage the independent FOF filters, preserving excitation and recurrence."""
import re


def transform(source):
    # The vendor renderer applies amplitudes[0] to every formant. All 25
    # entries of that column equal 16384, so both interpolations are constant.
    begin = source.index('static const int16_t formant_a_data')
    end = source.index('\n};', begin)
    gains = re.findall(r'\{\s*(\d+)\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*\}', source[begin:end])
    if len(gains) != 25 or set(gains) != {'16384'}:
        raise ValueError('FOF formant-zero gain is no longer constant')

    match = re.search(r'void DigitalOscillator::RenderVowelFof\([^{}]*\{.*?\n\}', source, re.S)
    if not match:
        raise ValueError('Missing FOF renderer')
    body = match[0]
    arrays = '  int32_t svf_lp[kNumFormants];\n  int32_t svf_bp[kNumFormants];\n  int16_t svf_f[kNumFormants];\n'
    gain = '''    // Preserve the original renderer's use of formant zero's amplitude for
    // all five filters; the other four amplitude calculations were unused.
    state_.fof.amplitude = InterpolateFormantParameter(
        formant_a_data, parameter_[1], parameter_[0], 0);
'''
    for old in (arrays, gain):
        if body.count(old) != 1:
            raise ValueError('FOF cached setup changed')
        body = body.replace(old, '')
    begin = body.index('  const int16_t amplitude = state_.fof.amplitude;')
    end = body.index('  uint32_t phase = phase_;', begin)
    body = body[:begin] + '''  const bool reset_filters = init_;
  init_ = false;
''' + body[end:]
    begin = body.index('  while (size) {')
    end = body.index('    int32_t in = this_saw_sample;', begin)
    excitation = body[begin:end].replace(
        '  while (size) {', '  for (unsigned sample = 0; sample < count; ++sample) {')
    if excitation.count('phase / (increment >> 16)') != 1:
        raise ValueError('FOF saw transition changed')
    excitation = excitation.replace('phase / (increment >> 16)',
                                    'macroosc::transition_quotient(phase, increment >> 16)')
    excitation += '''    excitation[sample] = this_saw_sample;
    sum[sample] = 0;
  }
'''
    # Each filter consumes the same excitation in the same order. Its
    # recurrence is independent of the other filters. Partial sums still
    # accumulate in formant order, with the original per-formant rounding.
    step = '''        const int32_t notch = excitation[INDEX] - (bp >> 6);
        lp += f * bp >> 15;
        CLIP(lp)
        const int32_t hp = notch - lp;
        bp += f * hp >> 15;
        CLIP(bp)
        // bp is clipped; bp * 16384 cannot overflow, and >>17 equals >>3.
        sum[INDEX] += bp >> 3;
'''
    body = body[:begin] + '''  // Adapter chunks are multiples of four and at most 24 samples.
  // The original renderer evaluates each filter at half the output rate.
  const unsigned count = size / 2;
  int32_t excitation[12], sum[12];
''' + excitation + '''  for (unsigned formant = 0; formant < 5; ++formant) {
    int32_t lp = reset_filters ? 0 : state_.fof.svf_lp[formant];
    int32_t bp = reset_filters ? 0 : state_.fof.svf_bp[formant];
    const int16_t f = state_.fof.coefficients[formant];
    for (unsigned sample = 0; sample < count; sample += 2) {
      {
''' + step.replace('INDEX', 'sample') + '''      }
      {
''' + step.replace('INDEX', 'sample + 1') + '''      }
    }
    state_.fof.svf_lp[formant] = lp;
    state_.fof.svf_bp[formant] = bp;
  }
  for (unsigned sample = 0; sample < count; ++sample) {
    int32_t out = sum[sample];
    CLIP(out)
    *buffer++ = (out + previous_sample) >> 1;
    *buffer++ = out;
    previous_sample = out;
  }
  phase_ = phase;
  state_.fof.next_saw_sample = next_saw_sample;
  state_.fof.previous_sample = previous_sample;
}
'''
    return '#include "optimized/exact_divide.hpp"\n' + source[:match.start()] + body + source[match.end():]
