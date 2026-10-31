"""Share exact drum pitch normalization and narrow its bounded interpolation."""


def transform(source):
    start = source.index('void DigitalOscillator::RenderStruckDrum(')
    end = source.index('\nvoid DigitalOscillator::RenderPlucked(', start)
    body = source[start:end]
    pitches = '''  for (size_t i = 0; i < kNumDrumPartials; ++i) {
    int16_t partial_pitch = pitch_ + kDrumPartials[i];
    state_.add.partial_phase_increment[i] = ComputePhaseIncrement(partial_pitch) << 1;
  }'''
    amplitude = '''      int32_t amplitude = a->partial_amplitude[i] + \\
          (((a->target_partial_amplitude[i] - a->partial_amplitude[i]) * fade) >> 15);'''
    replacements = [
        (pitches, '  macroosc::struck_drum_increments(state_.add.partial_phase_increment, pitch_);'),
        ('  int32_t fade = 0;', '  int16_t fade = -32768;'),
        (amplitude, '''      const int16_t amplitude = macroosc::struck_drum_amplitude(
          a->partial_amplitude[i], a->target_partial_amplitude[i], fade);'''),
    ]
    for old, new in replacements:
        if body.count(old) != 1:
            raise ValueError('Struck Drum pitch or interpolation changed')
        body = body.replace(old, new)
    if '  0, 0, 1041, 1747, 1846, 3072\n' not in source:
        raise ValueError('Drum partial pitches changed')
    if '  16986, 2654, 3981, 5308, 3981, 2985\n' not in source:
        raise ValueError('Drum amplitude bounds changed')
    return '#include "optimized/struck_drum_math.hpp"\n' + source[:start] + body + source[end:]
