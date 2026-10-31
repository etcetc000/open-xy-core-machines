"""Use exact reciprocal division in selected unsynchronized analog renderers."""
import re
from sync_dispatch import expand_square_transitions


def transform(source):
    names = ('CSawNoSync', 'SquareNoSync', 'SawNoSync', 'VariableSawNoSync',
             'SquareTripleFirst', 'SquareTripleMix', 'SawTripleFirst', 'SawTripleMix')
    for name in names:
        match = re.search(r'void AnalogOscillator::Render' + name + r'\([^{}]*\{.*?\n\}', source, re.S)
        if not match:
            raise ValueError('Missing analog division renderer ' + name)
        # Classic Saw benefits from a call; the shorter ordinary/Triple loops
        # are faster inline. Hard-sync selection remains in sync_dispatch.py.
        helper = 'macroosc::transition_quotient_call' if name == 'CSawNoSync' else 'macroosc::transition_quotient'
        body = match[0]
        count = 0
        for phase in ('phase', 'phase_'):
            for numerator, shift in ((phase + ' - pw', 16), (phase, 16), (phase, 7)):
                expression = ('(' + numerator + ')' if ' - ' in numerator else numerator) + ' / (phase_increment >> ' + str(shift) + ')'
                count += body.count(expression)
                body = body.replace(expression, helper + '(' + numerator + ', phase_increment >> ' + str(shift) + ')')
        expected = 2 if name.startswith('Saw') else 3
        if count != expected:
            raise ValueError('Analog transition divisions changed: ' + name)
        if name.startswith('Square'):
            body = expand_square_transitions(body)
        source = source[:match.start()] + body + source[match.end():]
    return source
