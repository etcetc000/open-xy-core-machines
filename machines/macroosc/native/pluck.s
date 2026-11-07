/* macroosc::native::pluck on the device: native/pluck.hpp holds
 * the definition and the portable reference this matches sample for sample.
 *
 * void macroosc_native_pluck(PluckString* strings, unsigned count, i16* out, PluckCall* call)
 * PluckString (32 bytes): 0 phase, 4 increment, 8 write, 12 shift, 16 mask,
 * 20 size, 24 excite, 28 line. PluckCall: 0 pairs, 4 random state (in and out),
 * 8 update probability, 12 keep = 32767 - loss, 16 previous sample (in and
 * out), 20 1664525, 24 1013904223, 28 the strings' stride in bytes. The
 * header calls only in range: 1..4 strings, 1..256 pairs, each size a power of
 * two with write below it (or a string never set up).
 *
 * A string's averaging steps run from write up to the read position, as
 * straight hardware loops. Usually they neither start at word 0 nor pass the
 * line's end: one run. From word 0 it is one run, after which the guard word
 * takes line[0]. Past the end they run to the end, with the guard word holding
 * line[0] (the last step's right neighbour), then from word 0 when steps
 * remain; afterwards the guard holds line[0], or its old value when none did.
 * A step stores its word either way (the unchanged word when the draw keeps
 * it) and scales the halved sum by (half * keep + half) >> 15: the halved sum
 * itself without a loss.
 * Registers: R6 the random state, R7 the pair's sum, P3 the string, P0 its
 * line, P4 the call, I0 the output, I2 the stride, M0 past the last string,
 * M1 the previous sample, M2 the sum while R7 steps, M3 and I1 the
 * interpolation's fraction and word. Clobbers R0..R3, P0..P2, I0..I2, B0..B3,
 * M0..M3, LC0/LT0/LB0, LC1/LT1/LB1, ASTAT.
 */
/* One averaging step at P1 (advanced): R0 = a, R1 = b (the loop's last
 * instruction makes it the next a), R3 the draw's multiplier on entry and
 * exit, R4 keep, R5 the update probability, R6 the random state; R2 and R7
 * are scratch. */
.macro STEP
    R6 *= R3;
    R1 = W[P1 + 2] (X);               /* b */
    R2 = R0 + R1 (NS) || R3 = [P4 + 24] || NOP;
    R7 = R2 >> 31;
    R2 = R2 + R7;
    R2 >>>= 1;                        /* halved toward zero */
    R6 = R6 + R3 (NS) || R3 = [P4 + 20] || NOP;   /* the draw */
    R7 = R2.L * R4.L (IS);
    R2 = R7 + R2;
    R7 = R6.L (Z);
    R2 >>>= 15;
    CC = R7 <= R5 (IU);
    IF !CC R2 = R0;
    W[P1++] = R2;
.endm

.section .text
.align 4
.global _macroosc_native_pluck
.type _macroosc_native_pluck, STT_FUNC
_macroosc_native_pluck:
    P1 = [SP + 12];                   /* the call */
    [--SP] = (R7:4, P5:3);
    P5 = R0;                          /* the first string */
    I0 = R2;
    P4 = P1;
    R2 = [P4 + 28];                   /* stride */
    P1 = [P4];                        /* pairs */
    I2 = R2;
    R1 *= R2;
    R1 = R1 + R0;
    M0 = R1;                          /* past the last string */
    R6 = [P4 + 4];                    /* random state */
    R0 = [P4 + 16];
    M1 = R0;                          /* previous sample */
    LSETUP(.Lpair, .Lpair_end) LC1 = P1;
.Lpair:
    R7 = 0;
    P3 = P5;
.Lstring:
    P0 = [P3 + 28];                   /* line */
    R0 = [P3 + 24];                   /* excitation left */
    CC = R0 == 0;
    IF !CC JUMP .Lexcite;
    R1 = [P3 + 4];
    R0 = [P3];
    R0 = R0 + R1 (NS) || R2 = [P3 + 12] || NOP;   /* phase; shift */
    R3 = 22;
    R3 = R3 + R2 (NS) || [P3] = R0 || NOP;
    R1 = R0;
    R1 >>= R3;
    R0 >>= R2;                        /* the interpolation position */
    R1 += 2;
    R2 = R0 >> 22;
    R4 = [P3 + 16];                   /* mask */
    P1 = R2;
    R3 = [P3 + 8];                    /* write */
    R1 = R1 & R4;                     /* read */
    R5 = R1 - R3;
    R0 >>= 6;
    R0 = R0.L (Z);
    M3 = R0;                          /* its fraction */
    R5 = R5 & R4;                     /* steps */
    P1 = P0 + (P1 << 1);              /* its word: read after the steps */
    CC = R5 == 0;
    IF CC JUMP .Linterpolate;
    [P3 + 8] = R1;
    I1 = P1;
    P1 = R3;
    M2 = R7;                          /* the pair's sum, while R7 steps */
    R1 = R3 + R5;
    CC = R4 < R1 (IU);                /* passes the end */
    IF CC JUMP .Lwrapping;
    CC = R3 == 0;
    IF CC JUMP .Lwrapping;
    P2 = R5;
    R4 = [P4 + 12];                   /* keep */
    R5 = [P4 + 8];                    /* update probability */
    P1 = P0 + (P1 << 1);              /* line[write] */
    R3 = [P4 + 20];                   /* the draw's multiplier */
    R0 = W[P1] (X);                   /* a */
    LSETUP(.Lstep, .Lstep_end) LC0 = P2;
