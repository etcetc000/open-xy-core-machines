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
// Stock delay's limits: at least 512 samples, and the longest delay reads two
// samples back from a full line.
const i32 kShortest = 512 << 8;
const i32 kLongest = static_cast<i32>((kLine - 2) << 8);
// A sixteenth of a quarter note at 1 BPM, in samples times ten: 44100 * 60 * 10 / 16.
const u32 kSixteenthAtOneTenthBpm = 1653750;
const u32 kNoTempo = 1200;      // 120 BPM, tenths
const i32 kFineSamples = 2205;  // Fine moves the delay 50 ms either way, like stock's
const i32 kMostFeedback = 2040109466;  // 0.95, Q31
const i32 kDarkest = 107374182;        // tone coefficient 0.05, Q31
const i32 kTone = 1503238554;          // 0.7 of the way from darkest to brightest

i32 add(i32 a, i32 b) {  // saturating
    if (b > 0 && a > kMaximum - b) return kMaximum;
    if (b < 0 && a < kMinimum - b) return kMinimum;
    return a + b;
}

i32 control(i32 word) { return word < 0 ? 0 : word; }

// Size picks one of stock delay's note lengths at the current tempo, and Fine
// moves it 50 ms either way: a delay in Q8 samples.
i32 delay_for(i32 size, i32 fine, u32 bpm_x10) {
    const u32 bpm = bpm_x10 ? bpm_x10 : kNoTempo;
    const u32 length = opxy::division_sixteenths[opxy::division(size)] * kSixteenthAtOneTenthBpm;
    const i32 whole = static_cast<i32>(((length / bpm) << 8) + ((length % bpm) << 8) / bpm);
    const i32 offset = (static_cast<i32>(static_cast<u32>(control(fine)) >> 15) - 32768) * kFineSamples / 128;
    const i32 value = whole + offset;
    return value < kShortest ? kShortest : value < kLongest ? value : kLongest;
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

void process(State& state, opxy::Buffer buffer, const opxy::Transport& transport, opxy::Channel left,
             opxy::Channel right, opxy::Parameters parameters) {
    if (!buffer.words || buffer.count < kBufferWords) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) left[i] = right[i] = 0;
        return;
    }
    i32* const left_line = buffer.words;
    i32* const right_line = buffer.words + kLine;
    const i32 delay = delay_for(parameters[0], parameters[1], transport.bpm_x10);
    const i32 feedback = opxy::q31::multiply(control(parameters[2]), kMostFeedback);
    const i32 tone = add(kDarkest, opxy::q31::multiply(kTone, kMaximum - kDarkest));
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
