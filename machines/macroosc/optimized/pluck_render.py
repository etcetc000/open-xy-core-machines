"""Plucked string: keep the advancing string's mask and length in registers.

Both are fixed while the string advances. Halving toward zero is adding the
sign bit, then a shift. Each random draw and comparison is unchanged.
"""

SIGNATURE = '''void DigitalOscillator::RenderPlucked(
    const uint8_t* sync,
    int16_t* buffer,
    size_t size) {'''

LOOP = (
    ("""        size_t num_loops = 0;
        while (write_ptr != read_ptr) {""",
     """        const size_t mask = p->mask, string_size = p->size;
        while (write_ptr != read_ptr) {"""),
    ("""          ++num_loops;
          size_t next = (write_ptr + 1) & p->mask;""",
     """          size_t next = (write_ptr + 1) & mask;"""),
    ("""            sum = sum < 0 ? -(-sum >> 1) : (sum >> 1);""",
     """            sum = (sum + static_cast<int32_t>(static_cast<uint32_t>(sum) >> 31)) >> 1;"""),
    ("""            dl[p->size] = dl[0];""",
     """            dl[string_size] = dl[0];"""),
)


def transform(source):
    start = source.index(SIGNATURE)
    end = source.index('\n}\n', start) + 3
    body = source[start:end]
    for old, new in LOOP:
        if body.count(old) != 1:
            raise ValueError('Plucked string loop changed')
        body = body.replace(old, new)
    return source[:start] + body + source[end:]
