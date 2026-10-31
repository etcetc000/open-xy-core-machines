"""Keep hot oscillator state local; specialize the fixed-width sync pair."""
import re

def transform(source):
    names=('RenderSquareMaster','RenderSquareSlave','RenderSawMaster',
           'RenderSawSlave','RenderSquareNoSync','RenderSquareTripleMix',
           'RenderSquareTripleFirst')
    for name in names:
        match=re.search(r'void AnalogOscillator::'+name+r'\([^{}]*\{.*?\n\}',source,re.S)
        if not match:
            raise ValueError('Missing analog state renderer '+name)
        body=match[0]
        body=re.sub(r'\bphase_\b','phase',body)
        body=re.sub(r'\bhigh_\b','high',body)
        body=body.replace('  BEGIN_INTERPOLATE_PHASE_INCREMENT',
            '  uint32_t phase=phase_;\n  bool high=high_;\n  BEGIN_INTERPOLATE_PHASE_INCREMENT',1)
        body=body.replace('  next_sample_ = next_sample;',
            '  phase_=phase;high_=high;\n  next_sample_ = next_sample;')
        if name in ('RenderSquareMaster','RenderSquareSlave'):
            # Only RenderDualSync calls these specialized methods. It sets
            # both parameters to zero; a shape-change Init also resets to zero.
            body=body.replace('  if (parameter_ > 32000) {\n    parameter_ = 32000;\n  }\n','')
            body=body.replace('uint32_t pw = static_cast<uint32_t>(32768 - parameter_) << 16;',
                'const uint32_t pw = 0x80000000u;  // DualSync always sets parameter zero.')
        source=source[:match.start()]+body+source[match.end():]
    return source
