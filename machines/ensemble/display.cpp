#include "display.h"

namespace ensemble {
inline void display_reset(DisplayState& state) { state.page.reset(); }

// The effect page (effect_page.h), drawn as stock's FX page: both rows of controls
// with a level bar each, the send's badge and its input and return meters. Captions read 00..99 as stock's FX page's do, whatever the control
// maps to; a choice shows its name.
inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace opxy::draw;
    const char* const titles[8] = {"Rate", "Depth", "Delay", "Feedback", "Shape", "Stereo", "Level", "Dry"};
    char values[8][header_capacity];
    for (opxy::u32 i = 0; i < 8; ++i) page::header_number(values[i], header_capacity, input.slots[i]);
    copy(values[4], header_capacity, input.slots[4] >> 16 < 16384 ? "sine" : "tri");
    page::effect_page(state.page, input, frame, titles, values, 0);  // its nodes from 0
}
}
