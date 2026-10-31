"""Exact direct mixing of the four triple-oscillator waveforms."""
import re


def specialize_sine_chunk(body):
    begin = body.index('  while (size--) {')
    end = body.index('\n  }', begin) + 4
    loop = body[begin:end]
    step = loop[len('  while (size--) {'):-4]
    body = body[:begin] + '''  if (size == 8) {
    // The FM chunk has exactly eight updates. Preserve the falling/equal
    // phase-increment complement, including its original extra decrement.
    const uint32_t phase_increment_increment = previous_phase_increment_ < phase_increment_
        ? (phase_increment_ - previous_phase_increment_) >> 3
        : ~((previous_phase_increment_ - phase_increment_) >> 3);
''' + ''.join('    {\n' + step + '\n    }\n' for _ in range(8)) + '''  } else {
    const uint32_t phase_increment_increment = previous_phase_increment_ < phase_increment_
        ? macroosc::chunk_math::phase_delta(phase_increment_ - previous_phase_increment_, size)
        : ~macroosc::chunk_math::phase_delta(previous_phase_increment_ - phase_increment_, size);
''' + loop + '\n  }' + body[end:]
    return body.replace('  BEGIN_INTERPOLATE_PHASE_INCREMENT',
                        '  uint32_t phase_increment = previous_phase_increment_;')


def transform_macro(source):
    old = '''  std::fill(&buffer[0], &buffer[size], 0);
  for (size_t i = 0; i < 3; ++i) {
    analog_oscillator_[i].RenderNoSync(nullptr, temp_buffer_, NULL, size);
    for (size_t j = 0; j < size; ++j) {
      buffer[j] += temp_buffer_[j] * 21 >> 6;
    }
  }'''
    new = '''  if (base_shape == OSC_SHAPE_SQUARE || base_shape == OSC_SHAPE_SAW) {
    analog_oscillator_[0].RenderTripleFirst(nullptr, buffer, nullptr, size);
    for (size_t i = 1; i < 3; ++i) {
      analog_oscillator_[i].RenderTripleMix(nullptr, buffer, nullptr, size);
    }
    return;
  }
  analog_oscillator_[0].RenderTripleSimpleFirst(nullptr, buffer, nullptr, size);
  for (size_t i = 1; i < 3; ++i) {
    analog_oscillator_[i].RenderTripleSimpleMix(nullptr, buffer, nullptr, size);
  }'''
    if source.count(old) != 1:
        raise ValueError('Triple oscillator mixing changed')
    return source.replace(old, new)


def transform_analog(source):
    methods = []
    for shape in ('Square', 'Saw', 'Triangle', 'Sine'):
        match = re.search(r'void AnalogOscillator::Render' + shape + r'NoSync\([^{}]*\{.*?\n\}', source, re.S)
        if not match:
            raise ValueError('Missing no-sync oscillator ' + shape)
        body = match[0].replace('Render' + shape + 'NoSync(', 'Render' + shape + 'TripleMix(')
        if shape == 'Triangle':
            body = body.replace('*buffer = triangle >> 1;', 'int16_t sample = triangle >> 1;').replace(
                '*buffer++ += triangle >> 1;', 'sample += triangle >> 1;\n    *buffer++ = sample;')
            body = body.replace('(phase_16 << 1) ^ (phase_16 & 0x8000 ? 0xffff : 0x0000)',
                                '((phase >> 15) & ~1u) ^ (0u - (phase >> 31))')
            body = body.replace('    uint16_t phase_16;\n', '').replace('    phase_16 = phase >> 16;\n', '')
        # Retain the int16 truncation that previously occurred in temp_buffer_.
        body, count = re.subn(r'\*buffer\+\+ = ([^;]+);',
                              r'*buffer += static_cast<int16_t>(\1) * 21 >> 6;\n    ++buffer;', body)
        if count != 1:
            raise ValueError('Unexpected oscillator output sites ' + shape)
        if shape == 'Triangle':
            # 704643072 / 2^31 == 21 / 64. The SDK operation truncates toward
            # minus infinity exactly like the original signed product >> 6.
            body, count = re.subn(r'static_cast<int16_t>\((.*?)\) \* 21 >> 6',
                                  r'opxy::q31::multiply(static_cast<int16_t>(\1), 704643072)', body)
            if count != 1:
                raise ValueError('Triangle mix gain changed')
        if shape == 'Sine':
            body = specialize_sine_chunk(body)
        methods.append(body)
        # The first oscillator writes the complete output; subsequent ones add.
        # This removes both the clear pass and the first oscillator's readback.
        methods.append(body.replace('TripleMix(', 'TripleFirst(').replace(
            '*buffer += ', '*buffer = '))
    match = re.search(r'void AnalogOscillator::RenderNoSync\([^{}]*\{.*?\n\}', source, re.S)
    if not match:
        raise ValueError('Missing no-sync dispatcher')
    dispatcher = match[0].replace('RenderNoSync(', 'RenderTripleMix(')
    dispatcher = dispatcher.replace('  RenderFn fn = no_sync_table_[shape_];', '')
    dispatcher = dispatcher.replace('  (this->*fn)(sync_in, buffer, sync_out, size);', '''  // Triple waveforms keep shape-change Init and
  // phase preparation identical to the general renderer.
  switch (shape_) {
    case OSC_SHAPE_SQUARE: RenderSquareTripleMix(sync_in, buffer, sync_out, size); break;
    case OSC_SHAPE_SAW: RenderSawTripleMix(sync_in, buffer, sync_out, size); break;
    case OSC_SHAPE_TRIANGLE: RenderTriangleTripleMix(sync_in, buffer, sync_out, size); break;
    case OSC_SHAPE_SINE: RenderSineTripleMix(sync_in, buffer, sync_out, size); break;
    default: break;
  }''')
    simple = dispatcher.replace('::RenderTripleMix(', '::RenderTripleSimpleMix(')
    for shape in ('Square', 'Saw'):
        simple = re.sub(r'    case OSC_SHAPE_' + shape.upper() + r':[^\n]+\n', '', simple)
    for shape in ('Triangle', 'Sine'):
        dispatcher = re.sub(r'    case OSC_SHAPE_' + shape.upper() + r':[^\n]+\n', '', dispatcher)
    simple_first = simple.replace('::RenderTripleSimpleMix(', '::RenderTripleSimpleFirst(').replace('TripleMix(', 'TripleFirst(')
    first = dispatcher.replace('TripleMix(', 'TripleFirst(')
    return source.replace('}  // namespace braids', '\n'.join(methods) + '\n' + dispatcher + '\n' + first + '\n' + simple + '\n' + simple_first + '\n}  // namespace braids')


def transform_header(source):
    source = source.replace(' private:',
                            '  void RenderTripleMix(const uint8_t*, int16_t*, uint8_t*, size_t);\n'
                            '  void RenderTripleFirst(const uint8_t*, int16_t*, uint8_t*, size_t);\n private:', 1)
    source = source.replace(' private:', '  void RenderTripleSimpleMix(const uint8_t*, int16_t*, uint8_t*, size_t);\n  void RenderTripleSimpleFirst(const uint8_t*, int16_t*, uint8_t*, size_t);\n private:', 1)
    extra = ''.join('  void Render' + shape + suffix + '(const uint8_t*, int16_t*, uint8_t*, size_t);\n'
                    for shape in ('Square', 'Saw', 'Triangle', 'Sine') for suffix in ('TripleMix', 'TripleFirst'))
    return source.replace(' private:', ' private:\n' + extra, 1)
