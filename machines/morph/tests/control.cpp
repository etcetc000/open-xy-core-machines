// Morph's control against a stand-in for the SDK's snapshot store.
#include "../control.cpp"
#include <cassert>
using namespace opxy;
using namespace morph;

static snapshots::Snapshot store[snapshots::slots];
static u32 engines_now[control::tracks];
extern "C" const void* sdk_snapshot(u32 slot) { return slot < snapshots::slots && store[slot].taken ? &store[slot] : nullptr; }
extern "C" u32 sdk_snapshot_engine(u32 track) { return engines_now[track]; }
extern "C" u32 sdk_snapshot_generation() { return 0; }

static control::Input input = {};
static control::Overlay tick(ControlState& state) {
    control::Overlay result = {};
    morph::control(state, input, result);
    return result;
}
// Enum controls store the middle of their choice: live, then slots 1-9, 0.
static i32 pick(u32 c) { return static_cast<i32>((2ull * c + 1) * 0x80000000ull / 22); }
static i32 snap(u32 slot) { return pick(slot + 1); }
static const control::Entry* find(const control::Overlay& o, u32 track, u32 index) {
    for (u32 i = 0; i < o.count; ++i)
        if (o.entries[i].track == track && o.entries[i].index == index) return &o.entries[i];
    return nullptr;
}

