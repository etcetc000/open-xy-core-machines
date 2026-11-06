/* macroosc::native::additive on the device: native/additive.hpp
 * holds the definition and the portable reference this matches sample for sample.
 *
 * void macroosc_native_additive(Additive* s, i16* out, unsigned pairs): pairs output pairs
 * (2 * pairs samples), amplitudes and targets in 0..32767 with each set summing
 * to at most 32767 (the header checks the targets; the amplitudes then stay so).
 *
 * One zero-overhead hardware loop per output pair, 124 instructions and no
 * pipeline stalls: each pair of harmonics is a 20-instruction group (two
 * table reads and interpolated sines, the dual fractional multiply by both
 * amplitudes, the two-lane sum and one-pole step). A table index reaches its
 * P register (P1 or P3) at least five instructions before the address that
 * uses it, so each group forms the next group's two indices while it works:
 * a Dreg-to-Preg move feeding an address stalls 4 cycles on the hardware
 * (the simulator does not show it). The last group forms the next pair's.
 * Exactness:
 *   - The sine read is value + (slope * fraction >> 16), fraction the signed
 *     16 bits phase >> 8: an (IS) 16 x 16 product whose high half is the shift,
 *     added to the value in 16 bits as the C int16 result keeps.
 *   - q31 multiply(sine << 16, a) = floor(sine * a / 2^15): the (T) fractional
 *     product's truncated high half. a >= 0, so -32768 * -32768 cannot occur.
 *   - Each term lies in [-a, a), so with the amplitudes summing to at most 32767
 *     the lane sums and their total fit 16 bits: the 16-bit adds are the C sum.
 *   - (target - a) >> 8 fits a 16-bit lane before and after the shift.
 *   - The first sample of a pair is (out + previous) >> 1: a 16-bit add and
 *     subtract (ASR), whose 17-bit sum is halved before it is kept.
 * Clobbers R0..R3, P0..P2, I0, I1, B0, B1 (L0 and L1 are left 0), A0, A1,
 * LC0/LT0/LB0, ASTAT.
 */
/* Struct offsets (Additive): 0 phase, 4 increment, 8 previous (16 bits),
 * 12 amplitude pairs, 16 target pairs, 20 sine table. Registers in the loop:
 * R7 phase, R6 harmonic phase, R5 lane sums, R4 the two sines, P4 sine table,
 * P1 and P3 the next two table indices, I1 amplitude pairs and I0 target
 * pairs (both circular over 6 words), P5 out. */

.section .text
.align 4
.global _macroosc_native_additive
.type _macroosc_native_additive, STT_FUNC
_macroosc_native_additive:
    CC = R2 == 0;
    IF CC JUMP 9f;
    P2 = R0;
    P0 = R2;                          /* pairs */
    [--SP] = (R7:4, P5:3);
    P5 = R1;
    R0 = [P2 + 12];
    B1 = R0; I1 = R0; L1 = 24;
    R0 = [P2 + 16];
    B0 = R0; I0 = R0; L0 = 24;
    P4 = [P2 + 20];
    R7 = [P2];
    R4 = [P2 + 4];
    R7 = R7 + R4;
    R6 = R7 >> 23;
    P1 = R6;                          /* harmonic 1's table index */
    R6 = R7 + R7 (NS);
    R6 = R6 >> 23;
    P3 = R6;                          /* harmonic 2's */
    R6 = R7;
    LSETUP (1f, 2f) LC0 = P0;
