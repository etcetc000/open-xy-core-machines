"""Stage independent Cymbal phase and filter paths without changing samples."""


def transform(source):
    start = source.index('void DigitalOscillator::RenderCymbal(')
    end = source.index('\n/*', start)
    body = source[start:end]
    begin = body.index('  while (size--) {')
    loop = body[begin:]
    phase_start = loop.index('    hat->phase[0]')
    filter_start = loop.index('    hat_noise = svf_')
    noise_start = loop.index('    int32_t noise =')
    if loop.count('ProcessNoPunch<SVF_MODE_BP>') != 1 or loop.count('ProcessNoPunch<SVF_MODE_HP>') != 1:
        raise ValueError('Cymbal prepared filter paths changed')
    filtering = loop[filter_start:noise_start]
    new = '''  // These six unsigned phases are independent of both filter histories
  // and the noise RNG. Stage four then two square bits in the separate audio
  // buffer: partial sums are in [0,4], then [0,6], exactly representable.
  // Save each final phase early using modulo-2^32 addition; local phases still
  // advance and emit every original sample. No rate or FM cadence changes.
'''
    for group_index, ids in enumerate((range(4), range(4, 6))):
        new += '  {\n'
        for i in ids:
            new += f'    uint32_t p{i} = hat->phase[{i}]; const uint32_t d{i} = increments[{i}];\n'
        for i in ids:
            new += f'    hat->phase[{i}] = p{i} + d{i} * size;\n'
        new += '    int16_t* out = buffer; unsigned remaining = size;\n    while (remaining--) {\n'
        for i in ids:
            new += f'      p{i} += d{i};\n'
        terms = [f'(p{i} >> 31)' for i in ids]
        if group_index:
            terms.insert(0, 'static_cast<unsigned>(*out)')
        new += '      *out = ' + ' + '.join(terms) + ';\n      ++out;\n    }\n  }\n'
    new += '''  // Preserve the BP recurrence for every square-wave sample. Its
  // clipped output fits int16 before the independent noise/HP and mix pass.
  {int16_t* out = buffer; unsigned remaining = size;
    while (remaining--) {
    int32_t hat_noise = (static_cast<int32_t>(*out) - 3) * 5461;
''' + filtering + '''      *out++ = hat_noise;
    }
  }
'''
    new += loop[:phase_start] + '    int32_t hat_noise = *buffer;\n' + loop[noise_start:]
    return source[:start] + body[:begin] + new + source[end:]