int main() {
    ControlState state = {};
    control_reset(state);
    input.track = 11;  // aux track 12
    for (u32 t = 0; t < control::tracks; ++t) engines_now[t] = 40 + t;
    // What the tracks play now: unlike either snapshot.
    for (u32 t = 0; t < control::tracks; ++t)
        for (u32 p = 0; p < control::track_parameters; ++p) input.values[t][p] = 0x55555555;
    // Snapshots 2 and 5: track 0's engine control 0 goes 0 -> 0x40000000, track 3's
    // stock parameter 20 goes 1000 -> 3000; everything else matches.
    const u32 pair[2] = {2, 5};
    for (u32 s : pair) {
        store[s].taken = 1;
        for (u32 t = 0; t < control::tracks; ++t) {
            store[s].engines[t] = 40 + t;
            for (u32 p = 0; p < control::track_parameters; ++p) store[s].values[t][p] = static_cast<i32>(t * 100 + p);
        }
    }
    store[2].values[0][0] = 0;
    store[5].values[0][0] = 0x40000000;
    store[2].values[3][20] = 1000;
    store[5].values[3][20] = 3000;

    // A missing snapshot overlays nothing and says which.
    input.values[11][0] = snap(0);
    input.values[11][1] = snap(5);
    control::Overlay o = tick(state);
    assert(o.count == 0 && (o.display[3] & status_no_a) && !(o.display[3] & status_no_b));
    assert((o.display[0] & 15) == 1 && (o.display[0] >> 4 & 15) == 6 && (o.display[0] >> 8 & 0x3ff) == 0x24);

    // A and B present: only what differs is overlaid, engines first.
    input.values[11][0] = snap(2);
    input.values[11][2] = 0;  // morph at A
    o = tick(state);
    assert(o.count == 2 && o.display[3] == 0);
    assert(o.entries[0].track == 0 && o.entries[0].index == 0 && o.entries[0].value == 0);
    assert(find(o, 3, 20)->value == 1000);
    input.values[11][2] = 0x7fffffff;  // morph at B
    o = tick(state);
    assert(find(o, 0, 0)->value == 0x40000000 && find(o, 3, 20)->value == 3000);
    input.values[11][2] = 0x40000000;  // halfway
    o = tick(state);
    assert(find(o, 0, 0)->value == 0x20000000 && find(o, 3, 20)->value == 2000);
    assert(static_cast<u32>(o.display[2]) == one / 2);

    // A track whose engine changed since either snapshot is left alone.
    engines_now[3] = 99;
    o = tick(state);
    assert(o.count == 1 && !find(o, 3, 20) && (o.display[3] >> 8) == (1 << 3));
    engines_now[3] = 43;

    // Muted: nothing overlaid, the page still shows the slots.
    input.muted = 1;
    o = tick(state);
    assert(o.count == 0 && (o.display[3] & status_muted) && (o.display[0] >> 8 & 0x3ff) == 0x24);
    input.muted = 0;

    // Glide: half a second of a two-second glide covers a quarter of the range.
    input.values[11][3] = 0x7fffffff;  // glide 2 s
    input.values[11][2] = 0;
    input.elapsed_us = 500000;
    o = tick(state);  // from one half toward 0: about a quarter of the range
    const u32 quarter = static_cast<u32>(o.display[2]);
    assert(quarter >= one / 4 - 8 && quarter <= one / 4 + 8);
    o = tick(state);
    o = tick(state);
    assert(o.display[2] == 0);
    input.elapsed_us = 0;  // no time passed: it holds
    input.values[11][2] = 0x7fffffff;
    assert(tick(state).display[2] == 0);
    // A reset jumps straight to the control.
    control_reset(state);
    assert(static_cast<u32>(tick(state).display[2]) == one);

    // More differences than one overlay holds: engine controls win, the rest are counted.
    input.values[11][3] = 0;
    for (u32 t = 0; t < control::tracks; ++t)
        for (u32 p = 0; p < control::track_parameters; ++p) store[5].values[t][p] += 1;
    o = tick(state);
    assert(o.count == control::overlay_capacity);
    for (u32 i = 0; i < o.count; ++i) assert(o.entries[i].index < 8);
    assert((static_cast<u32>(o.display[1]) & 0xffff) == control::overlay_capacity);
    assert((static_cast<u32>(o.display[1]) >> 16) == control::tracks * control::track_parameters - control::overlay_capacity);

    // Live: A is what the tracks play now. At A nothing is overlaid, so knobs,
    // locks and LFOs play as they are; toward B, B's values take over.
    for (u32 t = 0; t < control::tracks; ++t)
        for (u32 p = 0; p < control::track_parameters; ++p) {
            store[5].values[t][p] -= 1;
            input.values[t][p] = store[5].values[t][p];  // the tracks play B, but for two
        }
    input.values[0][0] = 0;
    input.values[3][20] = 1000;
    input.values[11][0] = pick(0);
    input.values[11][1] = snap(5);
    input.values[11][2] = 0;
    control_reset(state);
    o = tick(state);
    assert(o.count == 0 && o.display[3] == 0 && (o.display[0] & 15) == 0);
    input.values[11][2] = 0x7fffffff;
    o = tick(state);
    assert(find(o, 0, 0)->value == 0x40000000 && find(o, 3, 20)->value == 3000);
    // A parameter B leaves as the track plays it is never touched.
    input.values[3][20] = 3000;
    o = tick(state);
    assert(!find(o, 3, 20));
    // Halfway from what plays now: a knob turn (or a lock) moves the result.
    input.values[11][2] = 0x40000000;
    control_reset(state);
    input.values[0][0] = 0x20000000;
    o = tick(state);
    assert(find(o, 0, 0)->value == 0x30000000);
    // Only B's engine matters with a live A; both live overlay nothing.
    engines_now[3] = 99;
    o = tick(state);
    assert((o.display[3] >> 8) == (1 << 3));
    engines_now[3] = 43;
    input.values[11][1] = pick(0);
    o = tick(state);
    assert(o.count == 0 && !(o.display[3] & (status_no_a | status_no_b)));
    // An empty snapshot side still says so.
    input.values[11][1] = snap(7);
    o = tick(state);
    assert(o.count == 0 && (o.display[3] & status_no_b));

    // Helpers.
    for (u32 c = 0; c < choices; ++c) assert(choice(pick(c)) == c);
    assert(choice(0x7fffffff) == choices - 1 && choice(-5) == live && fraction(0x7fffffff) == one && fraction(0) == 0);
    assert(mix(0, 200, one / 2) == 100 && mix(0x7fffffff, 0, one) == 0 && mix(0, 0x7fffffff, one) == 0x7fffffff);
    const i32 half = mix(0x7fffffff, 0, one / 2);
    assert(half >= 0x3ffffff0 && half <= 0x40000010 && mix(0, 0x7fffffff, one - 1) < 0x7fffffff);
    for (u32 x = 0; x <= one; x += 997) {
        const i32 up = mix(1000, 0x7ffff000, x), down = mix(0x7ffff000, 1000, x);
        assert(up >= 1000 && up <= 0x7ffff000 && down >= 1000 && down <= 0x7ffff000);
    }
    assert(glide_time(one) > 1990000 && glide_time(one) <= 2000000 && glide_step(3, 2) == one);
}
