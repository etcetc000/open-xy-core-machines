#include "core.h"
#include "native/lay.hpp"
#include "native/ring.hpp"
#include "pick.h"
#include <opxy/q31.hpp>

// A plucked string (Karplus-Strong): a pluck circulates in a delay line one
// period long, losing a little treble and energy every pass, and a pickup at
// the pick point hears it. The OP-XY's own envelope and level shape the note;
// the controls shape the string: Bright, Pick, Damp and Spread.
namespace karplus {
namespace {
using opxy::i32;
using opxy::u32;

// Pluck samples laid per block. Laying a sample costs about a third of what a
// sounding frame of both strings does, so a block laying this many costs no
// more than one sounding. A note sounds from the block after its pluck is
// whole: 1.5 ms after it starts down to about 172 Hz, at most 12 ms for the
// lowest notes.
const u32 kPerBlock = 256;

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

// Q31 control 0..1 to a weight from low to high: low + (high - low) * c / 32767
// for c = 0..32767, without dividing (exact while (high - low) * c < 2^30 - 1).
i32 weight(i32 control, i32 low, i32 high) {
    const i32 c = control < 0 ? 0 : control >> 16;
    const i32 scaled = (high - low) * c;
    return low + ((scaled + (scaled >> 15) + 1) >> 15);
}

// A note's pluck: the string pulled aside at the pick point (a triangle
// peaking there), mixed by Bright with a burst of noise, itself low-passed by
// Bright and combed at the pick point. A soft pluck is mostly the pulled shape;
// a hard one mostly the burst. It fills the period behind the write position
// in the first string's line, laid from the bridge on over as many blocks as
// it needs; the strings sound once it is whole, and the second string reads
// its first pass from there too.
void begin(Voice& voice, u32 period, i32 bright, u32 position) {
    Pluck& p = voice.pluck;
    p.length = (period >> 8) + 1;
    p.peak = position * p.length >> 15;
    if (p.peak < 1) p.peak = 1;
    if (p.peak >= p.length) p.peak = p.length - 1;
    p.burst = weight(bright, 6554, 29491);
    p.smoothing = weight(bright, 2048, 32767);
    // The pulled shape, 0 at both ends and 12000 at the pick point, less its
    // mean so the string holds no offset, is laid already weighed by what
    // the noise leaves of it, Q8: it rises by `up` a sample to the pick point
    // and falls by `down` after it.
    const i32 keep = 32768 - p.burst;
    const u32 span = 12000u * static_cast<u32>(keep) >> 7;
    p.up = span / p.peak;
    p.down = span / (p.length - p.peak);
    p.shape = static_cast<u32>(-6000 * keep >> 7);
    p.ahead.seed = voice.seed;
    p.ahead.smooth = 0;
    p.done = 0;
}

// Lays up to kPerBlock more samples, in stretches that wrap around neither
// line: before the pick point the noise as it is, after it less the noise a
// pick's travel back, which the second line keeps meanwhile. Returns whether
// the pluck is whole.
bool lay(Voice& voice) {
    Pluck& p = voice.pluck;
    opxy::i16* const first = voice.strings[0].line;
    opxy::i16* const noisy = voice.strings[1].line;
    const u32 base = voice.write - p.length;
    u32 end = p.done + kPerBlock;
    if (end > p.length) end = p.length;
    const i32 burst = p.burst, smoothing = p.smoothing;
    Noise ahead = p.ahead;
    i32 shape = static_cast<i32>(p.shape);
    for (u32 i = p.done; i < end;) {
        const u32 at = (base + i) & kMask;
        const bool combing = i >= p.peak;
        const u32 back = (at - p.peak) & kMask;
        u32 count = (combing || end < p.peak ? end : p.peak) - i;
        if (count > kLength - at) count = kLength - at;
        if (combing && count > kLength - back) count = kLength - back;
        Laying stretch;
        stretch.laid = first + at;
        stretch.kept = noisy + at;
        stretch.before = combing ? noisy + back : nullptr;
        stretch.count = count;
        stretch.seed = ahead.seed;
        stretch.smooth = ahead.smooth;
        stretch.shape = shape;
        stretch.step = combing ? -static_cast<i32>(p.down) : static_cast<i32>(p.up);
        stretch.burst = burst;
        stretch.smoothing = smoothing;
        lay_stretch(&stretch);
        ahead.seed = stretch.seed;
        ahead.smooth = stretch.smooth;
        shape = stretch.shape;
        i += count;
    }
    p.ahead = ahead;
    p.shape = static_cast<u32>(shape);
    p.done = end;
    if (end < p.length) return false;
    voice.seed = ahead.seed;
    // Silence just before the pluck, where the second, lower string first reads.
    const u32 margin = (p.length >> 6) + 4;
    for (u32 i = 1; i <= margin && p.length + i < kLength; ++i) first[(base - i) & kMask] = 0;
    for (u32 s = 0; s < 2; ++s) voice.strings[s].previous = voice.strings[s].input = voice.strings[s].output = 0;
    return true;
}

// The allpass coefficient for each rest of 256..511 (Q8, 1 to 2 samples):
// (256 - rest) * 32768 / (256 + rest), as C divides.
const opxy::i16 kAllpass[256] = {
    0, -63, -127, -190, -254, -316, -379, -441, -504, -566, -627, -689, -750, -811, -872, -932,
    -992, -1053, -1112, -1172, -1231, -1291, -1349, -1408, -1467, -1525, -1583, -1641, -1699, -1756, -1813, -1870,
    -1927, -1984, -2040, -2096, -2152, -2208, -2263, -2319, -2374, -2429, -2484, -2538, -2593, -2647, -2701, -2755,
    -2808, -2862, -2915, -2968, -3021, -3073, -3126, -3178, -3230, -3282, -3334, -3385, -3437, -3488, -3539, -3590,
    -3640, -3691, -3741, -3791, -3841, -3891, -3941, -3990, -4039, -4088, -4137, -4186, -4235, -4283, -4332, -4380,
    -4428, -4475, -4523, -4570, -4618, -4665, -4712, -4759, -4805, -4852, -4898, -4945, -4991, -5037, -5082, -5128,
    -5173, -5219, -5264, -5309, -5354, -5398, -5443, -5487, -5532, -5576, -5620, -5664, -5707, -5751, -5794, -5838,
    -5881, -5924, -5967, -6010, -6052, -6095, -6137, -6179, -6221, -6263, -6305, -6347, -6388, -6430, -6471, -6512,
    -6553, -6594, -6635, -6675, -6716, -6756, -6797, -6837, -6877, -6917, -6956, -6996, -7036, -7075, -7114, -7153,
    -7192, -7231, -7270, -7309, -7347, -7386, -7424, -7462, -7501, -7539, -7576, -7614, -7652, -7689, -7727, -7764,
    -7801, -7839, -7875, -7912, -7949, -7986, -8022, -8059, -8095, -8131, -8167, -8203, -8239, -8275, -8311, -8347,
    -8382, -8417, -8453, -8488, -8523, -8558, -8593, -8628, -8662, -8697, -8731, -8766, -8800, -8834, -8868, -8902,
    -8936, -8970, -9004, -9037, -9071, -9104, -9138, -9171, -9204, -9237, -9270, -9303, -9336, -9368, -9401, -9433,
    -9466, -9498, -9530, -9562, -9595, -9627, -9658, -9690, -9722, -9753, -9785, -9816, -9848, -9879, -9910, -9941,
    -9972, -10003, -10034, -10065, -10096, -10126, -10157, -10187, -10217, -10248, -10278, -10308, -10338, -10368, -10398, -10428,
    -10457, -10487, -10517, -10546, -10575, -10605, -10634, -10663, -10692, -10721, -10750, -10779, -10808, -10836, -10865, -10894,
};

// A first-order allpass supplies a period's last 1..2 samples: it delays
// without dulling the tone, as linear interpolation would.
// The pickup sits at the pick point: it hears the string less the wave that
// left that point a pick's travel earlier, a comb that keeps the pick's tone
// for the whole note. Its level is brought back up for the fundamental.
Tap tap(u32 period, u32 position) {
    Tap result;
    result.delay = (period >> 8) - 1;
    const i32 rest = static_cast<i32>(period - (result.delay << 8));  // Q8, 1 to 2 samples
    result.coefficient = kAllpass[rest - 256];  // (256 - rest) * 32768 / (256 + rest)
    result.pickup = (period >> 8) * position >> 15;
    if (result.pickup < 1) result.pickup = 1;
    return result;
}

// One string through a block: each frame reads one period back, filters it,
// writes the result and hears it at the pickup. damp is the loop filter's
// weight on the previous sample (Q16, at most half): more weight loses more
// treble each pass. The block runs in stretches that do not wrap around the
// line, so the inner loop walks plain pointers. Until a string has passed
// once round (age, frames since it began to sound), it reads the pluck from
// `pluck`, the first string's line. The first string sets each output frame
// and the second adds to it, each at most a quarter scale.
template <bool add>
void run(String& string, const opxy::i16* pluck, u32 age, u32 write, const Tap& tap, i32 damp, i32 level,
         i32* out) {
    i32 input = string.input, output = string.output, previous = string.previous;
    for (u32 i = 0; i < opxy::block_frames;) {
        const u32 from = (write + i - tap.delay) & kMask, heard = (write + i - tap.pickup) & kMask,
                  to = (write + i) & kMask;
        u32 count = opxy::block_frames - i;
        if (count > kLength - from) count = kLength - from;
        if (count > kLength - heard) count = kLength - heard;
        if (count > kLength - to) count = kLength - to;
        const opxy::i16* read = string.line + from;
        const opxy::i16* earlier = string.line + heard;
        if (age + i < tap.delay) {
            if (count > tap.delay - (age + i)) count = tap.delay - (age + i);
            read = pluck + from;
        }
        if (age + i < tap.pickup) {
            if (count > tap.pickup - (age + i)) count = tap.pickup - (age + i);
            earlier = pluck + heard;
        }
        Ringing stretch;
        stretch.read = read;
        stretch.earlier = earlier;
        stretch.next = string.line + to;
        stretch.frame = out + i;
        stretch.count = count;
        stretch.input = input;
        stretch.output = output;
        stretch.previous = previous;
        stretch.coefficient = tap.coefficient;
        stretch.damp = damp;
        stretch.level = level;
        stretch.add = add;
        ring(&stretch);
        input = stretch.input;
        output = stretch.output;
        previous = stretch.previous;
        i += count;
    }
    string.input = input;
    string.output = output;
    string.previous = previous;
}
}

// Callbacks run serially per voice; owner is a stable nonzero voice ID.
// The lines need no clearing: a note reads only its pluck and the silence laid
// before it until the string has written over them.
inline void initialize(Voice& voice, Shared&, u32 owner) {
    voice.write = voice.excited = 0;
    voice.increment = 0;
    voice.seed = owner * 2654435761u | 1u;
}
inline void start(Voice& voice, Shared&, u32) { voice.excited = 0; }
inline void destroy(Voice&, Shared&, u32) {}

// Controls: Bright, Pick, Damp, Spread.
inline void render(Voice& voice, Shared&, u32, opxy::Output output, opxy::Parameters parameters,
                   opxy::Pitch pitch) {
    if (voice.excited < 2) {
        if (!voice.excited) {
            begin(voice, period_q8(pitch[0]), parameters[0], along(kPickPositions, parameters[1]));
            voice.excited = 1;
        }
        if (lay(voice)) {
            voice.excited = 2;
            voice.age = 0;
            voice.increment = 0;  // set the taps afresh for the new note's lines
        }
        for (unsigned i = 0; i < opxy::block_frames; ++i) output[i] = 0;
        return;
    }
    if (pitch[0] != voice.increment || parameters[1] != voice.controls[0] ||
        parameters[2] != voice.controls[1] || parameters[3] != voice.controls[2] || !voice.increment) {
        voice.increment = pitch[0];
        voice.controls[0] = parameters[1];
        voice.controls[1] = parameters[2];
        voice.controls[2] = parameters[3];
        const u32 period = period_q8(pitch[0]);
        const u32 position = along(kPickPositions, parameters[1]);
        voice.level = static_cast<i32>(along(kPickupLevels, parameters[1]));
        // Damp: from 1% treble loss per pass (a glassy ring) to a two-point
        // average (a dull, felt-muted string). The filter delays the loop by up
        // to half a sample, which the delay line gives back to keep the string
        // in tune.
        voice.damp = weight(parameters[2], 655, 32768);
        const u32 tuned = period - static_cast<u32>(voice.damp >> 8);
        // Spread: the second string up to about 25 cents flat (1.45%, Q16 950).
        const u32 apart = static_cast<u32>(weight(parameters[3], 0, 950));
        u32 second = tuned + ((tuned >> 6) * apart >> 10);
        if (second > (kLength - 4) << 8) second = (kLength - 4) << 8;
        voice.taps[0] = tap(tuned, position);
        voice.taps[1] = tap(second, position);
    }
    const opxy::i16* pluck = voice.strings[0].line;
    run<false>(voice.strings[0], pluck, voice.age, voice.write, voice.taps[0], voice.damp, voice.level, output);
    run<true>(voice.strings[1], pluck, voice.age, voice.write, voice.taps[1], voice.damp, voice.level, output);
    voice.write += opxy::block_frames;
    if (voice.age < kLength) voice.age += opxy::block_frames;
}
}
