"""Specialize filter-sweep dispatch while retaining every state update."""
import re


def transform(source):
    match = re.search(r'void DigitalOscillator::RenderDigitalFilter\([^{}]*\{.*?\n\}', source, re.S)
    if not match:
        raise ValueError('Missing digital filter renderer')
    original = match[0]
    begin = original.index('    if (filter_type & 2) {')
    end = original.index('    uint16_t balance', begin)
    # The function table sends only BP/HP here. All oscillator phases,
    # square-integrator updates and interpolation steps remain unchanged.
    high = original[:begin] + '''    saw_tri_signal = (carrier * window) >> 16;
    square_signal = pulse;
''' + original[end:]
    high = high.replace('RenderDigitalFilter(', 'RenderDigitalFilterBandHigh(', 1)
    # The algebraic mixer helps the general LP/Peak loop, but costs more in
    # the specialized BP/HP loop on the target compiler. Keep its old Mix.
    mix = 'Mix(saw_tri_signal, square_signal, balance)'
    if original.count(mix) != 1:
        raise ValueError('Filter-sweep mixer changed')
    low = original.replace(mix, 'macroosc::mix_exact(saw_tri_signal, square_signal, balance)')
    source = source[:match.start()] + low + '\n\n' + high + source[match.end():]
    entry = '  &DigitalOscillator::RenderDigitalFilter,\n'
    if source.count(entry) != 4 or source.count(entry * 4) != 1:
        raise ValueError('Filter-sweep dispatch changed')
    return source.replace(entry * 4, entry * 2 +
                          '  &DigitalOscillator::RenderDigitalFilterBandHigh,\n' * 2)


def transform_header(source):
    declaration = '  void RenderDigitalFilter(const uint8_t*, int16_t*, size_t);'
    if source.count(declaration) != 1:
        raise ValueError('Filter-sweep declaration changed')
    return source.replace(declaration, declaration +
                          '\n  void RenderDigitalFilterBandHigh(const uint8_t*, int16_t*, size_t);')
