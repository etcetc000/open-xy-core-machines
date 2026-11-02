#include <opxy/extension.hpp>
#include <opxy/snapshots.hpp>
// Snapshots: a menu for the SDK's ten snapshots, laid out like stock's bar menu.
// Pressing - and + together (the claim, a layer) opens it: held, for as long as
// they are; tapped, until they are pressed again or any key the menu does not
// use. The black keys (1-9, 0) or encoder 1 pick a slot; M1-M4 save, load,
// clear and clear all. Beside the slots, the big line is the selected slot's
// state (empty, saved, or playing: the tracks' stored values match it, under
// any overlay) and under it what
// M1-M4 last did ("saved 5"), until the next of them or until the menu closes.
// The SDK keeps the
// snapshots with the project; Morph (or any machine that reads snapshots) plays
// them back.
namespace opxy { namespace extensions { namespace snapshots {
namespace {
namespace store = opxy::snapshots;

const u32 kSlots = store::slots, kNone = kSlots;
const u32 kM1 = 6;
// One step per this many raw encoder counts (a detent).
const i32 kKnobCounts = 4;
// What the menu last did since it opened.
enum Flash : u32 { kFlashNone, kFlashSaved, kFlashLoaded, kFlashCleared, kFlashClearedAll };

// The layer's nodes, in extension.json's order.
enum Node : u32 {
    kCover, kCard, kBox0, kDark0 = kBox0 + kSlots, kFaint0 = kDark0 + kSlots, kSel = kFaint0 + kSlots,
    kSelLabel, kDivider, kState, kTracks, kHere, kStrip, kM0, kNodes = kM0 + 4
};
// The slots' pitch in pixels, across and down (two rows of five).
const i32 kPitch = 40;

struct Menu {
    u32 selected;      // stays between openings
    i32 knob;          // encoder counts toward the next step
    u32 active;        // the slot last saved or loaded here, or kNone
    u32 generation;    // the snapshots' generation as this menu last left them
    u32 flash, flash_slot;
};
Menu menu = {0, 0, kNone, 0, kFlashNone, kNone};

// What the menu shows; drawn again only when it changes.
struct View {
    u32 selected, taken, current, flash, flash_slot;
    bool playing;  // the tracks' stored values match the selected slot
    bool operator==(const View& o) const {
        return selected == o.selected && taken == o.taken && current == o.current && flash == o.flash &&
               flash_slot == o.flash_slot && playing == o.playing;
    }
};
View shown;
bool fresh;

View view() {
    View v = {menu.selected, 0, kNone, menu.flash, menu.flash_slot, store::holds(menu.selected)};
    for (u32 i = 0; i < kSlots; ++i) v.taken |= store::get(i) ? 1u << i : 0;
    // The slot last saved or loaded first, so of two equal slots the one in use shows.
    if (menu.active < kSlots && store::holds(menu.active)) v.current = menu.active;
    for (u32 i = 0; v.current == kNone && i < kSlots; ++i)
        if (store::holds(i)) v.current = i;
    return v;
}
void flash(u32 what, u32 slot) {
    menu.flash = what;
    menu.flash_slot = slot;
}
void select(u32 slot) { menu.selected = slot; }

void act(u32 action, const Actions& actions) {
    // The snapshots changed elsewhere since the last frame: forget the slot in use first.
    if (store::generation() != menu.generation) menu.active = kNone;
    const u32 slot = menu.selected;
    bool done = false;
    if (action == 0 && (done = actions.take_snapshot(slot))) { menu.active = slot; flash(kFlashSaved, slot); }
    if (action == 1 && (done = actions.load_snapshot(slot))) { menu.active = slot; flash(kFlashLoaded, slot); }
    if (action == 2 && (done = actions.clear_snapshots(1u << slot) != 0)) {
        if (menu.active == slot) menu.active = kNone;
        flash(kFlashCleared, slot);
    }
    if (action == 3 && (done = actions.clear_snapshots((1u << kSlots) - 1) != 0)) {
        menu.active = kNone;
        flash(kFlashClearedAll, slot);
    }
    // A press that did nothing (an empty slot to load or clear) leaves no stale message.
    if (!done) menu.flash = kFlashNone;
    menu.generation = store::generation();
}

bool layer(const Context&, const Layer& event, const Actions& actions) {
    switch (event.kind) {
    case Layer::open:
        menu.knob = 0;
        menu.flash = kFlashNone;
        fresh = true;
        return false;
    case Layer::close:
        return false;
    case Layer::frame:
        // A project loaded or another hand changed the snapshots: nothing of
        // this menu's is in use any more.
        if (store::generation() != menu.generation) {
            menu.generation = store::generation();
            menu.active = kNone;
        }
        return false;
    case Layer::turn:
        if (event.key != 0) return false;
        // Raw counts: one step per detent, the remainder kept; turning back starts over.
        if ((event.delta > 0) != (menu.knob > 0) && menu.knob) menu.knob = 0;
        menu.knob += event.delta;
        while (menu.knob >= kKnobCounts || menu.knob <= -kKnobCounts) {
            const bool up = menu.knob > 0;
            menu.knob += up ? -kKnobCounts : kKnobCounts;
            select((menu.selected + (up ? 1 : kSlots - 1)) % kSlots);
        }
        return true;
    case Layer::keypress: {
        const i32 black = keys::black_index(event.key);
        if (black >= 0) {
            if (event.pressed) select(static_cast<u32>(black));
            return true;
        }
        if (event.key >= kM1 && event.key < kM1 + 4) {
            if (event.pressed) act(event.key - kM1, actions);
            return true;
        }
        return false;
    }
    }
    return false;
}

// What M1-M4 last did, under the state until the next of them or until the
// menu closes, whichever slot is picked: "saved 5", "loaded 5", "cleared 5" or
// "cleared all".
void detail(const View& v, char* line) {
    const char* what = v.flash == kFlashSaved ? "saved " :
                       v.flash == kFlashLoaded ? "loaded " : v.flash == kFlashCleared ? "cleared " :
                       v.flash == kFlashClearedAll ? "cleared all" : "";
    u32 n = 0;
    for (; what[n] && n < 14; ++n) line[n] = what[n];
    if (n && line[n - 1] == ' ' && v.flash_slot < kSlots) line[n++] = keys::black_label(v.flash_slot);
    line[n] = 0;
}

void draw(const Canvas& canvas) {
    const View v = view();
    if (!fresh && v == shown) return;
    fresh = false;
    shown = v;
    for (u32 i = 0; i < kNodes; ++i) canvas.hide(i);
    const u32 fixed[] = {kCover, kCard, kDivider, kState, kTracks};
    for (u32 node : fixed) canvas.show(node);
    for (u32 s = 0; s < kSlots; ++s) {
        canvas.show(kBox0 + s);
        canvas.show((v.taken >> s & 1 ? kDark0 : kFaint0) + s);
    }
    const i32 dx = static_cast<i32>(v.selected % 5) * kPitch, dy = static_cast<i32>(v.selected / 5) * kPitch;
    canvas.show(kSel, dx, dy);
    canvas.show(kSelLabel, dx, dy);
    const char label[2] = {keys::black_label(v.selected), 0};
    canvas.text(kSelLabel, label);
    if (v.current < kSlots)
        canvas.show(kHere, static_cast<i32>(v.current % 5) * kPitch, static_cast<i32>(v.current / 5) * kPitch);
    // The big line: the selected slot's state.
    canvas.text(kState, !(v.taken >> v.selected & 1) ? "empty" : v.playing ? "playing" : "saved");
    char line[16];
    detail(v, line);
    canvas.text(kTracks, line);
    canvas.show(kStrip);
    for (u32 m = 0; m < 4; ++m) canvas.show(kM0 + m);
}
}
extern constexpr Hooks hooks = Hooks().with_layer(layer).with_draw(draw);
}}}
