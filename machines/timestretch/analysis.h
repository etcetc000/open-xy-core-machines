// SPDX-License-Identifier: AGPL-3.0-only
// Capicola derivative-crossing analysis, prepared outside the audio callback.
#pragma once
#include <opxy/sample.hpp>
#include <opxy/sample_analysis.hpp>

namespace timestretch { namespace sample_analysis {
using opxy::u32;
using opxy::i32;
constexpr u32 kMaxFrames = 0x000fffffu;
constexpr u32 kBlockFrames = 256;
constexpr u32 kBlockWords = 9;
constexpr i32 kKeyThreshold = 33; // Approximately 0.001 of normalized 16-bit PCM.

struct Analysis {
    opxy::SampleView sample;
    const u32* words;
    u32 word_count;
    bool ready;
    u32 keys;
    u32 channel;
    u32 activity;
    const Analysis* right;
};
struct Builder {
    Analysis analysis;
    u32* storage;
    u32 cursor;
    u32 keys;
    u32 last_key;
    i32 previous_derivative;
    i32 previous_value;
    bool failed;
};
constexpr u32 bitmap_words(u32 frames) {
    return frames >= 2 && frames <= kMaxFrames ? ((frames + 255) >> 8) * kBlockWords : 0;
}
// Integer key positions live entirely in the bitmap and its block ranks.
// Playback reads PCM; no reconstructed amplitude or fractional key is stored.
constexpr u32 storage_words(u32 frames) {
    return bitmap_words(frames);
}
inline bool valid_sample(const opxy::SampleView& sample) {
    return sample.data && storage_words(sample.frames) && (sample.channels == 1 || sample.channels == 2);
}
inline bool matches_channel(const Analysis* analysis, const opxy::SampleView& sample, u32 channel) {
    return analysis && analysis->ready && valid_sample(sample) && analysis->words &&
        analysis->keys && analysis->keys <= sample.frames &&
        analysis->word_count >= bitmap_words(sample.frames) &&
        analysis->sample.data == sample.data && analysis->sample.frames == sample.frames &&
        analysis->sample.channels == sample.channels && analysis->sample.rate == sample.rate &&
        analysis->channel == channel;
}
inline bool matches(const Analysis* analysis, const opxy::SampleView& sample) {
    return matches_channel(analysis, sample, 0) &&
        (sample.channels == 1 || matches_channel(analysis->right, sample, 1));
}
inline i32 analysis_pcm(const opxy::SampleView& sample, u32 frame, u32 channel = 0) {
    return opxy::sample_at(sample, frame, channel);
}
inline bool begin(Builder& builder, const opxy::SampleView& sample, u32* storage, u32 capacity, u32 channel = 0) {
    builder = Builder();
    const u32 required = bitmap_words(sample.frames);
    if (!valid_sample(sample) || channel >= sample.channels || !storage || capacity < required) return false;
    builder.analysis.sample = sample;
    builder.analysis.channel = channel;
    builder.analysis.words = storage;
    builder.analysis.word_count = capacity;
    builder.storage = storage;
    return true;
}
// Budget counts source frames; each iteration has constant work. Blocks are
// cleared on entry, so begin itself does not walk a sample-sized allocation.
inline bool prepare(Builder& builder, u32 frame_budget) {
    if (!builder.storage || builder.failed || builder.analysis.ready)
        return builder.analysis.ready;
    const opxy::SampleView& sample = builder.analysis.sample;
    const u32 frames = sample.frames, stride = sample.channels;
    // Frames are always in range here, so read the channel directly and
    // carry the three-sample window forward instead of rereading it.
    const opxy::i16* data = sample.data + builder.analysis.channel;
    u32 n = builder.cursor;
    const u32 end = frame_budget < frames - n ? n + frame_budget : frames;
    u32* storage = builder.storage;
    u32 keys = builder.keys, last_key = builder.last_key, activity = builder.analysis.activity;
    i32 previous_derivative = builder.previous_derivative, previous_value = builder.previous_value;
    i32 before = data[(n ? n - 1 : frames - 1) * stride];
    i32 here = n < frames ? data[n * stride] : 0;
    for (; n < end; ++n) {
        const u32 block = (n >> 8) * kBlockWords;
        if (!(n & 255)) {
            storage[block] = keys;
            for (u32 i = 1; i < kBlockWords; ++i) storage[block + i] = 0;
        }
        const i32 after = data[(n + 1 < frames ? n + 1 : 0) * stride];
        const i32 derivative = before - after;
        // <= 255 per frame and fewer than 2^20 frames: no accumulator overflow.
        activity += static_cast<u32>(derivative < 0 ? -derivative : derivative) >> 8;
        const i32 difference = here - previous_value;
        const bool crossing = ((previous_derivative > 0 && derivative <= 0) ||
                               (previous_derivative < 0 && derivative >= 0));
        // These points select grain duration, never waveform amplitude. A
        // finite-difference crossing and a small PCM threshold suffice here.
        // The forced 256-frame spacing bounds every bitmap neighbor search.
        const bool store = !n || n - last_key >= 256 ||
            (crossing && n - last_key > 1 &&
             (difference > kKeyThreshold || difference < -kKeyThreshold));
        if (store) {
            storage[block + 1 + ((n & 255) >> 5)] |= 1u << (n & 31);
            ++keys;
            last_key = n;
            previous_value = here;
        }
        previous_derivative = derivative;
        before = here;
        here = after;
    }
    builder.cursor = n;
    builder.keys = keys;
    builder.last_key = last_key;
    builder.analysis.activity = activity;
    builder.previous_derivative = previous_derivative;
    builder.previous_value = previous_value;
    builder.analysis.ready = builder.cursor == sample.frames;
    builder.analysis.keys = builder.keys;
    return builder.analysis.ready;
}
inline const Analysis& view(const Builder& builder) { return builder.analysis; }
inline u32 population(u32 x) {
    x -= (x >> 1) & 0x55555555u;
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0f0f0f0fu;
    return (x * 0x01010101u) >> 24;
}
inline u32 first_bit(u32 x) {
    u32 n = 0;
    if (!(x & 65535)) { x >>= 16; n += 16; }
    if (!(x & 255)) { x >>= 8; n += 8; }
    if (!(x & 15)) { x >>= 4; n += 4; }
    if (!(x & 3)) { x >>= 2; n += 2; }
    return n + ((x & 1) ? 0 : 1);
}
inline u32 last_bit(u32 x) {
    u32 n = 0;
    if (x >> 16) { x >>= 16; n += 16; }
    if (x >> 8) { x >>= 8; n += 8; }
    if (x >> 4) { x >>= 4; n += 4; }
    if (x >> 2) { x >>= 2; n += 2; }
    return n + (x >> 1);
}
inline u32 mask_word(const Analysis& analysis, u32 word) {
    return analysis.words[(word >> 3) * kBlockWords + 1 + (word & 7)];
}
// Count keyframes strictly before n. The last partial block is handled without
// indexing beyond its allocation, including an exact 256-frame boundary.
static inline u32 rank(const Analysis& analysis, u32 n) {
    if (!n) return 0;
    if (n > analysis.sample.frames) n = analysis.sample.frames;
    const u32 frame = n - 1;
    const u32 block = (frame >> 8) * kBlockWords;
    const u32 word = (frame & 255) >> 5;
    u32 count = analysis.words[block];
    for (u32 i = 0; i < 8; ++i) {
        if (i < word) count += population(analysis.words[block + 1 + i]);
    }
    count += population(analysis.words[block + 1 + word] & (0xffffffffu >> (31 - (frame & 31))));
    return count;
}
// Consecutive keys are at most 256 frames apart. Nine bitmap reads find a neighbor;
// the endpoint sentinel is returned when no later key exists in the sample.
inline u32 previous_key(const Analysis& analysis, u32 frame) {
    if (frame >= analysis.sample.frames) frame = analysis.sample.frames - 1;
    u32 word = frame >> 5;
    u32 bits = mask_word(analysis, word) & (0xffffffffu >> (31 - (frame & 31)));
    for (u32 i = 0; i < 9; ++i) {
        if (bits) return (word << 5) + last_bit(bits);
        if (!word) return 0;
        --word;
        bits = mask_word(analysis, word);
    }
    return 0; // Damaged input cannot turn a callback into an unbounded search.
}
inline u32 next_key(const Analysis& analysis, u32 frame) {
    if (frame >= analysis.sample.frames - 1) return analysis.sample.frames;
    ++frame;
    u32 word = frame >> 5;
    const u32 last = (analysis.sample.frames - 1) >> 5;
    u32 bits = mask_word(analysis, word) & (0xffffffffu << (frame & 31));
    for (u32 i = 0; i < 9; ++i) {
        if (bits) return (word << 5) + first_bit(bits);
        if (word == last) return analysis.sample.frames;
        bits = mask_word(analysis, ++word);
    }
    return analysis.sample.frames;
}
// Select a key by ordinal without walking waveform windows. At most twelve
// block comparisons, eight bitmap words and 32 bit tests (source <2^20).
inline u32 select_key(const Analysis& analysis, u32 ordinal) {
    if (ordinal >= analysis.keys) return analysis.sample.frames;
    u32 lo = 0, hi = (analysis.sample.frames - 1) >> 8;
    while (lo < hi) {
        const u32 mid = (lo + hi + 1) >> 1;
        if (analysis.words[mid * kBlockWords] <= ordinal) lo = mid; else hi = mid - 1;
    }
    u32 remaining = ordinal - analysis.words[lo * kBlockWords];
    for (u32 word = 0; word < 8; ++word) {
        u32 bits = analysis.words[lo * kBlockWords + 1 + word];
        const u32 count = population(bits);
        if (remaining >= count) { remaining -= count; continue; }
        for (u32 bit = 0; bit < 32; ++bit) if (bits & (1u << bit)) {
            if (!remaining) return lo * 256 + word * 32 + bit;
            --remaining;
        }
    }
    return analysis.sample.frames;
}
} }

