# Macro Osc performance checks

The candidate removes repeated setup work without changing synthesis rates,
render chunk boundaries, oscillator models, envelopes or modulation timing:

- Skip the FM-depth division when depth is zero or the divisor is one.
- Reuse the pitch conversion within an unmodulated block when successive
  chunk-start increments are identical. A changed increment is still converted
  at its original sample position.
- Use exact, precomputed parameter-interpolation quotients for the folded
  oscillators and three digital FM models. The include overlay leaves all
  vendored files unchanged. Other parameter interpolation macros retain their originals.
- Replace analog phase-interpolation division at the three existing chunk
  sizes with exact shifts or a fractional multiply. Keep the helper in its
  own translation unit: inlining its branches increased register pressure in
  several sample loops. The original decreasing-phase complement is preserved.
- Normalize nonzero pitch increments with the compiler's sign-bit count.
  The caller retains its zero-increment guard and the pitch lookup is unchanged.
- Combine bit/sample-rate reduction with output scaling in a single pass.
  Clean output retains the same sample conversion and leaves hold state alone.
- Skip digital delay-length setup when the selected renderer does not read it.
  Selection follows the actual dispatch function, not the stale ordering in
  the digital enum. The three models that use delay lengths still recalculate
  them before every render, including model switches.
- Replace bounded octave-folding loops with exact arithmetic, keeping the
  original loop for Wave Chords where it generates faster machine code.
  The [source overlays](../optimized/README.md) are reproducible derivatives
  of the unchanged vendor files with their original licence notices.
- Specialize analog sync roles and selected zero-sync digital paths. Internal
  hard sync preserves the master output and slave input, including fractional
  timing. Keep original paths wherever specialization regressed native costs.
- Give selected renderers nonaliasing output pointers, allowing the compiler
  to keep state in registers. Reject annotations that regress other renderers.
- Cache Vowel FOF coefficients in unused space in its existing state union.
  Invalidate on initialization and model changes; retain the original shared
  formant-amplitude behavior and control update timing.
- Use exact fractional multiplication for byte-wave interpolation in Wave Map
  only; other wavetable renderers retain their faster original implementation.
- Pack sine and folding-table values with exact adjacent differences, reducing
  table reads and sample-loop arithmetic without altering interpolation.
- Fuse Fold's two oscillators and mix into one loop, explicitly retaining both
  oversampling steps and independent oscillator state.
- Use exact fractional amplitude multiplication and additive phase progression
  in Harmonics, preserving its twelve partials and amplitude smoothing.
- Calculate hard-sync transition divisions from an integer reciprocal lower
  bound and exact residual correction. The sync waveform, fractional reset
  timing and BLEP samples remain identical. A 2 KiB read-only seed table
  replaces repeated long division in the specialized sync renderers.
- Reuse that exact transition quotient in Classic Saw, Square, Saw, Variable
  Saw and the direct Triple mixing loops. A separate comparison covers
  changing pitch, parameters, waveform selection and both Triple mix modes
  against the general renderers with their original divisions.
- Unroll Flute in four-sample groups while preserving its original three
  excitation advances per group. Wider local circular-buffer counters remove
  unnecessary narrowing; the stored delay counter still wraps at 16 bits.
  A compile-time adapter check enforces the required chunk divisibility.

No allocated voice or shared storage is added; paired lookup tables are
read-only and normalization needs no table. The quotient helper falls back
to the original division for other positive chunk sizes. Zero-sized renders
are not supported by the existing oscillator API.

## Audio comparison

`fixture.cpp` exercises all 47 models with six deterministic scenarios: static
controls, full external FM, moving controls and pitch under FM, four pitch
regions, lo-fi settings with drift, and model switching with retriggers. Each
case starts from fresh voice/shared state. `host.cpp` captures 128 blocks of
64 samples per case, or 2,310,144 samples in total.

Save the unmodified `engine.cpp` from the baseline revision outside this repo,
then run from the repository root (replace the paths for your installation):

```text
python machines/macroosc/performance/compare.py --sdk ../open-xy --reference-engine /external/baseline-engine.cpp --out /external/new-comparison --cxx clang++
python ../open-xy/sdk.py test --machine machines/macroosc --no-cpu
python machines/macroosc/optimized/generate_oscillators.py --check
python machines/macroosc/optimized/generate_dsp.py --check
python machines/macroosc/optimized/generate_packed_tables.py --check
```

The baseline uses that engine, original vendor includes and original vendor
translation units; the candidate uses the current engine and authored
overlays. The comparison maps each generated oscillator source back to its
original vendor file for the baseline. The other machine
files must match the baseline revision. Both builds retain the vendor's
existing wrapping signed arithmetic with `-fwrapv`. Output must match byte for
byte. Captures, binaries, source hashes and compiler logs are written only to
the fresh external output directory. The quotient unit test checks against
the original formula across all sizes from 1 through 65,536 and the maximum
unsigned size. The phase quotient test covers quotient/remainder boundaries
near zero and the maximum word, plus one million deterministic random inputs
at eight divisors. The normalization test covers every power-of-two boundary
and one million random nonzero words against the original shift loop.
The octave-folding test checks every possible clamped pitch input against
the original octave-by-octave loop.
The packed-table test exhausts all 16,777,216 significant sine phases and all
65,536 inputs to each folding table against the original table values and
independent wide-integer interpolation.
It also checks Harmonics' fractional amplitude scaling for every signed
16-bit sample across representative boundary gains in the normalized range.

This corpus is regression evidence, not proof for every possible control
sequence. It excludes the stock XY filter, amplitude envelope and mixer.
Host execution times are not device CPU scores.

## Native cost comparisons

Private native experiments compile the actual Blackfin GCC oscillator payload
and execute it in the BF707 functional simulator. Each of the same 282 cases
runs 24 blocks; the first eight warm up state and the remaining 16 are timed.
All 24 output blocks contribute to an audio hash checked against the original
payload. Only the render call is timed.

Functional simulator cycle counts omit physical cache/memory timing and stock
voice processing. Use their ratios to screen changes, then measure the final
machine with the standard whole-voice profiler on hardware. No simulator
result here establishes a hardware CPU percentage.

Broad table-multiply and inline phase-division replacements were rejected because
they improved some models while regressing others. The final patch keeps the
narrower setup changes described above. The compiler leading-zero builtin
was also rejected: the native toolchain emitted an unavailable runtime helper.

### Local results, 2026-10-29

The retained candidate matched all 2,310,144 host samples and all 282 native
case hashes. SDK preview/lifecycle checks, all three arithmetic tests and the complete
native machine compilation passed. Vendor files remained byte-for-byte.

| Native isolated-render case | Cycle reduction |
| --- | ---: |
| Classic Saw, static controls | 2.54% |
| Classic Saw, full external FM | 13.85% |
| Sine Tri Fold, static controls | 6.59% |
| Sine Tri Fold, full external FM with moving controls/pitch | 18.33% |
| Triple Square, full external FM with moving controls/pitch | 13.51% |
| Triple Triangle, full external FM with moving controls/pitch | 22.03% |
| Triple Sine, full external FM | 23.47% |
| Two-Op FM, full external FM | 16.62% |
| Saw Swarm, static controls | 9.80% |
| Wave Map, static controls | 1.98% |
| Clocked Noise, static controls | 15.38% |
| Median across all 282 cases | 4.72% |

Every measured case improved; the smallest reduction was 1.19%. The gains
are strongest in FM-heavy analog models and some simpler digital models,
not a uniform speedup across the whole engine. The isolated native payload
grew by 912 bytes over the original;
that is not a complete firmware-size comparison. Voice and shared-state
declarations are unchanged.

Compared with the previous retained candidate, the latest setup changes save
another 0.06–12.92% of isolated cycles (2.09% median). Its complete development
module adds 140 initialized bytes, with unchanged BSS and audio stack budget.
The measured cumulative median improvement rose from 2.19% to 4.72%.

An out-of-line byte-wave crossfade was rejected because it made wavetable
models slower. The first delay guard used the stale enum ordering and failed
audio comparisons; only the corrected dispatch-based guard is retained.
Changing the pitch helper initially introduced calls and register spills,
so the final overlay explicitly inlines the helpers and preserves the original
Wave Chords setup loop. All retained cases pass audio and native comparisons.

Those were local screening results; subsequent hardware results follow below.

### Hardware anchor and further optimization, 2026-10-30

Profiling build 001 measured all 47 models in three complete hardware passes,
with external FM off and full-depth external FM at ratio 8. The standard
submission workload remained separate and measured 2.615% median whole-voice
CPU. With full FM, 22/47 midpoint-control model cases were below 3.5%; Triple
Square reached 5.873%. Full FM is not an oversampling setting.

Further source changes are being screened against that hardware anchor before
another firmware build. Separate no-sync analog paths, selected byte-wave
math and cached FOF control calculations preserve the original audio in the
host corpus. Native cycle checks and conservative hardware-calibrated
estimates determine which changes are retained. This work has not established
below 3.5% on every model. No new hardware percentage should be inferred from
the simulator alone, and the midpoint benchmark does not cover every knob
setting, pitch or live project.

The subsequent compact-table candidate matches the same 2,310,144 host samples
and 282 native hashes. Six arithmetic tests and the SDK checks pass. Relative
to the preceding candidate, compact wavetable lookup saves roughly 20–22% in
the full-FM Wavetable, Wave Map and Wave Chords cases. Exact chunk quotients
save about 10% in the band-pass and high-pass sweep cases. Some non-FM cases
regress: filter models by about 0.2%, Wave Line by about 1.0%.

