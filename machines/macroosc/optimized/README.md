# Exact arithmetic and source overlays

The original `vendor/` files are unchanged. Oscillator sources and headers,
the resource source, SVF header and DSP header are reproducible derivatives, retaining the original
MIT notice. The generator checks the normalized source hashes and applies
only these semantic changes:

- Replace each octave-folding loop with `octave_shifts`. The helper is exact
  over the complete input range; its test compares every possible input with
  the original loop. Pitch helpers are explicitly inlined to prevent newly
  introduced calls and register spills. The expanded exact pitch table now
  makes this replacement beneficial in Wave Chords too; its earlier renderer
  retained the loop because the replacement then increased register pressure.
- Calculate the digital oscillator's delay length only when the selected
  renderer is Bowed, Blown or Fluted. Those are the only readers of `delay_`.
  Selection uses the actual member-function pointer, because the original
  digital shape enum and dispatch table disagree in this region. This does
  not change the dispatch table or model numbering.
- Use separate analog renderers when neither external sync input nor sync
  output is needed. This machine supplies no external sync. The internal
  hard-sync pair has separate master and slave renderers: the master retains
  sync output, and the slave retains its fractional sync input. Simple
  zero-sync specialization regressed folded waves; they use a separate fused
  pair renderer instead.
  These changes preserve phase, BLEP, parameter interpolation, modulation
  timing and synthesis rates.
- Remove unused digital sync reads only in renderers where native timings
  improve. Internal hard sync belongs to the analog pair; the adapter's
  digital sync buffer is always zero. VOSIM retains the original code because
  removing it worsened compiler scheduling. Wavetable and Wave Map now benefit
  from removing these reads with their aligned table and revised loop layouts.
- Mark separate output storage as nonaliasing in selected renderers. Apply
  this selectively: broader annotations increased register pressure and
  regressed other models. The adapter's output cannot overlap oscillator state.
- Cache Vowel FOF filter coefficients and its actually-used amplitude by the
  two controls. The cache lives inside unused space in the existing state
  union and resets on initialization/model changes. The original use of
  formant zero's amplitude for all five filters is preserved.
- Store each wavetable segment as an aligned signed16 sample/slope word.
  The shared table is128KiB. Wavetable, Wave Map and Wave Line use exact SDK
  fractional multiplication without a separate exceptional-slope dispatch.
  Wave Chords retains its smaller interleaved byte-pair table and wider-slope
  decoder. Wave Line selects from the shared table instead of a second byte
  array. The generated resource derivative omits the now-unused original wave
  array; vendor files remain unchanged.
  A duplicate final Wave Line entry defines its upper scan endpoint, where the
  original code read past the table. Audio equivalence claims apply to the
  regression corpus, not that formerly undefined endpoint.
- Wave Map and Wave Line evaluate their two internal samples in a compact loop
  into48 temporary int16 entries, then combine adjacent pairs. The adapter's
  existing24-frame limit bounds this scratch space. Preserve Map's rounding
  of each half-sample and Line's rounding after addition. Wavetable keeps its
  unrolled loop, which measured faster for that model. Oversampling is unchanged.
- Pack each sine/fold-table sample with its adjacent difference in one word.
  One aligned read replaces two sample reads and a subtraction. Values,
  interpolation fractions and rounding are unchanged. The separate generator
  pins the original resource hash and verifies every sample/difference fits
  its signed halfword. The three read-only tables total 3072 bytes; they add
  no voice state. The exhaustive interpolation test covers every effective
  sine phase and every folding-table input.
- Fuse the sine/triangle folding pair and final mix into one sample loop.
  Preserve each oscillator's initialization, phase interpolation, gain and
  rounding. Share the control interpolation fraction and explicitly unroll
  the two oversampling steps; a loop over those two steps caused spills and
  was slower on the native target. This avoids temporary output passes and
  leaves both oscillators' end states unchanged.
- In Harmonics, advance each partial's phase by addition modulo 32 bits and
  use the SDK fractional multiply for amplitude scaling. Normalization and
  the one-pole keep amplitudes in 0..32767, where the new multiply equals the
  original signed product and shift without saturation.
  Update amplitudes in their existing state storage and use cached targets
  directly when every partial is below the original frequency cutoff. The
  masked-target path and sample-by-sample smoothing remain unchanged. A layout
  assertion proves the previous-sample slot does not overlap this state.
