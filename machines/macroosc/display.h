#pragma once
#include <opxy/draw.hpp>

namespace macroosc {
// Four 14-segment characters drawn four times (the segments, then three glow
// layers underneath), then four dials: 228 nodes. Each layer has a lit and an
// unlit palette entry. Eight text slots: the page not shown, under the header,
// its four titles then its four values.
const unsigned kCharacters = 4, kLayers = 4, kDialNode = 224, kTitleText = 0, kValueText = 4;
// Each dial's pivot in the artwork (Q10 pixels): Timbre, Color, FM Ratio, FM Depth.
const opxy::i32 kDialX[4] = {106015, 242207, 379423, 379423}, kDialY[4] = {176 << 10, 176 << 10, 176 << 10, 78 << 10};
using Frame = opxy::draw::Frame<0, 0, 228, 8, 8>;
struct DisplayState {
  opxy::u32 model;  // The model shown, plus one; 0 before the first frame.
  opxy::u32 page;   // The page in the header, plus one; 0 before the first frame.
  opxy::u32 track;
  opxy::i32 since;  // Q10 seconds since the page last switched.
};
}  // namespace macroosc
