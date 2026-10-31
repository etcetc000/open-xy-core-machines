"""Preserve Morph's filter exactly while using its coefficient/state bounds."""
from pathlib import Path
import re


def transform(source):
    resources = (Path(__file__).parent.parent/'vendor/braids/resources.cc').read_text()
    match = re.search(r'const uint16_t lut_svf_cutoff\[\] = \{([^}]+)\}', resources)
    if not match:
        raise ValueError('Missing SVF coefficient table')
    values = list(map(int, re.findall(r'\d+', match[1])))
    if (len(values) != 257 or min(values) != 38 or max(values) != 25078 or
            values[112] >= 25078 or any(v != 25078 for v in values[113:])):
        raise ValueError('Morph coefficient bounds or plateau changed')
    start = source.index('void MacroOscillator::RenderMorph(')
    end = source.index('\nvoid MacroOscillator::RenderSawSquare(', start)
    body = source[start:end]
    old = '''  if (lp_cutoff < 0) {
    lp_cutoff = 0;
  } else if (lp_cutoff > 32767) {
    lp_cutoff = 32767;
  }
  int32_t f = macroosc::paired_u824(macroosc::svf_cutoff_pairs, lp_cutoff << 17);'''
    if body.count(old) != 1:
        raise ValueError('Morph cutoff setup changed')
    body = body.replace(old, '''  const int32_t f = macroosc::morph_cutoff(lp_cutoff);
  const int32_t filter_multiplier = f * 65536;''')
    old = '''    lp_state += (shifted_sample - lp_state) * f >> 15;
    CLIP(lp_state)'''
    if body.count(old) != 1:
        raise ValueError('Morph filter recurrence changed')
    body = body.replace(old, '''    lp_state = macroosc::morph_filter_step(
        lp_state, shifted_sample, filter_multiplier);''')
    return '#include "optimized/morph_math.hpp"\n' + source[:start] + body + source[end:]
