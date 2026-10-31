"""Exact Wave Line work sharing; keep both oversampling evaluations."""
import re

def transform(source):
    start=source.index('void DigitalOscillator::RenderWaveLine(')
    end=source.index('\n#define SEMI',start)
    body=source[start:end].replace('Mix(', 'macroosc::mix_exact(')
    for condition,next_condition in [
        ('  if (parameter_[1] < 8192) {','  } else if (parameter_[1] < 16384) {'),
        ('  } else if (parameter_[1] < 16384) {','  } else if (parameter_[1] < 24576) {')]:
        begin=body.index(condition)+len(condition)
        finish=body.index(next_condition,begin)
        original=body[begin:finish]
        fast=original.replace(
            'Crossfade(wave_0, wave_1, (phase >> 1) & 0xfe000000, rough_xfade)',
            'Interpolate824(wave_1, (phase >> 1) & 0xfe000000)').replace(
            'Crossfade(wave_0, wave_1, phase >> 1, rough_xfade)',
            'Interpolate824(wave_1, phase >> 1)')
        if original==fast:
            raise ValueError('Wave Line interpolation changed')
        body=body[:begin]+'''
    // The rough transition is an identity when both selected waves match.
    // Test once per call; changing waves retain the complete interpolation.
    if (wave_0==wave_1) {
'''+fast+'    } else {\n'+original+'    }\n'+body[finish:]
    body=body.replace('  int16_t rough, smooth;', '''  // Two samples per output, bounded by the adapter's24-frame chunk limit.
  int16_t oversampled[48];
  int16_t* scratch=oversampled;
  size_t remaining=size*2;
  int16_t rough, smooth;''')
    pattern=r'    while \(size--\) \{\n.*?      int32_t sample = 0;\n(.*?)\n      \*buffer\+\+ = sample >> 1;\n    \}'
    def single_step(match):
        lines=[line.rstrip() for line in match[1].splitlines()]
        while lines and not lines[0].strip():lines.pop(0)
        while lines and not lines[-1].strip():lines.pop()
        first_statement=next(line for line in lines if line.strip())
        second=next(i for i,line in enumerate(lines) if i>0 and line==first_statement)
        first='\n'.join(lines[:second]).rstrip()
        if first!='\n'.join(lines[second:]).rstrip():
            raise ValueError('Wave Line oversampling steps diverged')
        return '    while (remaining--) {\n'+first.replace('sample += ', '*scratch++ = ')+'\n    }'
    body,count=re.subn(pattern,single_step,body,flags=re.S)
    if count!=6:raise ValueError('Wave Line region count changed')
    body=body.replace('  phase_ = phase;', '''  for(unsigned i=0;i<size;++i)
    buffer[i]=(static_cast<int32_t>(oversampled[2*i])+oversampled[2*i+1])>>1;
  phase_ = phase;''',1)
    body=body.replace('Crossfade(', 'macroosc::wave_crossfade(')
    body=body.replace('Interpolate824(wave_', 'macroosc::wave_sample(wave_')
    # Quantized phases have exactly zero fractional interpolation weight.
    body,count=re.subn(r'macroosc::wave_(sample|crossfade)\(([^;\n]*?\(phase >> 1\) & 0x(?:fe|f8)000000[^;\n]*?)\)',
                       r'macroosc::wave_grid_\1(\2)',body)
    if count!=5:raise ValueError('Wave Line quantized lookup sites changed')
    old='''      rough = macroosc::wave_sample(wave_1, phase >> 1);
      smooth = macroosc::wave_crossfade(wave_1, wave_2, phase >> 1, smooth_xfade);'''
    new='''      const int16_t shared_sample=macroosc::wave_sample(wave_1, phase >> 1);
      rough = shared_sample;
      smooth = macroosc::wave_sample_crossfade(shared_sample,macroosc::wave_sample(wave_2, phase >> 1),smooth_xfade);'''
    if body.count(old)!=1:raise ValueError('Wave Line shared single-wave path changed')
    body=body.replace(old,new)
    old='''      rough = macroosc::wave_crossfade(wave_0, wave_1, phase >> 1, rough_xfade);
      smooth = macroosc::wave_crossfade(wave_1, wave_2, phase >> 1, smooth_xfade);'''
    new='''      const int16_t shared_sample=macroosc::wave_sample(wave_1, phase >> 1);
      rough = macroosc::wave_sample_crossfade(macroosc::wave_sample(wave_0, phase >> 1),shared_sample,rough_xfade);
      smooth = macroosc::wave_sample_crossfade(shared_sample,macroosc::wave_sample(wave_2, phase >> 1),smooth_xfade);'''
    if body.count(old)!=1:raise ValueError('Wave Line shared crossfade path changed')
    body=body.replace(old,new)
    # At each region boundary the second Mix operand has zero weight. Keep
    # Mix's original65535 weighting/rounding, not an identity substitution.
    body=body.replace('void DigitalOscillator::RenderWaveLine(',
                      'template<bool Zero> void DigitalOscillator::RenderWaveLineWeighted(',1)
    balance='uint32_t balance = parameter_[1] << 3;'
    if body.count(balance)!=1:raise ValueError('Wave Line mix balance changed')
    body=body.replace(balance,'uint32_t balance = Zero ? 0 : parameter_[1] << 3;')
    body+='''
void DigitalOscillator::RenderWaveLine(const uint8_t* sync,int16_t* buffer,size_t size) {
 if((parameter_[1]&8191)==0) RenderWaveLineWeighted<true>(sync,buffer,size);
 else RenderWaveLineWeighted<false>(sync,buffer,size);
}
'''
    return source[:start]+body+source[end:]


def transform_header(header):
    return header.replace(' private:',
        ' private:\n  template<bool Zero> void RenderWaveLineWeighted(const uint8_t*,int16_t*,size_t);',1)
