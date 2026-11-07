/* karplus_native_ring: native/ring.hpp's ring_reference on the device.
 *
 * R0 = Ringing*. Within the routine's range every sample, allpass output and
 * filtered value fits 16 bits, so the multiplies run on register halves:
 *  - toward_zero<15>(c * (x - o)): fractional MACs give 2(c x - c o) exactly;
 *    adding 2 * 32767 when the product is negative (x > o, as c <= 0; with
 *    c = 0 the product is 0 and the bias still truncates to 0) and taking the
 *    accumulator's top half truncated is floor((p + 32767 [p < 0]) / 2^15).
 *  - toward_zero<16>((o - s) * damp): mixed-mode MACs (signed times unsigned,
 *    not doubled) give the product itself; plus 65535 when o < s, top half.
 *  - toward_zero<15>(f * 32735): as the first, the sign being f's.
 *  - clamp15 of a half: a saturating vector shift left by 1, then right by 1.
 *  - clamp15(P >> 12) << 15 for P = diff * level: P << 5 saturates exactly
 *    where the clamp bites, then >> 17 (arithmetic) and << 15.
 * The sign masks come from saturating 16-bit differences, whose sign is exact.
 *
 * Registers: R0.L x, R1 = (input, output), R2 masks and the pickup
 * difference, R3 = (filtered, sample), R4 ringing then the frame,
 * R5 = (-32767, coefficient), R6 = (0xffff, damp), R7 = (32735, level).
 * I3 reads, I0 hears, I1 writes, each circular over its line (B/L, cleared
 * again at the end); P1 frame out, I2 frame in (adding); M3 the Ringing.
 * The loop is rotated: a frame's first instruction runs at the end of the one
 * before, beside that frame's store, so the first runs before the loop and
 * the last frame's store after it. The x after the last frame is read but unused.
 */
.section .text
.align 4
.global _karplus_native_ring
.type _karplus_native_ring, STT_FUNC
_karplus_native_ring:
    [--SP] = (R7:4);
    P2 = R0;
    M3 = R0;                          /* the Ringing, for the end */
    R1 = [P2 + 28];                   /* count */
    CC = R1 == 0;
    IF CC JUMP .Lout;
    R7 = R1;
    R3 = 4096 (Z);                    /* a line, in bytes */
    R0 = [P2 + 0];                    /* reads */
    B3 = R0;
    R2 = [P2 + 12];                   /* from */
    R2 = R2 << 1;
    R0 = R0 + R2;
    I3 = R0;
    L3 = R3;
    R0 = [P2 + 4];                    /* hears */
    B0 = R0;
    R2 = [P2 + 16];                   /* heard */
    R2 = R2 << 1;
    R0 = R0 + R2;
    I0 = R0;
    L0 = R3;
    R0 = [P2 + 8];                    /* writes */
    B1 = R0;
    R2 = [P2 + 20];                   /* to */
    R2 = R2 << 1;
    R0 = R0 + R2;
    I1 = R0;
    L1 = R3;
    P1 = [P2 + 24];                   /* frame out */
    R0 = [P2 + 24];
    I2 = R0;                          /* frame in */
    R0 = [P2 + 32];                   /* input */
    R2 = [P2 + 36];                   /* output (= previous) */
    R1 = PACK (R0.L, R2.L);
    R5 = [P2 + 44];
    R4.L = 0x8001;                    /* -32767 */
    R5 = PACK (R4.L, R5.L);
    R6 = [P2 + 48];
    R4.L = 0xffff;
    R6 = PACK (R4.L, R6.L);
    R3 = [P2 + 52];
    R4.L = 0x7fdf;                    /* 32735 */
    R3 = PACK (R4.L, R3.L);
    R0 = [P2 + 56];
    CC = R0 == 0;
    P2 = R7;                          /* count */
    R7 = R3;                          /* (32735, level) */
    R0.L = W[I3++];                   /* the first x */
    R2.L = R1.L - R0.L (S);           /* its first instruction */
    IF !CC JUMP .Ladding;

    LSETUP (.Ls_top, .Ls_end) LC0 = P2;
