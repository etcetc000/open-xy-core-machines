"""Exact direct no-sync dispatch and shared preparation for distinct oscillators."""
import re


SHAPES = ('SAW', 'VARIABLE_SAW', 'CSAW', 'SQUARE', 'TRIANGLE', 'SINE',
          'TRIANGLE_FOLD', 'SINE_FOLD', 'BUZZ')
RENDERERS = ('Saw', 'VariableSaw', 'CSaw', 'Square', 'Triangle', 'Sine',
             'TriangleFold', 'SineFold', 'Buzz')


def dispatch(prefix, buffer):
    result = '  switch (' + prefix + 'shape_) {\n'
    result += ''.join('    case OSC_SHAPE_' + shape + ': ' + prefix + 'Render' +
                      renderer + 'NoSync(nullptr, ' + buffer + ', nullptr, size); break;\n'
                      for shape, renderer in zip(SHAPES, RENDERERS))
    return result + '  }\n'


def transform_analog(source):
    pattern = r'AnalogOscillator::RenderFn AnalogOscillator::no_sync_table_\[\] = \{(.*?)\};'
    table = re.search(pattern, source, re.S)
    if not table:
        raise ValueError('Missing no-sync dispatch table')
    names = re.findall(r'&AnalogOscillator::Render(\w+)NoSync', table[1])
    if tuple(names) != RENDERERS:
        raise ValueError('No-sync dispatch order changed')
    match = re.search(r'void AnalogOscillator::RenderNoSync\([^{}]*\{.*?\n\}', source, re.S)
    if not match:
        raise ValueError('Missing no-sync renderer')
    body = match[0]
    load = '  RenderFn fn = no_sync_table_[shape_];'
    call = '  (this->*fn)(sync_in, buffer, sync_out, size);'
    if body.count(load) != 1 or body.count(call) != 1:
        raise ValueError('No-sync preparation changed')
    # Preserve the wrapper arguments, including its historical ignored sync
    # arguments. Only the member-pointer dispatch is replaced here.
    direct = dispatch('', 'buffer').rstrip().replace('nullptr, buffer, nullptr',
                                                    'sync_in, buffer, sync_out')
    body = body.replace(load, '').replace(call, direct)
    source = source[:match.start()] + body + source[match.end():]
    source, count = re.subn(pattern, '', source, flags=re.S)
    if count != 1 or 'no_sync_table_[' in source:
        raise ValueError('Unexpected remaining no-sync dispatch table use')

    # Both call sites pass distinct array elements. Init and phase calculation
    # affect only their own oscillator; no analog renderer uses shared mutable
    # state. Keep unequal pitches (including shape-change Init resets) separate.
    pair = '''
void AnalogOscillator::RenderPairNoSync(AnalogOscillator& other,
    int16_t* buffer, int16_t* other_buffer, size_t size) {
  if (shape_ != previous_shape_) { Init(); previous_shape_ = shape_; }
  if (other.shape_ != other.previous_shape_) { other.Init(); other.previous_shape_ = other.shape_; }
  phase_increment_ = ComputePhaseIncrement(pitch_);
  if (other.pitch_ == pitch_) {
    other.phase_increment_ = phase_increment_;
    if (pitch_ > kHighestNote) pitch_ = kHighestNote;
    else if (pitch_ < 0) pitch_ = 0;
    other.pitch_ = pitch_;
  } else {
    other.phase_increment_ = other.ComputePhaseIncrement(other.pitch_);
    if (pitch_ > kHighestNote) pitch_ = kHighestNote;
    else if (pitch_ < 0) pitch_ = 0;
    if (other.pitch_ > kHighestNote) other.pitch_ = kHighestNote;
    else if (other.pitch_ < 0) other.pitch_ = 0;
  }
'''
    pair += dispatch('', 'buffer') + dispatch('other.', 'other_buffer') + '}\n'
    if source.count('}  // namespace braids') != 1:
        raise ValueError('Analog namespace layout changed')
    return source.replace('}  // namespace braids', pair + '\n}  // namespace braids')


def transform_macro(source):
    for first, second in (('shape_1', 'shape_2'), ('saw_buffer', 'square_buffer')):
        old = ('  analog_oscillator_[0].RenderNoSync(nullptr, ' + first + ', NULL, size);\n'
               '  analog_oscillator_[1].RenderNoSync(nullptr, ' + second + ', NULL, size);')
        if source.count(old) != 1:
            raise ValueError('Paired macro call site changed')
        source = source.replace(old, '  analog_oscillator_[0].RenderPairNoSync(analog_oscillator_[1], ' +
                                first + ', ' + second + ', size);')
    return source


def transform_header(source):
    if source.count(' private:') != 1:
        raise ValueError('Analog header access layout changed')
    source = source.replace(' private:',
                            '  void RenderPairNoSync(AnalogOscillator&, int16_t*, int16_t*, size_t);\n private:', 1)
    return source.replace('  static RenderFn no_sync_table_[];\n', '')