namespace timestretch {
// One machine-owned allocation: four header words, then each channel's bitmap
// and block ranks. Both channels publish together; the SDK sees opaque words.
struct Preparation {
    sample_analysis::Builder builder;
    opxy::u32* storage;
    opxy::u32 capacity, used, channel;
    bool done, failed;
};
constexpr opxy::u32 kAnalysisHeader = 4;
constexpr opxy::u32 kAnalysisFormat = 0x53545232u;
inline opxy::u32 analysis_words(const opxy::SampleView& sample) {
    return sample_analysis::valid_sample(sample) ?
        kAnalysisHeader + sample.channels * sample_analysis::bitmap_words(sample.frames) : 0;
}
inline bool prepare_begin(Preparation& job, const opxy::SampleView& sample, opxy::u32* storage, opxy::u32 words) {
    job = Preparation{};
    job.storage = storage;
    job.capacity = words;
    job.used = kAnalysisHeader;
    if (!storage || words < kAnalysisHeader) return false;
    for (opxy::u32 i = 0; i < kAnalysisHeader; ++i) storage[i] = 0;
    return sample_analysis::begin(job.builder, sample, storage + kAnalysisHeader, words - kAnalysisHeader);
}
inline opxy::PrepareStatus prepare_step(Preparation& job, opxy::u32 frames, opxy::u32& cursor) {
    const opxy::SampleView sample = job.builder.analysis.sample;
    if (job.failed || !sample_analysis::valid_sample(sample)) return opxy::prepare_failed;
    if (job.done) { cursor = sample.frames; return opxy::prepare_done; }
    const bool complete = sample_analysis::prepare(job.builder, frames);
    cursor = (job.channel * sample.frames + job.builder.cursor) / sample.channels;
    if (job.builder.failed) { job.failed = true; return opxy::prepare_failed; }
    if (!complete) return opxy::prepare_running;
    const opxy::u32 used = sample_analysis::bitmap_words(sample.frames);
    job.storage[job.channel * 2] = job.builder.keys;
    job.storage[job.channel * 2 + 1] = job.builder.analysis.activity;
    job.used += used;
    if (++job.channel == sample.channels) { job.done = true; return opxy::prepare_done; }
    // Starting the other channel clears no sample-sized memory and does no
    // analysis, so this invocation still observes the caller's frame budget.
    const bool started =
        sample_analysis::begin(job.builder, sample, job.storage + job.used, job.capacity - job.used, job.channel);
    job.failed = !started;
    return started ? opxy::prepare_running : opxy::prepare_failed;
}
inline bool prepared(const Preparation& job, opxy::SampleAnalysis& out) {
    if (!job.done || job.failed) return false;
    out = opxy::SampleAnalysis{job.builder.analysis.sample, job.storage, job.used, kAnalysisFormat};
    return true;
}
// Validate the compact header before making per-block views. No persistent
// pointer refers to these stack objects; grain caches contain values only.
inline bool from_sdk(const opxy::SampleAnalysis& in, sample_analysis::Analysis& left, sample_analysis::Analysis& right) {
    if (in.info != kAnalysisFormat || !sample_analysis::valid_sample(in.sample) ||
        !in.words || in.word_count < kAnalysisHeader) return false;
    opxy::u32 offset = kAnalysisHeader;
    for (opxy::u32 channel = 0; channel < in.sample.channels; ++channel) {
        const opxy::u32 keys = in.words[channel * 2];
        const opxy::u32 words = sample_analysis::bitmap_words(in.sample.frames);
        if (!keys || keys > in.sample.frames || words > in.word_count - offset) return false;
        sample_analysis::Analysis& view = channel ? right : left;
        view = sample_analysis::Analysis{in.sample, in.words + offset, words, true, keys, channel,
                                         in.words[channel * 2 + 1], nullptr};
        offset += words;
    }
    left.right = in.sample.channels == 2 ? &right : nullptr;
    return offset == in.word_count;
}
}
