# Idle threshold

A settings extension: only `extension.json`, declaring the SDK's `release_floor`
capability. It links no code.

When a note is released, the OP-XY keeps rendering its voice until the amplitude
envelope falls 66 dB below its peak. This extension raises that idle threshold
to -40 dB. A release that falls below it fades out over one 1.45 ms block and
frees its voice, so release-heavy parts render fewer voices and use less CPU.

- The level is the envelope's own, before velocity, filter and track volume, so
  -40 dB is below the envelope's peak, not true dBFS: a tail usually ends around
  -50 dBFS or lower in the mix, which is hard to hear.
- Held notes never stop early; only released notes do. Stolen-voice fades and
  drum kits stay stock.
- The filter envelope shares the routine: its release ends at the same level.
- A note whose sustain is already below -40 dB loses its whole release when the
  key is let go.

Select it in the builder's Extensions step, or apply it to an existing OS image:

```sh
python ../open-xy/sdk.py settings apply --image os.ldr --extension extensions/release_floor --out ../build-idle
```

For an open-xy OS built with another SDK, add `--stock-os <stock os.ldr>`.

Needs the SDK's `release_floor` capability.
