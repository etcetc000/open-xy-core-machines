// The LFO page layer end to end: its callbacks driven as the SDK runtime calls
// them, with fake actions standing in for the SDK's copies of stock's LFO and
// a fake canvas recording what it paints.
#include "extension.cpp"
#include <cstring>

using namespace opxy;
using namespace opxy::extensions;
#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)

namespace {
// Stock's LFO on track 3: a value LFO; and the copies the SDK keeps.
Lfo stock_lfo = {true, lfo_types::value, {100, 200, 300, 400, 500, 600, 700, 800}};
Lfo copies[8][3];
u32 sets, popups;
char popup[32];
u32 insert(const Context&, u32, const Note*, u32) { return 0; }
u32 replace(const Context&, u32, const Note*, u32) { return 0; }
void notify(const char* text) { ++popups; std::strcpy(popup, text); }
bool set_lfo(u32 track, u32 copy, const Lfo& lfo) {
    if (track >= 8 || copy >= lfo_copies) return false;
    copies[track][copy] = lfo; ++sets; return true;
}
bool get_lfo(u32 track, u32 index, Lfo& lfo) {
    if (track >= 8 || index > lfo_copies) return false;
    lfo = index ? copies[track][index - 1] : stock_lfo; return true;
}
const Actions actions = {insert, notify, replace, set_lfo, get_lfo};
const Context context = {};

bool send(Layer::Kind kind, u32 key = 0, bool pressed = false, i32 delta = 0, bool shift = false, u32 track = 2) {
    const Layer event = {kind, key, pressed, delta, 16, shift, track};
    return multi_lfo::hooks.layer(context, event, actions);
}
bool turn(u32 encoder, i32 delta, bool shift = false) { return send(Layer::turn, encoder, false, delta, shift); }
bool press(u32 key, bool shift = false) {
    const bool taken = send(Layer::keypress, key, true, 0, shift);
    if (taken) send(Layer::keypress, key, false, 0, shift);
    return taken;
}

// What the layer paints: each node's visibility and text.
bool visible[9];
char texts[9][32];
void show_node(void*, u32 node, bool on, i32, i32) { if (node < 9) visible[node] = on; }
void text_node(void*, u32 node, const char* text) { if (node < 9) std::strcpy(texts[node], text); }
const Canvas canvas = {nullptr, show_node, text_node};
}

int main() {
    send(Layer::open);
    // LFO 1 is stock's: the layer takes nothing and paints nothing.
    CHECK(!turn(0, 1) && !turn(3, -2) && !press(keys::m4) && !press(keys::encoder_click));
    multi_lfo::hooks.draw(canvas);
    CHECK(!visible[0] && !visible[8]);
    // Shift + encoder 1 picks LFO 2, which starts as a copy of LFO 1, off.
    CHECK(turn(0, 1, true));
    CHECK(popups == 1 && !std::strcmp(popup, "LFO 2") && sets == 1);
    CHECK(!copies[2][0].on && copies[2][0].type == lfo_types::value && copies[2][0].words[7] == 800);
    // Its encoders turn its words, 1% a detent, kept in range.
    CHECK(turn(0, 3) && copies[2][0].words[0] == 100 + 3 * (0x7fffffff / 100));
    CHECK(turn(1, -5) && copies[2][0].words[1] == 0);
    CHECK(turn(3, 1000) && copies[2][0].words[3] == 0x7fffffff);
    // M4 turns it on; Shift + M4 steps its type.
    CHECK(press(keys::m4) && copies[2][0].on);
    CHECK(press(keys::m4, true) && copies[2][0].type == lfo_types::random);
    // Random's encoder 4 is word 31, as on stock's page; its click steps it (4 steps).
    CHECK(turn(3, 1) && copies[2][0].words[7] == 800 + 0x7fffffff / 100);
    CHECK(press(keys::encoder_click + 3) && copies[2][0].words[7] == 0x30000000);
    CHECK(press(keys::encoder_click + 3) && copies[2][0].words[7] == 0x50000000);
    // A click on a parameter without steps is taken and changes nothing.
    const u32 before = sets;
    CHECK(press(keys::encoder_click + 1) && sets == before);
    // Other keys stay stock's (M1 leaves the page).
    CHECK(!press(keys::m1));
    // It paints the copy.
    multi_lfo::hooks.draw(canvas);
    CHECK(visible[0] && visible[8] && !std::strcmp(texts[1], "LFO 2") && !std::strcmp(texts[2], "random"));
    CHECK(!std::strcmp(texts[3], "on") && !std::strcmp(texts[7], "3/4"));
    // LFO 3 is a copy of its own; going round reaches LFO 1 again.
    CHECK(turn(0, 1, true) && !std::strcmp(popup, "LFO 3") && copies[2][1].type == lfo_types::value);
    CHECK(turn(0, 1, true) && turn(0, 1, true) && !std::strcmp(popup, "LFO 1"));
    CHECK(!turn(0, 1));
    CHECK(turn(0, -1, true) && !std::strcmp(popup, "LFO 4"));
    // A frame rereads the copy (a project load may have replaced it).
    copies[2][2].type = lfo_types::duck;
    send(Layer::frame);
    multi_lfo::hooks.draw(canvas);
    CHECK(!std::strcmp(texts[2], "duck"));
    // Leaving the page and coming back starts at LFO 1, on the new track.
    send(Layer::close);
    send(Layer::open, 0, false, 0, false, 5);
    CHECK(!turn(0, 1));
    CHECK(turn(0, 1, true) && copies[5][0].type == lfo_types::value);
    return 0;
}
