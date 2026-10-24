#include "core.h"
#include <opxy/q31.hpp>
#include "tables.h"

namespace fm2op {
namespace {

using opxy::q31::multiply;
using opxy::q31::half;

// Output level, Q31: one eighth of full-scale sine (-18 dB). Stock volume
// readouts then match stock instruments' loudness; the declared output peak
// in definition.json is this level.
const i32 FM2_OUTPUT_LEVEL = 0x10000000;

inline i32 nonnegative(i32 value) { return value < 0 ? 0 : value; }

inline i32 sine(u32 phase) {
    const u32 index = phase >> 22;
    const i32 fraction = static_cast<i32>((phase & 0x3fffffu) << 9);
    return fm2op_sine_table[index] + multiply(
        fm2op_sine_table[index + 1] - fm2op_sine_table[index], fraction);
}

struct Targets {
    i32 ratio, amount, feedback, decay;
    explicit Targets(const i32* parameters) {
        const u32 position = static_cast<u32>(
            multiply(nonnegative(parameters[0]), FM2_RATIO_POSITION_SCALE));
        const u32 index = position >> 26;
        ratio = fm2op_ratios[index] + multiply(fm2op_ratios[index + 1] - fm2op_ratios[index],
            static_cast<i32>((position & 0x3ffffffu) << 5));
        const i32 a = nonnegative(parameters[1]);
        const i32 f = nonnegative(parameters[2]);
        amount = multiply(multiply(a, a), FM2_AMOUNT_SCALE);
        feedback = multiply(multiply(f, f), FM2_FEEDBACK_SCALE);
        const u32 d = static_cast<u32>(nonnegative(parameters[3]));
        const u32 di = d >> 23;
        decay = fm2op_decay[di] + multiply(fm2op_decay[di + 1] - fm2op_decay[di],
            static_cast<i32>((d & 0x7fffffu) << 8));
    }
};

inline void smooth(i32& current, i32 target) {
    current += multiply(target - current, FM2_SMOOTH);
}
}

// Each note starts from silence: phases, feedback history and brightness reset.
inline void start(Voice& voice, Shared&, u32) {
    voice.carrier_phase = voice.modulator_phase = 0;
    voice.last_modulator = voice.previous_modulator = 0;
    voice.brightness = opxy::q31::maximum;
    voice.initialized = 0;
    voice.ratio = voice.amount = voice.feedback = voice.decay = 0;
}
inline void initialize(Voice& voice, Shared& shared, u32 owner) { start(voice, shared, owner); }
inline void destroy(Voice&, Shared&, u32) {}

inline void render(Voice& voice, Shared&, u32, opxy::Output output,
                   opxy::Parameters parameters, opxy::Pitch pitch) {
    const Targets target(parameters);
    if (!voice.initialized) {
        voice.ratio = target.ratio;
        voice.amount = target.amount;
        voice.feedback = target.feedback;
        voice.decay = target.decay;
        voice.initialized = 1;
    }
    for (unsigned i = 0; i != opxy::block_frames; ++i) {
        smooth(voice.ratio, target.ratio);
        smooth(voice.amount, target.amount);
        smooth(voice.feedback, target.feedback);
        smooth(voice.decay, target.decay);
        const i32 history = half(voice.last_modulator) + half(voice.previous_modulator);
        const u32 feedback = static_cast<u32>(multiply(history, voice.feedback)) << 1;
        const i32 modulator = sine(voice.modulator_phase + feedback);
        voice.previous_modulator = voice.last_modulator;
        voice.last_modulator = modulator;
        const u32 offset = static_cast<u32>(
            multiply(multiply(modulator, voice.brightness), voice.amount)) << 3;
        output[i] = multiply(sine(voice.carrier_phase + offset), FM2_OUTPUT_LEVEL);
        voice.brightness = multiply(voice.brightness, voice.decay);
        const u32 increment = pitch[i];
        voice.carrier_phase += increment;
        voice.modulator_phase += static_cast<u32>(
            multiply(static_cast<i32>(increment >> 1), voice.ratio)) << 6;
    }
}

} // namespace fm2op
