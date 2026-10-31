"""Exact Flute state and filter simplifications; preserve RNG and delay lines."""

def transform(source):
    start=source.index('void DigitalOscillator::RenderFluted(')
    end=source.index('\nstruct WavetableDefinition',start)
    body=source[start:end]
    body=body.replace('int16_t* buffer,','int16_t* __restrict__ buffer,')
    body=body.replace('int8_t* dl_', 'int8_t* __restrict__ dl_')
    body=body.replace('    phase_ += phase_increment_;\n','')
    body=body.replace('  while (size--) {','''  // Neither phase nor the global RNG is observed within this loop.
  // Preserve their final state while avoiding repeated aliased state access.
  phase_ += phase_increment_ * size;
  uint32_t random_state=Random::state();
  const int32_t filter_multiplier=-static_cast<int32_t>(filter_coefficient)*524288;
  const int32_t breath_multiplier=static_cast<int32_t>(breath_intensity)<<3;
  while (size--) {''',1)
    old='    int32_t random_pressure = Random::GetSample() * breath_intensity >> 12;'
    if body.count(old)!=1:raise ValueError('Flute excitation changed')
    body=body.replace(old,'''    random_state=random_state*1664525u+1013904223u;
    int32_t random_pressure=macroosc::flute_noise(random_state,breath_multiplier);''')
    old='''    lp_state = (-filter_coefficient * bore_value + \\
        (4096 - filter_coefficient) * lp_state) >> 12;'''
    if body.count(old)!=1:raise ValueError('Flute body filter changed')
    body=body.replace(old,'    lp_state = macroosc::flute_body(lp_state,bore_value,filter_multiplier);')
    body=body.replace('dc_blocking_y0 = (kDCBlockingPole * dc_blocking_y0 >> 12);','dc_blocking_y0 = macroosc::flute_dc(dc_blocking_y0);')
    body=body.replace('  state_.phy.delay_ptr = delay_ptr;','  Random::Seed(random_state);\n  state_.phy.delay_ptr = delay_ptr;')
    # The delay lines have power-of-two lengths dividing65536, so wider
    # counters and direct subtraction preserve their modulo addressing.
    # State storage still narrows delay_ptr to the original16 bits on exit.
    body=body.replace('uint16_t delay_ptr =','uint32_t delay_ptr =')
    body=body.replace('uint16_t excitation_ptr =','uint32_t excitation_ptr =')
    for kind,length in (('bore','kWGFBoreLength'),('jet','kWGJetLength')):
        old='uint16_t '+kind+'_delay_ptr = delay_ptr + 2 * '+length+' \\\n        - '+kind+'_delay_integral;'
        new='uint32_t '+kind+'_delay_ptr = delay_ptr - '+kind+'_delay_integral;'
        if body.count(old)!=1:raise ValueError('Flute circular addressing changed')
        body=body.replace(old,new)
    begin=body.index('  while (size--) {')
    loop_end=body.index('\n  if (excitation_ptr >=',begin)
    sample=body[begin+len('  while (size--) {'):loop_end]
    if not sample.endswith('  }'):raise ValueError('Flute sample loop changed')
    sample=sample[:-3]
    advance='''    if (size & 3) {
      ++excitation_ptr;
    }'''
    if sample.count(advance)!=1:raise ValueError('Flute excitation cadence changed')
    sample=sample.replace(advance,'')
    loop='''  // Adapter chunks are multiples of four. Preserve the original
  // three excitation advances per four samples without per-sample branching.
  while (size) {
'''
    for i in range(4):
        loop+='    {\n'+sample+'\n    }\n'
        if i<3:loop+='    ++excitation_ptr;\n'
    loop+='    size-=4;\n  }'
    body=body[:begin]+loop+body[loop_end:]
    return '#include "optimized/flute_math.hpp"\n'+source[:start]+body+source[end:]
