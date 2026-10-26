// Page 2's Braids settings, shared by sound and screen: bit reduction, sample
// rate reduction and VCO drift. Each parameter's 0 leaves the sound as it was,
// so sounds saved before page 2 existed (slots 5..7 hold 0) are unchanged.
#pragma once
namespace macroosc {
// 2^(-k/8) in Q16 for k = 0..8: one octave, interpolated between eighths.
static const unsigned kHalvingQ16[] = {65536, 60097, 55109, 50535, 46341, 42495, 38968, 35734, 32768};
// 2^(-octaves) in Q16, for Q15 octaves (32768 = one octave), at most 15 octaves.
inline unsigned HalvingQ16(unsigned octaves_q15) {
  const unsigned whole = octaves_q15 >> 15, eighth = (octaves_q15 >> 12) & 7, fraction = octaves_q15 & 4095;
  const unsigned a = kHalvingQ16[eighth], b = kHalvingQ16[eighth + 1];
  return (a - (((a - b) * fraction) >> 12)) >> whole;
}
// Bits: 16 at 0 down to 2 at full, as the module's resolution setting.
inline unsigned BitsFromParameter(int q31) {
  return 16 - (((q31 < 0 ? 0 : static_cast<unsigned>(q31) >> 16) * 14) >> 15);
}
// The mask a sample keeps at that many bits; negative samples round down, as on the module.
inline short BitMask(unsigned bits) { return static_cast<short>(0xffffu << (16 - bits)); }
// Rate: a sample-and-hold whose Q16 step per output sample falls from 1 (every
// sample, 44.1 kHz) at 0 through four octaves to 1/16 (2.76 kHz) at full. Continuous,
// where the module stepped through fixed divisions of 96 kHz.
const unsigned kRateOctaves = 4;
inline unsigned RateStepQ16(int q31) {
  const unsigned amount = q31 < 0 ? 0 : static_cast<unsigned>(q31) >> 16;  // 0..32767
  return HalvingQ16(amount * kRateOctaves);
}
// Drift: a slow random walk of the oscillator's pitch, at full at most this
// many 1/128 semitones (about a third of a semitone) either way.
const int kDriftUnits = 48;
}  // namespace macroosc