- Interleave the two selected chord waves in a 16 KiB read-only table so one
  aligned load supplies both samples and slopes. Render two phase streams per
  pass using a 24-element temporary sum, preserving all four phases and exact
  summation before the final shift. No persistent voice storage is added.
- Use exact chunk quotients for the digital filter increment ramp, struck-drum
  ramp and Wave Line interpolation. Full-FM gains outweigh small non-FM costs
  in the measured cases; this is not a claim that every case gets faster.
- Precompute all 1536 original integer pitch-interpolation results in the top
  octave. A 6144-byte read-only table replaces repeated interpolation; octave
  folding and scaling are unchanged, including the separate chord lookup path.
  The test checks every table entry and every defined right-shift amount.
- Prepare Snare and Cymbal filter coefficients once per render call. Their
  fixed BP/HP modes and zero punch permit specialized sample loops with the
  original multiply, truncation and clipping. State layout and filter update
  timing are unchanged. The generic filter remains available to other models.
- Mix triple saw/square output directly into the destination. The first
  oscillator writes it; the other two add to it. Preserve the int16 sample
  truncation, gain, oscillator state and shape-change initialization. This
  removes the clear and temporary-buffer passes. Triangle and sine keep their
  existing renderers; measured dispatch costs increase slightly for those two.

- Fold shares gain and phase-increment interpolation only when both starting
  values and deltas match. Independent phases remain intact, and differing
  states use the complete path. Equal pitches share their pure lookup after
  the original shape-change initialization. Both oversampling steps remain.
- Fold and Wave Line use an exact single-product mixer with explicit unsigned
  wrapping. Wave Line also skips an identity crossfade when its two selected
  waves match; changing-wave transitions retain their full interpolation.
  These helpers add no tables or persistent state.
- Keep phase/high state local in the measured square and sync renderers.
  The sync pair uses its fixed 50% pulse width; its sole caller and shape-change
  initialization both set the pulse parameter to zero. Generic renderers keep
  their variable-width behavior. Independent sync/state tests compare both
  specialized paths against the complete generic renderer.
- Flute advances its unused phase in one operation and preserves the random
  generator's sequence through a local state word. Its filter and first noise
  multiply use exact fractional arithmetic; bounds and rounding are tested.
  Delay lines, excitation, filter coefficients and update timing are unchanged.
  The FM adapter also uses an exact fractional multiply for sine interpolation.

The derived files also include the authored pitch helper and normalize line
endings/trailing whitespace. Keeping generated copies allows the unchanged
SDK build to consume them without a custom build hook. Edit the generator,
not those copies. Reproduce and check from the repository root:

```text
python machines/macroosc/optimized/generate_oscillators.py
python machines/macroosc/optimized/generate_oscillators.py --check
python machines/macroosc/optimized/generate_dsp.py
python machines/macroosc/optimized/generate_dsp.py --check
python machines/macroosc/optimized/generate_packed_tables.py
python machines/macroosc/optimized/generate_packed_tables.py --check
python machines/macroosc/optimized/generate_wave_tables.py
python machines/macroosc/optimized/generate_wave_tables.py --check
python machines/macroosc/optimized/generate_pitch_table.py
python machines/macroosc/optimized/generate_pitch_table.py --check
python machines/macroosc/optimized/generate_filter.py
python machines/macroosc/optimized/generate_filter.py --check
python machines/macroosc/optimized/generate_division_table.py --check
```

The parameter-interpolation header overlay supplies exact quotients at the
existing render chunk sizes. Its phase helper lives in a separate translation
unit to avoid increasing register pressure in the oscillator sample loops.
`normalization.hpp` preserves the original nonzero pitch normalization using
the compiler's redundant-sign-bit count, removing its byte table and branches.
No helper adds persistent voice or shared state.

The hard-sync transition quotient uses a generated 2 KiB reciprocal seed
table, bounded integer refinement, and exact remainder correction. The table
has one external definition in `division_table.cpp`, shared by the analog
and digital translation units. Its result
is the original unsigned quotient, including fractions near phase crossings;
it does not approximate reset timing. `exact_divide.hpp` documents the lower
bound that makes correction safe. Square and Saw Sync inline the arithmetic
in their specialized paths. The original general renderers
remain available to the independent sync comparison test.

