"""Use exact signed-half sine interpolation only in measured renderers."""
import re


def transform(source, kind):
    selected = {
        'digital': {'RenderTripleRingMod': 3, 'RenderVosim': 2,
                    'RenderHarmonics': 1, 'RenderFm': 2,
                    'RenderChaoticFeedbackFm': 2, 'RenderStruckBell': 1,
                    'RenderStruckDrum': 1, 'RenderGranularCloud': 4,
                    'RenderDigitalModulation': 2},
        'analog': {'RenderSineTripleMix': 9, 'RenderSineTripleFirst': 9},
    }[kind]
    seen = set()
    def replace(match):
        name = match[1]
        body = match[0]
        if name not in selected:
            return body
        body, count = re.subn(r'macroosc::paired824\(macroosc::sine_pairs,\s*',
                             'macroosc::sine_signed_half(', body)
        if count != selected[name]:
            raise ValueError('Signed-half sine call count changed: '+name)
        seen.add(name)
        return body
    source = re.sub(r'void (?:Digital|Analog)Oscillator::(Render\w+)\([^{}]*\{.*?\n\}',
                    replace, source, flags=re.S)
    if seen != set(selected):
        raise ValueError('Missing signed-half sine renderer')
    return source
