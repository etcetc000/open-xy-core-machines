// SPDX-License-Identifier: AGPL-3.0-only
// Modified from Capicola; upstream revision/license in vendor/README.md.
// Sparse keyframes are prepared off the audio thread in analysis.h.
#pragma once
#include "analysis.h"
#include "pitch.h"

namespace timestretch { namespace stretch {
using namespace timestretch::sample_analysis;
constexpr u32 kOne = 16384;
// The drift budget: a splice is due when the grain is more than this many
// source frames off the tempo clock (40 ms at 44.1 kHz). The block
// loops test it when a playhead crosses a keyframe. Its crossfade spans as
// many frames.
constexpr u32 kLeashFrames = 1764;
inline u32 smooth(u32 p) {
    const u32 square = (p * p) >> 14;
    return (square * (3 * kOne - 2 * p)) >> 14;
}
inline i32 mix(i32 a, i32 b, u32 p) {
    return a + ((b - a) * static_cast<i32>(p) >> 14);
}
struct Key { u32 position; u32 ordinal; };
struct Source {
    opxy::SampleView sample;
    u32 begin;
    u32 frames;
    u32 shift;
    u32 loop;
    bool reverse;
    const Analysis* analysis;
    u32 base_rank;
    u32 key_count;
    // PCM addressing, fixed for the block: frame n of the region (in playback
    // order) starts at origin + n * stride.
    const opxy::i16* origin;
    i32 stride;
    u32 fraction_right, fraction_left;

