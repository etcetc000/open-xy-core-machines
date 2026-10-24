#include "display.h"
#include <opxy/q31.hpp>
#include "orbit.hpp"

namespace fm2op {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;
const i32 expired = 8192;

// Round only the screen caption; the parameter keeps its full Q31 precision.
inline void amount(char* out, unsigned capacity, opxy::i32 value) {
    const unsigned p = value < 0 ? 0 : ((static_cast<unsigned>(value) >> 10) * 100 + (1u << 20)) >> 21;
    char text[4];
    unsigned n = 0;
    if (p >= 100) text[n++] = '1';
    if (p >= 10) text[n++] = static_cast<char>('0' + p / 10 % 10);
    text[n++] = static_cast<char>('0' + p % 10);
    text[n] = 0;
    opxy::draw::copy(out, capacity, text);
}


inline void clear_pulses(DisplayState& state) {
    for (u32 i = 0; i < 4; ++i) {
        state.ages[i] = expired;
        state.decays[i] = 0;
    }
    state.jitter_age = expired;
    state.next_slot = 0;
}

inline void outline(Frame::path_type& path, i32 feedback, u32 phase) {
    const i32 amplitude = (opxy::q31::multiply(feedback, feedback) / 2097152) * 6;
    const u32 time = (phase >> 8) & 63;
    const u32 key = amplitude ? 1 + static_cast<u32>(amplitude) * 256 + time : 1;
    // Frame storage persists between calls. Only changed quantized geometry is
    // computed; each backend independently caches what it actually applied.
    if (path.key == key) return;
    for (u32 i = 0; i < Frame::point_count; ++i) {
        const i32 fiber = floor_q10(594 * orbit_sine[(i * 25 + time) & 63]
            + 307 * orbit_sine[(i * 42 - time) & 63] + 123 * orbit_sine[(i * 12) & 63]);
        const i32 radius = 43 * unit + floor_q10(fiber * amplitude);
        path.points[i].x = floor_q10(radius * orbit_sine[(i + 16) & 63]);
        path.points[i].y = floor_q10(radius * orbit_sine[i]);
    }
    path.key = key;
}
} // namespace

inline void display_reset(DisplayState& state) {
    state.phase = state.previous_event = state.track = state.next_slot = 0;
    state.seed = state.initialized = 0;
    clear_pulses(state);
}

// Only screen work: no audio reads, device calls, allocation or wall-clock API.
// Input is one frame snapshot. Multiple note starts between snapshots coalesce;
// a fresh event immediately replaces the oldest pulse, never waits for it.
inline void display(DisplayState& state, const opxy::draw::Input& input, Frame& frame) {
    using namespace opxy::draw;
    for (u32 i = 1; i < 4; ++i)
        amount(frame.header[i], sizeof(frame.header[i]), input.parameters[i]);
    const i32 ratio = clamp(input.parameters[0], 0, opxy::q31::maximum);
    const i32 amount = clamp(input.parameters[1], 0, opxy::q31::maximum);
    const i32 feedback = clamp(input.parameters[2], 0, opxy::q31::maximum);
    const i32 decay = clamp(input.parameters[3], 0, opxy::q31::maximum);
    const i32 elapsed = clamp(input.elapsed, 0, expired);
    const bool snapshot = !state.initialized || state.track != input.track;
    if (snapshot || input.reduced_motion) {
        clear_pulses(state);
    } else {
        for (u32 i = 0; i < 4; ++i) state.ages[i] = clamp(state.ages[i] + elapsed, 0, expired);
        state.jitter_age = clamp(state.jitter_age + elapsed, 0, expired);
        if (input.track < 8 && input.note_serial != state.previous_event) {
            const u32 slot = state.next_slot & 3;
            state.ages[slot] = 0;
            state.decays[slot] = decay;
            state.next_slot = (slot + 1) & 3;
            state.jitter_age = 0;
            ++state.seed;
        }
    }
    state.track = input.track;
    state.previous_event = input.note_serial;
    state.initialized = 1;
    if (!input.reduced_motion)
        state.phase = (state.phase + static_cast<u32>(clamp(elapsed, 0, 102) * (3 + (ratio >> 29)))) & 65535;
    const u32 phase = input.reduced_motion ? 0 : state.phase;
    const u32 index = phase >> 10;
    const i32 dx = 29 + floor_q10(orbit_sine[(index + 16) & 63] * 7);
    const i32 dy = floor_q10(orbit_sine[index] * 7);
    i32 jitter_x = 0, jitter_y = 0;
    if (!input.reduced_motion && state.jitter_age < 96) {
        const i32 amplitude = ((3 - (state.jitter_age >> 5)) * (1 + (amount >> 29))) >> 2;
        const u32 bits = static_cast<u32>(state.jitter_age >> 3) + state.seed;
        jitter_x = bits & 1 ? -amplitude : amplitude;
        jitter_y = bits & 2 ? -amplitude : amplitude;
    }
    i32 scale = unit + dy * 2;
    if (!input.reduced_motion && state.jitter_age < 64) scale += (64 - state.jitter_age) * 2;
    const i32 centers[4] = { (240 - dx + jitter_x) * unit, (120 - dy + jitter_y) * unit,
                            (240 + dx - jitter_x) * unit, (120 + dy - jitter_y) * unit };
    const u32 main_shade = 5 + static_cast<u32>(((amount >> 21) * 11) >> 10);
    for (u32 side = 0; side < 2; ++side) {
        // An offset between outlines keeps the fray from looking duplicated.
        outline(frame.paths[side], feedback, (phase + side * 8192) & 65535);
        place(frame, side, centers[side * 2], centers[side * 2 + 1], scale, main_shade, stroke);
        place(frame, 26 + side, centers[side * 2], centers[side * 2 + 1], scale, 0, keep);
    }
    for (u32 layer = 0; layer < 4; ++layer) {
        const u32 slot = (state.next_slot + layer) & 3;
        const i32 progress = (state.ages[slot] * orbit_reciprocal[state.decays[slot] >> 25]) >> 12;
        for (u32 ring = 0; ring < 6; ++ring) {
            const u32 node = 2 + layer * 6 + ring;
            const u32 side = ring < 3 ? 0 : 1;
            const i32 band = ring < 3 ? static_cast<i32>(ring) : static_cast<i32>(ring - 3);
            const i32 travel = progress - 192 * band;
            if (input.reduced_motion || travel <= 0 || travel >= unit) { hide(frame, node); continue; }
            const i32 intensity = 256 + (((amount >> 21) * 768) >> 10);
            const u32 shade = static_cast<u32>(clamp(((unit - travel) * intensity) >> 16, 0, 15));
            place(frame, node, centers[side * 2], centers[side * 2 + 1],
                  orbit_expansion[travel >> 4], shade, fill_and_stroke);
        }
    }
}
} // namespace fm2op