The standalone native module uses 207,576 initialized bytes, 179,304 BSS bytes
and a 380-byte audio-stack estimate. The latest change adds 43,752 initialized
bytes without increasing BSS or stack. A complete collection fit is not yet
verified: the frozen Field build SDK is older than this machine's target SDK,
and the newer SDK lacks that frozen build's Field-specific inputs.

Hardware-anchored central estimates put 34/47 models below 3.5%; the conservative
estimate puts 21/47 below it. Neither establishes the requested all-model target.
The newly defined Wave Line upper endpoint also needs listening coverage; its
previous out-of-bounds behavior is not an audio reference.

The next retained changes precompute exact pitch interpolation and specialize
Snare/Cymbal's fixed filter paths. Pitch lookup improves all 47 full-FM native
cases, typically by 1–4%, with 16.2% in Saw Swarm. Three non-FM Chaotic FM cases
regress by about 0.03%. Prepared filters then reduce full-FM Snare cycles by
14.3% and Cymbal cycles by 18.1%, with no measured case regressions in that step.

The combined candidate again matches all host samples and native hashes.
Seven arithmetic tests and SDK checks pass. Its standalone native module uses
212,468 initialized bytes, 179,304 BSS bytes and a 380-byte stack estimate.
Central estimates clear 3.5% on 37/47 models; conservative estimates clear it
on 21/47. The worst central/conservative estimates remain 4.355%/4.809%.
These are still anchored estimates, not measurements of the new candidate on
hardware. Full collection fit and the all-model performance goal remain open.

Direct triple-oscillator mixing then saves another 13.4% in Triple Saw and
10.0% in Triple Square under full FM, relative to the prepared-filter candidate.
It removes the temporary-buffer mix passes and the output-clear pass while
retaining the original sample truncation. Triangle and Sine full-FM cases cost
about 0.35% and 0.41% more respectively. This tradeoff is retained.

The combined candidate again passes all 2,310,144 host samples, 282 native
hashes and seven arithmetic tests plus SDK checks. The native module uses
214,076 initialized bytes, 179,304 BSS bytes and a 380-byte stack estimate.
Central estimates now clear 3.5% on 38/47 models; conservative estimates clear
it on 22/47. Triple Square is centrally estimated at 3.486%, with insufficient
margin for a confident hardware claim. Worst estimates remain 4.355%/4.809%.

Two further exact experiments were rejected: unrolling Harmonics' twelve
partials increased its full-FM cycles by 29%; replacing the shared mixer with
a single-product identity regressed most affected renderers. A broad fractional
multiply replacement for packed sine lookup also regressed several models.
These are not part of the retained implementation.

A rough memory projection adds the standalone-module growth to the previous
complete package and exceeds its usable 2 MiB reserve by about 3.7 KiB. That is
not a collection link result, since the SDK versions differ. The complete
collection and its eventual reserve must be checked before packaging firmware.

The next retained candidate reduces full-FM Harmonics cycles by 13.4% through
in-place amplitude updates, avoiding unnecessary target copies, and removing
the unused external-sync reads. Wave Chords improves by 14.9% using interleaved
wave pairs, the exact pitch-folding helper and two phase streams per pass.
All other measured cases are unchanged or improve, including transitions into
these models. Host audio and all 282 native hashes remain identical to the
original corpus; eight arithmetic/layout tests and the SDK checks pass.

Harmonics is centrally estimated at 3.673%, Wave Chords at 3.803%. The count
below 3.5% remains 38/47 centrally and 22/47 conservatively; worst estimates
improve to 4.239%/4.694%. These changes do not yet establish the goal.

The standalone module has 229,584 initialized bytes, 179,304 BSS bytes and the
same 380-byte maximum audio-stack estimate. The chord table adds 16 KiB of
read-only data; total initialized growth is 15,508 bytes after code savings.
The existing 24-frame render limit bounds the temporary chord sums. Persistent
voice storage is unchanged. The rough collection projection now exceeds the
old usable reserve by 19,272 bytes; an actual collection link is still needed.

Focused six-case native runs screened single-model experiments during this
step. They are not all-model qualification: only the subsequent complete
47-model run supports the retained results above. Narrower amplitude storage,
paired harmonic steps, subdivided sine tables and several alternate loop
layouts were exact but slower in the full-FM case and were not retained.

The next retained change reduces Fold's full-FM native cycles by 13.1% and
Wave Line's by 3.0%. Wave Line's moving-control and pitch-sweep cases improve
by about 22-24%. State-gated sharing, an exact mixer and identity-crossfade
elimination retain the original rates and all oversampling evaluations.
No measured case regresses. All 282 native hashes and 2,310,144 host samples
match; ten arithmetic/state tests and SDK checks pass. The added Fold test
covers unequal oscillator histories and one-sided shape changes, while the
mixer test checks 5,587,520 cases against independent wide arithmetic.

The central estimates are 3.771% for Fold and 4.119% for Wave Line. The worst
central/conservative estimates are 4.119%/4.487%; the counts below 3.5% remain
38/47 and 22/47. The all-model target remains unproven, and these are estimates
anchored to the previous hardware measurements, not new hardware results.

The standalone module has 230,592 initialized bytes, 179,304 BSS bytes and a
372-byte maximum audio-stack estimate: initialized growth of 1,008 bytes,
unchanged voice storage, and an eight-byte lower stack estimate. The rough
collection projection exceeds the old usable reserve by 20,280 bytes; complete
collection fit is still unverified across the differing SDK versions.

The following retained pass reduces full-FM native cycles by 11.5% for Flute,
7.5% for Square Sync and 3.8% for Triple Square. Other models gain smaller
amounts; no measured case regresses. All 282 native hashes and 2,310,144 host
samples remain identical. Twelve arithmetic/state tests and SDK checks pass.
The sync tests exercise saw/square switching and independent pitch changes;
Flute arithmetic tests check coefficient bounds, filter state, DC blocking and
noise scaling against independent wide-integer reference calculations.

Central estimates are 3.589% for Flute, 3.836% for Square Sync and 3.377% for
Triple Square. Conservative estimates remain 3.895%, 4.305% and 4.184%.
The count below 3.5% is still 38/47 centrally and 22/47 conservatively. These
results do not establish the all-model target or worst-case live performance.

The standalone module uses 230,624 initialized bytes and 179,304 BSS bytes,
with the same 372-byte maximum audio-stack estimate. This step adds only 32
initialized bytes and no tables or persistent storage. The rough collection
projection is 20,312 bytes above the old usable reserve; full link remains open.

An FM ratio-cache experiment was rejected: modest FM savings came with up to
5.4% slower non-FM cases from changed native register allocation. Direct sync
mixing and broad BLEP fractional-multiply replacements also regressed cases
and are not retained. The final adapter still computes every ratio increment
and modulation update at its original time.

The next retained wavetable pass shares an aligned32-bit sample/slope table
between Wavetable, Wave Map and Wave Line, keeping the compact chord pairs.
Wave Map and Wave Line render both internal samples into bounded scratch
storage before combining them, preserving each model's original rounding.
Unused digital sync reads and exceptional-slope dispatch are removed where
the new native layout benefits. Both oversampling evaluations remain intact.

Relative to the previous candidate, full-FM cycles fall by 18.0% for Wave Map,
15.1% for Wave Line and 8.1% for Wavetable. No measured case regresses. All
282 native hashes, 2,310,144 host samples, twelve arithmetic/state tests and
SDK checks pass. Table tests compare all original waves, slope boundaries,
random crossfades, chord pairs and all65 Wave Line selections independently.

Central estimates are 3.143% for Wave Map, 3.592% for Wave Line and 2.390% for
Wavetable. Their conservative estimates are 3.839%, 4.073% and 2.787%. The
counts below 3.5% are now39/47 centrally and22/47 conservatively; the worst
central/conservative estimates are3.836%/4.389%. The all-model goal remains
unmet, and the larger data table's physical-cache behavior is unmeasured.

The standalone native module has284,968 initialized bytes,179,304 BSS bytes
and the same372-byte maximum audio-stack estimate. Net initialized growth is
54,344 bytes after removing the separate Wave Line table and reducing code.
Voice RAM is unchanged. The rough complete-collection projection exceeds the
old usable reserve by74,656 bytes; a complete link with compatible SDK inputs
is still required before any firmware package.

The next retained pass shares adjacent pitch normalization in Saw Swarm and
stages Snare's three independent excitation/filter paths. Saw Swarm retains
the original wrapped interpolation product and both pitch-table endpoints.
Snare preserves each pulse/filter history and the noise generator's ordering;
its temporary sum buffer holds at most twelve half-rate samples. FM depth,
update cadence and synthesis rates are unchanged.

Full-FM native oscillator cycles fall by 4.65% for Saw Swarm and 4.58% for
Snare versus the previous candidate. No measured case regresses. All 282
native hashes and 2,310,144 host samples match, and SDK checks plus thirteen
host tests pass. The added pitch test covers the full adapter pitch range
with detune, random fractions, and every fraction at octave/clamp boundaries.

The count below 3.5% rises to 40/47 in the central estimate; the conservative
count remains 22/47. These are hardware-anchored local estimates, not new
hardware measurements or bounds across all settings and physical cache
states. The all-model target remains unproved. Initialized module size falls
by 68 bytes to 284,900, with unchanged 179,304-byte BSS and 372-byte maximum
audio-stack estimate. Complete collection fit remains unverified.

