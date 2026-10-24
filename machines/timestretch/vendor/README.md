# Capicola

The unmodified reference headers in `capicola/` are from
[heavylight-industries/capicola](https://github.com/heavylight-industries/capicola),
commit `8c64e855fac64054fdb38451db00866ea682d0d8` (1.1.0).
They are distributed under GNU AGPL version 3; see [LICENSE](LICENSE). They are
not compiled; they record what the adaptation below derives from.

`machines/timestretch/analysis.h` and `machines/timestretch/stretch.h` form a modified, fixed-point offline playback
adaptation of the Analyzer, SparseLine and Granule algorithms. It retains
independent time/pitch cursors, the 128-keyframe leash, adaptive crossfade
duration and placement on the time cursor. Audio is read directly from
immutable PCM with Q14 linear interpolation; it is not reconstructed from
keyframe amplitudes.

Analysis is prepared outside playback. Finite-difference derivative crossings
use a 33-count amplitude-change threshold (approximately 0.001 normalized PCM).
Integer key positions are stored in a bitmap with block ranks, without
fractional positions or amplitudes. A forced key after 256 frames bounds
neighbor searches; bounded rank/select queries measure the span of adjacent
keys for crossfade duration. Both channels publish together through the SDK's
opaque analysis interface.

The smooth crossfade shape uses integer arithmetic and its progression follows
the pitch ratio. Stereo channels have separate analysis grids but share PCM
read positions, splice timing and fade weights; the more active channel
supplies the splice decisions. This adaptation changes the audible output
relative to upstream's sparse waveform reconstruction.
The live-input fences, detector, waveshaper and
hardware UI are not part of this sample-player port.

The port and Timestretch code linking it are AGPL-3.0, and are linked only into
builds that include Timestretch; the SDK reaches the analysis through its
preparation interface. Other source files retain their respective licenses. Distributing a combined work requires satisfying
the applicable AGPL corresponding-source requirements; this notice does not
grant rights to redistribute proprietary stock firmware.
