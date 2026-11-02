// The snapshot menu end to end: its layer events as the SDK runtime sends them,
// a fake snapshot store and action table, and a canvas that records what shows.
#include "extension.cpp"
#include <cstring>

using namespace opxy;
using namespace opxy::extensions;
using namespace opxy::extensions::snapshots;
namespace menu = opxy::extensions::snapshots;
#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)

namespace {
// The SDK's snapshot store, as the menu reads it.
store::Snapshot slots[store::slots];
u32 engines[control::tracks], generation_now, held_slot = 99;
u32 takes, loads, clears, cleared_mask;
bool take(u32 slot) {
    ++takes;
    slots[slot].taken = 1;
    for (u32 t = 0; t < control::tracks; ++t) slots[slot].engines[t] = engines[t];
    ++generation_now;
    return true;
}
bool load(u32 slot) { ++loads; return slots[slot].taken; }
u32 clear(u32 mask) {
    ++clears; cleared_mask = mask;
    for (u32 i = 0; i < store::slots; ++i) if (mask >> i & 1) slots[i].taken = 0;
    ++generation_now;
    return 1;
}
void notify(const char*) {}
const Actions actions = {nullptr, notify, nullptr, take, load, clear};

// What the canvas shows: each node's visibility and offset, and its text.
struct Shown { bool on; i32 dx, dy; char text[32]; };
Shown nodes[kNodes];
void show_node(void*, u32 node, bool on, i32 dx, i32 dy) {
    if (node < kNodes) { nodes[node].on = on; nodes[node].dx = dx; nodes[node].dy = dy; }
}
void text_node(void*, u32 node, const char* text) {
    if (node < kNodes) std::strncpy(nodes[node].text, text, 31);
}
const Canvas canvas = {nullptr, show_node, text_node};

bool send(Layer::Kind kind, u32 key = 0, bool pressed = true, i32 delta = 0, bool latched = false, u32 held = 0) {
    const Layer event = {kind, key, pressed, delta, 16, latched, held};
    return menu::hooks.layer(Context(), event, actions);
}
void frame(bool latched = false, u32 held = 0) {
    send(Layer::frame, 0, false, 0, latched, held);
    menu::hooks.draw(canvas);
}
}

extern "C" const void* sdk_snapshot(u32 slot) { return slot < store::slots && slots[slot].taken ? &slots[slot] : nullptr; }
extern "C" u32 sdk_snapshot_engine(u32 track) { return engines[track]; }
extern "C" u32 sdk_snapshot_generation() { return generation_now; }
extern "C" bool sdk_snapshot_holds(u32 slot) { return slot == held_slot && slots[slot].taken; }

// The big line, then what M1-M4 last did.
bool says(const char* big, const char* last) {
    return !std::strcmp(nodes[kState].text, big) && !std::strcmp(nodes[kTracks].text, last);
}

int main() {
    for (u32 t = 0; t < control::tracks; ++t) engines[t] = 40 + t;
    CHECK(menu::hooks.layer && menu::hooks.draw && !menu::hooks.queued_control);

    // Held open: the card, ten empty slots, the selection and the bottom row at
    // once; the big line is the selected slot's state.
    send(Layer::open);
    frame(false, 16);
    CHECK(nodes[kCover].on && nodes[kCard].on && nodes[kStrip].on && nodes[kM0].on && nodes[kM0 + 3].on);
    CHECK(nodes[kFaint0 + 3].on && !nodes[kDark0 + 3].on && nodes[kSel].on && nodes[kSel].dx == 0);
    CHECK(says("empty", ""));

    // Black keys pick a slot (1-9, 0) and are taken, releases too; other keys are stock's.
    CHECK(send(Layer::keypress, 36) && send(Layer::keypress, 36, false));
    frame(false, 100);
    CHECK(nodes[kSel].dx == 4 * 40 && nodes[kSel].dy == 0 && !std::strcmp(nodes[kSelLabel].text, "5"));
    CHECK(!send(Layer::keypress, 16) && !send(Layer::keypress, 50));
    // M1 into an empty slot: new; what you hear now matches it.
    held_slot = 4;
    CHECK(send(Layer::keypress, kM1) && takes == 1);
    frame(false, 200);
    CHECK(nodes[kDark0 + 4].on && !nodes[kFaint0 + 4].on && says("playing", "saved 5"));
    // M1 again: saved over.
    CHECK(send(Layer::keypress, kM1) && takes == 2);
    frame(false, 300);
    CHECK(says("playing", "saved 5"));
    // Another slot: its state; the message stays.
    CHECK(send(Layer::keypress, 39));
    frame(false, 400);
    CHECK(says("empty", "saved 5"));
    CHECK(send(Layer::keypress, 36));
    held_slot = 99;
    frame(false, 500);
    CHECK(says("saved", "saved 5"));

    // Encoder 1 steps one slot per detent (four counts), wrapping; turning back
    // starts over; other encoders are stock's.
    CHECK(send(Layer::turn, 0, false, 3) && send(Layer::turn, 0, false, 1));
    frame(true);
    CHECK(nodes[kSel].dx == 0 && nodes[kSel].dy == 40 && !std::strcmp(nodes[kSelLabel].text, "6"));
    CHECK(send(Layer::turn, 0, false, 2) && send(Layer::turn, 0, false, -4));
    frame(true);
    CHECK(!std::strcmp(nodes[kSelLabel].text, "5"));
    CHECK(!send(Layer::turn, 1, false, 4));

    // The slot the tracks hold is underlined.
    held_slot = 4;
    frame(true);
    CHECK(nodes[kHere].on && nodes[kHere].dx == 4 * 40);
    held_slot = 99;
    frame(true);
    CHECK(!nodes[kHere].on);

    // Load, clear and clear all, with what each did.
    CHECK(send(Layer::keypress, kM1 + 1) && loads == 1);
    held_slot = 4;
    frame(true);
    CHECK(says("playing", "loaded 5"));
    CHECK(send(Layer::keypress, kM1 + 2) && clears == 1 && cleared_mask == 1u << 4);
    frame(true);
    CHECK(says("empty", "cleared 5") && nodes[kFaint0 + 4].on);
    CHECK(send(Layer::keypress, kM1 + 3) && clears == 2 && cleared_mask == 0x3ff);
    frame(true);
    CHECK(says("empty", "cleared all"));

    // Closing forgets the message.
    send(Layer::close);
    send(Layer::open);
    frame(true);
    CHECK(says("empty", ""));
    return 0;
}
