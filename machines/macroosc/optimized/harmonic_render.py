"""Elide harmonic state copies without changing smoothing or partial selection."""
import re


def transform(source):
    match = re.search(r'void DigitalOscillator::RenderHarmonics\([^{}]*\{.*?\n\}', source, re.S)
    if not match:
        raise ValueError('Missing Harmonics renderer')
    body = match[0].replace('int16_t* buffer,', 'int16_t* __restrict__ buffer,')
    replacements = [
        ('  int32_t amplitude[kNumAdditiveHarmonics];',
         '  int32_t* __restrict__ amplitude = state_.hrm.amplitude;'),
        ('  int32_t target_amplitude[kNumAdditiveHarmonics];',
         '  int32_t masked_targets[kNumAdditiveHarmonics];\n  const int32_t* target_amplitude = state_.hrm.target;'),
        ('    amplitude[i] = state_.hrm.amplitude[i];\n', ''),
        ('''  for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
    state_.hrm.amplitude[i] = amplitude[i];
  }
''', ''),
        ('''    if (*sync++ || *sync++) {
      phase = 0;
    }''', '    // The adapter supplies zero external sync for this renderer.'),
    ]
    for old, new in replacements:
        if body.count(old) != 1:
            raise ValueError('Harmonics state or sync access changed')
        body = body.replace(old, new)
    old = '''  for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {
    target_amplitude[i] = (phase_increment >> 16) * (i + 1) > 0x4000 ? 0 : state_.hrm.target[i];
  }'''
    new = '''  // Use the cached targets directly while all twelve partials are in
  // range. Otherwise preserve the original per-partial frequency mask.
  if ((phase_increment >> 16) * kNumAdditiveHarmonics > 0x4000) {
    const unsigned keep=macroosc::transition_quotient(0x4000,phase_increment>>16);
    for(unsigned i=0;i<keep;++i)masked_targets[i]=state_.hrm.target[i];
    for(unsigned i=keep;i<12;++i)masked_targets[i]=0;
    target_amplitude = masked_targets;
  }'''
    if body.count(old) != 1:
        raise ValueError('Harmonics target preparation changed')
    body = body.replace(old, new)
    begin = body.index('    int32_t peak =')
    end = body.index('\n  }\n  // Use the cached', begin)
    body = body[:begin] + '    PrepareHarmonicSpectrum(state_.hrm.target,parameter_[0],parameter_[1]);' + body[end:]
    # The marker already sets each halfword's sign bit. Masking them before
    # OR is redundant; zero-extend the low half to avoid sign spill.
    body = body.replace('(static_cast<uint32_t>(parameter_[0] & 0x7fff) << 16) | (parameter_[1] & 0x7fff)',
        '(static_cast<uint32_t>(parameter_[0]) << 16) | static_cast<uint16_t>(parameter_[1])')
    # Count forward through the same twelve partials with explicit cursors.
    # This avoids the original end-pointer setup without reversing summation
    # or changing when each amplitude is smoothed.
    begin = body.index('    uint32_t harmonic_phase = phase;')
    end = body.index('    CLIP(out)', begin)
    loop = body[begin:end]
    old = '    for (size_t i = 0; i < kNumAdditiveHarmonics; ++i) {'
    if loop.count(old) != 1:
        raise ValueError('Harmonics partial loop changed')
    loop = loop.replace(old, '    int32_t* amplitude_cursor=amplitude;\n'
                        '    const int32_t* target_cursor=target_amplitude;\n'
                        '    for(unsigned remaining=kNumAdditiveHarmonics;remaining;--remaining) {')
    loop = loop.replace('target_amplitude[i]', '*target_cursor').replace('amplitude[i]', '*amplitude_cursor')
    loop = loop.replace('      *amplitude_cursor += (*target_cursor - *amplitude_cursor) >> 8;',
                        '      *amplitude_cursor += (*target_cursor++ - *amplitude_cursor) >> 8;\n'
                        '      ++amplitude_cursor;')
    body = body[:begin] + loop + body[end:]
    return '#include "optimized/harmonic_spectrum.hpp"\n' + source[:match.start()] + body + source[match.end():]