    void bind_pcm() {
        const u32 channels = sample.channels;
        origin = sample.data + (reverse ? begin + frames - 1 : begin) * channels;
        stride = reverse ? -static_cast<i32>(channels) : static_cast<i32>(channels);
        fraction_right = shift >= 14 ? shift - 14 : 0;
        fraction_left = shift >= 14 ? 0 : 14 - shift;
    }
    void configure() {
        base_rank = rank(*analysis, begin + 1);
        key_count = rank(*analysis, begin + frames) - base_rank + 1;
    }
    u32 key_position(u32 anchor) const {
        return (reverse ? begin + frames - 1 - anchor : anchor - begin) << shift;
    }
    Key point(u32 anchor) const {
        const u32 forward = rank(*analysis, anchor) + 1 - base_rank;
        return Key{key_position(anchor), reverse ? key_count - forward : forward};
    }
};
// Only grain timing follows the sparse keyframe grid. Audio comes directly
// from PCM, so the cursor carries neither amplitudes nor interpolation state.
struct Cursor { u32 left, width, index, lap, next_anchor; };
struct Reference { u32 position; Cursor window; u32 lap; };
static inline Cursor cursor(const Source& s, u32 position, u32 lap) {
    const u32 frame = position >> s.shift;
    u32 left = 0, right = s.loop, ordinal = 0, next_anchor = 0;
    if (s.reverse) {
        const u32 physical = s.begin + s.frames - 1 - frame;
        u32 before = next_key(*s.analysis, physical ? physical - 1 : 0);
        if (before > s.begin && before < s.begin + s.frames) {
            Key key = s.point(before);
            if (key.position > position) {
                before = next_key(*s.analysis, before);
                if (before < s.begin + s.frames) key = s.point(before);
            }
            if (before < s.begin + s.frames) {
                left = key.position; ordinal = key.ordinal;
            }
        }
        const u32 after = previous_key(*s.analysis, before < s.begin + s.frames ? before - 1 : s.begin + s.frames - 1);
        if (after > s.begin) {
            const Key key = s.point(after);
            right = key.position; next_anchor = after;
        }
    } else {
        const u32 physical = s.begin + frame;
        u32 before = previous_key(*s.analysis, physical + 1 < s.begin + s.frames ? physical + 1 : physical);
        if (before > s.begin) {
            Key key = s.point(before);
            if (key.position > position) {
                before = previous_key(*s.analysis, before - 1);
                if (before > s.begin) key = s.point(before);
            }
            if (before > s.begin) {
                left = key.position; ordinal = key.ordinal;
            }
        }
        const u32 after = next_key(*s.analysis, before > s.begin ? before : s.begin);
        if (after < s.begin + s.frames) {
            const Key key = s.point(after);
            right = key.position; next_anchor = after;
        }
    }
    return Cursor{left, right - left, lap * s.key_count + ordinal, lap, next_anchor};
}
static inline void step_reference(Reference& h, const Source& s) {
    Cursor& c = h.window;
    if (c.lap == h.lap && h.position >= c.left) {
        if (h.position - c.left <= c.width) return;
        if (c.next_anchor) {
            const u32 anchor = s.reverse ? previous_key(*s.analysis, c.next_anchor - 1) :
                                         next_key(*s.analysis, c.next_anchor);
            u32 right = s.loop, next_anchor = 0;
            if (anchor > s.begin && anchor < s.begin + s.frames) {
                right = s.key_position(anchor);
                next_anchor = anchor;
            }
            if (h.position <= right) {
                const u32 left = c.left + c.width;
                c = Cursor{left, right - left, c.index + 1, h.lap, next_anchor};
                return;
            }
        }
    }
    c = cursor(s, h.position, h.lap);
}
inline void advance(Reference& h, const Source& s, u32 step) {
    h.position += step;
    if (h.position >= s.loop) { h.position -= s.loop; ++h.lap; }
}
// Interpolate the original stereo frame once, sharing positions and weights.
// The caller has validated the sample and kept the position inside the trim.
// Stereo is the sample's layout, fixed for a voice: callers that know it per
// block use this form, so the per-sample loop carries no channel test.
template <bool Stereo>
static inline void read_frame(const Source& s, u32 position, i32& left, i32& right) {
    const u32 whole = position >> s.shift;
    const u32 next = whole + 1 < s.frames ? whole + 1 : 0;
    const u32 part = position & ((1u << s.shift) - 1);
    const i32 fraction = static_cast<i32>((part >> s.fraction_right) << s.fraction_left);
    const opxy::i16* at = s.origin + static_cast<i32>(whole) * s.stride;
    const opxy::i16* after = s.origin + static_cast<i32>(next) * s.stride;
    const i32 a = at[0], b = after[0];
    left = a + ((b - a) * fraction >> 14);
    if (Stereo) {
        const i32 c = at[1], d = after[1];
        right = c + ((d - c) * fraction >> 14);
    } else right = left;
}
static inline void read_pcm(const Source& s, u32 position, i32& left, i32& right) {
    if (s.sample.channels == 2) read_frame<true>(s, position, left, right);
    else read_frame<false>(s, position, left, right);
}
struct Granule {
    Reference ideal;
    Reference actual;
    Reference temp;
    u32 fade;
    u32 fade_step;
    u32 fade_increment;  // fade_step at the current tuning
    bool splicing;
};
static inline void spawn(Granule& g, const Source& s, u32 position) {
    g.ideal = Reference{position, cursor(s, position, 0), 0};
    g.actual = g.ideal;
    g.temp = g.actual;
    g.fade = 0;
    g.fade_step = 0;
    g.fade_increment = 0;
    g.splicing = false;
}
// A grain splice begins: the incoming grain at the ideal clock, its fade.
static void splice_begin(Granule& g, const Source& s, u32 time_step, u32 pitch_step, u32 tuning) {
    g.temp = g.ideal;
    // Immutable samples have no moving writer fence/runway to clamp.
    const u32 duration = kLeashFrames;
    const u32 rate = s.sample.rate >= 8000 && s.sample.rate <= 192000 ? s.sample.rate : 44100;
    const u32 outputs = duration * 44100u / rate;
    g.fade_step = 0x40000000u / (outputs ? outputs : 1);
    // Seat an incoming grain behind the clock by 75% of its predicted
    // forward drift during the fade. Leave the ideal clock unchanged.
    g.fade_increment = scale_pitch(g.fade_step, tuning);
    if (pitch_step > time_step && g.fade_increment) {
        const u32 fade_ticks = (0x40000000u - 1u) / g.fade_increment + 1u;
        const u32 ticks = (fade_ticks / 4u) * 3u + ((fade_ticks % 4u) * 3u) / 4u;
        const u32 drift = pitch_step - time_step;
        // Bound before multiplying; at most a quarter-loop rewind.
        const u32 limit = s.loop / 4u;
        u32 offset = ticks && drift > limit / ticks ? limit : drift * ticks;
        // Never read a previous lap before playback has reached one.
        if (!g.temp.lap && offset > g.temp.position) offset = g.temp.position;
        if (offset > g.temp.position) {
            g.temp.position = s.loop - (offset - g.temp.position);
            --g.temp.lap;
        } else g.temp.position -= offset;
        g.temp.window = cursor(s, g.temp.position, g.temp.lap);
    }
    // Centre the drift on the clock: the grain drifts one way until the next
    // splice, so seat it half a fade (the drift budget) on the other side.
    // Its timing then swings around the beat instead of always one way.
    if (pitch_step != time_step) {
        const u32 half = duration / 2u;
        const u32 limit = s.loop / 4u;
        u32 offset = half > (limit >> s.shift) ? limit : half << s.shift;
        if (pitch_step > time_step) {
            if (!g.temp.lap && offset > g.temp.position) offset = g.temp.position;
            if (offset > g.temp.position) {
                g.temp.position = s.loop - (offset - g.temp.position);
                --g.temp.lap;
            } else g.temp.position -= offset;
        } else {
            const u32 room = s.loop - g.temp.position;
            if (offset >= room) {
                g.temp.position = offset - room;
                ++g.temp.lap;
            } else g.temp.position += offset;
        }
        g.temp.window = cursor(s, g.temp.position, g.temp.lap);
    }
    g.fade = 0;
    g.splicing = true;
}
// Whether a splice begins: the actual grain leads or trails the ideal clock
// by more than the drift budget, in source frames (laps unwrapped).
static inline bool splice_due(const Granule& g, const Source& s) {
    const long long lap = static_cast<long long>(static_cast<i32>(g.actual.lap - g.ideal.lap));
    const long long delta = lap * s.loop + static_cast<long long>(g.actual.position) - static_cast<long long>(g.ideal.position);
    const unsigned long long lead = static_cast<unsigned long long>(delta < 0 ? -delta : delta) >> s.shift;
    return !g.splicing && lead > kLeashFrames;
}
static inline void render(Granule& g, const Source& s, u32 time_step, u32 pitch_step,
                          u32 tuning, i32& left, i32& right) {
    step_reference(g.ideal, s);
    step_reference(g.actual, s);
    if (splice_due(g, s)) splice_begin(g, s, time_step, pitch_step, tuning);
    advance(g.ideal, s, time_step);
    read_pcm(s, g.actual.position, left, right);
    advance(g.actual, s, pitch_step);
    if (g.splicing) {
        i32 next_left, next_right;
        read_pcm(s, g.temp.position, next_left, next_right);
        const u32 weight = smooth(g.fade >> 16);
        left = mix(left, next_left, weight);
        right = mix(right, next_right, weight);
        advance(g.temp, s, pitch_step);
        // Upstream measures fades in source samples and multiplies their
        // phase increment by |pitch|. Keep live tuning during an active fade.
        g.fade += g.fade_increment;
        if (g.fade >= 0x40000000u) {
            g.actual = g.temp;
            g.splicing = false;
        }
    }
}
} }
