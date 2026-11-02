#include <opxy/extension.hpp>
#include <opxy/snapshots.hpp>
// Snapshots: a menu for the SDK's ten snapshots, laid out like stock's bar menu.
// Pressing - and + together (the claim, a layer) opens it: held, for as long as
// they are; tapped, until they are pressed again or any key the menu does not
// use. The black keys (1-9, 0) or encoder 1 pick a slot; once the bottom row
// shows (at once when tapped open, after a moment when held), M1-M4 save, load,
// clear and clear all. The SDK keeps the snapshots with the project; Morph (or
// any machine that reads snapshots) plays them back.
namespace opxy { namespace extensions { namespace snapshots {
namespace {
namespace store = opxy::snapshots;

const u32 kSlots = store::slots, kNone = kSlots;
const u32 kM1 = 6;
// One step per this many raw encoder counts (a detent).
const i32 kKnobCounts = 4;
// What the menu last did, shown this long in place of the state.
const u32 kFlashMs = 1200;
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
    bool strip;        // the bottom row shows
    u32 active;        // the slot last saved or loaded here, or kNone
    u32 generation;    // the snapshots' generation as this menu last left them
    u32 flash, flash_slot, flash_ms;
};
Menu menu = {0, 0, false, kNone, 0, kFlashNone, kNone, 0};

// What the menu shows; drawn again only when it changes.
struct View {
    bool strip;
    u32 selected, taken, fits, current, active, flash, flash_slot;
    bool operator==(const View& o) const {
        return strip == o.strip && selected == o.selected && taken == o.taken && fits == o.fits &&
               current == o.current && active == o.active && flash == o.flash && flash_slot == o.flash_slot;
    }
};
View shown;
bool fresh;

// How many tracks still hold the engine they held when `slot` was taken.
u32 fits(u32 slot) {
    const store::Snapshot* s = store::get(slot);
    u32 count = 0;
    for (u32 t = 0; s && t < control::tracks; ++t) count += store::matches(*s, t);
    return count;
}
View view() {
    View v = {menu.strip, menu.selected, 0, fits(menu.selected), kNone, menu.active,
              menu.flash_ms ? menu.flash : kFlashNone, menu.flash_slot};
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
    menu.flash_ms = kFlashMs;
}

void act(u32 action, const Actions& actions) {
    const u32 slot = menu.selected;
    if (action == 0 && actions.take_snapshot(slot)) { menu.active = slot; flash(kFlashSaved, slot); }
    if (action == 1 && actions.load_snapshot(slot)) { menu.active = slot; flash(kFlashLoaded, slot); }
    if (action == 2 && actions.clear_snapshots(1u << slot)) {
        if (menu.active == slot) menu.active = kNone;
        flash(kFlashCleared, slot);
    }
    if (action == 3) {
        actions.clear_snapshots((1u << kSlots) - 1);
        menu.active = kNone;
        flash(kFlashClearedAll, kNone);
    }
    menu.generation = store::generation();
}

bool layer(const Context&, const Layer& event, const Actions& actions) {
    switch (event.kind) {
    case Layer::open:
        menu.knob = 0;
        menu.flash_ms = 0;
        menu.strip = false;
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
        menu.flash_ms = menu.flash_ms > event.elapsed_ms ? menu.flash_ms - event.elapsed_ms : 0;
        menu.strip = event.latched || event.held_ms >= tap_ms;
        return false;
    case Layer::turn:
        if (event.key != 0) return false;
        // Raw counts: one step per detent, the remainder kept; turning back starts over.
        if ((event.delta > 0) != (menu.knob > 0) && menu.knob) menu.knob = 0;
        menu.knob += event.delta;
        while (menu.knob >= kKnobCounts || menu.knob <= -kKnobCounts) {
            const bool up = menu.knob > 0;
            menu.knob += up ? -kKnobCounts : kKnobCounts;
            menu.selected = (menu.selected + (up ? 1 : kSlots - 1)) % kSlots;
        }
        return true;
    case Layer::keypress: {
        const i32 black = keys::black_index(event.key);
        if (black >= 0) {
            if (event.pressed) menu.selected = static_cast<u32>(black);
            return true;
        }
        if (event.key >= kM1 && event.key < kM1 + 4) {
            // The bottom row acts once it shows, as on the bar menu.
            if (event.pressed && menu.strip) act(event.key - kM1, actions);
            return true;
        }
        return false;
    }
    }
    return false;
}

// "3 active", "saved 3": short words, as the column is about 90 pixels at 18 px.
void state_line(const View& v, char* line) {
    const char* what = nullptr;
    u32 slot = kNone, n = 0;
    if (v.flash == kFlashSaved) { what = "saved "; slot = v.flash_slot; }
    else if (v.flash == kFlashLoaded) { what = "loaded "; slot = v.flash_slot; }
    else if (v.flash == kFlashCleared) { what = "cleared "; slot = v.flash_slot; }
    else if (v.flash == kFlashClearedAll) what = "all clear";
    else if (v.current < kSlots) { line[n++] = keys::black_label(v.current); what = " active"; }
    else if (v.active < kSlots) { line[n++] = keys::black_label(v.active); what = " edited"; }
    else what = "unsaved";
    for (const char* c = what; *c && n < 10; ++c) line[n++] = *c;
    if (slot < kSlots) line[n++] = keys::black_label(slot);
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
    char line[12];
    state_line(v, line);
    canvas.text(kState, line);
    // The selected slot: empty, saved, or how many tracks a load would restore.
    const char partly[9] = {'f', 'i', 't', 's', ' ', static_cast<char>('0' + v.fits), '/', '8', 0};
    canvas.text(kTracks, !(v.taken >> v.selected & 1) ? "empty" : v.fits == control::tracks ? "saved" : partly);
    if (v.strip) {
        canvas.show(kStrip);
        for (u32 m = 0; m < 4; ++m) canvas.show(kM0 + m);
    }
}
}
extern constexpr Hooks hooks = Hooks().with_layer(layer).with_draw(draw);
}}}