.Ls_top:
    R2.L = R2.L >>> 15;                                 /* -1 where x > o */
    A0 = R0.L * R5.L;                                   /* x c */
    A0 -= R1.L * R5.L;                                  /* - o c */
    R3.L = (A0 += R2.L * R5.H) (T);                     /* toward_zero<15>(c (x - o)) */
    R3.L = R3.L + R1.H (NS);                            /* sample */
    R2.L = R1.L - R3.L (S);
    R2.L = R2.L >>> 15;                                 /* -1 where o < sample */
    A1 = R1.L * R6.L (M, IS);                           /* o damp */
    A1 -= R3.L * R6.L (M, IS);                          /* - sample damp */
    R2.H = (A1 -= R2.L * R6.H) (M, T);                  /* toward_zero<16> */
    R3.H = R3.L + R2.H (NS);                            /* filtered */
    R2.L = R3.H >>> 15;                                 /* -1 where negative */
    A0 = R3.H * R7.H;                                   /* filtered 32735 */
    R4.L = (A0 += R2.L * R5.H) (T);                     /* toward_zero<15> */
    R4 = R4 << 1 (V, S);
    R4 = R4 >>> 1 (V) || R2.L = W[I0++];                /* ringing; a pick's travel back */
    R2.L = R4.L - R2.L (NS) || W[I1++] = R4.L;          /* the string goes on */
    R1 = PACK (R0.L, R3.L);                             /* (input, output) for the next frame */
    R4 = R2.L * R7.L (IS) || R0.L = W[I3++];            /* (ringing - earlier) level; the next x */
    R4 = R4 << 5 (S);
    R4 = R4 >>> 17;
    R4 = R4 << 15;
.Ls_end:
    R2.L = R1.L - R0.L (S) || [P1++] = R4;
    JUMP .Ldone;

.Ladding:
    LSETUP (.La_top, .La_end) LC0 = P2;
.La_top:
    R2.L = R2.L >>> 15;
    A0 = R0.L * R5.L;
    A0 -= R1.L * R5.L;
    R3.L = (A0 += R2.L * R5.H) (T);
    R3.L = R3.L + R1.H (NS);
    R2.L = R1.L - R3.L (S);
    R2.L = R2.L >>> 15;
    A1 = R1.L * R6.L (M, IS);
    A1 -= R3.L * R6.L (M, IS);
    R2.H = (A1 -= R2.L * R6.H) (M, T);
    R3.H = R3.L + R2.H (NS);
    R2.L = R3.H >>> 15;
    A0 = R3.H * R7.H;
    R4.L = (A0 += R2.L * R5.H) (T);
    R4 = R4 << 1 (V, S);
    R4 = R4 >>> 1 (V) || R2.L = W[I0++];
    R2.L = R4.L - R2.L (NS) || W[I1++] = R4.L;
    R1 = PACK (R0.L, R3.L);
    R4 = R2.L * R7.L (IS) || R0.L = W[I3++];
    R4 = R4 << 5 (S) || R2 = [I2++];                    /* the frame so far */
    R4 = R4 >>> 17;
    R4 = R4 << 15;
    R4 = R4 + R2 (NS);
.La_end:
    R2.L = R1.L - R0.L (S) || [P1++] = R4;

.Ldone:
    L0 = 0;
    L1 = 0;
    L3 = 0;
    P2 = M3;
    R0 = R1.L (X);
    [P2 + 36] = R0;                   /* output */
    [P2 + 40] = R0;                   /* previous */
    R0 = R1 >>> 16;
    [P2 + 32] = R0;                   /* input */
.Lout:
    (R7:4) = [SP++];
    RTS;
.size _karplus_native_ring, .-_karplus_native_ring
