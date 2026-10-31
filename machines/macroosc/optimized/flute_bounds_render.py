"""Use Flute's byte-delay and envelope bounds without changing its recurrence."""


def transform(source):
    start = source.index('void DigitalOscillator::RenderFluted(')
    end = source.index('\nstruct WavetableDefinition', start)
    body = source[start:end]
    replacements = [
        ('''    if (jet_table_index > 65535) {
      jet_table_index = 65535;
    }
''', ''),
        ('    CLIP(out)', '    if (out < -32767) out = -32767;'),
        ('    random_pressure = random_pressure * breath_pressure >> 15;',
         '''    random_pressure = macroosc::flute_envelope_noise(
        static_cast<int16_t>(random_pressure), static_cast<int16_t>(breath_pressure >> 1));'''),
        ('    uint32_t bore_delay_ptr = delay_ptr - bore_delay_integral;\n', ''),
        ('    uint32_t jet_delay_ptr = delay_ptr - jet_delay_integral;\n', ''),
        ('    ++delay_ptr;', '    ++delay_ptr; ++bore_delay_ptr; ++jet_delay_ptr;'),
    ]
    for old, new in replacements:
        if body.count(old) != 4:
            raise ValueError('Flute four-step loop or sample bounds changed')
        body = body.replace(old, new)
    old = '  while (size) {'
    if body.count(old) != 1:
        raise ValueError('Flute loop count changed')
    body = body.replace(old, '''  // Both delay interpolants are signed bytes scaled by512. Their jet
  // index is at most254 and their output at most32512: upper clamps are
  // unreachable. Keep the original lower output clamp at -32767.
  // Rolling read indexes equal delay_ptr-delay_integral modulo2^32.
  uint32_t bore_delay_ptr = delay_ptr - bore_delay_integral;
  uint32_t jet_delay_ptr = delay_ptr - jet_delay_integral;
  while (size) {''')
    for kind in ('bore', 'jet'):
        expected = 'macroosc::mix_byte<9>(' + kind + '_dl_a, ' + kind + '_dl_b, ' + kind + '_delay_fractional)'
        if body.count(expected) != 4:
            raise ValueError('Flute byte-delay scaling changed')
    return source[:start] + body + source[end:]
