#pragma once
#include <opxy/extension.hpp>

namespace opxy { namespace extensions { namespace capture {

// Internal ghost pattern, independent of firmware addresses and input routing.
// The adapter supplies stock recording positions and stock-scaled tick deltas.
// No quantization or conversion from wall-clock time belongs in this buffer.
class RecordingHistory {
public:
    // Q8 pattern ticks since the take began; 64 bits so they never wrap.
    using Ticks = __UINT64_TYPE__;
    // Stock's ordinary note container and active recording list limits.
    static const u32 capacity = 120;
    static const u32 held_capacity = 20;
    constexpr RecordingHistory() : context_{}, entries_{}, count_(0), held_(0),
                                   elapsed_(0), playing_(false) {}

    // length is in stock pattern ticks, not bars or elapsed milliseconds.
    // Stop freezes the partial pass; a subsequent start begins a new take.
    void sync(const RecordingContext& context, bool playing) {
        const bool valid = context.recordable && context.track < 8 &&
            context.length > 0 && context.length < (1u << 23);
        const bool running = valid && playing;
        if (!(context == context_) || (running && !playing_)) clear();
        if (!valid) clear();
        if (!running && playing_) {
            for (u32 i = 0; i < count_; ++i) entries_[i].held = false;
            held_ = 0;
        }
        context_ = context;
        playing_ = running;
    }

    // Same Q8-scaled delta that stock accumulates for held recorded notes.
    // Completed notes stay across silence, but a take spans at most one
    // pattern length of playing: see note_on.
    void advance(u32 delta) {
        if (!playing_ || !delta) return;
        elapsed_ += delta;
        for (u32 i = 0; i < count_; ++i) {
            Entry& entry = entries_[i];
            if (entry.held) {
                const u32 maximum = context_.length << 8;
                entry.duration += delta < maximum - entry.duration ? delta : maximum - entry.duration;
                entry.note.length = entry.duration >> 8;
            }
        }
    }

    // Receives normalized live input only. The platform must exclude playback,
    // step entry and generated notes. A repeated key ends its previous hold.
    bool note_on(i32 position, u32 pitch, u32 velocity, u32 radius = 0) {
        if (!playing_ || pitch > 127 || velocity > 127) return false;
        note_off(pitch);
        if (!velocity) return false;
        // The take is the last pattern length of playing, ending at the newest
        // onset, so an earlier pass never shares steps with the one replacing
        // it. A note from about one loop ago, within the collision radius, is
        // the previous pass on this step even if timing drifted. A note held
        // for the whole loop (still held, or released less than a loop ago)
        // stays until the new pass plays its step.
        const u32 slack = radius < context_.length / 2 ? radius : context_.length / 2;
        const Ticks window = static_cast<Ticks>(context_.length - slack) << 8;
        u32 recent = 0;
        for (u32 i = 0; i < count_; ++i) {
            const Entry& old = entries_[i];
            const bool sounding = old.held ||
                (old.duration >= context_.length << 8 && elapsed_ - old.release < window);
            const bool replaced = !sounding || same_step(old.note.position, position, slack);
            if (elapsed_ - old.onset < window || !replaced) entries_[recent++] = old;
            else if (old.held) --held_;
        }
        count_ = recent;
        // Stock rejects a full container before attempting collision pruning.
        if (held_ == held_capacity || count_ == capacity) return false;
        u32 kept = 0;
        for (u32 i = 0; i < count_; ++i) {
            const Entry& old = entries_[i];
            const u32 distance = position >= old.note.position
                ? static_cast<u32>(position) - static_cast<u32>(old.note.position)
                : static_cast<u32>(old.note.position) - static_cast<u32>(position);
            if (old.note.pitch == pitch && distance < radius) {
                if (old.held) --held_;
            } else {
                entries_[kept++] = old;
            }
        }
        count_ = kept;
        Entry& entry = entries_[count_++];
        entry.note = {position, 0, static_cast<u8>(pitch), static_cast<u8>(velocity)};
        entry.onset = entry.release = elapsed_;
        entry.duration = 0;
        entry.held = true;
        ++held_;
        return true;
    }

    void note_off(u32 pitch) {
        if (!playing_) return;
        for (u32 i = 0; i < count_; ++i) {
            Entry& entry = entries_[i];
            if (entry.held && entry.note.pitch == pitch) {
                entry.held = false;
                --held_;
                entry.release = elapsed_;
            }
        }
    }

    u32 size() const { return count_; }

    // Keep input order for stock's collision/capacity policy, even when a
    // loop wrap makes recorded positions descend. A snapshot never consumes
    // history. The caller clears it only after a successful pattern commit.
    u32 snapshot(Note* notes, u32 available) const {
        if (!notes || available < count_) return 0;
        for (u32 i = 0; i < count_; ++i) notes[i] = entries_[i].note;
        return count_;
    }

    void clear() { count_ = held_ = 0; }

private:
    // Positions on the loop, including stock's pickup before zero, closer
    // than radius in either direction around the wrap.
    bool same_step(i32 a, i32 b, u32 radius) const {
        const u32 distance = a >= b ? static_cast<u32>(a) - static_cast<u32>(b)
                                    : static_cast<u32>(b) - static_cast<u32>(a);
        const u32 around = distance < context_.length ? context_.length - distance : 0;
        return distance < radius || around < radius;
    }

    struct Entry {
        Note note;
        Ticks onset, release;
        u32 duration;
        bool held;
    };
    RecordingContext context_;
    Entry entries_[capacity];
    u32 count_, held_;
    Ticks elapsed_;
    bool playing_;
};

}}}
