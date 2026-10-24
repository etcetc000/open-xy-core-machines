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
                Input input, std::vector<double>& left, std::vector<double>& right) {
    opxy::i32 l[opxy::block_frames], r[opxy::block_frames];
    for (unsigned b = 0; b < blocks; ++b) {
        for (unsigned i = 0; i < opxy::block_frames; ++i) l[i] = r[i] = input(b * opxy::block_frames + i);
        pingpong::process(state, buffer, l, r, parameters);
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

int main() {
    const opxy::Buffer buffer = {words, pingpong::kBufferWords};
    // Time 0.5 is 100 ms (4410 samples); bright tone, so repeats keep their shape.
    const opxy::i32 parameters[8] = {0x40000000, 0x7fffffff / 2, 0x7fffffff, 0x7fffffff};
    const size_t d = 4410;
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
    check("the second repeat is on the right, scaled by Feedback", std::fabs(second - 0.5 * 0.475) < 0.01);
    check("the third is back on the left", std::fabs(third - 0.5 * 0.475 * 0.475) < 0.01);
    check("no repeat on the other side", peak(right, d - 2, d + 2) == 0 && peak(left, 2 * d - 2, 2 * d + 2) == 0);

    // Outside a send there is no buffer: silence, and nothing written anywhere.
    pingpong::initialize(state);
    std::vector<double> l2, r2;
    run(state, opxy::Buffer{nullptr, 0}, parameters, 10, [](unsigned) { return 0x40000000; }, l2, r2);
    check("silent without a buffer", peak(l2, 0, l2.size()) == 0 && peak(r2, 0, r2.size()) == 0);

    // Full Feedback and a loud burst still die away once the input stops.
    const opxy::i32 loud[8] = {0, 0x7fffffff, 0x7fffffff, 0x7fffffff};  // shortest time, most feedback
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
