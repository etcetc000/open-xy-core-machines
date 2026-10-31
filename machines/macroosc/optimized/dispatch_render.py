"""Derive direct dispatch from the reviewed tables without changing renderers."""
import re


def transform(source, kind):
    if kind == 'analog':
        return source
    cls = kind.title()+'Oscillator'
    pattern = cls+r'::RenderFn '+cls+r'::fn_table_\[\] = \{(.*?)\};'
    table = re.search(pattern, source, re.S)
    if not table:
        raise ValueError('Missing '+kind+' dispatch table')
    entries = re.sub(r'//[^\n]*', '', table[1])
    names = re.findall(r'&'+cls+r'::(\w+)', entries)
    if len(names) != (48 if kind == 'macro' else 35):
        raise ValueError('Unexpected '+kind+' dispatch extent')
    renderer = re.search(r'void '+cls+r'::Render\([^{}]*\{.*?\n\}', source, re.S)
    if not renderer:
        raise ValueError('Missing '+kind+' dispatcher')
    body = renderer[0]
    load = '  RenderFn fn = fn_table_[shape_];\n'
    call = '  (this->*fn)(sync, buffer, size);'
    if body.count(load) != 1 or body.count(call) != 1:
        raise ValueError('Dispatch body changed')
    body = body.replace(load, '')
    if kind == 'digital':
        physical = [names.index(n) for n in ('RenderBowed','RenderBlown','RenderFluted')]
        if physical != list(range(physical[0],physical[0]+3)):
            raise ValueError('Physical renderers no longer contiguous')
        begin = body.index('  // Only these three renderers read delay_.')
        end = body.index('\n\n  if (pitch_', begin)
        body = body[:begin]+('  // These indexes are derived from the checked dispatch table.\n'
            '  // The vendor shape enum differs from that table in this region.\n'
            '  if (static_cast<unsigned>(shape_)-'+str(physical[0])+'u<=2u) {\n'
            '    delay_ = ComputeDelay(pitch_);\n  }')+body[end:]
        cases = list(enumerate(names))
        fallback = ''
    else:
        if names[15] != 'RenderSawComb' or not all(n=='RenderDigital' for i,n in enumerate(names) if i>=13 and i!=15):
            raise ValueError('Macro digital dispatch range changed')
        cases = [(i,n) for i,n in enumerate(names) if n!='RenderDigital']
        fallback = '    default: RenderDigital(sync,buffer,size); break;\n'
    dispatch = '  switch(static_cast<unsigned>(shape_)) {\n'
    dispatch += ''.join('    case '+str(i)+': '+n+'(sync,buffer,size); break;\n' for i,n in cases)
    dispatch += fallback+'  }'
    body = body.replace(call, dispatch)
    source = source[:renderer.start()]+body+source[renderer.end():]
    # No caller uses the pointer table after replacing this dispatcher. Drop
    # the definition so the SDK's non-GC module link does not keep dead data.
    source, count = re.subn(pattern, '', source, flags=re.S)
    if count != 1 or 'fn_table_[' in source:
        raise ValueError('Unexpected remaining dispatch-table access')
    return source