Hard-sync rendering now uses separate square and saw entry points. Each
includes its known waveform's render loop while retaining the original phase
interpolation, reset timing, shape-change initialization and output mixing.
This avoids the register-allocation regressions of inlining both waveforms
inside the old shared dispatcher. The generic entries remain available to
the tests; the adapter selects the specialized entries from the same shape.

Relative to the previous candidate, full-FM native oscillator cycles fall
4.26% for Square Sync and 3.90% for Saw Sync, with no tested regression.
All 282 native hashes and 2,310,144 host samples match. Thirteen host tests
and SDK checks pass, including an expanded sync test covering the complete
slave-pitch offset range, table clamping and waveform changes.

Central/conservative whole-voice estimates are 3.700%/4.209% for Square Sync
and 2.839%/3.302% for Saw Sync. Counts remain 40/47 and 22/47 below 3.5%;
the worst central estimate is now 3.800%. These remain local estimates with
the previously stated workload and physical-cache limitations.

The standalone module has 286,916 initialized bytes and 179,304 BSS bytes;
maximum audio stack remains 372 bytes. The change adds 2,016 initialized
bytes without new persistent state. No firmware package has been built.
Harmonic loop unrolling, reversed accumulation and a settled-amplitude path
were rejected. The latter was also slower in a separate 1,000-block full-FM
note, so no sustained-note saving is claimed or included in the estimates.

Wave Chords now shares the aligned table used by the other wavetable models,
removing its separate compact table and wide-slope decoding. At an exact wave
selection, the second wave has zero weight and is omitted; the four oscillator
phases and their sum are unchanged. Intermediate positions retain both waves,
the existing two-stream passes and all original interpolation rounding.

The default full-FM oscillator case is 37.36% faster, and the moving-control
case is 2.55% faster. All 282 native hashes and 2,310,144 host samples match,
with no tested cycle regressions; thirteen host tests and SDK checks pass.
An additional native sweep covers all 32 wave selections at fractions 0,
512 and 1023 under full-depth ratio-8 FM at C4. All 96 cases match retained
audio. Exact selections improve 37.35–54.38%; intermediate positions improve
2.56–29.05%. This sweep does not cover every pitch or chord configuration.

Wave Chords' central/conservative estimate is now 2.628%/3.502% at the midpoint
benchmark setting. The conservative result is still above the strict target.
Counts below 3.5% are 41/47 centrally and 22/47 conservatively; the worst
central/conservative estimates are 3.767%/4.342%. The all-model confidence
target remains unproved, including physical cache behavior and wider settings.

Removing the obsolete table and decoder reduces initialized size by 17,072
bytes to 269,844. BSS remains 179,304 bytes and maximum audio stack 372 bytes.
No new persistent state, FM reductions, synthesis-rate changes or firmware
package are introduced. The rough collection projection still exceeds the
old usable reserve by 59,532 bytes; a compatible complete link remains open.

The folded sine/triangle pair now shares the phase-step quotient when both
oscillators have identical initial and target increments. For unchanged gain
and mix controls, it computes their constant values once per call. Moving
controls retain the original interpolation. Gain scaling uses the exact
identity `floor(15*p/16) = p - ceil(p/16)` within the original parameter range.
Both internal evaluations, independent phase histories and mixer rounding
remain intact.

Full-FM native oscillator cycles fall 7.65%, with moving-control cycles down
1.61%. All 282 native hashes and 2,310,144 host samples match, with no tested
cycle regression. Thirteen host tests and SDK checks pass. The pair test now
also covers every gain setting with both a control change and a held block.
An additional 64-case native sweep spans four pitch anchors, four gains and
four balances under full-depth ratio-8 FM. All outputs match the prior
candidate and all cases improve, by 1.60–7.66%. These sampled conditions do
not establish a bound across every pitch, modulation setting or cache state.

Fold's central/conservative whole-voice estimates are 3.529%/4.165%. Counts
below 3.5% remain 41/47 and 22/47; worst central/conservative estimates are
now 3.700%/4.243%. The full confidence target remains unmet.

The standalone module has 270,284 initialized bytes and 179,304 BSS bytes.
This step adds 440 code bytes and increases the maximum audio-stack estimate
from 372 to 376 bytes, with no persistent-state growth. No firmware package
was built. The rough collection projection exceeds the old usable reserve
by 59,972 bytes; complete collection linking remains unverified.

Wave Line now shares adjacent wave samples, directly reads zero-fraction
grid positions, and skips zero-weight mix operands at Color boundaries.
Both internal samples and the original mix rounding remain unchanged.
Against the immediately preceding candidate, its full-depth ratio-8 FM
oscillator cost falls 30.03% at the standard midpoint. A separate 128-case
native sweep across two pitches, four scan positions and sixteen Color
settings improves every tested case by 0.46–46.21%, with identical hashes.
All 282 standard native cases and 2,310,144 host samples match; fifteen
host tests and SDK checks pass, with no standard native cycle regression.

Wave Line's hardware-anchored whole-voice estimates are now 2.699% central
and 3.445% conservative. Across all models, 43/47 central and 24/47
conservative estimates are below 3.5%. These remain estimates based on
older hardware captures, not current measurements or bounds for all settings.
The all-model confidence target remains unmet. The nonbootable module adds
632 initialized bytes to 276,316, with BSS unchanged at 179,304 bytes and
maximum audio stack unchanged at 376 bytes. No firmware was built.

Harmonics now prepares its spectrum using exact integer reciprocal division,
reuses the width divisor for all 24 distance calculations, and derives its
frequency-mask cutoff once. The parameter cache key also removes redundant
masks. The standard full-depth FM oscillator case improves 0.66%; changing
control cases improve 20.72–23.01%. All 282 native audio hashes and 2,310,144
host samples match, with no standard native cycle regression. Sixteen host
tests and SDK checks pass. The new independent spectrum test covers 458,752
control pairs, every possible width/distance combination, and all 65,536
frequency-mask divisors. A separate integer audit bounds width correction
to at most one iteration over its complete input domain.

A 128-case native sweep covers held and moving controls at four pitch
anchors and four Timbre/Color anchors, with full-depth ratio-8 FM. Every
case matches and improves: held controls by 0.54–2.94%, moving controls by
20.91–22.61%. The initial spectrum-only candidate regressed 32 held cases
by up to 0.36%; the cutoff-count change removed those regressions before
retention. These samples do not establish every setting or physical-cache
bound. Harmonics' whole-voice estimates are 3.635% central and 4.220%
conservative. Overall counts remain 43/47 and 24/47 below 3.5%; the goal
remains unmet. The nonbootable module has 279,076 initialized bytes,
179,304 BSS bytes and a 376-byte maximum audio stack. No firmware was built.

The reciprocal seed now has a single explicit definition. The final module
ELF contains one byte-identical 2 KiB table instead of two. An inline-local
table trial was rejected after a simulator fault and duplicate-definition
link failure; no firmware used it.

Packed overdrive curves remove a sample load and subtraction from selected
render loops. Full-FM oscillator costs fall 5.38% for Morph, 5.04% for Ring
Mod, 3.26% for Saw Swarm, 6.54% for Vowel Speech, 6.22% for Filtered Noise
and 4.78% for Twin Peaks. Saw Comb's once-per-call lookup regressed in the
trial and remains unchanged. All 282 native hashes and 2,310,144 host samples
match, with no standard native cycle regression. Sixteen host tests and SDK
checks pass; the curve test exhausts all 65,536 input values for each curve.

An additional 192-case native sweep covers the six affected models, four
pitch anchors, four Timbre/Color corner pairs and held/moving controls under
full-depth ratio-8 FM. Every case matches and improves, by 3.25–6.58% overall.
Saw Swarm's central estimate falls below 3.5% to 3.432%, while its conservative
estimate remains 3.985%. Overall counts are now 44/47 central and 24/47
conservative below target. These are hardware-anchored estimates, not new
device measurements or bounds across every setting/cache state.

Initialized size falls 164 bytes to 278,912 despite the two new packed
curves. BSS and maximum audio stack remain 179,304 and 376 bytes. No firmware
was built; the all-model confidence goal remains unmet.

Unsigned coefficient tables now pack each base with its exact signed slope.
The ordinary and fractional-multiply forms preserve unsigned wrap and the
original halfword result; the faster form is selected per measured renderer.
An exhaustive test checks both forms against independent wide arithmetic
over all 83,886,080 significant phases. Fold also replaces its conditional
triangle reflection with an equivalent phase-bit expression, preserving
phase quantization and both internal evaluations.

Standard full-FM oscillator savings are 2.14% for Fold, 8.06% for VOSIM,
3.09% for Twin Peaks, 1.98% for Particle Noise, 1.26% for Filtered Noise,
0.50% for Struck Drum, 0.44% for Morph and 0.22% for Saw Swarm. A 288-case
sweep spans nine affected models with held/moving controls, four pitch
anchors and four Timbre/Color corner pairs under full-depth ratio-8 FM.
Outputs match and no case regresses; FOF is unchanged with cached controls
and faster when its coefficients change. All 282 standard native hashes
and 2,310,144 host samples match. Seventeen host tests and SDK checks pass.

