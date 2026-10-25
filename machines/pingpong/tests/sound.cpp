// Sound checks for Ping Pong, compiled for the host and run by
// python sdk.py test --machine <folder>.
#include "../fx.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int failures = 0;
static void check(const char* what, bool ok) {
    if (!ok) { std::printf("FAIL %s\n", what); ++failures; }
}

static opxy::i32 words[pingpong::kBufferWords];

// Runs `blocks` blocks; input(n) is the sample n on both sides.
template <class Input>
static void run(pingpong::State& state, opxy::Buffer buffer, const opxy::i32* parameters, unsigned blocks,
                Input input, std::vector<double>& left, std::vector<double>& right,
                opxy::Transport transport = opxy::Transport{1500, 0, true}) {
    opxy::i32 l[opxy::block_frames], r[opxy::block_frames];
    for (unsigned b = 0; b < blocks; ++b) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) l[i] = r[i] = input(b * opxy::block_frames + i);
        pingpong::process(state, buffer, transport, l, r, parameters);
        for (unsigned i = 0; i < opxy::block_frames; ++i) {
            left.push_back(l[i] / 2147483648.0);
            right.push_back(r[i] / 2147483648.0);
        }
    }
}

// The largest magnitude in [from, to).
static double peak(const std::vector<double>& x, size_t from, size_t to) {
    double m = 0;
    for (size_t i = from; i < to && i < x.size(); ++i) m = std::fmax(m, std::fabs(x[i]));
    return m;
}

// The first sample on the left that is not silent: where the first repeat lands.
static size_t first_repeat(const opxy::i32* parameters, opxy::Transport transport) {
    for (opxy::u32 i = 0; i < pingpong::kBufferWords; ++i) words[i] = 0;
    pingpong::State state;
    pingpong::initialize(state);
    std::vector<double> left, right;
    run(state, opxy::Buffer{words, pingpong::kBufferWords}, parameters, 760,
        [](unsigned n) { return n == 0 ? 0x40000000 : 0; }, left, right, transport);
    for (size_t i = 0; i < left.size(); ++i)
        if (left[i] != 0) return i;
    return 0;
}

// Size word for stock delay's note length `index` (0 = 1/32 .. 7 = 1/2).
static opxy::i32 size(unsigned index) { return static_cast<opxy::i32>(index * 0x10000000u + 0x08000000u); }

int main() {
    const opxy::Buffer buffer = {words, pingpong::kBufferWords};
    // At 150 BPM a sixteenth (Size 2) is 100 ms, 4410 samples; Fine centred.
    const opxy::i32 parameters[8] = {size(2), 0x40000000, 0x7fffffff / 2, 0x7fffffff};
    const size_t d = 4410;
    const double tone = 0.05 + 0.7 * 0.95;  // each crossing's one-pole filter, first sample
    // Stale buffer: a previous effect left loud data in the scratch memory.
    for (opxy::u32 i = 0; i < pingpong::kBufferWords; ++i) words[i] = (i & 1) ? 0x60000000 : -0x60000000;
    pingpong::State state;
    pingpong::initialize(state);
    std::vector<double> left, right;
    run(state, buffer, parameters, 400, [](unsigned n) { return n == 0 ? 0x40000000 : 0; }, left, right);
    check("nothing before the first repeat, whatever the buffer held", peak(left, 0, d - 2) == 0 && peak(right, 0, 2 * d - 2) == 0);
    // An impulse of 0.5 on both sides is 0.5 into the left line; repeats alternate sides.
    const double first = peak(left, d - 2, d + 2), second = peak(right, 2 * d - 2, 2 * d + 2);
    const double third = peak(left, 3 * d - 2, 3 * d + 2);
    check("the first repeat is on the left at the set time", std::fabs(first - 0.5) < 0.01);
    check("the second repeat is on the right, scaled by Feedback", std::fabs(second - 0.5 * 0.475 * tone) < 0.01);
    check("the third is back on the left", std::fabs(third - 0.5 * 0.475 * 0.475 * tone * tone) < 0.01);
    check("no repeat on the other side", peak(right, d - 2, d + 2) == 0 && peak(left, 2 * d - 2, 2 * d + 2) == 0);

    // Size follows stock delay's note lengths at the tempo; Fine adds up to 50 ms.
    const double beats[8] = {0.125, 0.1875, 0.25, 0.375, 0.5, 0.75, 1.0, 2.0};
    for (unsigned i = 0; i < 7; ++i) {  // 1/2 at 120 BPM is longer than a line
        const opxy::i32 p[8] = {size(i), 0x40000000, 0, 0x7fffffff};
        const double expected = beats[i] * 44100 * 60 / 120;
        check("each Size is stock delay's note length", std::fabs(first_repeat(p, {1200, 0, true}) - expected) <= 1.5);
    }
    const opxy::i32 later[8] = {size(4), 0x7fffffff, 0, 0x7fffffff}, earlier[8] = {size(4), 0, 0, 0x7fffffff};
    check("Fine adds 50 ms", std::fabs(first_repeat(later, {1200, 0, true}) - (11025.0 + 2205)) <= 1.5);
    check("Fine takes 50 ms", std::fabs(first_repeat(earlier, {1200, 0, true}) - (11025.0 - 2205)) <= 1.5);
    check("no tempo plays as 120 BPM", first_repeat(later, {0, 0, false}) == first_repeat(later, {1200, 0, true}));
    const opxy::i32 longest[8] = {size(7), 0x7fffffff, 0, 0x7fffffff};
    check("a half note at a slow tempo stops at the line's length",
          first_repeat(longest, {200, 0, false}) == pingpong::kLine - 2);

    // Outside a send there is no buffer: silence, and nothing written anywhere.
    pingpong::initialize(state);
    std::vector<double> l2, r2;
    run(state, opxy::Buffer{nullptr, 0}, parameters, 10, [](unsigned) { return 0x40000000; }, l2, r2);
    check("silent without a buffer", peak(l2, 0, l2.size()) == 0 && peak(r2, 0, r2.size()) == 0);

    // Full Feedback and a loud burst still die away once the input stops.
    const opxy::i32 loud[8] = {size(0), 0x40000000, 0x7fffffff, 0x7fffffff};  // shortest Size, most feedback
    pingpong::initialize(state);
    std::vector<double> l3, r3;
    run(state, buffer, loud, 690 * 6, [](unsigned n) {
        return n < 4410 ? static_cast<opxy::i32>((n * 2654435761u) & 0x7fffffff) - 0x40000000 : 0;
    }, l3, r3);
    check("repeats die away at full Feedback", peak(l3, l3.size() - 4410, l3.size()) < 1e-3 &&
                                               peak(r3, r3.size() - 4410, r3.size()) < 1e-3);
    if (failures) return 1;
    std::puts("pingpong ok");
    return 0;
}
