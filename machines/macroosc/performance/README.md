# Macro Osc performance and validation

The optimized sources preserve all 47 models, existing synthesis rates,
full external FM mapping/depth/cadence, output level, voice allocation and
the XY envelope path. The original MIT-licensed `vendor/` tree is unchanged.
The [overlay documentation](../optimized/README.md) describes the exact
arithmetic and renderer transformations. Edit the generators, then regenerate
the checked-in sources; do not edit generated oscillator files directly.

## Hardware results, 2026-10-30

Profiling build 003 completed three independent passes on an OP-XY. Each pass
returned all 125 expected reports with valid frame integrity. All 47 Macro Osc
models were active with external FM off and with full-depth external FM at
ratio 8. Ratio 8 is the modulator/carrier ratio, not 8x oversampling.

The [per-model CSV](hardware-2026-10-30.csv) includes each pass and its median.
Values are average whole-voice block-budget percentages, including oscillator
and stock voice processing. Do not add the oscillator and whole-voice timers.

| Workload | Minimum | Median across models | Maximum | Model medians below 3.5% |
| --- | ---: | ---: | ---: | ---: |
| External FM off | 1.578% | 2.350% | 3.624% | 46/47 |
| Full-depth external FM, ratio 8 | 2.367% | 3.130% | 4.077% | 40/47 |

Only 39 full-FM cases stayed below 3.5% in every pass. Flute measured
3.496%, 3.473% and 3.525%. The largest three-pass spread was 0.172 percentage
points; no submodel maximum counter clipped. The separate standard Macro Osc
row measured 1.978%, versus 2.615% in the earlier build 001 (24.4% lower).
That historical comparison is not an interleaved A/B; package memory placement
and other collection contents differ.

| Full-FM model over target | Median whole-voice CPU |
| --- | ---: |
| Sine Tri Fold | 4.077% |
| Harmonics | 4.068% |
| Struck Bell | 3.868% |
| Saw Swarm | 3.752% |
| Wave Map | 3.650% |
| Struck Drum | 3.630% |
| Granular Cloud | 3.563% |

These are single-voice synthetic workloads with held/release blocks, MIDI note
60, midpoint Timbre/Color, full rate/bits and zero drift. Graphics are excluded.
They are not live-project utilization, guaranteed polyphony, or worst-case
settings. In particular, this older midpoint diagnostic is **not** the current
SDK's control-swept CPU record required for a catalog release. Run the current
SDK hardware sweep, including the combined heaviest control positions, before
publishing a release record. Several models still exceed the recommended 3.5%.

Build 003 used the DSP from commit
`2f2050d1e4e2b59b18b628ecb53a3ae57ba8d3f5`. The user confirmed that package was
installed; the MIDI identity does not attest the firmware hash. The package's
legacy profiler and 22-machine layout differ from a standalone current-SDK
build. Its private firmware reservation changes are not part of these sources.

## Subsequent Flute change

The source now also skips redundant modulo operations when an entire Flute
render chunk fits inside both circular delay buffers. Any boundary crossing
uses the original wrapped loop. Reads, writes, feedback order, arithmetic and
stored counters are unchanged; no persistent storage is added.

This change was **not included in hardware build 003**. Fresh native simulation
matched all 282 original audio hashes and introduced no timing regressions
against that build's qualified source. Flute's full-FM isolated oscillator cost
fell 10.743%; 128 additional fixed/moving-control and pitch cases improved
10.520-10.708%. The functional simulator does not model physical cache/memory
timing or the stock mixer, so these reductions are not measured hardware CPU.

The full host corpus matched 2,310,144 samples; sustained Flute tests matched
another 4,194,304 samples. An independent address audit checked 1,536,000 steps:
every position in both rings, chunk sizes 1-24, and unsigned-counter wrap
origins. Local target compilation reported 291,412 initialized bytes,
179,304 BSS bytes and a 504-byte audio stack bound; the Flute change adds 960
initialized bytes with unchanged BSS/stack. These are standalone module sizes,
not total firmware or device heap consumption.

## Reproduce the checks

From the repository root with the SDK beside it:

```text
python ../open-xy/sdk.py test --machine machines/macroosc --no-cpu
python machines/macroosc/optimized/generate_oscillators.py --check
python machines/macroosc/optimized/generate_dsp.py --check
python machines/macroosc/optimized/generate_packed_tables.py --check
python machines/macroosc/optimized/generate_wave_tables.py --check
python machines/macroosc/optimized/generate_pitch_table.py --check
python machines/macroosc/optimized/generate_filter.py --check
python machines/macroosc/optimized/generate_division_table.py --check
```

The 25 host tests cover interpolation bounds, exact division, packed tables,
sync/shape transitions, state initialization, filter math and FM ratios, with
exhaustive domains where feasible and deterministic wider samples elsewhere.
The SDK also checks preview ABI, fixed memory, parameter/pitch ownership, peak
bounds, lifecycle and drawing. Generator checks pin normalized vendor source
hashes and require reproducible outputs. Vendor licence notices are retained.

For an independent original-versus-optimized audio comparison, save
`machines/macroosc/engine.cpp` from baseline commit
`5b7ed36` outside the repository, then run (substitute local paths):

```text
python machines/macroosc/performance/compare.py --sdk ../open-xy --reference-engine /external/baseline-engine.cpp --out /external/new-comparison --cxx clang++
```

The reference builds that original adapter and the untouched vendor sources.
The candidate builds the checked-in overlays. Both retain the vendor's wrapping
signed arithmetic with `-fwrapv`. The fixture exercises static controls, full
FM, moving controls/pitch, four pitch regions, lo-fi/drift, and model switching
with retriggers: 47 models x 6 scenarios x 128 blocks x 64 samples. Output must
match byte for byte. Captures, hashes and compiler logs stay in the fresh
external output directory. Host execution time is not a device CPU score.

The corpus is regression evidence, not proof for every control sequence. The
original Wave Line table had an undefined upper endpoint; the optimized table
duplicates its final entry there. Equivalence claims exclude that previously
undefined read. The production adapter constrains render chunks to 8, 16 or 24
samples; FOF/Flute staging and bounded temporary buffers rely on the enforced
grouping and size assertions in `engine.cpp`.