Removing six unreferenced derived tables saves 2,568 initialized bytes;
the original sine remains for direct reads by Digital Modulation. Final
module size is 281,232 initialized bytes, a net increase of 2,320 bytes for
this step. BSS remains 179,304 bytes and maximum audio stack 376 bytes.
The full native corpus is cycle- and hash-identical before/after removal.

Fold's central estimate is now 3.455%, with its conservative estimate still
4.110%. Overall counts are 45/47 central and 24/47 conservative below 3.5%.
Harmonics and Square Sync remain above the central target. These remain
old-hardware-anchored estimates, not new hardware measurements or bounds
for all settings/cache states. No firmware was built; the goal remains unmet.

The square-wave transition loop is now expanded into its complete possible
sequence (rise, fall, rise). The condition and phase are invariant inside
the original loop, and the fall consumes its only self-reset. This preserves
all transitions, including narrow pulse widths and large phase increments.
Inlining the exact quotient is faster with this structure. Generic reference
renderers retain the original loop and division for independent comparisons.
Snare and Cymbal also use packed coefficient loads during filter preparation;
Kick's general filter processing remains unchanged after broader changes
proved slower.

Compared with the previous retained candidate, standard full-FM oscillator
cost falls 22.94% for Square Sync, 7.91% for Triple Square, 5.80% for Square
+ Sub, 3.29% for Wave Morph, 2.98% for Saw + Sub, 2.44% for Saw Square,
and 1.42% for Snare. All 282 standard native hashes match, with no cycle
regressions. All 2,310,144 host samples match; 17 host tests and SDK checks
pass. Initialized module size is 284,512 bytes; BSS is unchanged at 179,304
bytes and maximum audio stack remains 376 bytes.

Two additional native sweeps cover 416 full-FM cases: 128 Square/Saw Sync
pitch/detune/balance combinations, and 288 cases across nine square and drum
models with held/moving controls, four pitch anchors and four control
corners. Every audio hash matches, and no case costs more than the previous
candidate. The complete transition sequence also has a finite-state audit
covering all combinations of its controlling Boolean predicates.

Square Sync's central estimate falls from 3.517% to 2.857%; its conservative
estimate remains 3.615%. Overall, 46/47 central estimates and 25/47 conservative
estimates fall below 3.5%. Harmonics remains above the central target at
3.635%. These are estimates anchored to the earlier hardware captures;
they do not establish the requested all-model confidence or include new
hardware measurements. No bootable firmware has been built.

Vowel FOF now prepares its original saw excitation once per chunk, processes
each of the five independent filters over that sequence, then performs the
original output interpolation. Filter history, clipping, per-formant
rounding and summation order are unchanged. The original renderer applies
the first formant's gain to every filter. All 25 entries of that gain column
are 16384, making the original bounded multiply/shift exactly `bp >> 3`.
The generator checks that invariant. Two filter steps per loop and exact
phase-crossing division remove further overhead without changing the rate.

Standard full-FM oscillator cost falls 31.78% from the previous retained
candidate. All 282 native audio hashes and 2,310,144 host samples match,
without a standard native cycle regression. Seventeen host tests and SDK
checks pass. Initialized module size decreases by 100 bytes to 284,412;
BSS and maximum audio stack remain 179,304 and 376 bytes.

FOF's central/conservative estimates are now 2.490%/3.369%, bringing the
counts below 3.5% to 46/47 central and 26/47 conservative. These remain
estimates anchored to the earlier hardware captures. Harmonics remains
above the central target, and the all-model confidence goal is still unmet.
Packed Harmonics state, cached packing, stronger alias annotations and
duplicated sample loops were tested but not retained: they were no faster.

Triple Sine and Triple Triangle now use direct destination mixing alongside
Triple Saw and Square. Separate dispatch for the two simpler waveforms keeps
the older paths inexpensive. The gain and int16 truncation remain identical.
Triangle uses the exact reflection expression from Fold and an exact 21/64
fractional multiply. Sine specializes an eight-sample chunk into all eight
original updates and divides its phase delta by eight with a shift, retaining
the original falling/equal-increment complement. Other sizes keep the loop.

Full-FM oscillator costs fall 21.65% for Triple Sine and 16.77% for Triple
Triangle. All 282 standard native cases match without cycle regressions;
all 2,310,144 host samples match. The existing analog comparison now covers
all four Triple waveforms, including first/add mixing and shape/pitch changes,
and exhausts all 65,536 signed16 gain inputs. Seventeen host tests and SDK
checks pass. Initialized size grows by 1,612 bytes to 286,024; BSS remains
179,304 bytes and maximum audio stack remains 376 bytes.

A further 256 native cases cover all four Triple waveforms, four pitch
anchors and sixteen Timbre/Color detune combinations under full-depth
ratio-8 FM. Every audio hash matches and every case costs less than the
previous candidate. This sampled sweep does not establish a physical
cache bound or cover every setting.

Triple Triangle's central/conservative estimates are 3.002%/3.424%; Triple
Sine's are 2.807%/3.454%. The counts below 3.5% are now 46/47 central and
28/47 conservative. Harmonics remains the sole central holdout. Estimates
still use earlier hardware captures; the all-model confidence goal remains
unmet and no bootable firmware was built.

Snare's prepared bandpass filter now clips into a local result before one
final state write. This produces cheaper target code in Snare's crowded loop.
The original path remains for Cymbal and the general filter remains for Kick;
applying the change indiscriminately slowed those models. Arithmetic, state
layout, all internal steps and full FM are unchanged.

The full-FM Snare oscillator cost falls 5.85%. All 282 standard native cases
match with no cycle regressions, all 2,310,144 host samples match, and 17 host
tests plus SDK checks pass. The direct-filter test now also covers LP mode
and excitation beyond the 16-bit audio range. A 250-case Snare sweep includes
five pitch anchors, five-by-five controls, held/moving controls and repeated
attacks; all hashes match and all cases are no slower (gain range
5.76% to 5.82%).

Initialized size is 285,952 bytes (72 fewer); BSS remains 179,304
bytes and maximum audio stack remains 376 bytes. Snare's central/conservative
estimates are 3.269%/3.811%. The total below 3.5% remains
46/47 central and 28/47 conservative. These use earlier hardware anchors;
they do not prove all-settings/cache bounds. The goal remains unmet and
no bootable firmware was built or flashed.

The nonaliasing output annotation now also covers Saw Swarm, the shared
four-model filter-sweep renderer and Vowel Speech. Their output block is
separate from oscillator state. A broad screening run showed regressions
for VOSIM, Kick and Granular Cloud, so those were excluded. Harmonics now
uses forward amplitude/target cursors for the same twelve partials and
identical smoothing order; this provides a small additional saving.

Full-FM oscillator costs fall about 7.9% for Lowpass/Peak Sweep, 5.44% for
Bandpass/Highpass Sweep, 0.96% for Swarm, 0.20% for Vowel and 0.16% for
Harmonics. All 282 standard native cases match without regressions; all
2,310,144 host samples match, and 17 host tests plus SDK checks pass. An
additional 224 full-FM cases cover these seven models at four pitches,
four corner-control pairs and held/moving controls, with repeated attacks.
All hashes match and none of these cases regress.

Initialized size is 286,020 bytes (+68); BSS remains 179,304 bytes
and maximum audio stack remains 376 bytes. Bandpass Sweep's conservative
estimate is 3.479%, bringing the conservative count below 3.5% to
29/47. Highpass Sweep is still 3.511%; Harmonics remains the sole
central holdout at 3.630%. These remain old-hardware-calibrated estimates,
not all-settings/cache bounds. No bootable firmware was built or flashed.

Bandpass and Highpass Sweep now have their own dispatch entry, eliminating
the invariant mode branch inside their sample loop. Both carriers and every
square-integrator/state update remain present. Lowpass and Peak retain the
general renderer but use the existing exact algebraic mixer. Applying that
mixer to the specialized loop was slower, so BP/HP retain their original Mix.
The reproducible transformation is in optimized/digital_filter_render.py.

Full-FM oscillator costs fall another 6.39% for BP/HP and about 1.65% for
LP/Peak. All 282 standard native cases match without regressions; all
2,310,144 host samples match, and 17 host tests plus SDK checks pass. The
existing independent exact-mixer test covers 5,587,520 inputs. A further
320 native cases cover five pitch anchors, both Timbre endpoints, four
Color values including 16383/16384, held/moving controls and retriggers.
All hashes match and none of these cases regress.

Initialized size is 286,556 bytes (+536); BSS remains 179,304 bytes
and maximum audio stack remains 376 bytes. Conservative BP/HP estimates
are 3.376%/3.408%. The count below 3.5% is now 30/47 conservative,
with 46/47 central. These remain old-hardware-calibrated estimates, not
all-settings/cache bounds. No bootable firmware was built or flashed.

Bowed String and Flute now use a signed-byte-specific delay mixer. The
residual fits int32, so this preserves the original floor and -a correction
without the generic wrapping conversion. Both original output scales are
preserved. Blown Reed retains its int16 mixer. The generator checks the
byte-backed delay declarations and exact call counts.

Full-FM oscillator costs fall about1.78% for Bowed and2.09% for Flute. All
282 standard native cases and160 wider physical-model cases match without
regressions; all2,310,144 host samples match. Seventeen host tests and SDK
checks pass, including3,866,624 additional byte-mixer cases at both scales
against an independent int64 reference. Initialized size falls80 bytes to
286,476; BSS179,304 and maximum audio stack376 are unchanged.

