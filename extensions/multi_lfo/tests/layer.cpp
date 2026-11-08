// The LFO page layer end to end: its callback driven as the SDK runtime calls
// it, with fake actions standing in for the SDK's four LFOs.
#include "extension.cpp"
#include <cstring>

using namespace opxy;
using namespace opxy::extensions;
#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)

namespace {
// The LFO each track's page shows, as the SDK keeps it.
u32 shown[8];
u32 shows, popups;
char popup[32];
u32 insert(const Context&, u32, const Note*, u32) { return 0; }
u32 replace(const Context&, u32, const Note*, u32) { return 0; }
void notify(const char* text) { ++popups; std::strcpy(popup, text); }
bool show_lfo(u32 track, u32 lfo) {
    if (track >= 8 || lfo >= lfo_count) return false;
    shown[track] = lfo; ++shows; return true;
}
u32 shown_lfo(u32 track) { return track < 8 ? shown[track] : 0; }
const Actions actions = {insert, notify, replace, show_lfo, shown_lfo};
const Context context = {};

bool send(Layer::Kind kind, u32 key = 0, bool pressed = false, i32 delta = 0, bool shift = false, u32 track = 2) {
    const Layer event = {kind, key, pressed, delta, shift, track};
    return multi_lfo::hooks.layer(context, event, actions);
}
bool turn(u32 encoder, i32 delta, bool shift = false, u32 track = 2) {
    return send(Layer::turn, encoder, false, delta, shift, track);
}
bool press(u32 key, bool shift = false) { return send(Layer::keypress, key, true, 0, shift); }
}

int main() {
    send(Layer::open);
    // Stock keeps its page: every encoder, click and key, with or without Shift.
    CHECK(!turn(0, 1) && !turn(1, 1, true) && !turn(3, -2, true));
    CHECK(!press(keys::m4) && !press(keys::m4, true) && !press(keys::encoder_click) && !press(keys::m1));
    CHECK(shows == 0 && popups == 0);
    // Shift + encoder 1 shows the next LFO, with a popup naming it.
    CHECK(turn(0, 1, true) && shown[2] == 1 && popups == 1 && !std::strcmp(popup, "LFO 2"));
    CHECK(turn(0, 3, true) && shown[2] == 2 && !std::strcmp(popup, "LFO 3"));
    // Round: after LFO 4 comes LFO 1, and left from LFO 1 is LFO 4.
    CHECK(turn(0, 1, true) && turn(0, 1, true) && shown[2] == 0 && !std::strcmp(popup, "LFO 1"));
    CHECK(turn(0, -1, true) && shown[2] == 3 && !std::strcmp(popup, "LFO 4"));
    // Each track keeps its own.
    CHECK(turn(0, 1, true, 5) && shown[5] == 1 && shown[2] == 3);
    // Closing and opening the page changes nothing; the SDK keeps what is shown.
    send(Layer::close);
    send(Layer::open);
    CHECK(turn(0, 1, true) && shown[2] == 0 && popups == 7);
    return 0;
}
