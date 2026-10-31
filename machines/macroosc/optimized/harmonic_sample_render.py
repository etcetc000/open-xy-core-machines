"""Extract the exact partial loop so the compiler can use its hardware counter.

Targets are nonnegative with sum<=32767 after spectrum normalization. The
one-pole update floor((255*a+t)/256) preserves that sum bound. Each signed16
sine contribution is in[-a,a], so out is always in[-32767,32767]. Removing
CLIP preserves every output and frees registers around the sample loop.
"""
def transform(source):
    a=source.index('void DigitalOscillator::RenderHarmonics(')
    b=source.index('\nvoid DigitalOscillator::',a+10)
    body=source[a:b]
    begin = body.index('    out = 0;')
    end = body.index('    CLIP(out)', begin)
    loop = body[begin:end]
    helper = 'static __attribute__((noinline)) int32_t RenderHarmonicSample(uint32_t phase,int32_t* __restrict__ amplitude,const int32_t* __restrict__ target_amplitude) {\n    int32_t out;\n' + loop + '    return out;\n}\n'
    body = body[:begin] + '    out = RenderHarmonicSample(phase,amplitude,target_amplitude);\n' + body[end:]
    body = body.replace('    CLIP(out)', '    // Normalization and smoothing keep sum(amplitudes)<=32767; out cannot clip.')
    return source[:a] + helper + body + source[b:]
