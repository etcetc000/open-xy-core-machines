"""Plucked string: the native pluck routine renders the strings; the loop stays as
the fallback, with the advancing string's mask and length kept in registers.

Both are fixed while the string advances. Halving toward zero is adding the
sign bit, then a shift. Each random draw and comparison is unchanged.
macroosc::native::pluck runs the same per-pair loop over the three strings (the
same draws in the same order, from stmlib's random state, and the same
excitation, averaging, loss, guard word and interpolation) when its arguments
are in its range. Each string's fields live in the routine's PluckString,
the first member of its PluckState.
"""
import re

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


SAMPLES = """  int16_t previous_sample = state_.plk[0].previous_sample;

  while (size) {"""

KERNEL = """  int16_t previous_sample = state_.plk[0].previous_sample;

  // The native routine renders all the strings' pairs when the call is in its
  // range; the loop below covers the rest.
  for (size_t i = 0; i < kNumPluckVoices; ++i) {
    state_.plk[i].string.line = delay_lines_->ks + i * 1025;
  }
  uint32_t random = Random::state();
  if (!(size & 1) && macroosc::native::pluck(&state_.plk[0].string, kNumPluckVoices, sizeof(PluckState), buffer,
                                          static_cast<unsigned>(size >> 1), random, update_probability, loss,
                                          previous_sample)) {
    Random::Seed(random);
    state_.plk[0].previous_sample = previous_sample;
    return;
  }

  while (size) {"""



# The string's fields live in its routine state (PluckState.string).
FIELDS = {'phase': 'phase', 'phase_increment': 'increment', 'write_ptr': 'write', 'shift': 'shift',
          'mask': 'mask', 'size': 'size', 'initialization_ptr': 'excite'}
FIELD = re.compile(r'->(' + '|'.join(sorted(FIELDS, key=len, reverse=True)) + r')\b')

STATE_OLD = """struct PluckState {
  size_t size;
  size_t write_ptr;
  size_t shift;
  size_t mask;
  size_t pluck_position;
  size_t initialization_ptr;
  uint32_t phase;
  uint32_t phase_increment;
  uint32_t max_phase_increment;
  int16_t previous_sample;
  uint8_t polyphony_assigner;
};"""

STATE_NEW = """struct PluckState {
  // OP-XY: size, write position, shift, mask, excitation left, phase and
  // increment, as the native pluck routine reads them.
  macroosc::native::PluckString string;
  size_t pluck_position;
  uint32_t max_phase_increment;
  int16_t previous_sample;
  uint8_t polyphony_assigner;
};"""

EXTENT_OLD = 'return state_.plk[k].size + 1; }'
EXTENT_NEW = 'return state_.plk[k].string.size + 1; }'


def transform_header(header):
    for old, new in ((STATE_OLD, STATE_NEW), (EXTENT_OLD, EXTENT_NEW)):
        if header.count(old) != 1:
            raise ValueError('Plucked string state changed')
        header = header.replace(old, new)
    return header.replace('#pragma once\n', '#pragma once\n#include "native/pluck.hpp"\n', 1)


def transform(source):
    start = source.index(SIGNATURE)
    end = source.index('\n}\n', start) + 3
    body = source[start:end]
    for old, new in LOOP:
        if body.count(old) != 1:
            raise ValueError('Plucked string loop changed')
        body = body.replace(old, new)
    if body.count(SAMPLES) != 1:
        raise ValueError('Plucked string loop changed')
    body = body.replace(SAMPLES, KERNEL)
    body = FIELD.sub(lambda m: '->string.' + FIELDS[m[1]], body)
    return '#include "native/pluck.hpp"\n' + source[:start] + body + source[end:]