Conservative estimates are3.084% for Bowed and3.652% for Flute. The count
below3.5% remains30/47 conservative and46/47 central. These are calibrated
estimates, not current hardware measurements or all-settings/cache bounds.
Other Macro mixer substitutions were slower and are not retained. No
bootable firmware was built or flashed.

Ten models now use an exact signed-half sine lookup: Triple Sine, Ring Mod,
VOSIM, Harmonics, Two-Op FM, Chaotic FM, Struck Bell/Drum, Granular Cloud
and QPSK. Each original sine interval has two packed entries, based at its
lower/upper endpoint with the same slope. The signed fraction makes the
upper half algebraically identical while allowing signed16 multiplication.
All16,777,216 significant phases match an independent int64 reference.
Full-FM oscillator savings range1.08-9.82%; Harmonics saves3.25% and Two-Op
FM9.82%. Fold, filter sweeps and Feedback FM were slower and retain their
existing lookup. No FM depth, cadence, rates, voices or smoothing changed.

All282 standard native cases and800 wider cases match with no cycle
regressions. All2,310,144 host samples match;17 host tests and SDK checks
pass. Initialized size288,536(+2,060), BSS179,304 and audio stack376 unchanged.
The additional2KiB table coexists with the older lookup for other engines;
physical cache timing is not modeled by these simulator tests.

Harmonics central/conservative estimates fall to3.534%/4.151%. Counts remain
46/47 central and30/47 conservative below3.5%; the goal is not achieved.
These are old-hardware-calibrated estimates, not fresh hardware results.
No bootable firmware or device operation was performed.

Shared render setup now uses direct dispatch derived from the reviewed
Macro and Digital function tables. The Macro default is its common Digital
renderer; analog/SawComb cases remain explicit. Physical-model delay setup
uses indexes derived from the checked Digital table, preserving its enum
mismatch. Unused member-pointer table definitions are removed. All35/48
valid dispatch targets and all46 DSP renderer bodies are unchanged.

FM ratio increment now uses an exact q31 fractional product with a top-bit
correction, preserving the full uint32 input domain and final phase wrap.
An independent uint64 reference passes4,980,736 boundary/sweep/random cases.
The first preview/module checks exposed a missing global int32_t type;
using SDK opxy::i32 fixes portability. Recompiled native engine object bytes
are identical, binding the earlier native payload to the corrected source.
Fresh host/SDK/module checks then pass, with18 host tests and all2,310,144
host samples matching. All282 standard native cases and1504 wider cases
covering all47 models also match with no cycle regressions.

Full-FM oscillator savings are0.745-4.409% across all47 models. Central
estimates now all pass3.5%, with Harmonics highest at3.480%. Conservative
passes rise30->33: Bell, Cloud and Plucked String cross the threshold, the
last only narrowly. Worst conservative remains Harmonics4.113%. These are
old-hardware-calibrated estimates, not fresh hardware/all-settings bounds;
the high-confidence goal is not achieved.

Initialized size falls180 bytes to288,356; BSS179,304 is unchanged. SDK stack
bound rises376->448 because direct calls expose nested paths hidden by its
single-indirection traversal. Comparing247 common functions shows no local
frame growth; both dispatcher frames shrink4bytes. The new bound includes
impossible cross-model paths and is conservative. No stack reserve changed.
No firmware, flash, MTP, commit or push was performed.

Swarm now shares pitch normalization when all seven detuned oscillators fit
one octave, preserving the old boundary/clamp fallback and wrapped integer
interpolation. Four-plus-three phase passes use the existing output buffer
for bounded exact partial sums before the unchanged overdrive/filter pass.
All phase advances, RNG order, filter arithmetic and FM behavior are retained.

Validation:282 standard native cases and2,310,144 host samples match, with
no native cycle regressions.32 wider full-FM Swarm cases and600 direct
octave/clamp boundary cases match and improve. The setup helper exhausts
all16384 pitch x32768 control combinations (plus boundary/random checks),
3,789,406,208 individual increment comparisons against original lookup.
18 host tests, preview/lifecycle, generators and nonbootable module pass.

Swarm full-FM oscillator cycles fall12.763% at midpoint and13.348% with
moving controls. Central estimate is3.007%, conservative3.700%, down from
3.933%.47/47 central and33/47 conservative models pass3.5%; the goal remains
unachieved, with worst conservative Harmonics4.113%. No fresh hardware
measurements; the estimates do not establish all-settings/cache bounds.

Initialized bytes288568 (+212), BSS179304 unchanged. Swarm local frame
shrinks76->72bytes; SDK overall stack bound448 unchanged. No new tables,
scratch allocation or reserve change. No firmware, flash, MTP, commit/push.

Wave Map uses an exact wrapped32-bit sample interpolation product. Product
wrap changes the shifted signed quotient only by multiples of65536, which
the original int16 sample conversion discards. Existing fractional multiply
remains at other wave call sites: global substitution regressed Wave Line.
No table, signal-flow, evaluation-rate, crossfade or FM changes.

All282 native cases and2,310,144 host samples match, with no cycle regressions.
80 wider full-FM Wave Map cases also match without regressions. Arithmetic
passes8,583,380,992 comparisons, including all24-bit fractions for every
possible wave slope, plus every signed16 base at boundary anchors.18 host
tests, preview/lifecycle, generators and nonbootable module pass.

Wave Map full-FM oscillator cycles fall1.2103%; improvement is modest.
47/47 central and33/47 conservative estimates still pass3.5%, so the full
high-confidence goal remains unmet. No new hardware measurements. Initialized
bytes288564 (-4),BSS179304 andSDKstackbound448 unchanged. No firmware/flash.
Other private loop, arithmetic and data-packing experiments were rejected;
their results remain in experiments065 rather than entering machine source.

Swarm grouped phase loops now count down while advancing an output pointer.
Only the second group stores its final unsigned phase state before rendering
as initial+increment*size; independent locals still advance for every sample.
This equivalent state scheduling saves seven cycles per ordinary render call
in the focused corpus, without changing setup, RNG, filter or FM behavior.

All282 native cases and2,310,144 host samples match with no cycle regressions.
32 wider full-FM Swarm cases and96 direct cases covering chunk lengths1..24
also match without regressions.18 host tests, preview/lifecycle, generators
and nonbootable module pass. Full-FM oscillator saving0.606% is modest.

Initialized bytes288544 (-20),BSS179304 andSDKstackbound448 unchanged. No new
tables/scratch or approximations. Overall47/47 central and33/47 conservative
estimates remain under3.5%; the goal is not achieved. No fresh hardware,
firmware, flash, MTP, commit or push. Other loop/unroll/Fold trials rejected.

Snare now skips only the first three excitation pulses when all have zero
state and no delayed trigger. Both resonant filters still execute every
original sample, and the noise pulse/RNG path is unchanged. A generated
Excitation overlay adds a state predicate without adding fields. The shared
SVF dirty byte now independently tracks frequency and resonance, avoiding
unchanged damping lookups during FM. All arithmetic and rates remain exact.

Full-FM Snare oscillator cycles improve21.358%; movingFM21.348%; clean26.518%.
Kick improves2.694% under fullFM and Cymbal0.170%. All282 native audio hashes,
2,310,144 host samples,250 wider Snare fullFM cases and300 attack/retrigger
cases match.19host tests, preview/lifecycle, generator checks and nonbootable
module pass. New pulse test compares2,293,760 steps with original vendor;
filter test now compares independent coefficient invalidations against the
original vendor class, not the optimized general method.

Tradeoff:50 clean attack cases retriggering every1or2 blocks cost up to0.652%
more oscillator cycles; all tested fullFM attack cases improve. Native model
switching cases can cost1or2 extra cycles across16 blocks; no steady-case
regressions. These are not universal peak-cost guarantees.

Initialized bytes288792 (+248), BSS179304 unchanged. Snare local frame84->92,
Kick72->68; overall SDK stack bound448 unchanged. No tables, persistent RAM,
heap reserve or voice count changes. Snare central estimate2.667%, conservative
3.436%; overall47/47 central and34/47 conservative below3.5%. Harmonics/Fold
still about4.11/4.10 conservatively. Old-hardware calibration only; no fresh
hardware or full-settings/cache proof. No firmware, flash, MTP, commit or push.

Cymbal now stages its six independent square phases in four-plus-two groups,
then its BP filter, then the original noise/RNG/HP and mix loop. Partial sums
0..4 then0..6 fit the existing int16 output buffer; clipped BP output also
fits exactly. Early final phase stores use unsigned initial+increment*size,
while local phases emit each original sample. No arithmetic, FM, rates, voice
count, tables or persistent state changed. A checked generator owns the edit.

