#include <opxy/player.hpp>

// Chord player. On the OP-XY's two-octave F-to-E keyboard, the lower octave
// holds chord modifiers and the upper octave plays chord roots. Modifiers never
// sound, whatever octave the keyboard is shifted to:
//   black F# G# A# C#  chord type  dim min maj sus
//   white F G A B C D E  extension  6 m7 M7 9 11 13 b9
//   black D#             unused (silent)
// A root plays the most recently held chord type with every held extension,
// or itself alone when no type is held. Notes outside the keyboard's two
// octaves (from the sequencer) are roots.
namespace chords {
using opxy::u8;
using opxy::u32;
using opxy::i32;
using opxy::player::Controls;
using opxy::player::Display;
using opxy::player::Plan;

const u32 kF = 5;  // pitch class of the keyboard's first key
enum Type : u8 { kNone, kDim, kMin, kMaj, kSus, kTypes };
const u32 kExtensionCount = 7;
struct Root {
    u8 note, velocity, type, extensions;  // note + 1; zero is an empty slot
};

struct State {
    u8 held;        // bit per chord type
    u8 last;        // most recently pressed held type
    u8 extensions;  // bit per extension
    u8 descending;  // alternate direction: the next chord's order
    u32 seed;       // random direction
    u8 root;        // the last root played, plus one; 0 before the first
    u8 notes[12];   // the note holding each modifier key, plus one; 0 when up
    u8 display_type, display_extensions;
    u8 order[4];    // held types in press order; last wins (Secret Chords off)
    Root roots[8];
};

namespace {
const u8 kChords[kTypes][3] = {{0}, {0, 3, 6}, {0, 3, 7}, {0, 4, 7}, {0, 5, 7}};
const u8 kExtensions[kExtensionCount] = {9, 10, 11, 14, 17, 21, 13};
const i32 kVoicingSteps = 6;  // the voicing encoder spans -6..+6 inversion steps

// Orchid's upper type row on black keys, lower extension row on white keys.
const u8 kModifier[12] = {kTypes, kDim, kTypes + 1, kMin, kTypes + 2, kMaj,
                        kTypes + 3, kTypes + 4, kSus, kTypes + 5, 0, kTypes + 6};
// The modifier octave key of each chord type and extension, for the page.
const u8 kTypeKey[kTypes] = {0, 1, 3, 5, 8};
const u8 kExtensionKey[kExtensionCount] = {0, 2, 4, 6, 7, 9, 11};

u8 effective_extensions(u8 extensions) {
    // 6 and 13 are the same pitch class; prefer the higher register when both
    // are requested. This leaves room for more distinct chord colours.
    return extensions & 32 ? static_cast<u8>(extensions & ~1u) : extensions;
}

// The modifier key (0..11 from F) a note plays, or -1 for a root.
int modifier_key(u32 note, u32 lowest_key) {
    return note >= lowest_key && note < lowest_key + 12 ? static_cast<int>(note - lowest_key) : -1;
}

u32 steps(i32 value, u32 count) {  // Q31 0..1 -> 0..count-1, count <= 65536
    const u32 v = value < 0 ? 0u : static_cast<u32>(value) >> 16;
    const u32 step = (v * count) >> 15;
    return step < count ? step : count - 1;
}

void add(i32* notes, u32& count, i32 pitch) {
    for (u32 i = 0; i < count; ++i)
        if (notes[i] == pitch) return;
    if (count < opxy::player::max_tones) notes[count++] = pitch;
}

void sort(i32* notes, u32 count) {
    for (u32 i = 1; i < count; ++i)
        for (u32 j = i; j > 0 && notes[j - 1] > notes[j]; --j) {
            const i32 t = notes[j];
            notes[j] = notes[j - 1];
            notes[j - 1] = t;
        }
}

// Voicing: each step up moves the lowest note an octave up, each step down
// the highest note an octave down.
void voice(i32* notes, u32 count, i32 voicing) {
    for (; voicing > 0; --voicing) { notes[0] += 12; sort(notes, count); }
    for (; voicing < 0; ++voicing) { notes[count - 1] -= 12; sort(notes, count); }
}

// Bounded text: appends while there is room, always terminated.
struct Text {
    char* out;
    u32 size, length;
    Text& operator<<(const char* s) {
        while (*s && length + 1 < size) out[length++] = *s++;
        out[length] = 0;
        return *this;
    }
    Text& operator<<(i32 n) {
        char digits[12];
        u32 count = 0, v = n < 0 ? static_cast<u32>(-n) : static_cast<u32>(n);
        do { digits[count++] = static_cast<char>('0' + v % 10); v /= 10; } while (v);
        if (n < 0) *this << "-";
        while (count) { const char c[2] = {digits[--count], 0}; *this << c; }
        return *this;
    }
};
template <u32 N> Text text(char (&out)[N]) { out[0] = 0; return Text{out, N, 0}; }

const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Name the captured chord, not just the modifiers now held by the left hand.
void name(Text& out, u32 root, u32 type, u32 extensions) {
    out << kNoteNames[root % 12];
    if (type == kNone) return;
    bool m7 = extensions & 2, M7 = extensions & 4;
    const bool e6 = extensions & 1, e9 = extensions & 8;
    const bool e11 = extensions & 16, e13 = extensions & 32, flat9 = extensions & 64;
    u32 q = type;
    const bool seventh = m7 || M7;
    const char* n = e13 ? "13" : e11 ? "11" : e9 ? "9" : "7";
    static const char* const quality[] = {"", "dim", "minor", "major", "sus4"};
    out << " ";
    if (m7) {
        static const char* const words[] = {"", "half-dim ", "minor ", "dominant ", ""};
        out << words[q] << n;
        if (q == kSus) out << " sus4";
    } else if (M7) {
        static const char* const words[] = {"", "dim major ", "minor major ", "major ", "major "};
        out << words[q] << n;
        if (q == kSus) out << " sus4";
    } else if (e6 && q == kDim) {
        out << "diminished 7";
    } else {
        out << quality[q];
    }
    if (m7 && M7) out << " + maj7";
    if (e6 && (seventh || q != kDim)) out << (seventh ? " 13" : " 6");
    if (!seventh && e9) out << " add 9";
    if (!seventh && e11) out << " add 11";
    if (!seventh && e13) out << " add 13";
    if (flat9) out << " b9";
}
}

// A modifier is released by the note that pressed it, so an octave shift
// while it is held cannot leave it stuck.
bool release(State& state, u32 note) {
    if (note > 127) return true;
    int key = -1;
    for (int k = 0; k < 12; ++k)
        if (state.notes[k] == note + 1) key = k;
    if (key < 0) {
        for (auto& root : state.roots)
            if (root.note == note + 1) root = Root{};
        return false;
    }
    state.notes[key] = 0;
    const int m = kModifier[key];
    if (m >= kDim && m < kTypes) {
        state.held &= static_cast<u8>(~(1u << m));
        state.last = kNone;
        for (u8 t : state.order)
            if (state.held & (1u << t)) state.last = t;
    } else if (m >= kTypes) {
        state.extensions &= static_cast<u8>(~(1u << (m - kTypes)));
    }
    return true;
}

void make_plan(State& state, const Controls& controls, const Root& root, Plan& plan) {
    plan.count = 0;
    const u32 note = root.note - 1;

    i32 notes[opxy::player::max_tones];
    u32 count = 0;
    if (root.type == kNone) {
        notes[count++] = static_cast<i32>(note);
    } else {
        u32 extensions = 0;
        for (u32 i = 0; i < kExtensionCount; ++i) if (root.extensions & (1u << i)) ++extensions;
        // Keep the root, third/suspension and every selected extension. Only
        // an overfull selection omits the fifth to fit the eight-tone ABI.
        const u32 base = extensions + 3 > opxy::player::max_tones ? 2 : 3;
        for (u32 i = 0; i < base; ++i) notes[count++] = static_cast<i32>(note + kChords[root.type][i]);
        for (u32 i = 0; i < kExtensionCount; ++i)
            if (root.extensions & (1u << i)) add(notes, count, static_cast<i32>(note + kExtensions[i]));
    }
    sort(notes, count);
    voice(notes, count, static_cast<i32>(steps(controls.value[2], 2 * kVoicingSteps + 1)) - kVoicingSteps);
    // Spread opens the voicing: every second note rises an octave.
    if (steps(controls.value[3], 2)) {
        for (u32 i = 1; i < count; i += 2) notes[i] += 12;
        sort(notes, count);
    }

    // Direction: up, down, alternating per chord, or a random rotation.
    const u32 direction = steps(controls.value[1], 4);
    bool down = direction == 1;
    if (direction == 2) {
        down = state.descending != 0;
        state.descending = !state.descending;
    }
    u32 rotate = 0;
    if (direction == 3) {
        state.seed = state.seed * 1664525u + 1013904223u;
        rotate = (state.seed >> 16) % count;
    }
    const u32 strum = steps(controls.value[0], 64);
    for (u32 i = 0; i < count; ++i) {
        const u32 k = (i + rotate) % count;
        const i32 pitch = notes[down ? count - 1 - k : k];
        if (pitch < 0 || pitch > 127) continue;
        opxy::player::Tone& tone = plan.tones[plan.count];
        tone.pitch = static_cast<u8>(pitch);
        tone.velocity = root.velocity;
        tone.delay = static_cast<opxy::u16>(plan.count * strum);
        ++plan.count;
    }
}

bool press(State& state, const Controls& controls, u32 note, u32 velocity, Plan& plan) {
    plan.count = 0;
    if (note > 127 || velocity > 127) return true;
    const int key = modifier_key(note, controls.lowest_key);
    if (key >= 0) {
        // Ignore repeat note-ons until this physical control has been released.
        if (state.notes[key]) return true;
        state.notes[key] = static_cast<u8>(note + 1);
        const u8 m = kModifier[key];
        if (m >= kDim && m < kTypes) {
            state.held |= static_cast<u8>(1u << m);
            state.last = m;
            u32 n = 0;
            for (u8 t : state.order) if (t && t != m) state.order[n++] = t;
            state.order[n++] = m;
            while (n < 4) state.order[n++] = 0;
        } else if (m >= kTypes) {
            const u8 bit = static_cast<u8>(1u << (m - kTypes));
            state.extensions |= bit;
        }
        return true;
    }
    Root* target = nullptr;
    for (auto& root : state.roots) if (root.note == note + 1) target = &root;
    if (!target) for (auto& root : state.roots) if (!root.note) { target = &root; break; }
    if (!target) return true;  // eight roots are already held
    *target = Root{static_cast<u8>(note + 1), static_cast<u8>(velocity), state.last,
                   static_cast<u8>(state.last ? effective_extensions(state.extensions) : 0)};
    make_plan(state, controls, *target, plan);
    state.root = target->note;
    state.display_type = target->type;
    state.display_extensions = target->extensions;
    // An entirely out-of-range chord has no stock record to release later.
    if (!plan.count) *target = Root{};
    return true;
}

void reset(State& state) {
    state = State{};
}

// The page: root bits 0..11 draw on the right, modifier bits 12..23 on the
// left. These are artwork identities, not physical key indices. The chord
// appears in words beneath them, along with the four
// encoder values.
void display(const State& state, const Controls& controls, Display& display) {
    display.lit = 0;
    if (state.root) display.lit |= 1u << ((state.root - 1 + 12 - kF) % 12);
    for (u32 type = kDim; type < kTypes; ++type)
        if (state.held & (1u << type)) display.lit |= 1u << (12 + kTypeKey[type]);
    for (u32 i = 0; i < kExtensionCount; ++i)
        if (state.extensions & (1u << i)) display.lit |= 1u << (12 + kExtensionKey[i]);
    Text line = text(display.text);
    if (state.root) name(line, state.root - 1, state.display_type, state.display_extensions);
    Text strum = text(display.values[0]);
    strum << static_cast<i32>(steps(controls.value[0], 64));
    static const char* const directions[4] = {"up", "dn", "alt", "rnd"};
    Text direction = text(display.values[1]);
    direction << directions[steps(controls.value[1], 4)];
    const i32 voicing = static_cast<i32>(steps(controls.value[2], 2 * kVoicingSteps + 1)) - kVoicingSteps;
    Text voice_text = text(display.values[2]);
    if (voicing > 0) voice_text << "+";
    voice_text << voicing;
    Text spread = text(display.values[3]);
    spread << (steps(controls.value[3], 2) ? "on" : "off");
}
}
