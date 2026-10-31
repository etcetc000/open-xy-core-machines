"""Specialize the two known hard-sync waveforms at their render entry points."""
import re


def expand_square_transitions(body):
    # The condition and phase do not change inside the original loop. A rise
    # sets high; a fall consumes the only self-reset and clears high. Therefore
    # the complete event sequence is at most rise/fall/rise. Keeping the last
    # rise makes this exact even for phase increments beyond a half cycle.
    begin = body.index('    while (transition_during_reset || !sync_reset) {')
    end = body.index('\n    if (sync_reset)', begin)
    rise = '''        uint32_t t = macroosc::transition_quotient(phase - pw, phase_increment >> 16);
        this_sample += ThisBlepSample(t);
        next_sample += NextBlepSample(t);
        high = true;'''
    fall = '''        uint32_t t = macroosc::transition_quotient(phase, phase_increment >> 16);
        this_sample -= ThisBlepSample(t);
        next_sample -= NextBlepSample(t);
        high = false;'''
    return body[:begin] + '''    if (transition_during_reset || !sync_reset) {
      if (!high && phase >= pw) {
''' + rise + '''
      }
      if (high && self_reset) {
''' + fall + '''
        if (phase >= pw) {
''' + rise + '''
        }
      }
    }
''' + body[end:]


def transform_macro(source):
    old = '''  analog_oscillator_[0].RenderSyncMaster(nullptr, buffer, sync_buffer_, size);
  analog_oscillator_[1].RenderSyncSlave(sync_buffer_, temp_buffer_, nullptr, size);'''
    new = '''  if (base_shape == OSC_SHAPE_SQUARE) {
    analog_oscillator_[0].RenderSquareSyncMaster(nullptr, buffer, sync_buffer_, size);
    analog_oscillator_[1].RenderSquareSyncSlave(sync_buffer_, temp_buffer_, nullptr, size);
  } else {
    analog_oscillator_[0].RenderSawSyncMaster(nullptr, buffer, sync_buffer_, size);
    analog_oscillator_[1].RenderSawSyncSlave(sync_buffer_, temp_buffer_, nullptr, size);
  }'''
    if source.count(old) != 1:
        raise ValueError('DualSync dispatch changed')
    return source.replace(old, new)


def transform_analog(source):
    entries = []
    for mode in ('Master', 'Slave'):
        match = re.search(r'void AnalogOscillator::RenderSync' + mode + r'\([^{}]*\{.*?\n\}', source, re.S)
        if not match:
            raise ValueError('Missing sync dispatcher ' + mode)
        body = match[0]
        selector = '  RenderFn fn = shape_ == OSC_SHAPE_SQUARE ? &AnalogOscillator::RenderSquare' + mode + ' : &AnalogOscillator::RenderSaw' + mode + ';\n'
        call = '  (this->*fn)(sync_in, buffer, sync_out, size);'
        if body.count(selector) != 1 or body.count(call) != 1:
            raise ValueError('Sync dispatcher shape changed')
        body = body.replace(selector, '')
        for shape in ('Square', 'Saw'):
            entry = body.replace('RenderSync' + mode, 'Render' + shape + 'Sync' + mode)
            entries.append(entry.replace(call, '  Render' + shape + mode + '(sync_in, buffer, sync_out, size);'))
        # Retain the generic entry points for independent reference tests.
        generic = body.replace(call, '  if (shape_ == OSC_SHAPE_SQUARE) RenderSquare' + mode + '(sync_in, buffer, sync_out, size);\n  else RenderSaw' + mode + '(sync_in, buffer, sync_out, size);')
        source = source[:match.start()] + generic + source[match.end():]
    definitions = []
    for shape in ('Square', 'Saw'):
        for mode in ('Master', 'Slave'):
            name = 'Render' + shape + mode
            match = re.search(r'void AnalogOscillator::' + name + r'\([^{}]*\{.*?\n\}', source, re.S)
            if not match:
                raise ValueError('Missing sync renderer ' + name)
            body = match[0]
            quotient = 'macroosc::transition_quotient'
            for numerator, shift in [('phase - pw', 16), ('phase', 16), ('phase', 7)]:
                expression = ('(' + numerator + ')' if numerator == 'phase - pw' else numerator) + ' / (phase_increment >> ' + str(shift) + ')'
                body = body.replace(expression, quotient + '(' + numerator + ', phase_increment >> ' + str(shift) + ')')
            if shape == 'Square':
                body = expand_square_transitions(body)
            definitions.append(body.replace('void AnalogOscillator::', 'inline __attribute__((always_inline)) void AnalogOscillator::', 1))
            source = source[:match.start()] + source[match.end():]
    return '#include "optimized/exact_divide.hpp"\n' + source.replace('}  // namespace braids', '\n'.join(definitions + entries) + '\n}  // namespace braids')


def transform_header(header):
    declarations = '\n'.join('  void Render' + shape + 'Sync' + mode + '(const uint8_t*, int16_t*, uint8_t*, size_t);'
                             for mode in ('Master', 'Slave') for shape in ('Square', 'Saw'))
    return header.replace(' private:', declarations + '\n private:', 1)
