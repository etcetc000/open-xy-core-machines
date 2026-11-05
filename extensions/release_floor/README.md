# Idle threshold

A settings extension: only `extension.json`, declaring the SDK's `release_floor`
capability. It links no code.

When a note is released, the OP-XY keeps rendering its voice until the amplitude
envelope falls 66 dB below its peak. This extension raises that idle threshold
to a chosen value from -66 dB (stock) to -20 dB. A release that falls below it
fades out over one 1.45 ms block and frees its voice, so release-heavy parts
render fewer voices and use less CPU.

- The level is the envelope's own, before velocity, filter and track volume, so
  the threshold is a level below the envelope's peak, not true dBFS: at -40 dB
  a tail usually ends around -50 dBFS or lower in the mix.
- Held notes never stop early; only released notes do. Stolen-voice fades and
  drum kits stay stock.
- The filter envelope shares the routine: its release ends at the same
  threshold, fading the rest of its depth to zero over that block. At high
  thresholds a resonant patch with a long filter release may tick.
- A higher threshold audibly shortens long, soft releases (pads, reverb-like
  tails). -40 dB is the default here; -66 dB is stock behavior.

Choose the value in the builder's Extensions step, set `settings.release_floor`
in `extension.json`, or apply it to an existing OS image:

```sh
python ../open-xy/sdk.py settings apply --image os.ldr --extension extensions/release_floor --set release_floor=-48 --out ../build-idle-48
```

For an open-xy OS built with another SDK, add `--stock-os <stock os.ldr>`.

Needs the SDK's `release_floor` capability.
