/* macroosc::native::wave_quad on the device: native/wave_quad.hpp
 * holds the definition and the portable reference this matches sample for sample.
 *
 * void macroosc_native_wave_quad(WaveQuad* s, i16* out, unsigned size), balance 1..65535
 * (the header checks before calling), any size.
 *
 * One zero-overhead hardware loop per sample, 58 instructions: each stream
 * takes 14, with every load and store in a parallel slot. The phases stay in
 * the struct. Each stream steps the next stream's phase and moves its table
 * index to P1 early, so the Dreg-to-Preg move is four instructions ahead of
 * the address (a 1-cycle hardware stall, not 4); stream 0's index and phase
 * store come from the previous sample, and the sample's sum is stored last.
 * Exactness, per stream:
 *   - A read keeps 16 bits: value + floor(slope * f / 2^16) mod 2^16. The 32-bit
 *     product slope * f wraps, but its bits 16..31 are those of the true product,
 *     and the high-half add (value + product.H) keeps exactly those 16 bits.
 *   - The crossfade l + floor((r - l) * w / 2^16) equals
 *     floor((l * (65536 - w) + r * w) / 2^16): one mixed-mode (signed x
 *     unsigned, unshifted) MAC pair in A1, truncated to its high half. It stays
 *     between l and r, so it fits 16 bits; w = 0 would need 65536 and is excluded.
 *   - The four values accumulate in A0 as x * 8192 fractional (2 * 8192 * x);
 *     the truncated high half is floor(sum / 4), the C sum >> 2.
 * Clobbers R0..R3, P0..P2, I3 (L3 stays 0), A0, A1, LC0/LT0/LB0, ASTAT.
 */
/* Struct offsets (WaveQuad): 0 wave a, 4 wave b, 8..20 phases, 24..36 increments,
 * 40 balance. Registers in the loop: P4 wave a, P3 = b - a, P2 struct, P5 out,
 * R6 = [65536 - w : w], R7 = [8192 : extract pattern for the 24-bit fraction]. */

.section .text
.align 4
.global _macroosc_native_wave_quad
.type _macroosc_native_wave_quad, STT_FUNC
_macroosc_native_wave_quad:
    CC = R2 == 0;
    IF CC JUMP 9f;
    P2 = R0;
    P0 = R2;                          /* samples */
    [--SP] = (R7:4, P5:3);
    P5 = R1;
    R3 = P2;
    R3 += 8;
    I3 = R3;                          /* stream 0's phase */
    R0 = [P2 + 8];
    R1 = [P2 + 24];
    R0 = R0 + R1;
    R1 = R0 >> 25;
    P1 = R1;                          /* stream 0's table index */
    P4 = [P2];
    R1 = [P2 + 4];
    R3 = P4;
    R1 = R1 - R3;
    P3 = R1;                          /* wave b - wave a */
    R3 = [P2 + 40];
    R4 = - R3;
    R6 = PACK(R4.L, R3.L);
    R7.L = 0x0118;
    R7.H = 8192;
    LSETUP (1f, 2f) LC0 = P0;
    /* stream 0 */
1:  P0 = P4 + (P1 << 2);
    R1 = EXTRACT(R0, R7.L) (Z) || R2 = [P0 ++ P3] || [I3] = R0;
    R4 = R2 >>> 16 || R3 = [P0];
    R5 = R3 >>> 16 || R0 = [P2 + 12];
    R4 = R4 * R1 (IS, NS);
    R5 = R5 * R1 (IS, NS);
    R4.H = R2.L + R4.H (NS) || R1 = [P2 + 28];
    R0 = R0 + R1 (NS);
    R1 = R0 >> 25 || [P2 + 12] = R0;
    P1 = R1;
    R5.H = R3.L + R5.H (NS);
    A1 = R4.H * R6.H (M);
    R4.H = (A1 += R5.H * R6.L) (M, T);
    /* stream 1 */
    P0 = P4 + (P1 << 2);
    R1 = EXTRACT(R0, R7.L) (Z) || R2 = [P0 ++ P3];
    A0 = R4.H * R7.H;
    R4 = R2 >>> 16 || R3 = [P0];
    R5 = R3 >>> 16 || R0 = [P2 + 16];
    R4 = R4 * R1 (IS, NS);
    R5 = R5 * R1 (IS, NS);
    R4.H = R2.L + R4.H (NS) || R1 = [P2 + 32];
    R0 = R0 + R1 (NS);
    R1 = R0 >> 25 || [P2 + 16] = R0;
    P1 = R1;
    R5.H = R3.L + R5.H (NS);
    A1 = R4.H * R6.H (M);
    R4.H = (A1 += R5.H * R6.L) (M, T);
    /* stream 2 */
    P0 = P4 + (P1 << 2);
    R1 = EXTRACT(R0, R7.L) (Z) || R2 = [P0 ++ P3];
    A0 += R4.H * R7.H;
    R4 = R2 >>> 16 || R3 = [P0];
    R5 = R3 >>> 16 || R0 = [P2 + 20];
    R4 = R4 * R1 (IS, NS);
    R5 = R5 * R1 (IS, NS);
    R4.H = R2.L + R4.H (NS) || R1 = [P2 + 36];
    R0 = R0 + R1 (NS);
    R1 = R0 >> 25 || [P2 + 20] = R0;
    P1 = R1;
    R5.H = R3.L + R5.H (NS);
    A1 = R4.H * R6.H (M);
    R4.H = (A1 += R5.H * R6.L) (M, T);
    /* stream 3 */
    P0 = P4 + (P1 << 2);
    R1 = EXTRACT(R0, R7.L) (Z) || R2 = [P0 ++ P3];
    A0 += R4.H * R7.H;
    R4 = R2 >>> 16 || R3 = [P0];
    R5 = R3 >>> 16 || R0 = [P2 + 8];
    R4 = R4 * R1 (IS, NS);
    R5 = R5 * R1 (IS, NS);
    R4.H = R2.L + R4.H (NS) || R1 = [P2 + 24];
    R0 = R0 + R1 (NS);
    R1 = R0 >> 25;
    P1 = R1;
    R5.H = R3.L + R5.H (NS);
    A1 = R4.H * R6.H (M);
    R4.H = (A1 += R5.H * R6.L) (M, T);
    NOP;
    R3.L = (A0 += R4.H * R7.H) (T);
2:  W [P5 ++] = R3;
    (R7:4, P5:3) = [SP++];
9:  RTS;
.size _macroosc_native_wave_quad, .-_macroosc_native_wave_quad
