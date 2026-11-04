#include "display.h"
#include "controls.h"
#include <opxy/track_control.hpp>
// "live" and the ten snapshot slots, one per black key (dim when empty, lit when
// taken, brightest for A and B), A and B's markers under them, and the morph line
// with where it is heard.
// Everything comes from the control's display words (controls.h).
namespace morph {
const opxy::i32 live_x = 20, slot_x0 = 60, slot_pitch = 40, slot_y = 90, line_x0 = 60, line_width = 360, line_y = 160;

inline void display_reset(DisplayState& state) { state.unused = 0; }

inline void display(DisplayState&, const opxy::draw::Input&, Frame& frame) {
    using namespace opxy::draw;
    const opxy::u32 slots = static_cast<opxy::u32>(opxy::control::from_control(0));
    const opxy::u32 position = static_cast<opxy::u32>(opxy::control::from_control(2));
    const opxy::u32 status = static_cast<opxy::u32>(opxy::control::from_control(3));
    // Choices: 0 live, else slot + 1.
    const opxy::u32 a = slots & 15, b = slots >> 4 & 15, taken = slots >> 8 & 0x3ff;
    const auto x_of = [](opxy::u32 c) {
        return c == 0 ? live_x : slot_x0 + slot_pitch * static_cast<opxy::i32>(c - 1);
    };
    place(frame, 14, live_x * unit, slot_y * unit, unit, a == 0 || b == 0 ? 15 : 9, fill_and_stroke);
    for (opxy::u32 s = 0; s < 10; ++s) {
        const bool chosen = s + 1 == a || s + 1 == b;
        const opxy::u32 shade = chosen ? (taken & (1u << s) ? 15 : 6) : taken & (1u << s) ? 9 : 2;
        place(frame, s, (slot_x0 + slot_pitch * static_cast<opxy::i32>(s)) * unit, slot_y * unit, unit, shade,
              taken & (1u << s) ? fill_and_stroke : stroke);
    }
    place(frame, 11, x_of(a) * unit, (slot_y + 26) * unit, unit,
          status & status_no_a ? 5 : 13, fill_and_stroke);
    place(frame, 12, x_of(b) * unit, (slot_y + 26) * unit, unit,
          status & status_no_b ? 5 : 13, fill_and_stroke);
    // Where the morph is heard; dim while it steers nothing: muted, a side
    // missing, or A and B the same.
    const bool idle = (status & (status_muted | status_no_a | status_no_b)) != 0 || a == b;
    const opxy::u32 x = position > one ? one : position;
    place(frame, 10, (line_x0 + static_cast<opxy::i32>((x * static_cast<opxy::u32>(line_width)) >> 16)) * unit,
          line_y * unit, unit, idle ? 5 : 15, fill_and_stroke);
    // The muted cover over the line sits off screen unless muted.
    place(frame, 13, status & status_muted ? 0 : 600 * unit, 0, unit, 0, fill_and_stroke);
    // The word "muted" under the line, hidden by its own cover unless muted.
    place(frame, 15, status & status_muted ? 600 * unit : 0, 0, unit, 0, fill_and_stroke);
}
}
