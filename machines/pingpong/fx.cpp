#include "core.h"
#include <opxy/q31.hpp>

// A ping-pong delay: the input repeats on the left, then on the right, then on
// the left again, each repeat a little quieter and darker. A send effect: it
// returns only the repeats; Dry keeps the source in the stock mixer.
namespace pingpong {
namespace {
using opxy::i32;
using opxy::u32;
const i32 kMaximum = opxy::q31::maximum, kMinimum = -kMaximum - 1;
// Time from 10 ms to 1 s, evenly in ratio: 441 * 100^(k/16) samples, Q8.
const i32 kDelay[17] = {112896,  150549,  200761,  267719,  357008,  476078,  634861,  846601, 1128960,
                        1505492, 2007606, 2677186, 3570085, 4760785, 6348609, 8466006, 11289600};
// The longest delay reads two samples back from a full line.
const i32 kLongest = static_cast<i32>((kLine - 2) << 8);
const i32 kMostFeedback = 2040109466;  // 0.95, Q31
const i32 kDarkest = 107374182;        // tone coefficient 0.05, Q31

i32 add(i32 a, i32 b) {  // saturating
    if (b > 0 && a > kMaximum - b) return kMaximum;
    if (b < 0 && a < kMinimum - b) return kMinimum;
    return a + b;
}

i32 control(i32 word) { return word < 0 ? 0 : word; }

// The time control (Q31) as a delay, Q8 samples, between the table's 17 points.
i32 delay_for(i32 word) {
    const u32 c = static_cast<u32>(control(word)) >> 15;  // 0..65535
    const u32 at = c >> 12, part = c & 4095;
    // A step is under 2^22 and part under 2^12: a sixteenth of their product fits 32 bits.
    const i32 value = at >= 16 ? kDelay[16] : kDelay[at] + (kDelay[at + 1] - kDelay[at]) / 16 * static_cast<i32>(part) / 256;
    return value < kLongest ? value : kLongest;
}

// A sample `age` positions back (1 is the last written), or silence where this
// effect has not written since initialize: the buffer is never cleared.
i32 sample(const i32* line, const State& state, u32 age) {
    if (age > state.written) return 0;
    return line[state.write >= age ? state.write - age : state.write + kLine - age];
}

// The line `delay` (Q8) samples back, between its two nearest samples.
i32 tap(const i32* line, const State& state, i32 delay) {
    const u32 whole = static_cast<u32>(delay) >> 8, part = static_cast<u32>(delay) & 255;
    const i32 near = sample(line, state, whole), far = sample(line, state, whole + 1);
    return near / 256 * static_cast<i32>(256 - part) + far / 256 * static_cast<i32>(part);
}

// About a 23 ms glide, so a new time slides like tape instead of clicking.
i32 glide(i32 value, i32 target) {
    const i32 difference = target - value, step = difference / 1024;
    return value + (step ? step : (difference > 0) - (difference < 0));
}
}

void initialize(State& state) {
    // An engine reset runs this again: the lines' old contents are now unwritten.
    state = State();
}

void process(State& state, opxy::Buffer buffer, opxy::Channel left, opxy::Channel right,
             opxy::Parameters parameters) {
    if (!buffer.words || buffer.count < kBufferWords) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) left[i] = right[i] = 0;
        return;
    }
    i32* const left_line = buffer.words;
    i32* const right_line = buffer.words + kLine;
    const i32 delay = delay_for(parameters[0]);
    const i32 feedback = opxy::q31::multiply(control(parameters[1]), kMostFeedback);
    const i32 tone = add(kDarkest, opxy::q31::multiply(control(parameters[2]), kMaximum - kDarkest));
    if (!state.delay) state.delay = delay;  // start at the set time, not gliding from zero
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        state.delay = glide(state.delay, delay);
        state.feedback = glide(state.feedback, feedback);
        state.tone = glide(state.tone, tone);
        const i32 out_left = tap(left_line, state, state.delay);
        const i32 out_right = tap(right_line, state, state.delay);
        // Each repeat crosses to the other side, damped by Tone and scaled by Feedback.
        state.low_left = add(state.low_left, opxy::q31::multiply(add(out_left, -state.low_left), state.tone));
        state.low_right = add(state.low_right, opxy::q31::multiply(add(out_right, -state.low_right), state.tone));
        const i32 input = add(opxy::q31::half(left[i]), opxy::q31::half(right[i]));
        left_line[state.write] = add(input, opxy::q31::multiply(state.low_right, state.feedback));
        right_line[state.write] = opxy::q31::multiply(state.low_left, state.feedback);
        state.write = state.write + 1 == kLine ? 0 : state.write + 1;
        if (state.written < kLine) ++state.written;
        left[i] = out_left;
        right[i] = out_right;
    }
    // Dry belongs to the stock source mixer, via machine.json dryParameter.
}
}
