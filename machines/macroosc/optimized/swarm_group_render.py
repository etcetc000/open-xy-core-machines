"""Exact Swarm setup and four-plus-three phase passes; filter order unchanged."""


def transform(source):
    start = source.index('void DigitalOscillator::RenderSawSwarm(')
    end = source.index('\nvoid DigitalOscillator::RenderComb(', start)
    body = source[start:end]
    setup = body.index('  int32_t detune =')
    strike = body.index('  if (strike_)', setup)
    if body[setup:strike].count('macroosc::swarm_increment(') != 1:
        raise ValueError('Swarm setup changed')
    body = body[:setup] + '''  uint32_t increments[7];
  macroosc::swarm_increments(increments, pitch_, parameter_[0]);
''' + body[strike:]
    begin = body.index('  while (size--) {')
    filter_start = body.index('    sample = macroosc::paired88(', begin)
    end_filter = body.index('  state_.saw.lp = lp;', filter_start)
    filtering = body[filter_start:end_filter]
    if not filtering.endswith('  }\n'):
        raise ValueError('Swarm filter loop changed')
    filtering = filtering[:-4]
    new = '''  // Every phase advances exactly once per sample. Partial sums fit int16:
  // four saws plus offset span [-28672,4092], all seven [-28672,28665].
  // Reuse the separate output buffer, then retain the original filter order.
'''
    for group_index, ids in enumerate((range(4), range(4, 7))):
        new += '  {\n'
        for i in ids:
            state = 'phase_' if i == 0 else f'state_.saw.phase[{i - 1}]'
            new += f'    uint32_t phase{i} = {state};\n'
            new += f'    const uint32_t increment{i} = increments[{i}];\n'
            if group_index == 1:
                # Unsigned phase addition wraps exactly. Precomputing this
                # group's final state shortens register lifetimes on target;
                # the independent locals still emit every original sample.
                new += f'    {state} = phase{i} + increment{i} * size;\n'
        new += '    int16_t* out = buffer;\n    unsigned remaining = size;\n'
        new += '    while (remaining--) {\n'
        for i in ids:
            new += f'      phase{i} += increment{i};\n'
        terms = [f'static_cast<int32_t>(phase{i} >> 19)' for i in ids]
        initial = '-28672' if group_index == 0 else 'static_cast<int32_t>(*out)'
        new += '      *out = ' + initial + '\n          + ' + '\n          + '.join(terms) + ';\n'
        new += '      ++out;\n'
        new += '    }\n'
        for i in ids:
            state = 'phase_' if i == 0 else f'state_.saw.phase[{i - 1}]'
            if group_index == 0:
                new += f'    {state} = phase{i};\n'
        new += '  }\n'
    new += '  while (size--) {\n    int32_t notch, hp, sample = *buffer;\n' + filtering + '  }\n'
    body = body[:begin] + new + body[end_filter:]
    return source[:start] + body + source[end:]
