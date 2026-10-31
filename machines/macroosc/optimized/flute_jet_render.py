"""Fold the exact signed-byte jet index and lower clamp into its existing table."""
import re

def transform(source):
    start=source.index('void DigitalOscillator::RenderFluted(')
    end=source.index('\nstruct WavetableDefinition',start)
    body=source[start:end]
    replacements=[
        ('macroosc::mix_byte<9>(jet_dl_a, jet_dl_b, jet_delay_fractional)',
         'macroosc::flute_jet_index(jet_dl_a, jet_dl_b, jet_delay_fractional)'),
        ("    int32_t jet_table_index = pressure_delta;\n    if (jet_table_index < 0) {\n      jet_table_index = 0;\n    }\n",
         '    const unsigned jet_table_index = pressure_delta;\n'),
        ('lut_blowing_jet[jet_table_index >> 8]','lut_blowing_jet[jet_table_index]'),
    ]
    for old,new in replacements:
        if body.count(old)!=4:raise ValueError('Flute jet loop changed')
        body=body.replace(old,new)
    body=body.replace('  // Both delay interpolants are signed bytes scaled by512. Their jet\n  // index is at most254 and their output at most32512: upper clamps are\n  // unreachable. Keep the original lower output clamp at -32767.',
        '  // The bore interpolant remains a signed byte scaled by512. The jet\n  // table encodes the original byte scaling and negative-index clamp.\n  // Keep the original lower output clamp at -32767.')
    return '#include "optimized/flute_jet.hpp"\n'+source[:start]+body+source[end:]

def transform_resources(source):
    match=re.search(r'const int16_t lut_blowing_jet\[\] = \{(.*?)\};',source,re.S)
    if not match:raise ValueError('Original Flute jet table missing')
    values=list(map(int,re.findall(r'-?\d+',match[1])))
    if len(values)!=257:raise ValueError('Original Flute jet table extent changed')
    mapped=[values[2*max(0,i if i<128 else i-256)] for i in range(256)]+values[256:]
    data='\n'+''.join('  '+', '.join(map(str,mapped[i:i+16]))+',\n' for i in range(0,len(mapped),16))
    return source[:match.start(1)]+data+source[match.end(1):]
