"""Remove inert Kick excitation work; preserve every audible filter step."""


def transform(source):
    start = source.index('void DigitalOscillator::RenderKick(')
    end = source.index('\nvoid DigitalOscillator::RenderSnare(', start)
    body = source[start:end]
    # Kick discards this pulse's sample and observes only done(). Its state
    # cannot reach another renderer: DigitalOscillator::Init resets all pulses
    # on every digital shape change. Keep its trigger counter cadence exactly.
    if body.count('    pulse_[2].Process();') != 1:
        raise ValueError('Kick timer pulse use changed')
    body = body.replace('    pulse_[2].Process();', '    pulse_[2].AdvanceTriggerOnly();')
    begin = body.index('  while (size) {')
    end_loop = body.index('  state_.svf.lp =', begin)
    loop = body[begin:end_loop]
    excitation_begin = loop.index('    excitation += pulse_[0].Process();')
    excitation_end = loop.index('    pulse_[2].', excitation_begin)
    zero_loop = loop[:excitation_begin] + loop[excitation_end:]
    body = body[:begin] + '''  // A zero excitation state without a pending trigger remains zero.
  // Keep the original timer, pitch, punch, resonator and lowpass updates.
  if (pulse_[0].silent() && pulse_[1].silent()) {
''' + zero_loop + '  } else {\n' + loop + '  }\n' + body[end_loop:]
    return source[:start] + body + source[end:]
