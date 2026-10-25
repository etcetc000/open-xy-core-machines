// Authored Macro Osc sound around the vendored Braids DSP. The SDK supplies
// storage, per-sample pitch increments and the firmware adapters; the vendored
// DSP stays independently pinned.
#include "core.h"
#include "fm.h"
#include "pitch.h"
#include "stmlib/utils/dsp.h"
#include "stmlib/utils/random.h"

namespace stmlib {
uint32_t Random::rng_state_;
}
namespace macroosc {
using braids::MacroOscillator;
using braids::MacroOscillatorShape;
const unsigned kNumShapes = 47, kBlockSize = 64, kMaxChunk = 24;
const uint8_t kNoSync[kMaxChunk] = {};
// FM: a sine modulator at a ratio of the note retunes the carrier every
// kFmChunk samples (5.5 kHz), which keeps an x8 modulator unaliased up to
// about F4. Each extra Braids render call costs per-call setup, so FM renders
// cost more than unmodulated ones.
const unsigned kFmChunk = 8;
// Output level per int16 Braids sample (32768 would be half-scale Q31): -6.8 dB
// from half scale, so stock volume readouts match stock instruments and 2op FM.
// The declared output peak in definition.json is 32768 * kOutputLevel.
const int32_t kOutputLevel = 14910;
inline bool UsesDelayLines(MacroOscillatorShape shape) {
  return shape == braids::MACRO_OSC_SHAPE_SAW_COMB ||
         (shape >= braids::MACRO_OSC_SHAPE_PLUCKED && shape <= braids::MACRO_OSC_SHAPE_FLUTED);
}

// The bowed, blown and fluted models sustain a delay line whose length follows
// the pitch; FM swinging it far stops them sounding at all. Full depth swings
// these by at most what each survives at any note, ratio, Timbre and Color
// (host audit): a tenth for blown, a twentieth for bowed, a fiftieth for
// fluted. The snare's tuned filters fall silent near zero frequency, so its
// swing stops at half. Every other model takes the full swing.
inline uint32_t DepthDivisor(MacroOscillatorShape shape) {
  switch (shape) {
    case braids::MACRO_OSC_SHAPE_BLOWN: return 10;
    case braids::MACRO_OSC_SHAPE_BOWED: return 20;
    case braids::MACRO_OSC_SHAPE_FLUTED: return 50;
    case braids::MACRO_OSC_SHAPE_SNARE: return 2;
    default: return 1;
  }
}
// These models decay or smooth once per render call, not per sample: they keep
// the unmodulated call sizes under FM, so a touch of depth leaves their decay alone.
inline bool PerCallState(MacroOscillatorShape shape) {
  return shape == braids::MACRO_OSC_SHAPE_STRUCK_BELL || shape == braids::MACRO_OSC_SHAPE_STRUCK_DRUM ||
         shape == braids::MACRO_OSC_SHAPE_DIGITAL_FILTER_LP || shape == braids::MACRO_OSC_SHAPE_DIGITAL_FILTER_PK;
}

inline uint32_t Unipolar15(int32_t q31) {
  return q31 < 0 ? 0 : static_cast<uint32_t>(q31) >> 16;  // 0..32767
}

inline MacroOscillatorShape ShapeFromParameter(int32_t q31) {
  return static_cast<MacroOscillatorShape>((Unipolar15(q31) * kNumShapes) >> 15);  // 0..46
}

// Inverse of the stock increment: Braids pitch in 1/128 semitone, MIDI 0 = 0.
static int32_t PitchFromIncrement(uint32_t increment) {
  if (increment == 0) {
    return 0;
  }
  uint32_t exponent = 31;
  while (!(increment & 0x80000000UL)) {
    increment <<= 1;
    --exponent;
  }
  uint32_t index = (increment >> 23) & 0xff;
  uint32_t fraction = (increment >> 15) & 0xff;
  uint32_t a = braids::kOpxyLog2Q16[index];
  uint32_t b = braids::kOpxyLog2Q16[index + 1];
  uint32_t log2 = (exponent << 16) + a + (((b - a) * fraction) >> 8);
  return static_cast<int32_t>((log2 * 3) >> 7) + braids::kOpxyPitchOffset;
}

void destroy(Voice& voice, Shared& state, opxy::u32 owner) {
  state.delays.release(owner, voice.delay_slot);
  voice.delay_slot = 0;
  voice.osc.set_delay_lines(nullptr);
  voice.strike_pending = 0;
}
void initialize(Voice& voice, Shared& state, opxy::u32 owner) {
  destroy(voice, state, owner);
  std::memset(static_cast<void*>(&voice), 0, sizeof(voice));
  voice.rng_state = 0x21u + (owner - 1) * 0x9e3779b9u;
  voice.osc.Init();
  voice.strike_pending = 1;
}
void start(Voice& voice, Shared&, opxy::u32) {
  voice.strike_pending = 1;
  voice.fm_phase = 0;
}

inline int32_t ClampPitch(int32_t pitch) {
  return pitch < 0 ? 0 : pitch > 16383 ? 16383 : pitch;
}
void render(Voice& voice, Shared& state, opxy::u32 owner, opxy::Output out,
            opxy::Parameters parameters, opxy::Pitch pitch_increments) {
  MacroOscillator* osc = &voice.osc;
  MacroOscillatorShape shape = ShapeFromParameter(parameters[0]);
  const uint32_t depth = Unipolar15(parameters[4]) / DepthDivisor(shape);
  osc->set_parameters(Unipolar15(parameters[1]), Unipolar15(parameters[2]));

  bool changed = shape != osc->shape();
  osc->set_shape(shape);  // Strikes on change, as on the module.
  if (UsesDelayLines(shape)) {
    if (changed || voice.strike_pending) {
      voice.delay_slot = state.delays.acquire(owner, voice.delay_slot);
    }
  } else if (voice.delay_slot != 0) {
    state.delays.release(owner, voice.delay_slot);
    voice.delay_slot = 0;
    osc->set_delay_lines(nullptr);
  }
  if (voice.strike_pending) {
    osc->Strike();
    voice.strike_pending = 0;
  }

  if (UsesDelayLines(shape) && !state.delays.get(owner, voice.delay_slot)) {
    // Storage was reassigned to a newer note; stay silent until restruck.
    for (size_t i = 0; i < kBlockSize; ++i) {
      out[i] = 0;
    }
    return;
  }

  osc->set_delay_lines(state.delays.get(owner, voice.delay_slot));
  stmlib::Random::Seed(voice.rng_state);
  int16_t block[kMaxChunk];
  if (depth == 0) {
    for (size_t offset = 0; offset < kBlockSize; offset += kMaxChunk) {
      size_t size = kBlockSize - offset < kMaxChunk ? kBlockSize - offset : kMaxChunk;
      osc->set_pitch(ClampPitch(PitchFromIncrement(pitch_increments[offset])));
      osc->Render(kNoSync, block, size);
      for (size_t i = 0; i < size; ++i) {
        out[offset + i] = static_cast<int32_t>(block[i]) * kOutputLevel;
      }
    }
  } else {
    // Linear FM: at full depth the carrier frequency swings between zero and
    // twice the note. Increments stay below half the sample rate (2^31).
    const uint32_t ratio = RatioQ16FromParameter(parameters[3]);
    const size_t chunk = PerCallState(shape) ? kMaxChunk : kFmChunk;
    for (size_t offset = 0; offset < kBlockSize; offset += chunk) {
      const size_t size = kBlockSize - offset < chunk ? kBlockSize - offset : chunk;
      const uint32_t increment = pitch_increments[offset] < 0x80000000u ? pitch_increments[offset] : 0x7fffffffu;
      const int32_t swing = (static_cast<int32_t>(depth) * stmlib::Interpolate824(braids::wav_sine, voice.fm_phase)) >> 15;
      const uint32_t carrier = increment + static_cast<uint32_t>(static_cast<int32_t>(increment >> 15) * swing);
      voice.fm_phase += RatioIncrement(increment, ratio) * static_cast<uint32_t>(size);
      osc->set_pitch(ClampPitch(PitchFromIncrement(carrier)));
      osc->Render(kNoSync, block, size);
      for (size_t i = 0; i < size; ++i) {
        out[offset + i] = static_cast<int32_t>(block[i]) * kOutputLevel;
      }
    }
  }
  voice.rng_state = stmlib::Random::state();
}
}  // namespace macroosc
