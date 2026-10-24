#pragma once
#include <opxy/sample.hpp>
#include <opxy/transport.hpp>
#include <opxy/sample_engine.hpp>
#include "stretch.h"
#include "pitch.h"

// SPDX-License-Identifier: AGPL-3.0-only
// Tempo-locked varispeed or Capicola pitch-preserving sample playback.
// Fixed-point arithmetic; no audio callback allocations.
namespace timestretch {
struct TimeStep { opxy::u32 value, remainder, divisor, whole; };
struct Voice {
    opxy::SampleView sample;
    const opxy::SampleAnalysis* analysis;  // the SDK cache entry; immutable while the voice lives
    opxy::u32 position;   // frames << shift
    opxy::u32 shift;      // fractional bits; frames << shift stays below 2^31
    bool sync_pending;
    bool ranks_valid;
    stretch::Granule grain;
    bool stretching;
    opxy::u32 region_begin;
    opxy::u32 region_end;
    bool reverse;
    opxy::u32 source_bpm_x10; // 0 uses bars; otherwise source tempo in tenths, 200..3000
    opxy::u32 time_remainder;
    opxy::u32 time_divisor;
    bool was_playing;
    int pitch;
    int live_pitch_cents;
    bool play_once;
    bool finished;
    // Derived settings only; playheads and the exact tempo remainder stay live.
    TimeStep time_step;
    opxy::u32 timing_key;
    int tuning_cents;
    opxy::u32 tuning;
    opxy::u32 pitch_step;
    opxy::u32 base_rank;
    opxy::u32 key_count;
};
constexpr opxy::u32 kBeats[] = {1, 2, 4, 8, 16, 32};

opxy::u32 beats(opxy::i32 parameter);
// Output samples one loop must span: beats * 60 s * 44100 / BPM.
opxy::u32 duration(opxy::u32 beats, opxy::u32 bpm_x10);
// Loop frame matching the transport's place in the loop (ticks at 96 per quarter).
opxy::u32 synced_position(opxy::u32 frames, opxy::u32 beats, opxy::u32 ticks96);
inline opxy::u32 frame(const Voice& voice) { return voice.position >> voice.shift; }

void initialize(Voice&, const opxy::SampleView&);
void start(Voice&);
void render_sample(Voice&, const opxy::SampleBlock&);
// Original sample tempo in tenths; project tempo remains the playback target. 0 selects bars.
void set_source_bpm_x10(Voice&, opxy::u32);
// Tick-aligned source frame for a loop of arbitrary musical duration.
opxy::u32 synced_source_position(opxy::u32 frames, opxy::u32 rate, opxy::u32 source_bpm_x10, opxy::u32 ticks96);
// The owner publishes a completed immutable cache and keeps it alive for the voice.
void bind_analysis(Voice&, const opxy::SampleAnalysis*);
// Half-open source frame range. Invalid/empty ranges render silence.
void region(Voice&, opxy::u32 begin, opxy::u32 end, bool reverse = false);
// Scale by a SampleRegion level (unsigned Q27; opxy::sample_unity_level is unity).
void gain(opxy::i32* output, opxy::u32 value, opxy::u32 channels = 1);
void render(Voice&, opxy::i32* output, const opxy::i32* parameters, const opxy::u32* increments,
            const opxy::Transport&, opxy::u32 output_channels = 1);
}
