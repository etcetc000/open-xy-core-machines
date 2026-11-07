"""Triple Square: the band-limited square loops in the native blep_square routine.

macroosc::native::blep_square runs RenderSquareTripleFirst/Mix's original sample
loop (same increment interpolation, edge times and polynomial steps) when the
state is in the routine's range; the original loop stays as the fallback.
AnalogOscillator::RenderSquareTriple prepares all three oscillators as
RenderTripleFirst/Mix would and renders them in one routine call.
"""

LOOP = '  int32_t next_sample = next_sample_;\n  while (size--) {\n'

KERNEL = '''  // The native routine runs the band-limited square loop (sync is unused here)
  // whenever the state is in its range; the loop below covers the rest.
  macroosc::native::BlepSquare square = {phase, phase_increment, phase_increment_increment, next_sample_, high};
  if (macroosc::native::blep_square(square, buffer, size, 21, MIX)) {
    phase_=square.phase;high_=square.high;
    next_sample_ = square.next_sample;
    previous_phase_increment_ = square.increment;
    return;
  }
'''

TRIPLE = r'''namespace {
inline bool SameSquare(const macroosc::native::BlepSquare& a, const macroosc::native::BlepSquare& b) {
  return a.phase == b.phase && a.increment == b.increment && a.increment_step == b.increment_step &&
      a.next_sample == b.next_sample && a.high == b.high;
}
}  // namespace

void AnalogOscillator::RenderSquareTriple(AnalogOscillator* osc, int16_t* __restrict__ buffer, size_t size) {
  // RenderTripleFirst/Mix's preparation and the interpolated increment, for
  // each oscillator; one native routine call then renders all three when every one
  // is in its range (the steady shortcut below is an exact special case of the
  // same loop). Otherwise each renders in turn exactly as before.
  macroosc::native::BlepSquare square[3];
  const auto save = [](AnalogOscillator& o, const macroosc::native::BlepSquare& s) {
    o.phase_ = s.phase;
    o.high_ = s.high;
    o.next_sample_ = s.next_sample;
    o.previous_phase_increment_ = s.increment;
  };
  // phase_delta_inline's quotient by the size: a shift for 8 and 16.
  const unsigned shift = size == 8 ? 3u : size == 16 ? 4u : 0u;
  for (size_t i = 0; i < 3; ++i) {
    AnalogOscillator& o = osc[i];
    if (o.shape_ != o.previous_shape_) {
      o.Init();
      o.previous_shape_ = o.shape_;
    }
    const uint32_t target = o.ComputePhaseIncrement(o.pitch_);
    o.phase_increment_ = target;
    if (o.pitch_ > kHighestNote) {
      o.pitch_ = kHighestNote;
    } else if (o.pitch_ < 0) {
      o.pitch_ = 0;
    }
    const uint32_t increment = o.previous_phase_increment_;
    const bool rising = increment < target;
    const uint32_t distance = rising ? target - increment : increment - target;
    const uint32_t delta = shift ? distance >> shift : macroosc::chunk_math::phase_delta_inline(distance, size);
    square[i].phase = o.phase_;
    square[i].increment = increment;
    square[i].increment_step = rising ? delta : ~delta;
    square[i].next_sample = o.next_sample_;
    square[i].high = o.high_;
  }
  // Oscillators in the same state (unison: Timbre or Color at the middle)
  // render the same samples, and the buffer's 16-bit sums do not depend on
  // their order: render one of a matching pair or triple, written, multiply
  // it by the count, and add the remaining oscillator.
  int single = -1;
  int16_t copies = 2;
  if (SameSquare(square[1], square[2])) {
    if (SameSquare(square[0], square[1])) copies = 3; else single = 0;
  } else if (SameSquare(square[0], square[1])) {
    single = 2;
  } else if (SameSquare(square[0], square[2])) {
    single = 1;
  } else {
    copies = 0;
  }
  if (copies) {
    const int shared = single == 0 ? 1 : 0;
    macroosc::native::BlepSquare& s = square[shared];
    if (macroosc::native::blep_square(s, buffer, size, 21, false)) {
      for (size_t n = 0; n < size; ++n) buffer[n] = static_cast<int16_t>(buffer[n] * copies);
      for (int i = 0; i < 3; ++i) if (i != single) save(osc[i], s);
      if (single < 0) return;
      if (macroosc::native::blep_square(square[single], buffer, size, 21, true)) {
        save(osc[single], square[single]);
      } else {
        osc[single].RenderSquareTripleMix(nullptr, buffer, nullptr, size);
      }
      return;
    }
  }
  macroosc::native::BlepSquare* const squares[3] = {&square[0], &square[1], &square[2]};
  if (macroosc::native::blep_squares(squares, 3, buffer, size, 21)) {
    for (size_t i = 0; i < 3; ++i) save(osc[i], square[i]);
    return;
  }
  osc[0].RenderSquareTripleFirst(nullptr, buffer, nullptr, size);
  osc[1].RenderSquareTripleMix(nullptr, buffer, nullptr, size);
  osc[2].RenderSquareTripleMix(nullptr, buffer, nullptr, size);
}
'''

MACRO_OLD = '''  if (base_shape == OSC_SHAPE_SQUARE || base_shape == OSC_SHAPE_SAW) {
    analog_oscillator_[0].RenderTripleFirst(nullptr, buffer, nullptr, size);'''

MACRO_NEW = '''  if (base_shape == OSC_SHAPE_SQUARE) {
    AnalogOscillator::RenderSquareTriple(analog_oscillator_, buffer, size);
    return;
  }
''' + MACRO_OLD

DECLARATION = '  void RenderPairNoSync(AnalogOscillator&, int16_t*, int16_t*, size_t);\n'
FOLD_INCLUDE = '#include "native/fold_pair.hpp"\n'


def transform_analog(source):
    for name, mix in (('RenderSquareTripleMix', 'true'), ('RenderSquareTripleFirst', 'false')):
        start = source.index('void AnalogOscillator::' + name + '(')
        at = source.index(LOOP, start)
        if source.count('void AnalogOscillator::' + name + '(') != 1 or at > source.index('\n}\n', start):
            raise ValueError(name + ' loop changed')
        source = source[:at] + KERNEL.replace('MIX', mix) + source[at:]
    anchor = 'void AnalogOscillator::RenderTripleFirst('
    if source.count(anchor) != 1 or source.count(FOLD_INCLUDE) != 1:
        raise ValueError('Triple renderers changed')
    source = source.replace(anchor, TRIPLE + anchor)
    return source.replace(FOLD_INCLUDE, '#include "native/blep_square.hpp"\n' + FOLD_INCLUDE)


def transform_macro(source):
    if source.count(MACRO_OLD) != 1:
        raise ValueError('Triple dispatch changed')
    return source.replace(MACRO_OLD, MACRO_NEW)


def transform_header(header):
    if header.count(DECLARATION) != 1:
        raise ValueError('Analog pair declarations changed')
    return header.replace(DECLARATION, DECLARATION +
                          '  // Triple Square\'s three oscillators together: the first written, the others added.\n'
                          '  static void RenderSquareTriple(AnalogOscillator* oscillators, int16_t* buffer, size_t size);\n')