.Lstep:
    STEP
.Lstep_end:
    R0 = R1;
    R7 = M2;
    P1 = I1;

.Linterpolate:
    /* l + ((r - l) * fraction >> 16), kept to 16 bits. */
    R3 = W[P1] (X);
    R2 = W[P1 + 2] (X);
    R1 = M3;
    R2 = R2 - R3;
    R2 *= R1;
    R2 >>>= 16;
    R2 = R2 + R3;
    R2 = R2.L (X);
    R7 = R7 + R2;
.Lnext:
    P1 = I2;
    P3 = P3 + P1;
    P1 = M0;
    CC = P3 == P1;
    IF !CC JUMP .Lstring (BP);
    R0 = 32767 (X);
    R7 = MIN(R7, R0);
    R0 = -32767 (X);
    R7 = MAX(R7, R0);
    R0 = M1;
    R0 = R0 + R7;
    R0 >>>= 1;
    W[I0++] = R0.L;
    M1 = R7;
.Lpair_end:
    W[I0++] = R7.L;

    R0 = M1;
    [P4 + 16] = R0;
    [P4 + 4] = R6;
    (R7:4, P5:3) = [SP++];
    RTS;

.Lexcite:
    /* line[excite - 1] = (line[excite - 1] + 3 * draw) >> 2, also the sample. */
    R0 += -1;
    P1 = R0;
    R1 = [P4 + 20];
    [P3 + 24] = R0;
    R6 *= R1;
    R1 = [P4 + 24];
    P1 = P0 + (P1 << 1);
    R6 = R6 + R1;
    R1 = R6 >>> 16;
    R2 = R1 << 1;
    R1 = R1 + R2;
    R2 = W[P1] (X);
    R1 = R1 + R2;
    R1 >>>= 2;
    W[P1] = R1;
    R7 = R7 + R1;
    JUMP .Lnext;

.Lwrapping:
    /* R3 = write, R5 = steps, R1 = write + steps. From word 0 (staying below
     * the end): one run, then the guard word takes line[0]. */
    R2 = [P3 + 20];                   /* size */
    CC = R3 == 0;
    P1 = R2;
    IF CC JUMP .Lfrom_zero;
    /* Past the end: a run to the end with the guard word holding line[0]
     * (the last step's right neighbour), then from word 0 when steps remain;
     * afterwards the guard holds line[0], or its old value when none did. */
    R4 = R2 - R3;                     /* the first run */
    R1 = R1 - R2;                     /* the second */
    B3 = R1;
    P1 = P0 + (P1 << 1);
    B0 = P1;                          /* the guard word */
    R0 = W[P1] (Z);
    B2 = R0;
    P2 = R4;
    R0 = W[P0] (Z);
    W[P1] = R0;
    P1 = R3;
    R4 = [P4 + 12];                   /* keep */
    R5 = [P4 + 8];                    /* update probability */
    R3 = [P4 + 20];                   /* the draw's multiplier */
    P1 = P0 + (P1 << 1);              /* line[write] */
    R0 = W[P1] (X);                   /* a */
    LSETUP(.Lend_step, .Lend_step_end) LC0 = P2;
.Lend_step:
    STEP
.Lend_step_end:
    R0 = R1;
    R0 = B3;
    P2 = B3;
    P1 = B0;
    CC = R0 == 0;
    IF CC JUMP .Lrestore;
    P1 = P0;
    R0 = W[P0] (X);                   /* a */
    LSETUP(.Lstart_step, .Lstart_step_end) LC0 = P2;
.Lstart_step:
    STEP
.Lstart_step_end:
    R0 = R1;
    P1 = B0;
    R0 = W[P0] (Z);
    W[P1] = R0;                       /* the guard: line[0] */
    R7 = M2;
    P1 = I1;
    JUMP .Linterpolate;
.Lrestore:
    R0 = B2;
    W[P1] = R0;                       /* the guard as it was */
    R7 = M2;
    P1 = I1;
    JUMP .Linterpolate;
.Lfrom_zero:
    P2 = R5;
    P1 = P0;
    R4 = [P4 + 12];                   /* keep */
    R5 = [P4 + 8];                    /* update probability */
    B0 = R2;
    R3 = [P4 + 20];                   /* the draw's multiplier */
    R0 = W[P0] (X);                   /* a */
    LSETUP(.Lzero_step, .Lzero_step_end) LC0 = P2;
.Lzero_step:
    STEP
.Lzero_step_end:
    R0 = R1;
    P1 = B0;
    P1 = P0 + (P1 << 1);
    R0 = W[P0] (Z);
    W[P1] = R0;                       /* the guard: line[0] */
    R7 = M2;
    P1 = I1;
    JUMP .Linterpolate;
.size _macroosc_native_pluck, .-_macroosc_native_pluck
