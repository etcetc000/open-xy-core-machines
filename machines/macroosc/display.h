#pragma once
#include <opxy/draw.hpp>

namespace macroosc {
// Four 14-segment characters drawn four times (the segments, then three glow
// layers underneath), then four dials, then the eight steps of the header bar:
// 236 nodes. Each layer has a lit and an unlit palette entry. Sixteen text
// slots draw both pages over the stock header: four titles and four values in
// the bar's colours for the page being edited, then four of each in grey for
// the other.
const unsigned kCharacters = 4, kLayers = 4, kDialNode = 224, kBarNode = 228, kBarSteps = 8;
const unsigned kTitleText = 0, kValueText = 4, kGreyTitleText = 8, kGreyValueText = 12;
// Each dial's pivot in the artwork (Q10 pixels): Timbre, Color, FM Ratio, FM Depth.
const opxy::i32 kDialX[4] = {106015, 242207, 379423, 379423}, kDialY[4] = {176 << 10, 176 << 10, 176 << 10, 78 << 10};
using Frame = opxy::draw::Frame<0, 0, 236, 16, 8>;
struct DisplayState {
  opxy::u32 model;  // The model shown, plus one; 0 before the first frame.
  opxy::u32 page;   // The page being edited, plus one; 0 before the first frame.
  opxy::u32 track;
  opxy::i32 since;  // Q10 seconds since the page last switched.
};
}  // namespace macroosc
