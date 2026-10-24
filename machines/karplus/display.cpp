#include "display.h"
#include "pick.h"

// A string between two pegs. Each note sends a small wave out both ways from
// the Pick position; the waves bounce off the pegs, which hold still, inverting
// as on a real string, and fade faster the more the string is damped. The
// waves of up to kPlucks notes ring at once and add up. Bright sets their
// height, and Detune spreads faint companion strings above and below. Display
// code only: no audio state.
namespace karplus {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;

// Round only the screen caption; the parameter keeps its full Q31 precision.
inline void amount(char* out, unsigned capacity, opxy::i32 value) {
    const unsigned p = value < 0 ? 0 : ((static_cast<unsigned>(value) >> 10) * 100 + (1u << 20)) >> 21;
    char text[4];
    unsigned n = 0;
    if (p >= 100) text[n++] = '1';
    if (p >= 10) text[n++] = static_cast<char>('0' + p / 10 % 10);
    text[n++] = static_cast<char>('0' + p % 10);
    text[n] = 0;
    opxy::draw::copy(out, capacity, text);
}

const i32 kLeft = 50, kRight = 430, kY = 116;   // the string's ends and height, pixels
const i32 kLength = kRight - kLeft;            // 380 pixels
// The string is drawn in kPieces outlines (an outline reaches at most 64 pixels
// from its centre), each overlapping its neighbours so the string never breaks.
const i32 kPieces = 4, kPiece = kLength / kPieces, kOverlap = 3;
const i32 kSpeed = 260;                        // the waves' speed, pixels per second
const i32 kWave = 14;                          // half a wave's width, pixels
const i32 kRibbon = 256;                       // the outline's way back, Q10: a quarter pixel below
const u32 kPeg = 4, kMarker = 6, kCompanion = 7, kCompanions = 4;

// A control (Q31 0..1) as 0..1024.
i32 fraction(i32 control) { return (control < 0 ? 0 : control >> 16) * 1024 / 32767; }

// One wave: a single sine cycle across 2 * kWave pixels around its centre.
// d is the distance from the centre in Q10 pixels; the result is Q10, -1..1.
i32 wave(i32 d) {
    if (d <= -kWave * unit || d >= kWave * unit) return 0;
    return sine(d / kWave * 1800 / unit);
}

// The string's displacement at x (Q10 pixels from the left peg) from a wave
// centred at c on an unbounded line. On a string fixed at both pegs, the wave
// is accompanied by mirror images, inverted at every peg and repeating every
// two lengths, which hold both ends still: y(0) = y(kLength) = 0.
i32 displacement(i32 x, i32 c) {
    const i32 period = 2 * kLength * unit;
    i32 centre = c % period;
    if (centre < 0) centre += period;
    i32 sum = 0;
    for (i32 n = -1; n <= 1; ++n) {
        const i32 at = centre + n * period;
        sum += wave(x - at) - wave(-x - at);
    }
    return sum;
}
}

inline void display_reset(DisplayState& state) {
    for (u32 p = 0; p < kPlucks; ++p) state.plucks[p].amplitude = state.plucks[p].time = state.plucks[p].pick = 0;
    state.serial = state.track = state.initialized = 0;
    state.key = 1;
}

