/* karplus_native_lay: native/lay.hpp's lay_reference on the device.
 *
 * R0 = Laying*. Per sample, with the fractional multiplier (products doubled)
 * and truncating extraction of an accumulator's top half (floor(A / 2^16)):
 *   smooth' = floor((smooth * (32768 - w) + noise * w) / 32768)
 *           = smooth + ((noise - smooth) * w >> 15), exactly;
 *   laid    = clamp15((shape >> 8) + floor(share * burst / 32768)), the sum
 *             saturated to 16 bits first (the clamp keeps it exact).
 * clamp15 of a 16-bit half: a saturating vector shift left by 1, then right.
 * The generator runs on seed ^ 0x80000000, whose recurrence is the same (the
 * multiplier is odd) and whose arithmetic >> 17 is the noise itself.
 * The shape is kept as shape << 8 (it fits: its whole part fits 16 bits), so
 * its whole part is the register's high half.
 * Registers in the loops: R0 seed', R2.L smooth, R2.H the noise behind,
 * R3 shape << 8, R5.L = 32768 - w, R6 = (burst, w); R1, R4, R7 scratch.
 * I0 laid, I1 kept, I2 before, I3 cycles a 3-word pool on the stack:
 * the multiplier, the increment, step << 8; P1 points at a fourth word.
 * The loops are rotated to fill the multiplier's gaps: a sample's generator
 * multiply runs at the end of the sample before (the first before the loop),
 * so the last runs one draw ahead and the seed is kept aside in the fourth
 * word as each draw is made; and the shape is advanced before it is used,
 * starting a step behind.
 */
.section .text
.align 4
.global _karplus_native_lay
.type _karplus_native_lay, STT_FUNC
_karplus_native_lay:
    [--SP] = (R7:4);
    SP += -16;
    P0 = R0;
    R1 = [P0 + 12];                   /* count */
    CC = R1 == 0;
    IF CC JUMP .Lout;
    P2 = R1;
    R1 = [P0 + 0];
    I0 = R1;
    R1 = [P0 + 4];
    I1 = R1;
    R1 = [P0 + 8];
    I2 = R1;
    R0 = [P0 + 16];
    BITTGL (R0, 31);                  /* seed' */
    R2 = [P0 + 20];
    R3 = [P0 + 24];
    R3 <<= 8;
    R4 = [P0 + 28];
    R4 <<= 8;
    R3 = R3 - R4;                     /* a step behind */
    R6 = [P0 + 32];
    R5 = [P0 + 36];
    R6 = PACK (R6.L, R5.L);           /* (burst, w) */
    R7 = 0x8000 (Z);
    R7 = R7 - R5;
    R5 = R7;                          /* 32768 - w */
    R7.H = 0x0019;
    R7.L = 0x660D;                    /* 1664525 */
    [SP + 0] = R7;
    R7.H = 0x3C6E;
    R7.L = 0xF35F;                    /* 1013904223 */
    [SP + 4] = R7;
    [SP + 8] = R4;                    /* step << 8 */
    I3 = SP;
    B3 = SP;
    L3 = 12;
    P1 = SP;
    P1 += 12;                         /* the seed, as drawn */
    R7 = [I3++];                      /* the multiplier */
    R0 *= R7;                         /* the first sample's */
    CC = R1 == 0;
    IF CC JUMP .Lpulled;

    /* Past the pick point: the share is the noise less the noise behind. */
    LSETUP (.Lc_top, .Lc_end) LC0 = P2;
.Lc_top:
    A0 = R2.L * R5.L || R4 = [I3++] || R2.H = W[I2++];  /* smooth * (32768 - w); the increment; behind */
    R0 = R0 + R4 (NS) || R4 = [I3++];                   /* seed'; step << 8 */
    R7 = R0 >>> 17 || [P1] = R0;                        /* noise; the seed so far */
    R3 = R3 + R4 (NS);                                  /* this sample's shape */
    R2.L = (A0 += R7.L * R6.L) (T);                     /* smooth' */
    R1.H = R2.L - R2.H (NS) || W[I1++] = R2.L || R7 = [I3++];  /* keep it; the multiplier */
    R1 = R1 << 1 (V, S);
    R1 = R1 >>> 1 (V);                                  /* the clamped share */
    R0 *= R7;                                           /* the next sample's draw */
    R1.H = (A1 = R1.H * R6.H) (T);                      /* share * burst >> 15 */
    R1.H = R1.H + R3.H (S);                             /* plus the shape, saturated */
    R1 = R1 << 1 (V, S);
    R1 = R1 >>> 1 (V);
.Lc_end:
    W[I0++] = R1.H;                                     /* lay */
    JUMP .Ldone;

    /* Before it: the share is the noise. */
.Lpulled:
    LSETUP (.Lp_top, .Lp_end) LC0 = P2;
.Lp_top:
    A0 = R2.L * R5.L || R4 = [I3++];
    R0 = R0 + R4 (NS) || R4 = [I3++];
    R7 = R0 >>> 17 || [P1] = R0;
    R3 = R3 + R4 (NS) || R1 = [I3++];
    R2.L = (A0 += R7.L * R6.L) (T);
    R0 *= R1;
    R1.H = (A1 = R2.L * R6.H) (T) || W[I1++] = R2.L;
    R1.H = R1.H + R3.H (S);
    R1 = R1 << 1 (V, S);
    R1 = R1 >>> 1 (V);
.Lp_end:
    W[I0++] = R1.H;

.Ldone:
    L3 = 0;
    R0 = [P1];
    BITTGL (R0, 31);
    [P0 + 16] = R0;
    R2 = R2.L (X);
    [P0 + 20] = R2;
    R3 >>>= 8;                        /* the last sample's shape */
    R4 = [P0 + 28];
    R3 = R3 + R4;                     /* and the step after it */
    [P0 + 24] = R3;
.Lout:
    SP += 16;
    (R7:4) = [SP++];
    RTS;
.size _karplus_native_lay, .-_karplus_native_lay
