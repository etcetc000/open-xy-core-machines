#include "core.h"

namespace ringmod {
namespace {
// Quarter-cycle sine, Q15: round(32767*sin(pi*i/128)), i=0..64.
constexpr opxy::i32 kSine[] = {
    0,     804,   1608,  2410,  3212,  4011,  4808,  5602,  6393,  7179,  7962,  8739,  9512,
    10278, 11039, 11793, 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868,
    19519, 20159, 20787, 21403, 22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319,
    26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571, 30852, 31113,
    31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757, 32767,
};

// DDS increments at 44.1 kHz: round(2^32 * 0.25 * 2^(14*i/64) / 44100).
constexpr opxy::i32 kFrequency[] = {
    24348,     28334,     32973,     38372,     44654,     51965,     60473,     70374,
    81896,     95305,     110908,    129067,    150198,    174789,    203407,    236710,
    275465,    320565,    373050,    434127,    505205,    587919,    684176,    796193,
    926550,    1078249,   1254785,   1460225,   1699300,   1977517,   2301286,   2678064,
    3116530,   3626783,   4220577,   4911591,   5715740,   6651549,   7740573,   9007897,
    10482714,  12198995,  14196274,  16520557,  19225382,  22373056,  26036081,  30298835,
    35259507,  41032365,  47750383,  55568308,  64666221,  75253689,  87574588,  101912725,
    118598372, 138015874, 160612503, 186908762, 217510373, 253122228, 294564629, 342792182,
    398915783,
};

// Twice the TPT low-pass coefficient, Q15: 2*g/(1+g),
// g = tan(pi*10000*(i/32)^2/44100). Used for the complementary high pass.
constexpr opxy::i32 kLowCut[] = {
    0,     46,    182,   408,   721,   1120,  1602,  2161,  2795,  3499,  4269,
    5099,  5986,  6923,  7907,  8933,  9997,  11095, 12224, 13381, 14562, 15767,
    16992, 18237, 19502, 20785, 22086, 23408, 24750, 26114, 27504, 28922, 30371,
};

opxy::u32 nonnegative(opxy::i32 value) {
    return value < 0 ? 0u : static_cast<opxy::u32>(value);
}

// Convex interpolation with a Q15 weight and exact endpoints. Unsigned
// distance spans the full Q31 range; split products fit in 32 bits.
opxy::i32 blend(opxy::i32 a, opxy::i32 b, opxy::u32 weight) {
    if (weight >= 32767)
        return b;
    const opxy::u32 start = static_cast<opxy::u32>(a), end = static_cast<opxy::u32>(b);
    const opxy::u32 distance = b >= a ? end - start : start - end;
    const opxy::u32 delta =
        (distance >> 15) * weight + (((distance & 32767) * weight + 16384) >> 15);
    return static_cast<opxy::i32>(b >= a ? start + delta : start - delta);
}

// A complementary first-order split: high = input - low. Both state updates
// are convex blends (twice-alpha < 1), even at full-scale input.
opxy::i32 lowpass(opxy::i32 input, opxy::i32& memory, opxy::u32 twice_alpha) {
    const opxy::i32 low = blend(memory, input, twice_alpha >> 1);
    const opxy::i32 next = blend(memory, input, twice_alpha);
    // Prevent a fixed-point limit cycle from leaving an oscillator audible
    // after the input stops. Move at least one LSB toward a different input.
    memory = next == memory && input != memory ? memory + (input > memory ? 1 : -1) : next;
    return low;
}

// input - low. Half-scale subtraction accommodates bipolar transients;
// saturate only if the extracted high band exceeds Q31.
opxy::i32 high_band(opxy::i32 input, opxy::i32 low) {
    if (low == 0)
        return input;
    const opxy::i32 half = (input >> 1) - (low >> 1);
    if (half > 0x3fffffff)
        return 0x7fffffff;
    if (half < -0x40000000)
        return -0x7fffffff - 1;
    return static_cast<opxy::i32>(static_cast<opxy::u32>(half) << 1);
}

opxy::i32 smooth(opxy::i32 value, opxy::i32 target) {
    const opxy::i32 difference = target - value;
    // Approximately 1.5 ms time constant; finite arrival at both endpoints.
    return value + (difference > 0 ? (difference + 63) >> 6 : -((-difference + 63) >> 6));
}

// Linearly interpolated quarter-wave table; phase wraps naturally in u32.
opxy::i32 sine(opxy::u32 phase) {
    const opxy::u32 quadrant = phase >> 30;
    opxy::u32 position = phase & 0x3fffffff;
    if (quadrant & 1)
        position = 0x40000000 - position;
    const opxy::u32 index = position >> 24;
    const opxy::i32 value =
        index == 64 ? kSine[64]
                    : kSine[index] + ((kSine[index + 1] - kSine[index]) *
                                          static_cast<opxy::i32>((position >> 9) & 32767) >>
                                      15);
    return quadrant & 2 ? -value : value;
}

// Polynomial correction over one sample on either side of a discontinuity.
// Q16 phase / width and Q15 amplitude keep all products in 32 bits.
opxy::i32 blep(opxy::u32 phase, opxy::u32 width) {
    if (!width)
        return 0;
    if (phase < width) {
        const opxy::i32 t = static_cast<opxy::i32>((phase << 15) / width);
        return 2 * t - ((t * t) >> 15) - 32768;
    }
    if (phase > 65536 - width) {
        const opxy::i32 t = static_cast<opxy::i32>(((65536 - phase) << 15) / width);
        return 32768 - 2 * t + ((t * t) >> 15);
    }
    return 0;
}

opxy::i32 square(opxy::u32 phase, opxy::u32 increment) {
    const opxy::u32 p = phase >> 16, width = increment >> 16;
    const opxy::i32 value =
        (p < 32768 ? 32767 : -32767) + blep(p, width) - blep((p + 32768) & 65535, width);
    return value > 32767 ? 32767 : value < -32767 ? -32767 : value;
}

// Signed Q31 audio times bipolar Q15 carrier. Split the magnitude so no
// 64-bit product is needed. Saturate INT_MIN * -1 at positive full scale.
opxy::i32 modulate(opxy::i32 input, opxy::i32 carrier) {
    const opxy::u32 bits = static_cast<opxy::u32>(input);
    const opxy::u32 magnitude = input < 0 ? 0u - bits : bits;
    const opxy::u32 gain = static_cast<opxy::u32>(carrier < 0 ? -carrier : carrier);
    const opxy::u32 scaled =
        gain == 32767 ? magnitude : (magnitude >> 15) * gain + (((magnitude & 32767) * gain) >> 15);
    if ((input < 0) != (carrier < 0))
        return static_cast<opxy::i32>(0u - scaled);
    return scaled > 0x7fffffff ? 0x7fffffff : static_cast<opxy::i32>(scaled);
}
} // namespace

void initialize(State& state) {
    state = State();
}

void process(State& state, opxy::Channel left, opxy::Channel right, opxy::Parameters parameters) {
    const opxy::u32 frequency = nonnegative(parameters[0]) >> 16;
    const opxy::i32 shape = static_cast<opxy::i32>(nonnegative(parameters[1]) >> 16);
    const opxy::u32 cut = nonnegative(parameters[2]) >> 16;
    const opxy::u32 f = frequency >> 9, c = cut >> 10;
    const opxy::i32 increment =
        frequency == 32767 ? kFrequency[64]
                           : blend(kFrequency[f], kFrequency[f + 1], (frequency & 511) << 6);
    const opxy::i32 coefficient =
        cut == 32767 ? kLowCut[32] : blend(kLowCut[c], kLowCut[c + 1], (cut & 1023) << 5);
    if (!state.started) {
        state.increment = increment;
        state.shape = shape;
        state.low_cut = coefficient;
        state.started = 1;
    }
    for (unsigned i = 0; i < opxy::block_frames; ++i) {
        state.increment = smooth(state.increment, increment);
        state.shape = smooth(state.shape, shape);
        state.low_cut = smooth(state.low_cut, coefficient);
        opxy::i32 a = left[i], b = right[i];
        if (state.low_cut) {
            a = high_band(a, lowpass(a, state.low_left, state.low_cut));
            b = high_band(b, lowpass(b, state.low_right, state.low_cut));
        } else {
            state.low_left = state.low_right = 0;
        }
        const opxy::i32 carrier =
            blend(sine(state.phase), square(state.phase, state.increment), state.shape);
        left[i] = modulate(a, carrier);
        right[i] = modulate(b, carrier);
        state.phase += static_cast<opxy::u32>(state.increment);
    }
    // Dry belongs to the stock source mixer, via machine.json dryParameter.
    // This callback produces only the wet return and never reads parameter 3.
}
} // namespace ringmod
