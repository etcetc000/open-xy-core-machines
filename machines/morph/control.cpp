#include <opxy/snapshots.hpp>
#include "controls.h"
// Morph blends between two sides, A and B, each one of the SDK's snapshots or
// "live", what the tracks play now (knob turns, locks and LFOs included), and
// overlays every instrument track parameter the two set differently with a point
// between them. A step's parameter lock always plays as written (the SDK keeps
// overlays off it). It never takes a snapshot and never edits a track: what it plays is
// an overlay, gone when it is muted, removed or its snapshots are missing.
namespace morph {
struct ControlState {
    opxy::u32 position;  // Q16, as heard: follows the morph control at the glide's rate
    opxy::u32 started;   // 0 until the first tick after a reset
};
inline void control_reset(ControlState& state) { state.position = 0; state.started = 0; }

namespace detail {
// One side of the morph: a snapshot, or the tracks as they play (null).
inline opxy::i32 side(const opxy::snapshots::Snapshot* s, const opxy::control::Input& input, opxy::u32 track,
                      opxy::u32 index) {
    return s ? s->values[track][index] : input.values[track][index];
}
// Adds every parameter of [first, last) the two sides set differently, on each
// usable track, at the point between them; counts what didn't fit. A value equal
// to what the track plays anyway is left alone, so a live side at its end
// overlays nothing and every knob, lock and LFO there plays as usual.
inline void add_range(const opxy::snapshots::Snapshot* a, const opxy::snapshots::Snapshot* b,
                      const opxy::control::Input& input, opxy::u32 x, opxy::u32 first, opxy::u32 last,
                      opxy::u32 usable, opxy::control::Overlay& overlay, opxy::u32& dropped) {
    for (opxy::u32 track = 0; track < opxy::control::tracks; ++track) {
        if (!(usable & (1u << track))) continue;
        for (opxy::u32 index = first; index < last; ++index) {
            const opxy::i32 from = side(a, input, track, index), to = side(b, input, track, index);
            if (from == to) continue;
            const opxy::i32 value = mix(from, to, x);
            if ((x == 0 || x >= one) && value == input.values[track][index]) continue;
            if (overlay.count >= opxy::control::overlay_capacity) { ++dropped; continue; }
            opxy::control::add(overlay, track, index, value);
        }
    }
}
}

inline void control(ControlState& state, const opxy::control::Input& input, opxy::control::Overlay& overlay) {
    using namespace opxy;
    const i32* own = input.values[input.track < control::all_tracks ? input.track : 0];
    const u32 a_choice = choice(own[0]), b_choice = choice(own[1]);
    const u32 target = fraction(own[2]);
    // Glide: the heard position moves toward the control at one full sweep per
    // glide time; no glide, or the first tick, jumps.
    const u32 glide_us = glide_time(fraction(own[3]));
    if (!state.started || !glide_us || !input.elapsed_us) {
        if (!state.started || !glide_us) state.position = target;
    } else {
        const u32 step = glide_step(input.elapsed_us, glide_us);
        if (state.position < target) state.position = target - state.position > step ? state.position + step : target;
        else state.position = state.position - target > step ? state.position - step : target;
    }
    state.started = 1;

    // A live side is no snapshot; a snapshot side may be empty.
    const snapshots::Snapshot* a = a_choice == live ? nullptr : snapshots::get(a_choice - 1);
    const snapshots::Snapshot* b = b_choice == live ? nullptr : snapshots::get(b_choice - 1);
    const bool has_a = a_choice == live || a, has_b = b_choice == live || b;
    u32 taken = 0;
    for (u32 s = 0; s < snapshots::slots; ++s) taken |= snapshots::get(s) ? 1u << s : 0;
    u32 status = (input.muted ? status_muted : 0) | (has_a ? 0 : status_no_a) | (has_b ? 0 : status_no_b);
    u32 usable = 0, changed = 0, dropped = 0;
    // A track is usable while it holds every snapshot side's engine.
    if (has_a && has_b)
        for (u32 t = 0; t < control::tracks; ++t) {
            // A track that holds no engine now and held none then is not changed, just empty.
            if (!snapshots::engine(t) && (!a || !a->engines[t]) && (!b || !b->engines[t])) continue;
            if ((!a || snapshots::matches(*a, t)) && (!b || snapshots::matches(*b, t))) usable |= 1u << t;
            else changed |= 1u << t;
        }
    if (has_a && has_b && (a || b) && !input.muted) {
        // The engines' own controls first, on every track, then stock's track
        // parameters: when more differ than one overlay holds, the sound engines win.
        detail::add_range(a, b, input, state.position, 0, 8, usable, overlay, dropped);
        detail::add_range(a, b, input, state.position, 8, control::track_parameters, usable, overlay, dropped);
    }
    // No marks: Morph stays on its own page and never paints over other tracks' pages.
    overlay.display[0] = static_cast<i32>(a_choice | b_choice << 4 | taken << 8);
    overlay.display[1] = static_cast<i32>(overlay.count | (dropped > 0xffff ? 0xffff : dropped) << 16);
    overlay.display[2] = static_cast<i32>(state.position);
    overlay.display[3] = static_cast<i32>(status | changed << 8);
}
}
