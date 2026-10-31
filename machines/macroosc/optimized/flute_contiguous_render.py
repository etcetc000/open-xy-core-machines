"""Use contiguous Flute delay-buffer spans when no read or write wraps."""
def transform(source):
    a=source.index('void DigitalOscillator::RenderFluted(')
    b=source.index('\nstruct WavetableDefinition',a)
    body=source[a:b]
    start=body.index('  while (size) {')
    end=body.index('\n  if (excitation_ptr >=',start)
    original=body[start:end]
    fast=original
    setup=''
    guards=[]
    for kind,letter,length in [('bore','b','kWGFBoreLength'),('jet','j','kWGJetLength')]:
        setup+=f'  uint32_t read_{letter}={kind}_delay_ptr%{length};\n'
        guards.append(f'(read_{letter}>=1 && read_{letter}+size<={length})')
        replacements=[
            (f'dl_{letter}[{kind}_delay_ptr % {length}]',f'dl_{letter}[read_{letter}]'),
            (f'dl_{letter}[({kind}_delay_ptr - 1) % {length}]',f'dl_{letter}[read_{letter}-1]'),
            (f'++{kind}_delay_ptr;',f'++read_{letter};'),
            (f'dl_{letter}[delay_ptr % {length}]',f'dl_{letter}[write_{letter}++]'),
        ]
        for old,new in replacements:
            if fast.count(old)!=4:raise ValueError('Flute delay loop changed')
            fast=fast.replace(old,new)
        setup+=f'  uint32_t write_{letter}=delay_ptr%{length};\n'
        guards.append(f'(write_{letter}+size<={length})')
    comment=('  // Render chunks contain at most24samples. Within a contiguous span,\n'
             '  // modulo addressing equals ordinary indexing, including the previous\n'
             '  // byte. Preserve sample/read/write order for overlapping delay spans.\n'
             '  // Any boundary crossing uses the original ring-buffer loop.\n')
    body=body[:start]+comment+setup+'  if('+' && '.join(guards)+') {\n'+fast+'\n  } else {\n'+original+'\n  }'+body[end:]
    return source[:a]+body+source[b:]