Full-FM oscillator cycles improve9.085%,moving9.473%,clean10.016%. All282
original native hashes and2,310,144 host samples match, with no standard
native cycle regressions.19host tests, preview/lifecycle, seven generator
checks, sourcebindings and nonbootable module pass. Wider results:
[{"cases": 250, "audio_identical": true, "best_cycle_percent": -9.496506617684929, "worst_cycle_percent": -8.730449978156397, "regressions": 0, "scope": "Cymbal5pitch anchors x5timbre x5color xheld/moving;24blocks8warm16timed, full-depth ratio8FM. Initial attack included in hashes; retrigger31 is outside24blocks. Baseline retained067; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 96, "audio_identical": true, "best_cycle_percent": -12.318412482011665, "worst_cycle_percent": -8.864863884341368, "regressions": 0, "scope": "Direct Cymbal chunk lengths1..24 x4controls; pitch anchors cycled around octave/clamp boundaries.16blocks8warm,64samples per block split by selected length plus remainder. No adapter/FM path in this isolated chunk test. Baseline retained067; physical cache/stock costs unmodeled.", "hardware_measured": false}]

Initialized bytes288740(-52),BSS179304 unchanged. Cymbal local frame92->32;
overall SDK stack bound448 unchanged. No heap reserve change. Cymbal central
estimate2.982953%,conservative3.485978%; overall
47/47 central and35/47 conservative below3.5%. Old-hardware-calibrated estimates
only, not all-settings/cache bounds. Harmonics/Fold remain about4.11/4.10.
No firmware, flash, MTP, commit or push. All other renderers unchanged.
The initial direct chunk fixture selected the vendor HAT enum, whose actual
table entry is Flute; the unchanged baseline failed on missing delay storage.
The corrected fixture selects the verified Cymbal table index. Both artifacts
are retained; no engine fix was needed for this test-fixture failure.

Kick now skips its first two excitation pulses only when both have zero state
and no pending trigger. Its third excitation is used only as a timer, so its
unused waveform calculation is removed while the original trigger countdown
and pitch-switch timing remain. That unused private sample state intentionally
differs; DigitalOscillator::Init resets it on every shape change before another
renderer can read it. All audible punch, resonator and lowpass updates remain.
SVF resonance setters now preserve cached damping when resonance is unchanged,
retaining any pending dirty flags. Generated overlays own all modifications.

FullFM Kick oscillator cycles improve21.255%,moving20.996%,clean26.392%.
All282 native original audio hashes and2,310,144 host samples match.19host
tests, preview/lifecycle, seven generators, sourcebindings and nonbootable
module pass. Excitation tests additionally compare11,760,336 timer steps to
the original vendor and10,752 post-Init samples. Filter tests cover repeated
unchanged resonance. Wider tests:
[{"cases": 250, "audio_identical": true, "best_cycle_percent": -19.960900621175615, "worst_cycle_percent": -19.171690425080655, "regressions": 0, "scope": "Kick5pitch anchors x5timbre x5color xheld/moving;64blocks8warm56timed, full-depth ratio8FM. Retriggers31/62. Baseline retained068; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 300, "audio_identical": true, "best_cycle_percent": -21.919956824582997, "worst_cycle_percent": -0.07756206904575569, "regressions": 0, "scope": "Kick5pitch x5opposedcontrolpairs x6retriggerperiods xclean/fullFM;16blocks zero warm, includes attack. Controls move in FM cases. Periods16/31 both mean one initial attack in this short fixture; wide test separately covers31/62. Baseline retained068; physical cache/stock costs unmodeled.", "hardware_measured": false, "groups": {"clean interval1": {"best": -0.07824297573685302, "worst": -0.07756206904575569, "regressions": 0}, "clean interval2": {"best": -0.07922486393129313, "worst": -0.07836683515536347, "regressions": 0}, "clean interval4": {"best": -8.280249608465262, "worst": -8.118192739452478, "regressions": 0}, "clean interval8": {"best": -17.35318951672985, "worst": -17.036717975391312, "regressions": 0}, "clean interval16": {"best": -21.919956824582997, "worst": -21.620227896531652, "regressions": 0}, "clean interval31": {"best": -21.919956824582997, "worst": -21.620227896531652, "regressions": 0}, "fullFM interval1": {"best": -1.243376206968294, "worst": -0.4697676926856409, "regressions": 0}, "fullFM interval2": {"best": -0.8370761078408617, "worst": -0.4515056256144456, "regressions": 0}, "fullFM interval4": {"best": -8.319343406080959, "worst": -8.053790544746365, "regressions": 0}, "fullFM interval8": {"best": -14.868102475693078, "worst": -14.448078393042874, "regressions": 0}, "fullFM interval16": {"best": -18.005030090721274, "worst": -17.569730224051217, "regressions": 0}, "fullFM interval31": {"best": -18.005030090721274, "worst": -17.569730224051217, "regressions": 0}}}]

No standard steady-case cycle regressions. Two model-switch cases add9cycles
over16blocks (at most0.0112% oscillator cycles); documented separately. Code
grows548bytes to289288 initialized,BSS179304 unchanged. Kick localframe68->64;
overallSDKstackbound448 unchanged. No tables, buffers, heap reserve or voices
added. FullFM depth, evaluations and rates unchanged. Kick central estimate
2.698721%,conservative3.167925%;47/47central and36/47
conservative under3.5. Still not an all-settings/cache bound or fresh hardware
measurement. Goal remains unmet; Harmonics/Fold still about4.11/4.10.
Seven harmonic traversal trials were slower and rejected. No firmware,
flash, MTP, commit or push. Full collection fit remains unresolved.

Struck Drum shares exact octave normalization among its six partials below
the upper clamp, while high pitches retain individual clamping. The duplicate
root partial shares its increment. Octave shifts use the original table word,
preserving bits which would be lost by shifting a rounded root increment.
Amplitude interpolation now uses a signed fade from -32768 to0 and the exact
identity target+floor((target-base)*(fade-32768)/32768). Its bounded factors
and result fit16 bits, allowing cheaper native multiplication. No change to
FM depth/cadence, six partials, internal sample rate, RNG, filters or voices.

FullFM Struck Drum oscillator cycles improve6.6434%, moving6.6515%, clean6.8512%.
All282 native original hashes and2,310,144 host samples match.20host tests,
preview/lifecycle, seven generators, source bindings and nonbootable module
pass. No standard-case native cycle regressions. Math tests exhaust107526
partial increments and check4,106,236 amplitude interpolation cases.
Wider tests:
[{"cases": 250, "audio_identical": true, "best_cycle_percent": -6.876374668183538, "worst_cycle_percent": -5.181366170065537, "regressions": [], "scope": "Struck Drum:5pitch anchors x5timbre x5color xheld/moving;64blocks8warm56timed, full-depth ratio8FM. Retriggers31/62. Baseline retained069; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 300, "audio_identical": true, "best_cycle_percent": -7.0663223254024965, "worst_cycle_percent": -4.365649648668512, "regressions": [], "scope": "Struck Drum:5pitch x5opposedcontrolpairs x6retriggerperiods xclean/fullFM;16blocks zero warm. Moving controls in FM cases. Periods16/31 both mean one initial attack in this short fixture; the wide test separately covers31/62. Baseline retained069; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 1008, "audio_identical": true, "best_cycle_percent": -14.612097557965543, "worst_cycle_percent": -3.5481794119751053, "regressions": [], "scope": "Direct Struck Drum:21 pitches around normalization, fast-path and clamp boundaries x12 even chunk lengths2..24 x4control endpoint pairs.16blocks8warm8timed, retriggers4/9. Each64sampleblock split into requested chunks and an even remainder. Direct renderer only, not an adapter/fullFM benchmark. Baseline retained069; physical cache/stock costs unmodeled.", "hardware_measured": false}]

Initialized size289524 (+236bytes), BSS179304 unchanged. Struck Drum local
frame84->92; overall SDK stack bound448 unchanged. No added persistent state
or heap allocation. The fallback's12byte temporary offset array reuses the
existing partial scratch region in native code; the old static table drops.
Whole-voice central estimate
3.068817%, conservative3.467250%.
47/47central and37/47conservative under3.5. These use old hardware calibration,
not fresh hardware or all-settings/cache bounds. Ten conservative holdouts
remain; Harmonics/Fold still about4.11/4.10. Four earlier partial-streaming
variants were slower and rejected. No firmware, flash, MTP, commit or push.
Full collection fit remains unresolved.

Flute now omits unreachable upper jet/output clamps, uses exact signed16
noise-envelope multiplication, and advances delay read indexes alongside
the unchanged write index. Byte-backed interpolants bound the jet/output
below the removed upper clamps; their original lower clamps remain. Noise
is in[-16800,16799] and the197 envelope entries in[0,21299], so dividing
noise*(2*envelope) by32768 equals dividing noise*envelope by16384 exactly.
All six-million bounded pairs are tested against independent int64 floor.

FullFM Flute oscillator cycles improve3.16235%, clean4.44823%; all282 native
original hashes match, with no standard-case cycle regressions. All2,310,144
host corpus samples and4,194,304 sustained Flute samples match the original.
Sustained corpus has32 cases:4pitches x4control pairs xclean/fullFM ratio8,
2048blocks percase without retriggers, covering two16bit counter wraps.
20host tests, preview/lifecycle, seven generators, source bindings and
nonbootable module pass. Expanded math tests cover6,619,200 noise/envelope
pairs and256 clamp-boundary values, in addition to previous math coverage.
Wider native tests:
[{"cases": 250, "audio_identical": true, "best_cycle_percent": -3.132910549175305, "worst_cycle_percent": -3.0959528135450443, "regressions": [], "scope": "Flute:5pitch anchors x5timbre x5color xheld/moving;64blocks8warm56timed, full-depth ratio8FM with original depth divisor50. Retriggers31/62. Baseline retained070; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 552, "audio_identical": true, "best_cycle_percent": -4.5046570702794275, "worst_cycle_percent": -2.331776789603235, "regressions": [], "scope": "Direct Flute:23 pitches around normalization, coefficient-index and clamp boundaries x6 chunk lengths4..24 step4 x4control endpoint pairs.16blocks8warm8timed, retrigger9. Each64sampleblock split into requested chunks and a multiple-of-four remainder. Actual dispatch18 and allocated delay storage. Direct renderer only, not an adapter/fullFM benchmark. Baseline retained070; physical cache/stock costs unmodeled.", "hardware_measured": false}]

Initialized size289496 (-28bytes), BSS179304 unchanged. Flute local frame
88->100; overall SDK stack bound448 unchanged. No added tables, buffers,
persistent state or heap allocation. FM depth divisor50, modulation cadence,
internal rates, delay read/write order and voice counts unchanged.
Whole-voice central estimate3.110381%, conservative
3.549874%: Flute is closer but still above target.47/47central
and37/47conservative under3.5. Old hardware calibration, not fresh hardware
or all-settings/cache bounds.24 private variants screened; larger unrolling,
centered byte interpolation and alternate first-noise multiplications were
slower. No firmware, flash, MTP, commit or push. Full collection fit unresolved.

Wave Morph uses an exact Q31 filter update, retains the lower state clamp,
and skips the unreachable upper clamp. Its original cutoff table is constant
from entry 113 onward: a direct constant return in that range removes the
lookup and subsumes the old upper cutoff clamp. All other ranges preserve
the original lookup. No state, buffer, FM cadence or synthesis-rate changes.
The state/coefficient bounds prove exact arithmetic and clamp removal.
Tests compare 98,304 cutoffs and 35,960,575 filter updates with independent
original-table/int64 references.

Full-FM oscillator saving 2.963207%, moving controls 3.177575%, clean 3.267670%.
All 282 native original audio hashes and 2,310,144 host samples match; no
standard-case cycle regressions. 21 host tests, SDK preview/lifecycle, seven
generators, source bindings and nonbootable module pass. Wider native tests:
[{"cases": 250, "audio_identical": true, "best_cycle_percent": -3.423810104122993, "worst_cycle_percent": -1.9989419588875457, "regressions": [], "scope": "Wave Morph: 5 pitch anchors x 5 timbre x 5 color x held/moving; 64 blocks, 8 warm and 56 timed, full-depth ratio-8 FM with original depth divisor 50. Retriggers at blocks 31/62. Baseline retained071; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 1080, "audio_identical": true, "best_cycle_percent": -4.304682040531094, "worst_cycle_percent": -2.350123732737286, "regressions": [], "scope": "Direct Wave Morph: 5 pitches x 9 timbres around shape transitions x 8 color settings including three control neighbors at the cutoff plateau x chunks 8/16/24. Integer color halving maps two control neighbors to the same cutoff. 16 blocks, 8 warm and 8 timed; Strike at block 9. 64-sample blocks split into requested chunks and an 8/16-sample remainder. Direct renderer, not adapter/full-FM benchmark. Baseline retained071; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 324, "audio_identical": true, "best_cycle_percent": -4.261009914687575, "worst_cycle_percent": -2.32929820397183, "regressions": [], "scope": "Direct Wave Morph: 4 pitches x 9 timbres x 3 distinct cutoff values 14465/14464/14463 x chunks 8/16/24. Color offsets -2/0/+2 account for integer halving and exercise both sides of the plateau. 16 blocks, 8 warm and 8 timed; Strike at block 9. Direct renderer, not adapter/full-FM benchmark. Baseline retained071; physical cache/stock costs unmodeled.", "hardware_measured": false}]

Initialized size 289504 (+8 bytes), BSS 179304 unchanged, SDK stack bound 448
unchanged, all local frames unchanged. No persistent state or heap added.
Wave Morph central estimate 2.795840%, conservative
3.533290%; still above 3.5. 47/47 central,
37/47 conservative pass. Old hardware calibration, not fresh hardware or
all-settings/cache bounds. 19 private variants screened. Initial qualification
was superseded after the audit caught the generator's early macro return
skipping the new transformation; corrected native077/host086/checks083/module061
and all three wider sweeps qualify the actual transformed renderer.
No firmware, flash, MTP, commit or push. Full collection fit unresolved.

Analog no-sync dispatch now calls the same nine targets directly, retaining
the original shape reset, pitch calculation and clamp. The generator validates
the target order and removes the unused member-pointer table. Morph and Saw
Square prepare their two distinct oscillators together, sharing the phase
increment and pitch clamp only when their post-Init pitches match. Unequal
pitches retain separate work. All sample renderer bodies, interpolation states,
FM depth/cadence, rates, voices and object layouts are unchanged.

Full-FM oscillator savings: Morph 2.458320%, Saw Square 2.223425%; other affected
analog models roughly 0.5-0.95%. All 282 native original audio hashes and
2,310,144 host corpus samples match, without standard-case cycle regressions.
22 host tests, SDK preview/lifecycle, seven generators, source bindings and
nonbootable module pass. The new pair test compares 737,300 samples against
independent generic renders: all nine analog shapes, independent shape changes,
equal/unequal pitches at both clamp boundaries, chunks 1..24, parameter/aux
changes and follow-up renders observing retained state. Wider full-FM tests:
[{"cases": 250, "audio_identical": true, "best_cycle_percent": -2.491215087592469, "worst_cycle_percent": -2.148367974414356, "regressions": [], "scope": "Wave Morph: 5 pitch anchors x 5 timbre x 5 color x held/moving; 64 blocks, 8 warm and 56 timed, full-depth ratio-8 FM with original depth divisor 50. Retriggers at blocks 31/62. Baseline retained072; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 250, "audio_identical": true, "best_cycle_percent": -2.2542843595475204, "worst_cycle_percent": -1.9058495903765071, "regressions": [], "scope": "Saw Square: 5 pitch anchors x 5 timbre x 5 color x held/moving; 64 blocks, 8 warm and 56 timed, full-depth ratio-8 FM with original depth divisor 50. Retriggers at blocks 31/62. Baseline retained072; physical cache/stock costs unmodeled.", "hardware_measured": false}]

Initialized size290112 (+608 bytes), BSS179304 unchanged. SDK static stack bound
504 (was448); ordinary no-sync local frame24->20, new pair local frame56. Other
existing local frames unchanged. No added persistent state or heap allocation.
Morph central estimate2.743164%, conservative
3.497450%; Saw Square central
2.864571%, conservative3.561032%.
47/47 central and38/47 conservative pass. Morph's margin is narrow; continue
optimizing. Old hardware calibration, not fresh hardware or all-settings/cache
bounds. Eight private variants screened; the common-shape switch and if-chain
slowed other shapes and were rejected. No firmware, flash, MTP, commit or push.
Full collection fit and actual hardware acceptance remain unresolved.

Parameter-1 interpolation in SawSquare/Sub/DualSync now obtains the exact
32767/size quotient from a24-entry uint16 table for chunk lengths1..24, retaining
the original division elsewhere. Every table entry is independently checked;
65,537 host input sizes match the original quotient. Start/delta, accumulation,
rounding, previous control values and all renderer source bodies are unchanged.
No FM depth/cadence, rate, voice count, persistent-state or heap change.

Full-FM oscillator savings6.62-8.21% across the five affected models, with clean
cases also faster. All282 original native hashes and2,310,144 host samples match;
no standard-case cycle regressions.22host tests, preview/lifecycle, seven
generators, source bindings, static audit and nonbootable module pass.
Wider native tests: [{"cases": 250, "audio_identical": true, "best_cycle_percent": -8.425467291367427, "worst_cycle_percent": -5.204445851400297, "regressions": [], "scope": "Five models (SawSquare, SquareSub, SawSub, SquareSync, SawSync) x5 pitch anchors x5 paired control settings (four corners and midpoint) xheld/moving. Full-depth ratio8FM,64blocks8warm56timed; retriggers31/62. Baseline retained073; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 480, "audio_identical": true, "best_cycle_percent": -14.323013800820593, "worst_cycle_percent": -1.6428004271281105, "regressions": [], "scope": "Direct Macro renderer: the same five models x24 chunk lengths1..24 x4 paired pitch/control settings. Each of16blocks renders exactly the selected chunk length (not64samples),8warm8timed; controls alternate with137/271 offsets, Strike9. All returned samples hashed. Not adapter/full-FM CPU estimates. Baseline retained073; physical cache/stock costs unmodeled.", "hardware_measured": false}]

Initialized size290256 (+144 bytes, including the single48-byte read-only table),
BSS179304 and SDK stack bound504 unchanged. The three affected local frames
increase52->56; other frames unchanged. Conservative estimates now include
SawSquare3.439726%, SawSub3.494818%, SquareSync3.489461%:41/47pass,47/47central.
Six remain: Harmonics, SineTriFold, TripleSquare, WaveMap, SawSwarm and Flute.
Old hardware calibration only, not fresh hardware or all-settings/cache bounds.
Six private variants screened. Simple inline conditional quotients helped FM
but regressed some clean cases; the table avoids those regressions. The isolated
Q31 square attenuation was slower and was not retained. No firmware, flash,
MTP, commit or push. Full collection fit/hardware acceptance remain unresolved.

Triple Square now uses its caller-guaranteed fixed half duty. A bounded path
batches phase updates only when the entire chunk provably contains no edge;
the first sample preserves the saved BLEP tail and all following samples have
the exact original constant output. The complete original event path remains
for possible edges, high increments, larger chunks and inconsistent high state
after variable-PWM switches. All FM cadence/depth, rates and voices unchanged.

Full-FM oscillator savings24.408547%; clean48.967371%. All282 original native
hashes and2,310,144 host samples match; no standard cycle regressions. New
transition test compares1,392,162samples including PWM switches and follow-ups.
Independent guard audit checks338,224cases/1,775,231sample updates.23host tests,
preview/lifecycle, all seven generators and static proof/source binding pass.
The first new host-test compile rejected implicit memset pointers; explicit
void casts corrected this test-only warning. Fresh checks087/host090/module065
pass; checks086 is preserved as failed evidence and is not the accepted SDK run.
Wider native tests: [{"cases": 250, "audio_identical": true, "best_cycle_percent": -28.389833648253127, "worst_cycle_percent": -1.0207617578276706, "regressions": [], "production_cases": 250, "production_best_cycle_percent": -28.389833648253127, "production_worst_cycle_percent": -1.0207617578276706, "scope": "TripleSquare x5 pitch anchors x5 Timbre anchors x5 Color anchors xheld/moving. Full-depth ratio8FM,64blocks8warm56timed; retriggers31/62. Baseline retained074; physical cache/stock costs unmodeled.", "hardware_measured": false}, {"cases": 480, "audio_identical": true, "best_cycle_percent": -56.350818496833554, "worst_cycle_percent": 3.1457687195392214, "regressions": [{"case": 0, "percent": 2.4088748019017503}, {"case": 24, "percent": 2.101188830522527}, {"case": 48, "percent": 2.101188830522527}, {"case": 72, "percent": 1.8632017651385224}, {"case": 96, "percent": 2.427339508144355}, {"case": 120, "percent": 2.0884858477603796}, {"case": 144, "percent": 2.0884858477603796}, {"case": 168, "percent": 1.8326501085121683}, {"case": 192, "percent": 2.427339508144355}, {"case": 216, "percent": 2.4871586915382604}, {"case": 240, "percent": 2.4871586915382604}, {"case": 264, "percent": 2.531052261542066}, {"case": 265, "percent": 1.1640922319229974}, {"case": 288, "percent": 2.712933753943214}, {"case": 312, "percent": 2.661448140900191}, {"case": 313, "percent": 1.0824934677118314}, {"case": 336, "percent": 2.661448140900191}, {"case": 337, "percent": 1.0824934677118314}, {"case": 360, "percent": 2.6251390433815303}, {"case": 361, "percent": 1.8164735358596973}, {"case": 362, "percent": 1.3078758949880642}, {"case": 363, "percent": 0.5096566523605128}, {"case": 364, "percent": 0.31337342254593725}, {"case": 384, "percent": 3.1457687195392214}, {"case": 385, "percent": 2.0085698982324596}, {"case": 386, "percent": 1.0542709955672747}, {"case": 387, "percent": 0.18004660029655728}, {"case": 408, "percent": 2.483788082711369}, {"case": 409, "percent": 1.5525214710416124}, {"case": 410, "percent": 0.7730426164519244}, {"case": 411, "percent": 0.02658631690890534}, {"case": 432, "percent": 2.483788082711369}, {"case": 433, "percent": 1.5525214710416124}, {"case": 434, "percent": 0.7730426164519244}, {"case": 435, "percent": 0.02658631690890534}, {"case": 456, "percent": 2.0156657963446456}, {"case": 457, "percent": 1.2341062079281873}, {"case": 458, "percent": 0.5746640750443621}], "production_cases": 60, "production_best_cycle_percent": -56.350818496833554, "production_worst_cycle_percent": -1.0797050561797805, "scope": "Direct Macro TripleSquare renderer: five pitch anchors x24 chunk lengths1..24 x4 paired control corners. Each of16blocks renders exactly the selected chunk length,8warm8timed; controls alternate137/271 offsets, Strike9. All returned samples hashed. Not adapter/full-FM CPU estimates. Baseline retained074; physical cache/stock costs unmodeled.", "hardware_measured": false}]
The initial checkpoint rejected38 direct-render timing regressions in1..5sample
calls(up to3.145769%). Inspection of both unchanged adapter call sites proves
production lengths are exclusively8,16,24; all60 corresponding direct cases and
all250full-FM adapter cases improve. The acceptance gate now distinguishes these
actual voice calls from extra boundary tests; it retains all480audio checks and
explicitly records those38 non-production timing regressions.

Initialized size290516 (+260 bytes); BSS179304 and SDK stack bound504 unchanged.
Two local Triple Square frames52->56; others unchanged. No tables/state/heap
added. Triple Square estimates central2.321728%, conservative3.389051%;42/47
conservative and47/47central pass. Remaining: Harmonics, SineTriFold, WaveMap,
SawSwarm, Flute. These use old hardware calibration, not fresh measurement or
all-settings/cache guarantees. Seven private variants screened: the guarded
per-edge loop, alternate gain and outlined quotient were inferior. No firmware,
flash, MTP, commit or push. Full collection fit remains unresolved.

Harmonics now extracts the unchanged twelve-partial loop into a small helper,
allowing the compiler to emit a hardware-counted loop. Spectrum normalization
and the unchanged smoothing recurrence bound the amplitude sum by32767; the
output clamp was therefore redundant and has been removed. No approximation,
new state/table, FM cadence/depth, rate or voice-limit change.

Fresh native081:282 original audio hashes, no timing regressions versus080.
Host091:2,310,144 samples identical. Checks088:24 host tests and preview,
ownership/lifecycle/drawing checks pass. The new range test covers1,048,576
transitions and12,582,912 independently checked gain values. All seven
generators match. Full-FM Harmonics oscillator cost falls2.896109%; all128
wider stable/moving control cases improve2.3778..2.9635%, with identical audio.

Module066:initialized290496bytes,
BSS179304bytes, stack bound504bytes.
Harmonics estimates:central3.398875%,
conservative4.057399%. Still47central and42
conservative models below3.5. Old hardware calibration only, no physical-cache
or all-settings guarantee; full collection fit unresolved. Goal unachieved.
No firmware, flash, MTP, commit or push; SDK and vendor unchanged.

Fold keeps its two-sample sine/triangle sums in32-bit temporaries. Each
interpolated waveshaper value is signed16; each halved term lies[-16384,16383].
The sum of two lies[-32768,32766], so the intermediate int16 conversions were
redundant. Final narrowing, mix rounding, phase updates, both oversampling
steps and all FM behavior remain unchanged. Only three local declarations
changed. No table/state allocation or SDK/vendor change.

Native082:all282 original audio hashes match, no cycle regressions versus081.
Host092:2,310,144 samples identical. Checks089:24host tests plus preview,
ownership/lifecycle/drawing checks pass. Existing Fold test covers unequal
gains/rates/phases,shape resets and every gain value. All7generators match.
Full-FM Fold oscillator cost improves0.800292%; wider128case sweep improvements
-0.824289..-0.759099% with identical audio.
Module067 initialized290492,BSS179304,
stack504. Fold estimate central3.412227%,
conservative4.078052%. Still47central/42conservative
under3.5. Goal remains active/unachieved; old hardware calibration only,
physical cache/all-settings timing and full collection fit unresolved.
No firmware,flash,MTP,commit,push or other device operations.

Flute's existing257-entry jet table now encodes the original signed-byte
interpolation scale, negative clamp and doubled index. The byte mix itself
retains its exact weighted sum and floor rounding; every following pressure,
delay, filter, RNG, excitation and output operation is unchanged. No new
table/state allocation, approximation, FM/rate/voice change or SDK extension.
The generator preserves the original terminal entry and resource extent.

Fresh native083:282 original audio hashes, no timing regressions versus082.
Host093:2,310,144 samples identical. Checks090:25host tests and preview,
ownership/lifecycle/drawing pass. The new independent64-bit test checks every
byte pair at endpoints and all rounding boundaries, plus all remapped entries.
All7generators match. Full-FM oscillator cost improves1.399553%; the128-case
control/pitch sweep with retrigger improves1.381568..1.387960%.
Module068:initialized290452,BSS179304,
stack504. Flute estimates:
central3.075990%,conservative3.525061%.
Still47central/42conservative under3.5. Old hardware calibration only;
physical cache/all-settings timing and full collection fit remain unproven.
No firmware,flash,MTP,commit,push. SDK and vendor unchanged. Goal unachieved.

Flute now uses ordinary delay-buffer indices when an entire render chunk fits
without a read or write crossing either ring boundary. Boundary crossings retain
the original modulo-addressed loop. Arithmetic, sample order, persistent state,
FM depth/cadence and rates are unchanged. No new per-voice or shared memory.

Fresh native084:282 original audio hashes, no timing regressions versus083.
Host094:2,310,144samples identical; sustained Flute:4,194,304samples identical.
Checks091:25host tests plus preview/ownership/lifecycle/drawing pass.
All7generators match. Independent address audit:1,536,000steps covering both
ring lengths, every position, chunk sizes1..24 and uint32 counter-wrap origins.
Full-FM oscillator cost improves10.742977%;128wide control/pitch cases improve
10.519869..10.708024%.
Module069:initialized291412,BSS179304,
stack504. Flute estimates:
central2.815698%,conservative3.337265%.
Now47central/43conservative models below3.5. Old hardware calibration only;
physical cache and arbitrary-setting costs remain unmeasured. No new firmware,
device operations, commit or push. SDK and vendor unchanged. Goal unachieved.
Profiling003 remains pinned to the preceding committed retained078 source.