inline void display(DisplayState& state, const Input& input, Frame& frame) {
    for (u32 i = 0; i < 4; ++i)
        amount(frame.header[i], sizeof(frame.header[i]), input.parameters[i]);
    const i32 elapsed = clamp(input.elapsed, 0, 205);  // at most 0.2 s per frame
    const i32 bright = fraction(input.parameters[0]);
    const i32 at = static_cast<i32>(along(kPickPositions, input.parameters[1]) >> 5);  // the pick point, 0..512 of 1024
    const i32 damp = fraction(input.parameters[2]), detune = fraction(input.parameters[3]);
    const i32 pick = at * kLength;  // Q10 pixels from the left peg
    // Whether the string was off rest: a frame that brings it to rest, by a reset
    // or a fade, still draws it at rest.
    bool was_moving = false;
    for (u32 p = 0; p < kPlucks; ++p) was_moving = was_moving || state.plucks[p].amplitude;
    if (!state.initialized || state.track != input.track || input.reduced_motion) {
        for (u32 p = 0; p < kPlucks; ++p) state.plucks[p].amplitude = 0;
    } else if (input.note_serial != state.serial) {
        // A new pluck takes the quietest slot: a faded one, or the softest still ringing.
        u32 slot = 0;
        for (u32 p = 1; p < kPlucks; ++p)
            if (state.plucks[p].amplitude < state.plucks[slot].amplitude) slot = p;
        state.plucks[slot].amplitude = (4 + bright * 10 / 1024) * unit;  // 4 to 14 pixels, twice that as it starts
        state.plucks[slot].time = 0;
        state.plucks[slot].pick = pick;
    }
    state.initialized = 1;
    state.track = input.track;
    state.serial = input.note_serial;
    // Each pluck settles at 0.5 (little Damp) to 2.5 (full Damp) per second, slowly
    // enough to cross the string and come back before it fades.
    const i32 rate = 512 + damp * 2048 / 1024;
    bool moving = false;
    for (u32 p = 0; p < kPlucks; ++p) {
        Pluck& pluck = state.plucks[p];
        const i32 settle = ((pluck.amplitude * rate) >> 10) * elapsed >> 10;
        pluck.amplitude -= settle > 0 ? settle : pluck.amplitude > 0 && elapsed > 0;
        if (pluck.amplitude < 0) pluck.amplitude = 0;
        pluck.time = clamp(pluck.time + elapsed, 0, 60 * unit);
        moving = moving || pluck.amplitude;
    }
    // Each outline covers its piece of the string: out along it, then back over
    // the same positions a quarter pixel lower. The two passes stay together,
    // and no point repeats, which the device's parser needs.
    for (i32 piece = 0; piece < kPieces; ++piece) {
        Frame::path_type& path = frame.paths[piece];
        const i32 start = piece ? piece * kPiece - kOverlap : 0;
        const i32 end = piece + 1 < kPieces ? (piece + 1) * kPiece + kOverlap : kLength;
        const i32 centre = (start + end) * unit / 2;  // Q10 pixels from the left peg
        for (u32 i = 0; i < 32; ++i) {
            const i32 x = start * unit + (end - start) * static_cast<i32>(i) * unit / 31;  // from the left peg
            // Every ringing pluck's two waves, out from its pick point each way.
            i32 y = 0;
            for (u32 p = 0; p < kPlucks; ++p) {
                const Pluck& pluck = state.plucks[p];
                if (!pluck.amplitude) continue;
                const i32 travel = pluck.time * kSpeed;
                y += floor_q10(pluck.amplitude *
                               (displacement(x, pluck.pick + travel) + displacement(x, pluck.pick - travel)));
            }
            Point& out = path.points[i];
            out.x = x - centre;
            out.y = clamp(y, -40 * unit, 40 * unit);
            path.points[63 - i].x = out.x;
            path.points[63 - i].y = out.y + kRibbon;
        }
        place(frame, static_cast<u32>(piece), kLeft * unit + centre, kY * unit, unit, 0, stroke);
    }
    // New geometry while the string moves and as it comes to rest; at rest it stays as it is.
    if (moving || was_moving) state.key = state.key % 0x7ffffffeu + 1;
    for (i32 piece = 0; piece < kPieces; ++piece) frame.paths[piece].key = state.key;
    place(frame, kPeg, kLeft * unit, kY * unit, unit, 1, fill_and_stroke);
    place(frame, kPeg + 1, kRight * unit, kY * unit, unit, 1, fill_and_stroke);
    place(frame, kMarker, kLeft * unit + pick, (kY + 34) * unit, unit, 1, fill_and_stroke);
    // Companion strings, spread evenly by Detune up to 20 pixels apart: the
    // inner pair lighter (palette 2), the outer pair darker (palette 3).
    for (u32 c = 0; c < kCompanions; ++c) {
        const i32 step = c < 2 ? static_cast<i32>(c) + 1 : 1 - static_cast<i32>(c);  // +1, +2, -1, -2
        if (detune)
            place(frame, kCompanion + c, (kLeft + kLength / 2) * unit, kY * unit + step * detune * 20, unit,
                  step == 1 || step == -1 ? 2 : 3, stroke);
        else
            // Below the screen: hide() parks at x 640, from which a 380-pixel
            // line would still reach back onto the 480-pixel screen.
            place(frame, kCompanion + c, hidden_x * unit, node_bottom * unit, unit, 0, keep);
    }
}
}
