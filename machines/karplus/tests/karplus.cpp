// Sound checks for Karplus, compiled for the host and run by
//   python <open-xy>/sdk.py test --machine .
#include "../engine.cpp"
#include <cmath>
#include <cstdio>
#include <initializer_list>

namespace {
int failures = 0;
void check(const char* what, bool ok) {
    if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
}

const unsigned kBlocks = 690;  // one second
// Renders one note for a second.
void play(karplus::Voice& voice, double hz, const opxy::i32* parameters, opxy::i32* out) {
    karplus::Shared shared;
    karplus::initialize(voice, shared, 1);
    karplus::start(voice, shared, 1);
    opxy::u32 pitch[opxy::block_frames];
    for (unsigned i = 0; i < opxy::block_frames; ++i) pitch[i] = static_cast<opxy::u32>(hz / 44100.0 * 4294967296.0);
    for (unsigned b = 0; b < kBlocks; ++b) karplus::render(voice, shared, 1, out + b * opxy::block_frames, parameters, pitch);
}

double energy(const opxy::i32* out, unsigned from, unsigned count) {
    double sum = 0;
    for (unsigned i = from; i < from + count; ++i) sum += static_cast<double>(out[i]) * out[i];
    return sum / count;
}

// The shortest lag between 0.5 ms and 50 ms where the signal matches itself
// almost as well as it does anywhere: one period, not a multiple of it.
double period(const opxy::i32* out, unsigned from) {
    static double score[2205];
    double best = 0;
    for (unsigned lag = 22; lag < 2205; ++lag) {
        double sum = 0;
        for (unsigned i = from; i < from + 4096; ++i) sum += static_cast<double>(out[i]) * out[i + lag];
        score[lag] = sum;
        if (sum > best) best = sum;
    }
    for (unsigned lag = 23; lag + 1 < 2205; ++lag)
        if (score[lag] >= 0.9 * best && score[lag] >= score[lag - 1] && score[lag] >= score[lag + 1]) {
            // Parabolic interpolation around the peak, for fractional periods.
            const double a = score[lag - 1], b = score[lag], c = score[lag + 1];
            const double d = a - 2 * b + c;
            return lag + (d ? 0.5 * (a - c) / d : 0);
        }
    return 0;
}

bool bounded(const opxy::i32* out) {
    for (unsigned i = 0; i < kBlocks * opxy::block_frames; ++i)
        if (out[i] < -1073741824 || out[i] > 1073741824) return false;
    return true;
}

// Energy of harmonic k of hz over a tenth of a second from `from`.
double harmonic(const opxy::i32* out, unsigned from, double hz, unsigned k) {
    double re = 0, im = 0;
    for (unsigned i = 0; i < 4410; ++i) {
        const double phase = 6.283185307179586 * hz * k * i / 44100.0;
        re += out[from + i] * std::cos(phase);
        im += out[from + i] * std::sin(phase);
    }
    return re * re + im * im;
}

// Even harmonics' energy over odd ones', harmonics 1 to 8, a fifth of a second in.
double even_to_odd(const opxy::i32* out, double hz) {
    double even = 0, odd = 0;
    for (unsigned k = 1; k <= 8; ++k) (k % 2 ? odd : even) += harmonic(out, 8820, hz, k);
    return even / odd;
}

bool one_string(const karplus::Voice& voice) {
    for (unsigned i = 0; i < karplus::kLength; ++i)
        if (voice.strings[0].line[i] != voice.strings[1].line[i]) return false;
    return true;
}
}

int main() {
    static karplus::Voice voice;
    static opxy::i32 out[kBlocks * opxy::block_frames];
    const opxy::i32 kHalf = 0x40000000, kFull = 0x7fffffff;
    // Bright, Pick, Damp, Spread.
    for (opxy::i32 damp : {0, kHalf, kFull}) {
        const opxy::i32 parameters[8] = {kHalf, kHalf / 2, damp, 0};
        for (double hz : {110.0, 220.0, 440.0, 880.0}) {
            play(voice, hz, parameters, out);
            const double found = period(out, 2048), expected = 44100.0 / hz;
            check("in tune within 1% at every Damp", found > expected * 0.99 && found < expected * 1.01);
            check("rings on, then fades", energy(out, 30000, 4096) < energy(out, 1024, 4096) &&
                                          energy(out, 30000, 4096) > 0);
            check("within the declared peak", bounded(out));
        }
    }
    // Damp darkens: a tenth of a second in, the felt string's waveform changes
    // less from sample to sample, for its energy, than the glassy string's.
    auto roughness = [&](const opxy::i32* p) {
        play(voice, 220.0, p, out);
        double change = 0;
        for (unsigned i = 4410; i < 8506; ++i)
            change += static_cast<double>(out[i] - out[i - 1]) * (out[i] - out[i - 1]);
        return change / 4096 / energy(out, 4410, 4096);
    };
    const opxy::i32 glassy[8] = {kFull, kHalf / 4, 0, 0}, felt[8] = {kFull, kHalf / 4, kFull, 0};
    check("Damp darkens the ring", roughness(felt) < 0.5 * roughness(glassy));
    // Pick: plucked and heard at the middle, the string loses its even
    // harmonics for the whole note; at the bridge it keeps them.
    const opxy::i32 middle[8] = {kHalf, kFull, kHalf / 4, 0}, bridge[8] = {kHalf, 0, kHalf / 4, 0};
    play(voice, 110.0, middle, out);
    const double hollow = even_to_odd(out, 110.0);
    check("in tune plucked at the middle", period(out, 2048) > 44100.0 / 110 * 0.99 && period(out, 2048) < 44100.0 / 110 * 1.01);
    play(voice, 110.0, bridge, out);
    const double full = even_to_odd(out, 110.0);
    check("in tune plucked at the bridge", period(out, 2048) > 44100.0 / 110 * 0.99 && period(out, 2048) < 44100.0 / 110 * 1.01);
    check("the middle sounds hollow", hollow < 0.05);
    check("the bridge keeps its even harmonics", full > 0.3);
    check("plucked at the bridge within the declared peak", bounded(out));
    // Spread separates the two strings; without it they stay one.
    const opxy::i32 single[8] = {kHalf, kHalf / 2, kHalf / 4, 0}, doubled[8] = {kHalf, kHalf / 2, kHalf / 4, kFull};
    play(voice, 220.0, single, out);
    check("no Spread keeps one string", one_string(voice));
    play(voice, 220.0, doubled, out);
    check("Spread separates the strings", !one_string(voice));
    check("spread within the declared peak", bounded(out));
    if (failures) std::printf("%d failures\n", failures);
    return failures ? 1 : 0;
}
