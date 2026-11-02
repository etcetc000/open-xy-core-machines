#include "../display.cpp"
#include <cassert>
static opxy::i32 words[4];
extern "C" opxy::i32 sdk_control_display(opxy::u32 index) { return index < 4 ? words[index] : 0; }
int main() {
    using namespace opxy::draw;
    morph::DisplayState state = {};
    morph::Frame frame = {};
    Input input = {};
    morph::display_reset(state);
    // Nothing from the control (a browser preview): all slots empty, morph idle at A.
    words[3] = morph::status_no_a | morph::status_no_b;
    morph::display(state, input, frame);
    for (unsigned s = 0; s < 10; ++s) assert(frame.nodes[s].paint == stroke);
    assert(frame.nodes[10].shade == 5 && frame.nodes[10].x == 60 * unit);
    // A = slot 2, B = slot 9 (the black key labelled 0), both taken, heard halfway.
    // Choices: 0 live, else slot + 1.
    words[0] = 3 | 10 << 4 | 0x204 << 8;
    words[2] = static_cast<opxy::i32>(morph::one / 2);
    words[3] = 0;
    morph::display(state, input, frame);
    assert(frame.nodes[2].shade == 15 && frame.nodes[9].shade == 15 && frame.nodes[2].paint == fill_and_stroke);
    assert(frame.nodes[0].shade == 2 && frame.nodes[10].shade == 15 && frame.nodes[10].x == 240 * unit);
    assert(frame.nodes[11].x == (60 + 2 * 40) * unit && frame.nodes[12].x == (60 + 9 * 40) * unit);
    assert(frame.nodes[13].x > 480 * unit && frame.nodes[15].x == 0);  // "muted" covered
    words[3] = morph::status_muted;
    morph::display(state, input, frame);
    assert(frame.nodes[13].x == 0 && frame.nodes[15].x > 480 * unit && frame.nodes[10].shade == 5);
    // A and B the same: nothing to steer, the marker dims.
    words[0] = 3 | 3 << 4 | 0x204 << 8;
    words[3] = 0;
    morph::display(state, input, frame);
    assert(frame.nodes[10].shade == 5);
    // A live: its box lit, A's marker under it.
    words[0] = 0 | 3 << 4;
    words[3] = 0;
    morph::display(state, input, frame);
    assert(frame.nodes[14].shade == 15 && frame.nodes[11].x == 20 * unit && frame.nodes[12].x == (60 + 2 * 40) * unit);
}
