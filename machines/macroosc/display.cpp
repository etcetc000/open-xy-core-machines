// Shared display behavior. The SDK applies this bounded frame to the retained
// segment, glow and dial nodes, the page row's text and the header values, in
// the browser and on the device.
#include "display.h"
#include "fm.h"
#include "lofi.h"
#include "models.hpp"
#include "segments.hpp"

namespace macroosc {
// Four characters show the model; its code replaces the first control's value.
// A press switches between two pages (definition.json "press": "page"): the
// header shows one, and the row under it shows the other in grey.
// The knobs turn 144 degrees either side of 12 o'clock, end to end of their ticks.
const opxy::i32 kSweep = 1440;
const unsigned kModels = sizeof(models) / sizeof(models[0]);
// Parameter slots (definition.json); page 2 is slots 5, 6, 7 and 4 on positions 0..3.
const unsigned kModelSlot = 0, kTimbreSlot = 1, kColorSlot = 2, kRatioSlot = 3, kDepthSlot = 4, kBitsSlot = 5,
               kRateSlot = 6, kDriftSlot = 7;
const unsigned kPages[2][4] = {{kModelSlot, kTimbreSlot, kColorSlot, kRatioSlot},
                               {kBitsSlot, kRateSlot, kDriftSlot, kDepthSlot}};
// Each slot's title as the stock header writes it.
const char* const kTitles[8] = {"model", "timbre", "color", "fm ratio", "fm depth", "bits", "rate", "drift"};
// Header (stock) and page row positions, Q10 pixels: titles at each column's
// left, values 65 pixels in; the row's values are the header's at 0.65 scale.
const opxy::i32 kColumn = 120 << 10, kTitleX = 5 << 10, kValueX = 65 << 10;
const opxy::i32 kHeaderTitleY = 10 << 10, kHeaderValueY = 17818, kRowY = 34 << 10, kRowValueScale = 666;
// A switch slides the page leaving the header down into the row, its values
// shrinking, on an exponential ease-out: the distance left halves every 21 ms,
// so it is within 1% after 0.15 s, when it settles.
const opxy::i32 kSettle = 154;            // 0.15 s, Q10
const unsigned kOctavesPerQ10 = 1496;     // Q15 halvings per Q10 second: one per 21 ms

// Round only the screen caption; the parameter keeps its full Q31 precision.
inline void amount(char* out, unsigned capacity, opxy::i32 value) {
  const unsigned p = value < 0 ? 0 : ((static_cast<unsigned>(value) >> 10) * 100 + (1u << 20)) >> 21;
  char text[4];
  unsigned n = 0;
  if (p >= 100) text[n++] = '1';
  if (p >= 10) text[n++] = static_cast<char>('0' + p / 10 % 10);
  text[n++] = static_cast<char>('0' + p % 10);
  text[n] = 0;
  opxy::draw::copy(out, capacity, text);
}
// One decimal place; audio retains the full interpolated Q16 ratio.
inline void ratio(char* out, unsigned capacity, unsigned q16) {
  const unsigned tenths = (q16 * 10 + 32768) >> 16;
  char text[4] = {static_cast<char>('0' + tenths / 10), '.', static_cast<char>('0' + tenths % 10), 0};
  opxy::draw::copy(out, capacity, text);
}
inline void bits(char* out, unsigned capacity, unsigned count) {
  char text[3] = {static_cast<char>(count >= 10 ? '1' : '0' + count), static_cast<char>('0' + count % 10), 0};
  opxy::draw::copy(out, capacity, count >= 10 ? text : text + 1);
}
// The held sample rate in kHz: whole above 10, else with one decimal.
inline void rate(char* out, unsigned capacity, unsigned step) {
  const unsigned hz = (44100u * step) >> 16;
  char text[5];
  if (hz >= 9950) {
    const unsigned k = (hz + 500) / 1000;
    text[0] = static_cast<char>('0' + k / 10);
    text[1] = static_cast<char>('0' + k % 10);
    text[2] = 'k';
    text[3] = 0;
  } else {
    const unsigned tenths = (hz + 50) / 100;
    text[0] = static_cast<char>('0' + tenths / 10);
    text[1] = '.';
    text[2] = static_cast<char>('0' + tenths % 10);
    text[3] = 'k';
    text[4] = 0;
  }
  opxy::draw::copy(out, capacity, text);
}
// A slot's value as the header and the page row show it.
inline void caption(char* out, unsigned capacity, unsigned slot, const opxy::draw::Input& input, unsigned model) {
  const opxy::i32 value = input.slots[slot];
  switch (slot) {
    case kModelSlot: opxy::draw::copy(out, capacity, models[model].code); break;
    case kRatioSlot: ratio(out, capacity, RatioQ16FromParameter(value)); break;
    case kBitsSlot: bits(out, capacity, BitsFromParameter(value)); break;
    case kRateSlot: rate(out, capacity, RateStepQ16(value)); break;
    default: amount(out, capacity, value); break;
  }
}
// Between the row and the header: `left` (Q16) of the way back up.
inline opxy::i32 between(opxy::i32 row, opxy::i32 header, unsigned left) {
  return row + ((header - row) * static_cast<opxy::i32>(left >> 6) >> 10);
}

inline void display_reset(DisplayState& state) {
  state.model = 0;
  state.page = 0;
  state.track = 0;
  state.since = kSettle;
}

inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
  const opxy::i32 model = input.slots[kModelSlot];
  unsigned index = model < 0 ? 0 : ((static_cast<unsigned>(model) >> 16) * kModels) >> 15;
  // The characters change only with the model; the renderers skip unchanged
  // nodes. The layers are the segments (palette 0 and 1), then each glow layer
  // (2 and 3, 4 and 5, 6 and 7).
  if (state.model != index + 1) {
    for (unsigned layer = 0; layer < kLayers; ++layer)
      paint_segments(frame, layer * kCharacters * kSegments, models[index].masks, kCharacters, 2 * layer,
                     2 * layer + 1);
    state.model = index + 1;
  }
  // The header's page: page 2 once the first position shows an alternate.
  const unsigned page = input.shown[0] >= 4 ? 1 : 0;
  if (state.page && state.track == input.track && state.page != page + 1 && !input.reduced_motion)
    state.since = 0;
  else if (state.since < kSettle)
    state.since += opxy::draw::clamp(input.elapsed, 0, kSettle);
  if (input.reduced_motion || state.since > kSettle) state.since = kSettle;
  state.page = page + 1;
  state.track = input.track;
  const unsigned left = state.since >= kSettle ? 0 : HalvingQ16(static_cast<unsigned>(state.since) * kOctavesPerQ10);
  for (unsigned i = 0; i < 4; ++i) {
    caption(frame.header[i], sizeof(frame.header[i]), input.shown[i], input, index);
    const unsigned slot = kPages[1 - page][i];
    const opxy::i32 x = i * kColumn;
    opxy::draw::text(frame, kTitleText + i, x + kTitleX, between(kRowY, kHeaderTitleY, left), opxy::draw::unit, kTitles[slot]);
    char value[opxy::draw::header_capacity];
    caption(value, sizeof(value), slot, input, index);
    opxy::draw::text(frame, kValueText + i, x + kValueX, between(kRowY, kHeaderValueY, left),
                     between(kRowValueScale, opxy::draw::unit, left), value);
  }
  // Knobs: Timbre and Color along the bottom; on the right, the linked pair
  // the fourth encoder switches between, each showing its own parameter: FM
  // Ratio below, stepping between ratios, and FM Depth above.
  const unsigned slots[4] = {kTimbreSlot, kColorSlot, kRatioSlot, kDepthSlot};
  for (unsigned i = 0; i < 4; ++i) {
    opxy::draw::place(frame, kDialNode + i, kDialX[i], kDialY[i], opxy::draw::unit, 0, opxy::draw::keep);
    opxy::draw::turn(frame, kDialNode + i, opxy::draw::sweep(input.slots[slots[i]], kSweep));
  }
}
}  // namespace macroosc
