"""Clear the waveguide models' delay lines on a strike by words, not bytes."""
import re

CLEARS = (
    ('Bowed', ('dl_b', 'bowed.bridge'), ('dl_n', 'bowed.neck')),
    ('Blown', ('dl', 'bore'),),
    ('Fluted', ('dl_b', 'fluted.bore'), ('dl_j', 'fluted.jet')),
)


def transform(source):
    for name, *fields in CLEARS:
        match = re.search(r'void DigitalOscillator::Render' + name + r'\([^{}]*\{.*?\n\}', source, re.S)
        if not match:
            raise ValueError('Missing delay-line renderer ' + name)
        body = match[0]
        for pointer, field in fields:
            old = 'memset(%s, 0, sizeof(delay_lines_->%s));' % (pointer, field)
            if body.count(old) != 1:
                raise ValueError('Delay-line clear changed for ' + field)
            body = body.replace(old, 'macroosc::clear_words(%s, sizeof(delay_lines_->%s));' % (pointer, field))
        source = source[:match.start()] + body + source[match.end():]
    return '#include "optimized/clear_words.hpp"\n' + source
