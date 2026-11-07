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
 * The shape is kept as shape << 8 (its whole part fits 16 bits at every
 * sample, so there it is the register's high half; between samples the
 * register may wrap, which adding back undoes).
 * Registers in the loops: R0 seed', R2.L smooth, R2.H the noise behind,
 * R3 shape << 8, R5.L = 32768 - w, R6 = (burst, w); R1, R4, R7 scratch.
 * I0 first line, I1 second line, I2 the noise behind: circular over the
 * lines (B/L), so a stretch wraps round them. I3 cycles a 3-word pool on the
 * stack: the multiplier, the increment, the slope << 8; P1 points at a
 * fourth word.
 * The loops are rotated to fill the multiplier's gaps: a sample's generator
 * multiply runs at the end of the sample before (the first before the loops),
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
    R0 = [P0 + 0];                    /* first */
    B0 = R0;
    R1 = [P0 + 8];                    /* at */
    R1 = R1 << 1;
    R0 = R0 + R1;
    I0 = R0;
    R0 = [P0 + 4];                    /* noisy */
    B1 = R0;
    B2 = R0;
    R0 = R0 + R1;
    I1 = R0;
    R0 = 4096 (Z);
    L0 = R0;
    L1 = R0;
    L2 = R0;
    R0 = [P0 + 12];                   /* pulled */
    R1 = [P0 + 20];                   /* peak */
    R0 = R0 - R1;
    R1 = [P0 + 8];
    R0 = R0 + R1;                     /* at + pulled - peak */
    R1 = 2047 (Z);
    R0 = R0 & R1;
    R0 = R0 << 1;
    R1 = B2;
    R0 = R0 + R1;
    I2 = R0;                          /* the noise behind the first combed sample */
    R0 = [P0 + 24];
    BITTGL (R0, 31);                  /* seed' */
    R2 = [P0 + 28];
    R3 = [P0 + 32];
    R3 <<= 8;
    R4 = [P0 + 36];
    R4 <<= 8;                         /* up << 8 */
    R3 = R3 - R4;                     /* a step behind */
    R6 = [P0 + 44];
    R5 = [P0 + 48];
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
    [SP + 8] = R4;                    /* up << 8 */
    I3 = SP;
    B3 = SP;
    L3 = 12;
    P1 = SP;
    P1 += 12;                         /* the seed, as drawn */
    [P1] = R0;
    R7 = [I3++];                      /* the multiplier */
    R0 *= R7;                         /* the first sample's */
    P2 = [P0 + 12];
    CC = P2 == 0;
    IF CC JUMP .Lcomb;

    /* Before the pick point: the share is the noise. */
    LSETUP (.Lp_top, .Lp_end) LC0 = P2;
.Lp_top:
    A0 = R2.L * R5.L || R4 = [I3++];                    /* smooth * (32768 - w); the increment */
    R0 = R0 + R4 (NS) || R4 = [I3++];                   /* seed'; up << 8 */
    R7 = R0 >>> 17 || [P1] = R0;                        /* noise; the seed so far */
    R3 = R3 + R4 (NS) || R1 = [I3++];                   /* this sample's shape; the multiplier */
    R2.L = (A0 += R7.L * R6.L) (T);                     /* smooth' */
    R0 *= R1;                                           /* the next sample's draw */
    R1.H = (A1 = R2.L * R6.H) (T) || W[I1++] = R2.L;    /* noise * burst >> 15; keep it */
    R1.H = R1.H + R3.H (S);                             /* plus the shape, saturated */
    R1 = R1 << 1 (V, S);
    R1 = R1 >>> 1 (V);
.Lp_end:
    W[I0++] = R1.H;                                     /* lay */

.Lcomb:
    P2 = [P0 + 16];
    CC = P2 == 0;
    IF CC JUMP .Ldone;
    R4 = [SP + 8];                    /* up << 8, or 0 */
    R1 = [P0 + 40];
    R1 <<= 8;                         /* down << 8 */
    R3 = R3 + R4;
    R3 = R3 + R1;                     /* a step behind the first sample past the pick point */
    R1 = -R1;
    [SP + 8] = R1;                    /* the slope: -down << 8 */

    /* Past the pick point: the share is the noise less the noise behind. */
    LSETUP (.Lc_top, .Lc_end) LC0 = P2;
.Lc_top:
    A0 = R2.L * R5.L || R4 = [I3++] || R2.H = W[I2++];  /* smooth * (32768 - w); the increment; behind */
    R0 = R0 + R4 (NS) || R4 = [I3++];                   /* seed'; -down << 8 */
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

.Ldone:
    L0 = 0;
    L1 = 0;
    L2 = 0;
    L3 = 0;
    R0 = [P1];
    BITTGL (R0, 31);
    [P0 + 24] = R0;
    R2 = R2.L (X);
    [P0 + 28] = R2;
    R3 >>>= 8;                        /* the last sample's shape */
    R4 = [P0 + 36];                   /* and the step after it */
    R1 = [P0 + 16];
    CC = R1 == 0;
    IF CC JUMP .Lstep;
    R4 = [P0 + 40];
    R4 = -R4;
.Lstep:
    R3 = R3 + R4;
    [P0 + 32] = R3;
    SP += 16;
    (R7:4) = [SP++];
    RTS;
.size _karplus_native_lay, .-_karplus_native_lay
