"""Exact fixed-duty Triple Square rendering with edge-free chunk batching."""
import re


def transform(source):
    for mode in ('Mix', 'First'):
        match = re.search(r'void AnalogOscillator::RenderSquareTriple' + mode +
                          r'\([^{}]*\{.*?\n\}', source, re.S)
        if not match:
            raise ValueError('Missing Triple Square renderer ' + mode)
        body = match[0]
        clamp = '  if (parameter_ > 32000) {\n    parameter_ = 32000;\n  }'
        width = 'uint32_t pw = static_cast<uint32_t>(32768 - parameter_) << 16;'
        begin = '  BEGIN_INTERPOLATE_PHASE_INCREMENT\n'
        if any(body.count(x) != 1 for x in (clamp, width, begin)):
            raise ValueError('Triple Square preparation changed')
        # The sole production caller, Macro RenderTriple, sets all parameters
        # to zero. Shape-change Init also resets this value to zero. Preserve
        # the general variable-duty Square renderer for other callers.
        body = body.replace(clamp, '').replace(width, 'const uint32_t pw = 0x80000000u;')
        fast = '''
  // Every interpolated increment is in [0, largest] under these guards:
  // rising interpolation never overshoots; complemented falling interpolation
  // can undershoot the target by at most size. The product cannot overflow.
  // A consistent high state and total travel below the next half-cycle edge
  // mean all samples after the saved BLEP tail have the same exact value.
  const uint32_t largest = previous_phase_increment_ > phase_increment_
      ? previous_phase_increment_ : phase_increment_;
  if (size && size <= 24 && high == (phase >> 31) &&
      previous_phase_increment_ >= size && phase_increment_ >= size &&
      largest <= 0x7fffffffu / 24u &&
      largest * size < 0x80000000u - (phase & 0x7fffffffu)) {
    const unsigned count = size;
    const int16_t first = static_cast<int16_t>((next_sample_ - 16384) << 1);
    *buffer OP (first * 21 >> 6); ++buffer;
    const int32_t steady = high ? 10751 : -10752;
    while (--size) { *buffer OP steady; ++buffer; }
    // Unsigned arithmetic reproduces each original wrap, including the
    // complemented falling/equal step and its final previous-increment state.
    phase_ = phase + count * phase_increment +
        ((count * (count + 1u)) >> 1) * phase_increment_increment;
    previous_phase_increment_ = phase_increment + count * phase_increment_increment;
    next_sample_ = high ? 32767 : 0;
    return;
  }
'''.replace('OP', '+=' if mode == 'Mix' else '=')
        body = body.replace(begin, begin + fast)
        # Retain the complete original event path, including multiple events
        # per sample and inconsistent high state after variable-PWM switches.
        source = source[:match.start()] + body + source[match.end():]
    return source
