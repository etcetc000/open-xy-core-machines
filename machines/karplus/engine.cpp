#include "core.h"
#include "pick.h"
#include <opxy/q31.hpp>

// A plucked string (Karplus-Strong): a pluck circulates in a delay line one
// period long, losing a little treble and energy every pass, and a pickup at
// the pick point hears it. The OP-XY's own envelope and level shape the note;
// the controls shape the string: Bright, Pick, Damp and Detune.
namespace karplus {
namespace {
using opxy::i32;
using opxy::u32;

// Loop gain per pass: a long natural ring, which the amp envelope cuts short.
const i32 kGain = 32735;  // Q15, 0.999

// One period, in samples with 8 fractional bits, for a phase increment
// (2^32 per cycle), within the delay line.
u32 period_q8(u32 increment) {
    const u32 longest = (kLength - 4) << 8;
    if (increment < 2) return longest;
    const u32 whole = 0xffffffffu / increment;
    if (whole >= kLength - 4) return longest;
    const u32 rest = 0xffffffffu - whole * increment;
    u32 fraction = rest / ((increment >> 8) + 1);
    if (fraction > 255) fraction = 255;
    return (whole << 8) + fraction;
}

i32 noise(u32& seed) {
    seed = seed * 1664525u + 1013904223u;
    return static_cast<i32>(seed >> 17) - 16384;  // Q15, half scale
}

i32 clamp15(i32 value) {
    return value > 16383 ? 16383 : value < -16384 ? -16384 : value;
}

// Q31 control 0..1 to a weight from low to high.
i32 weight(i32 control, i32 low, i32 high) {
    const i32 c = control < 0 ? 0 : control >> 16;
    return low + (high - low) * c / 32767;
}

// Fills one period with the pluck: the string pulled aside at the pick point
// (a triangle peaking there), mixed by Bright with a burst of noise, itself
// low-passed by Bright and combed at the pick point. A soft pluck is mostly the
// pulled shape; a hard one mostly the burst. Both strings start from it.
void excite(Voice& voice, u32 period, i32 bright, u32 position) {
    const u32 length = (period >> 8) + 1;
    u32 peak = position * length >> 15;  // the pick point, samples from the bridge
    if (peak < 1) peak = 1;
    if (peak >= length) peak = length - 1;
    const i32 burst = weight(bright, 6554, 29491);  // the noise's share, Q15
    const i32 smoothing = weight(bright, 2048, 32767);
    opxy::i16* line = voice.strings[0].line;
    i32 smooth = 0;
    for (u32 i = 0; i < length; ++i) {
        smooth += (noise(voice.seed) - smooth) * smoothing >> 15;
        line[(voice.write - length + i) & kMask] = static_cast<opxy::i16>(smooth);
    }
    for (u32 i = length; i-- > peak;) {
        const u32 at = (voice.write - length + i) & kMask, before = (at - peak) & kMask;
        line[at] = static_cast<opxy::i16>(clamp15(line[at] - line[before]));
    }
    // The pulled shape, 0 at both ends and 12000 at the pick point, less its
    // mean so the string holds no offset.
    for (u32 i = 0; i < length; ++i) {
        const i32 shape = i < peak ? static_cast<i32>(i * 12000 / peak)
                                   : static_cast<i32>((length - i) * 12000 / (length - peak));
        const u32 at = (voice.write - length + i) & kMask;
        line[at] = static_cast<opxy::i16>(clamp15(((shape - 6000) * (32768 - burst) + line[at] * burst) / 32768));
    }
    for (u32 i = 0; i < kLength; ++i) voice.strings[1].line[i] = line[i];
    for (u32 s = 0; s < 2; ++s) voice.strings[s].previous = voice.strings[s].input = voice.strings[s].output = 0;
}

// A first-order allpass supplies a period's last 1..2 samples: it delays
// without dulling the tone, as linear interpolation would. Its coefficient
// needs a division, so it is set once per block, while the pitch holds.
// The pickup sits at the pick point: it hears the string less the wave that
// left that point a pick's travel earlier, a comb that keeps the pick's tone
// for the whole note. Its level is brought back up for the fundamental.
struct Tap {
    u32 delay;        // whole samples read back
    i32 coefficient;  // Q15, (1 - d) / (1 + d) for the remaining d samples
    u32 pickup;       // samples between the string and the pickup's second reading
};
Tap tap(u32 period, u32 position) {
    Tap result;
    result.delay = (period >> 8) - 1;
    const i32 rest = static_cast<i32>(period - (result.delay << 8));  // Q8, 1 to 2 samples
    result.coefficient = (256 - rest) * 32768 / (256 + rest);
    result.pickup = (period >> 8) * position >> 15;
    if (result.pickup < 1) result.pickup = 1;
    return result;
}

i32 pickup(const String& string, u32 write, Tap tap, i32 latest, u32 level) {
    const i32 earlier = string.line[(write - tap.pickup) & kMask];
    return clamp15((latest - earlier) * static_cast<i32>(level) / 4096);
}

// One pass of a string: read one period back, filter, and write the result.
// damp is the loop filter's weight on the previous sample (Q16, at most
// half): more weight loses more treble each pass. Dividing by powers of two
// rounds toward zero, so the string dies away instead of settling at -1.
i32 step(String& string, u32 write, Tap tap, i32 damp) {
    const i32 x = string.line[(write - tap.delay) & kMask];
    const i32 sample = string.input + tap.coefficient * (x - string.output) / 32768;
    string.input = x;
    string.output = sample;
    const i32 filtered = sample + (string.previous - sample) * damp / 65536;
    string.previous = sample;
    const i32 next = clamp15(filtered * kGain / 32768);
    string.line[write & kMask] = static_cast<opxy::i16>(next);
    return next;
}
}

// Callbacks run serially per voice; owner is a stable nonzero voice ID.
inline void initialize(Voice& voice, Shared&, u32 owner) {
    for (u32 s = 0; s < 2; ++s) {
        for (u32 i = 0; i < kLength; ++i) voice.strings[s].line[i] = 0;
        voice.strings[s].previous = 0;
    }
    voice.write = voice.excited = 0;
    voice.seed = owner * 2654435761u | 1u;
}
inline void start(Voice& voice, Shared&, u32) { voice.excited = 0; }
inline void destroy(Voice&, Shared&, u32) {}

// Controls: Bright, Pick, Damp, Detune.
inline void render(Voice& voice, Shared&, u32, opxy::Output output, opxy::Parameters parameters,
                   opxy::Pitch pitch) {
    const u32 period = period_q8(pitch[0]);
    const u32 position = along(kPickPositions, parameters[1]), level = along(kPickupLevels, parameters[1]);
    if (!voice.excited) {
        excite(voice, period, parameters[0], position);
        voice.excited = 1;
    }
    // Damp: from 1% treble loss per pass (a glassy ring) to a two-point average
    // (a dull, felt-muted string). The filter delays the loop by up to half a
    // sample, which the delay line gives back to keep the string in tune.
    const i32 damp = weight(parameters[2], 655, 32768);
    const u32 tuned = period - static_cast<u32>(damp >> 8);
    // Detune: the second string up to about 25 cents flat (1.45%, Q16 950).
    const u32 apart = static_cast<u32>(weight(parameters[3], 0, 950));
    u32 second = tuned + ((tuned >> 6) * apart >> 10);
    if (second > (kLength - 4) << 8) second = (kLength - 4) << 8;
    const Tap taps[2] = {tap(tuned, position), tap(second, position)};
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        const i32 first = pickup(voice.strings[0], voice.write, taps[0],
                                 step(voice.strings[0], voice.write, taps[0], damp), level);
        const i32 other = pickup(voice.strings[1], voice.write, taps[1],
                                 step(voice.strings[1], voice.write, taps[1], damp), level);
        ++voice.write;
        output[i] = (first + other) / 2 * 65536;  // at most half scale
    }
}
}
