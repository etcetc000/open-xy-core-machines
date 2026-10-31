"""Use the bounded signed-byte mixer only for byte-backed delay lines."""
import re


def transform(source):
    for name, shift, fields in (
            ('Bowed', 8, (('b', 'bowed.bridge'), ('n', 'bowed.neck'))),
            ('Fluted', 9, (('b', 'fluted.bore'), ('j', 'fluted.jet')))):
        match = re.search(r'void DigitalOscillator::Render' + name + r'\([^{}]*\{.*?\n\}', source, re.S)
        if not match:
            raise ValueError('Missing byte-delay renderer ' + name)
        body = match[0]
        for pointer, field in fields:
            declaration = (r'int8_t\* (?:__restrict__ )?dl_' + pointer +
                           r' = delay_lines_->' + re.escape(field) + ';')
            if len(re.findall(declaration, body)) != 1:
                raise ValueError('Delay sample type changed for ' + field)
        pattern = r'Mix\(\s*([^,]+), ([^,]+), ([^)]+)\) << ' + str(shift)
        body, count = re.subn(pattern, lambda m: 'macroosc::mix_byte<' + str(shift) +
                             '>(' + m[1] + ', ' + m[2] + ', ' + m[3] + ')', body)
        if count != (2 if name == 'Bowed' else 8):
            raise ValueError('Byte-delay mixer count changed for ' + name)
        source = source[:match.start()] + body + source[match.end():]
    return '#include "optimized/byte_mix.hpp"\n' + source