1:  P0 = P4 + (P1 << 2);
    R1 = R6 >> 8 || R0 = [P0];
    P0 = P4 + (P3 << 2);
    R3 = R0.H * R1.L (IS) || R2 = [P0];
    R6 = R6 + R7 (NS);
    R4.L = R0.L + R3.H (NS);
    R1 = R6 >> 8 || R0 = [I1];
    R6 = R6 + R7 (NS);
    R3 = R2.H * R1.L (IS);
    R4.H = R2.L + R3.H (NS) || R1 = [I0 ++];
    R2 = R6 >> 23;
    R3.H = (A1 = R4.H * R0.H), R3.L = (A0 = R4.L * R0.L) (T);
    P1 = R2;
    R2 = R6 + R7 (NS);
    R5 = R3;
    R2 = R2 >> 23;
    R1 = R1 -|- R0;
    P3 = R2;
    R1 = R1 >>> 8 (V);
    R2 = R0 +|+ R1;
    P0 = P4 + (P1 << 2);
    R1 = R6 >> 8 || R0 = [P0] || [I1 ++] = R2;
    P0 = P4 + (P3 << 2);
    R3 = R0.H * R1.L (IS) || R2 = [P0];
    R6 = R6 + R7 (NS);
    R4.L = R0.L + R3.H (NS);
    R1 = R6 >> 8 || R0 = [I1];
    R6 = R6 + R7 (NS);
    R3 = R2.H * R1.L (IS);
    R4.H = R2.L + R3.H (NS) || R1 = [I0 ++];
    R2 = R6 >> 23;
    R3.H = (A1 = R4.H * R0.H), R3.L = (A0 = R4.L * R0.L) (T);
    P1 = R2;
    R2 = R6 + R7 (NS);
    R5 = R3 +|+ R5;
    R2 = R2 >> 23;
    R1 = R1 -|- R0;
    P3 = R2;
    R1 = R1 >>> 8 (V);
    R2 = R0 +|+ R1;
    P0 = P4 + (P1 << 2);
    R1 = R6 >> 8 || R0 = [P0] || [I1 ++] = R2;
    P0 = P4 + (P3 << 2);
    R3 = R0.H * R1.L (IS) || R2 = [P0];
    R6 = R6 + R7 (NS);
    R4.L = R0.L + R3.H (NS);
    R1 = R6 >> 8 || R0 = [I1];
    R6 = R6 + R7 (NS);
    R3 = R2.H * R1.L (IS);
    R4.H = R2.L + R3.H (NS) || R1 = [I0 ++];
    R2 = R6 >> 23;
    R3.H = (A1 = R4.H * R0.H), R3.L = (A0 = R4.L * R0.L) (T);
    P1 = R2;
    R2 = R6 + R7 (NS);
    R5 = R3 +|+ R5;
    R2 = R2 >> 23;
    R1 = R1 -|- R0;
    P3 = R2;
    R1 = R1 >>> 8 (V);
    R2 = R0 +|+ R1;
    P0 = P4 + (P1 << 2);
    R1 = R6 >> 8 || R0 = [P0] || [I1 ++] = R2;
    P0 = P4 + (P3 << 2);
    R3 = R0.H * R1.L (IS) || R2 = [P0];
    R6 = R6 + R7 (NS);
    R4.L = R0.L + R3.H (NS);
    R1 = R6 >> 8 || R0 = [I1];
    R6 = R6 + R7 (NS);
    R3 = R2.H * R1.L (IS);
    R4.H = R2.L + R3.H (NS) || R1 = [I0 ++];
    R2 = R6 >> 23;
    R3.H = (A1 = R4.H * R0.H), R3.L = (A0 = R4.L * R0.L) (T);
    P1 = R2;
    R2 = R6 + R7 (NS);
    R5 = R3 +|+ R5;
    R2 = R2 >> 23;
    R1 = R1 -|- R0;
    P3 = R2;
    R1 = R1 >>> 8 (V);
    R2 = R0 +|+ R1;
    P0 = P4 + (P1 << 2);
    R1 = R6 >> 8 || R0 = [P0] || [I1 ++] = R2;
    P0 = P4 + (P3 << 2);
    R3 = R0.H * R1.L (IS) || R2 = [P0];
    R6 = R6 + R7 (NS);
    R4.L = R0.L + R3.H (NS);
    R1 = R6 >> 8 || R0 = [I1];
    R6 = R6 + R7 (NS);
    R3 = R2.H * R1.L (IS);
    R4.H = R2.L + R3.H (NS) || R1 = [I0 ++];
    R2 = R6 >> 23;
    R3.H = (A1 = R4.H * R0.H), R3.L = (A0 = R4.L * R0.L) (T);
    P1 = R2;
    R2 = R6 + R7 (NS);
    R5 = R3 +|+ R5;
    R2 = R2 >> 23;
    R1 = R1 -|- R0;
    P3 = R2;
    R1 = R1 >>> 8 (V);
    R2 = R0 +|+ R1;
    P0 = P4 + (P1 << 2);
    R1 = R6 >> 8 || R0 = [P0] || [I1 ++] = R2;
    P0 = P4 + (P3 << 2);
    R3 = R0.H * R1.L (IS) || R2 = [P0];
    R6 = R6 + R7 (NS);
    R4.L = R0.L + R3.H (NS);
    R1 = R6 >> 8 || R0 = [I1] || [P2] = R7;
    R6 = [P2 + 4];
    R3 = R2.H * R1.L (IS);
    R4.H = R2.L + R3.H (NS) || R1 = [I0 ++];
    R7 = R7 + R6 (NS) || R2 = W [P2 + 8] (X);
    R3.H = (A1 = R4.H * R0.H), R3.L = (A0 = R4.L * R0.L) (T);
    R6 = R7 >> 23;
    R5 = R3 +|+ R5;
    P1 = R6;
    R3.L = R5.L + R5.H (NS);
    R1 = R1 -|- R0;
    R2 = R3 +|+ R2, R5 = R3 -|- R2 (ASR) || W [P2 + 8] = R3;
    R1 = R1 >>> 8 (V) || W [P5 ++] = R2;
    R6 = R7 + R7 (NS) || W [P5 ++] = R3;
    R6 = R6 >> 23;
    P3 = R6;
    R0 = R0 +|+ R1;
2:  R6 = PACK (R7.H, R7.L) || [I1 ++] = R0;
    L0 = 0;
    L1 = 0;
    (R7:4, P5:3) = [SP++];
9:  RTS;
.size _macroosc_native_additive, .-_macroosc_native_additive
