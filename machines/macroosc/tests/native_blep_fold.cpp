// native/blep_square.hpp and native/fold_pair.hpp: the square routine's reciprocal edge
// position, exhaustively over its divisor range; in range the square renders the
// reference and out of range nothing; the fold pair's range check, and its state advancing.
// The routines (native/*.s) were matched against these references sample for sample on
// the BF707 functional simulator; a host build, as here, runs the references.
#include "native/blep_square.hpp"
#include "native/fold_pair.hpp"
using namespace opxy;
static u32 r = 1;
static u32 rnd() { r = r * 1664525u + 1013904223u; return r ^ (r >> 15); }
// The square routine's edge position with a fixed divisor d in 16385..32767 and
// R = 2^31 / d - 65536: t0 = (n + nh R + (nl R >> 16)) >> 15, one remainder
// correction on t0's low half, then the clamp. It must equal min(n / d, 65535).
static u32 estimate(u32 n, u32 d) {
    const u32 R = opxy::divide::quotient16(0x80000000u - (d << 16), d);
    u32 t = (n + (n >> 16) * R + (((n & 0xffffu) * R) >> 16)) >> 15;
    if (n - (t & 0xffffu) * d >= d) ++t;
    return t < 65535 ? t : 65535;
}
int main() {
    for (u32 d = 16385; d < 32768; ++d) {
        const u32 top = (d << 16) + 0xffffu < 0x80000000u ? (d << 16) + 0xffffu : 0x7fffffffu;
        for (u32 k = 0; k < 8; ++k) {
            const u32 q = k < 4 ? k * 21845u : 65532u + k - 4;
            const u32 ns[] = {q * d, q * d + d - 1, q * d + (rnd() % d)};
            for (u32 n : ns) {
                if (n > top) continue;
                const u32 t = n / d < 65535 ? n / d : 65535;
                if (estimate(n, d) != t) return 1;
            }
        }
        for (u32 i = 0; i < 64; ++i) {
            const u32 n = rnd() % (top + 1), t = n / d < 65535 ? n / d : 65535;
            if (estimate(n, d) != t) return 2;
        }
    }
    // The portable square: in range it renders the reference, out of range nothing.
    macroosc::native::BlepSquare s = {0x40000000u, 1u << 29, 0xffffffffu, 0, false}, t = s;
    i16 a[24], b[24];
    for (int i = 0; i < 24; ++i) a[i] = b[i] = static_cast<i16>(i);
    if (!macroosc::native::blep_square(s, a, 24, 21, true)) return 3;
    macroosc::native::blep_square_reference(t, b, 24, 21, true);
    for (int i = 0; i < 24; ++i) if (a[i] != b[i]) return 4;
    if (s.phase != t.phase || s.next_sample != t.next_sample || s.high != t.high) return 5;
    macroosc::native::BlepSquare wide = {0, 0x90000000u, 0, 0, false};
    if (macroosc::native::blep_square(wide, a, 8, 21, false) || wide.increment != 0x90000000u) return 6;
    macroosc::native::BlepSquare torn = {0x90000000u, 1u << 20, 0, 0, false};
    if (macroosc::native::blep_square_in_range(torn, 8, 21)) return 7;
    // The fold pair: controls in range for the routine, and its state advances.
    static u32 table[256];
    for (u32 i = 0; i < 256; ++i) table[i] = rnd();
    macroosc::native::FoldPair f = {1, 2, 1u << 28, 1u << 27, 0xffffffffu, 5, 32767 / 24, 100, 32667, 32767, -32767, 0, 32767,
                           table, table, table};
    if (!macroosc::native::fold_pair_in_range(f, 24) || macroosc::native::fold_pair_in_range(f, 25)) return 8;
    f.triangle_delta = -32768;
    if (macroosc::native::fold_pair_in_range(f, 24)) return 9;
    macroosc::native::fold_pair(f, a, 24);
    return f.sine_increment == (1u << 28) - 24 && f.triangle_increment == (1u << 27) + 24 * 5 ? 0 : 10;
}
