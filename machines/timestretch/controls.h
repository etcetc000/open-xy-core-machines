// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <opxy/sample_engine.hpp>
#include "assignment.h"

namespace timestretch {
// Pad settings (opxy::PadSettings): the SDK owns serialization and stock layouts.
struct Controls { bool once; bool stretch; opxy::u32 bpm_x10; opxy::u32 length; };
// Stock's live controls are bipolar around Q31 half-scale. The saved pad
// settings remain the base; the middle third of a mode control inherits it.
inline opxy::u32 live_parameter(const opxy::i32* live, unsigned index) {
    const opxy::u32 neutral = index == 7 ? 0x7fffffffu : 0x40000000u;
    return !live ? neutral : live[index] < 0 ? 0u : opxy::u32(live[index]);
}
inline int live_offset(opxy::u32 value, int span) {
    const int scaled = (int(value >> 15) - 32768) * span;
    return scaled >= 0 ? (scaled + 16384) / 32768 : -((-scaled + 16384) / 32768);
}
inline bool live_mode(opxy::u32 value, bool saved) {
    return value < 0x2aaaaaaau ? false : value >= 0x55555555u ? true : saved;
}
struct EffectiveControls { Controls pad; int pitch_cents; };
inline Controls decode_controls(opxy::PadSettings value) {
    // Two tag bits, two flags and twelve tempo bits. An untagged word is a pad
    // without saved settings: loop, repitch, one bar.
    if ((value & 3u) != 1u) return Controls{false, false, 0, 2};
    const opxy::u32 tempo = value >> 4;
    return Controls{(value & 4) != 0, (value & 8) != 0,
                    tempo >= 6 && tempo <= 2806 ? tempo + 194 : 0, tempo < 6 ? tempo : 2};
}
inline opxy::PadSettings encode_controls(const Controls& c) {
    return static_cast<opxy::PadSettings>(1u | (c.once ? 4u : 0) | (c.stretch ? 8u : 0) |
                                          ((c.bpm_x10 ? c.bpm_x10 - 194u : c.length) << 4));
}
inline EffectiveControls effective_controls(opxy::PadSettings value, const opxy::i32* live) {
    Controls c = decode_controls(value);
    const int cents = live_offset(live_parameter(live, 0), 1200);
    c.once = live_mode(live_parameter(live, 3), c.once);
    c.stretch = live_mode(live_parameter(live, 6), c.stretch);
    if (c.bpm_x10) {
        const int bpm = int(c.bpm_x10) + live_offset(live_parameter(live, 5), 1400);
        c.bpm_x10 = bpm < 200 ? 200 : bpm > 3000 ? 3000 : opxy::u32(bpm);
    } else {
        const int length = int(c.length) + live_offset(live_parameter(live, 5), 5);
        c.length = length < 0 ? 0 : length > 5 ? 5 : opxy::u32(length);
    }
    return EffectiveControls{c, cents};
}
// Unsigned Q31 multiplication, including exact unity at the parameter's
// largest representable value. All partial products fit in a 32-bit word.
inline opxy::u32 live_scale(opxy::u32 value, opxy::u32 fraction) {
    if (fraction == 0x7fffffffu) return value;
    const opxy::u32 a = value & 65535u, b = value >> 16;
    const opxy::u32 c = fraction & 65535u, d = fraction >> 16;
    const opxy::u32 low = a * c;
    const opxy::u32 carry = b * c + (low >> 16);
    const opxy::u32 middle = (carry & 65535u) + a * d;
    const opxy::u32 high = b * d + (carry >> 16) + (middle >> 16);
    return (high << 1) | (middle >> 15 & 1u);
}
inline opxy::u32 live_marker(opxy::u32 marker, opxy::u32 frames, opxy::u32 value) {
    if (marker > frames) marker = frames;
    if (value < 0x40000000u)
        return value ? marker - live_scale(marker, (0x40000000u - value) << 1) : 0;
    return marker + live_scale(frames - marker, value == 0x7fffffffu ? value : (value - 0x40000000u) << 1);
}
// A pad without saved settings: loop, repitch, one bar.
inline opxy::PadSettings sample_defaults() {
    return encode_controls(Controls{false, false, 0, 2});
}
inline opxy::PadSettings sample_assign(const opxy::SampleAssignment& input) {
    Controls c = input.replacing ? decode_controls(input.previous) :
        Controls{false, false, 0, suggested_length(input.frames, input.rate, input.bpm_x10)};
    // A recording plays at the project tempo it was recorded at (tenths BPM);
    // a file may name its tempo.
    const opxy::u32 tempo = input.bpm_x10;
    const opxy::u32 bpm = !input.recorded ? filename_bpm(input.filename, input.filename_bytes) * 10 :
        tempo >= 200 && tempo <= 3000 ? tempo : 0;
    if (bpm) c.bpm_x10 = bpm;
    return encode_controls(c);
}
inline void drum_control(opxy::PadSettings& value, const opxy::DrumControl& event) {
    Controls c = decode_controls(value);
    const int delta = event.direction < 0 ? -1 : event.direction > 0 ? 1 : 0;
    if (!event.shifted && event.knob == 3) {
        if (event.pressed) c.once = !c.once;
        else if (delta) c.once = delta > 0;
    } else if (event.shifted && event.knob == 1) {
        if (event.pressed) c.bpm_x10 = c.bpm_x10 ? 0 : 1200;
        else if (c.bpm_x10) {
            int bpm = int(c.bpm_x10) + delta * (event.held ? 10 : 1);
            c.bpm_x10 = bpm < 200 ? 200 : bpm > 3000 ? 3000 : opxy::u32(bpm);
        } else {
            int length = int(c.length) + delta;
            c.length = length < 0 ? 0 : length > 5 ? 5 : opxy::u32(length);
        }
    } else if (event.shifted && event.knob == 2) {
        if (event.pressed) c.stretch = !c.stretch;
        else if (delta) c.stretch = delta > 0;
    }
    value = encode_controls(c);
}
inline void sample_caption(char* target, const char* text) {
    unsigned n = 0;
    while (n < 15 && text[n]) { target[n] = text[n]; ++n; }
    target[n] = 0;
}
inline void drum_panel(opxy::PadSettings value, const opxy::SampleRegion& saved, const opxy::i32* live,
                       opxy::DrumPanel& panel) {
    const EffectiveControls effective = effective_controls(value, live);
    const Controls c = effective.pad;
    panel.pitch_cents = effective.pitch_cents;
    panel.repeat = !c.once;
    panel.reverse = live_mode(live_parameter(live, 4), saved.reverse);
    panel.level = live_parameter(live, 7);
    if (c.bpm_x10) {
        char* text = panel.shifted[0];
        if (c.bpm_x10 >= 1000) *text++ = char('0' + c.bpm_x10 / 1000);
        *text++ = char('0' + c.bpm_x10 / 100 % 10);
        *text++ = char('0' + c.bpm_x10 / 10 % 10);
        *text++ = '.';
        *text++ = char('0' + c.bpm_x10 % 10);
        sample_caption(text, " bpm");
    } else {
        const char* labels[] = {"1/4 bar", "1/2 bar", "1 bar", "2 bars", "4 bars", "8 bars"};
        sample_caption(panel.shifted[0], labels[c.length]);
    }
    sample_caption(panel.shifted[1], c.stretch ? "timestretch" : "repitch");
}
}
