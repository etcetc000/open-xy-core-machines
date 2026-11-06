"""Harmonics: the twelve-partial loop as the native additive routine.

macroosc::native::additive defines the same output pair: per pair the phase steps,
each partial k adds floor(sine(k * phase) * a_k / 2^15) and then glides by
(target_k - a_k) >> 8, and the pair is (out + previous) >> 1, then out. The
routine keeps amplitudes and targets two to a word, so the state holds them
packed: the same 0..32767 values, cleared to zero as before. Spectrum
normalization keeps the targets' sum at most 32767; the one-pole keeps the
amplitudes' sum there, so the routine's 16-bit lane sums are the C sums.
"""

START = 'static __attribute__((noinline)) int32_t RenderHarmonicSample('
END = 'void DigitalOscillator::RenderStruckDrum('

NEW = '''void DigitalOscillator::RenderHarmonics(
    const uint8_t* sync,
    int16_t* __restrict__ buffer,
    size_t size) {
  // OP-XY: the spectrum depends only on the two parameters. Recompute its
  // 49 divisions when they change, not on every render call.
  const uint32_t key = 0x80008000u |
      (static_cast<uint32_t>(parameter_[0]) << 16) | static_cast<uint16_t>(parameter_[1]);
  if (state_.hrm.key != key) {
    state_.hrm.key = key;
    PrepareHarmonicSpectrum(state_.hrm.target,parameter_[0],parameter_[1]);
    for (unsigned j = 0; j < kNumAdditiveHarmonics / 2; ++j) {
      state_.hrm.packed_target[j] = static_cast<uint16_t>(state_.hrm.target[2 * j]) |
          static_cast<uint32_t>(static_cast<uint16_t>(state_.hrm.target[2 * j + 1])) << 16;
    }
  }
  // The native additive routine. The adapter supplies zero external sync for
  // this renderer.
  macroosc::native::Additive additive;
  additive.phase = phase_;
  additive.increment = phase_increment_ << 1;
  additive.previous = state_.add.previous_sample;
  additive.amplitude = state_.hrm.amplitude;
  additive.target = state_.hrm.packed_target;
  additive.sine = macroosc::sine_half_pairs;
  // Use the cached targets directly while all twelve partials are in
  // range. Otherwise preserve the original per-partial frequency mask.
  uint32_t masked_targets[kNumAdditiveHarmonics / 2];
  if ((additive.increment >> 16) * kNumAdditiveHarmonics > 0x4000) {
    const unsigned keep=macroosc::transition_quotient(0x4000,additive.increment>>16);
    for (unsigned j = 0; j < kNumAdditiveHarmonics / 2; ++j) {
      const uint32_t pair = state_.hrm.packed_target[j];
      masked_targets[j] = 2 * j + 1 < keep ? pair : 2 * j < keep ? pair & 0xffffu : 0;
    }
    additive.target = masked_targets;
  }
  macroosc::native::additive(additive, buffer, size >> 1);
  state_.add.previous_sample = additive.previous;
  phase_ = additive.phase;
}

'''

STATE_OLD = '''struct HarmonicsState {
  int32_t amplitude[kNumAdditiveHarmonics];
  // OP-XY: spectrum cached per parameter pair; Init() clears the key.
  uint32_t key;
  int32_t target[kNumAdditiveHarmonics];
};'''

STATE_NEW = '''struct HarmonicsState {
  // OP-XY: amplitudes packed two to a word (harmonic 2j + 1 low, 2j + 2
  // high) for the native additive routine, with the targets packed the same.
  uint32_t amplitude[kNumAdditiveHarmonics / 2];
  // OP-XY: spectrum cached per parameter pair; Init() clears the key.
  uint32_t key;
  int32_t target[kNumAdditiveHarmonics];
  uint32_t packed_target[kNumAdditiveHarmonics / 2];
};'''


def transform(source):
    start = source.index(START)
    end = source.index(END, start)
    if source.count(START) != 1 or 'void DigitalOscillator::RenderHarmonics(' not in source[start:end]:
        raise ValueError('Harmonics renderer changed')
    return '#include "native/additive.hpp"\n' + source[:start] + NEW + source[end:]


def transform_header(header):
    if header.count(STATE_OLD) != 1:
        raise ValueError('Harmonics state changed')
    return header.replace(STATE_OLD, STATE_NEW)
