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
// A repitch block's inputs, packed (block_inputs), and what render derived
// from them for its loop: only what the loop reads. The next block with
// identical inputs reuses them (render_sample). It shares storage with the
// grain (Voice): only repitch blocks use it, only timestretch blocks use the
// grain, and every change of mode respawns the grain before it is read.
constexpr unsigned kRepitchInputs = 15;
struct RepitchBlock {
    opxy::u32 inputs[kRepitchInputs];
    const opxy::i16* origin;      // stretch::Source's PCM addressing
    opxy::u32 frames, loop;
    opxy::u32 remainder, divisor; // the TimeStep fields the loop reads
    opxy::u32 advances[2];
    opxy::u32 level;
    opxy::u8 shift;
    opxy::i8 stride;
    bool crossings[2];
};
struct Voice {
    opxy::SampleView sample;
    const opxy::SampleAnalysis* analysis;  // the SDK cache entry; immutable while the voice lives
    opxy::u32 position;   // frames << shift
    opxy::u32 shift;      // fractional bits; frames << shift stays below 2^31
    bool sync_pending;
    bool start_pending; // a new note starts at its marker, even while transport runs
    bool slicing;       // a frozen audition advances its clock while analysis loads
    bool grains_ready;
    bool ranks_valid;
    bool repitch_ready;  // the last block took the repitch loop; repitch describes it
    union {
        stretch::Granule grain;  // timestretch blocks
        RepitchBlock repitch;    // repitch blocks
    };
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
// The adapter keeps one Voice per voice slot, 32 of them (definition.json): a
// byte here is 32 bytes of RAM in every image that carries the machine. The
// limits hold for 32-bit pointers (the device).
static_assert(sizeof(void*) != 4 || sizeof(RepitchBlock) <= sizeof(stretch::Granule),
              "the repitch cache must fit in the grain's storage");
static_assert(sizeof(void*) != 4 || sizeof(Voice) <= 220, "Timestretch's voice grew past 220 bytes");
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
// Source frames per output frame for a frozen forward tap audition.
void slice_clock(const opxy::SampleSlice&, opxy::SampleSliceRate&);
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
// gain() for render()'s own output only: every word a 16-bit value times 32768.
void gain_rendered(opxy::i32* output, opxy::u32 value, opxy::u32 channels);
void render(Voice&, opxy::i32* output, const opxy::i32* parameters, const opxy::u32* increments,
            const opxy::Transport&, opxy::u32 output_channels = 1);
}