`analog_division.py` extends the same exact quotient to Classic Saw, Square,
Saw, Variable Saw and the direct Triple mixing renderers. Classic Saw calls
the helper; the other added paths inline it. The general renderer keeps the
original division for independent waveform and state-transition comparisons.

Harmonics prepares its spectrum outside the sample loop. Its positive
integer divisions use the exact reciprocal helper, with the shared width
divisor prepared once for all 24 distance calculations. Residual correction
preserves the original quotients. The parameter cache key omits masks whose
bits are already set by the marker. When upper partials exceed the frequency
limit, one exact quotient determines how many targets to copy; the remaining
targets are zeroed. Smoothing, partial counts and sample
rates are unchanged; the independent spectrum test covers every possible
width/distance combination and 458,752 control pairs.

Wave Line reads quantized phases directly from the existing table, since
their fractional interpolation weight is zero. Adjacent crossfades share
the middle wave sample. At Color region boundaries, a specialized path
omits the zero-weight mix operand while preserving the original 65535
weighting and rounding. Both oversampling evaluations remain intact.

Unsigned filter and resonator coefficients, plus the VOSIM bell window,
use packed unsigned samples and signed slopes. The ordinary multiply keeps
the original unsigned wrap; the fractional-multiply form differs only by
multiples of 65536 discarded by the unsigned halfword return. Struck Drum
and Particle Noise use the measured fractional form; the other selected
lookups use the ordinary form. Tests exhaust all 83,886,080 significant
phases across the six tables in both forms. Six obsolete derived tables
are omitted from generated resources; vendor data stays unchanged.
Snare and Cymbal also use packed cutoff/damping loads in their prepared
filter setup. Kick keeps its original general filter path; changing its
lookup increased native cost. The prepared-filter test compares against
the general path's original coefficient interpolation and sample arithmetic.

Fold computes its triangle's reflection mask directly from the phase sign
bit. Clearing the low bit after shifting preserves the original high-half
phase quantization, both oversampling steps and final signed conversion.

The moderate and violent overdrive curves also use packed sample/slope pairs.
Only measured sample-loop lookups use them: Morph, Ring Mod, Saw Swarm,
Vowel Speech, Filtered Noise and Twin Peaks. Saw Comb keeps its original
once-per-call lookup. Tests exhaust all 65,536 input positions for both
curves against the original tables and rounding.

Square Sync expands its transition loop into the complete possible event
sequence: rise, fall, rise. Phase and the loop condition are fixed during
that sequence, and the fall consumes the only self-reset. Explicit branches
remove the loop without assuming a pitch limit or skipping any transition.
The resulting paths benefit from inlining the exact transition quotient.
The same complete sequence is used in the unsynchronized Square and Triple
Square paths, including their variable pulse width. The third possible rise
is retained even when the increment is larger than that pulse width.

Vowel FOF stages its saw excitation, five independent filters, and output
interpolation. Each filter receives the original sample sequence; its
recurrence and the formant summation order are unchanged. Processing two
filter steps per loop reduces overhead without reducing their number.
The original uses formant zero's gain for every filter. All 25 entries of
that gain column are exactly 16384, so the generator verifies the column
and replaces the bounded product/right shift with its exact `bp >> 3`.
The saw's phase-crossing division uses the same exact quotient helper as
the analog oscillators. The adapter asserts the scratch-buffer and grouping
limits at compile time; no persistent state or voice count is changed.

All four Triple waveforms now mix directly into their destination, preserving
the original int16 truncation and 21/64 gain. Triangle retains both internal
phase steps and their separate rounding before mixing. Its reflection uses
the exact phase-bit expression already used by Fold, and its bounded gain
uses the exact fractional multiply. Sine specializes eight-sample calls into
eight complete updates, with the phase-delta division by eight replaced by
a shift. Other chunk sizes keep the original loop. Independent tests compare
all four direct paths with the generic renderer across pitch and shape
changes, and exhaust all signed16 inputs for the fractional gain identity.

All these changes are evaluated against the original adapter and original
vendor translation units by the [performance checks](../performance/README.md).
