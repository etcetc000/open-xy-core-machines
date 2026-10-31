# Timestretch sample engine

Timestretch plays samples in time with the project, either by repitching or
with pitch-preserving, Capicola-derived timestretch. It forks the
stock drum sampler (`"base": "drumSampler"`): each pad has its own sample and settings.

Requires SDK 1.7 or later for tempo-aware lazy chop. Saved BPM/bar memory uses
the 18-bit settings format introduced in SDK 1.6; older firmware cannot read it.

## Playing it

| Encoder | Main page | Shift |
| --- | --- | --- |
| 1 | Pitch, -12 to +12 semitones in 0.1 steps; press resets | Stock reverse |
| 2 | Sample start | Loop length or source BPM; press switches between them |
| 3 | Sample end | `repitch` or `timestretch`; press toggles |
| 4 | Loop or play once; press toggles | Stock volume |

- **Length and tempo.** Lengths are 1/4, 1/2, 1, 2, 4 and 8 bars of 4/4.
  Source BPM runs from 20.0 to 300.0. Shift + turn encoder 2 changes it by
  0.1 BPM; hold that encoder down while turning for 1 BPM. A turn suppresses
  the release's mode-switch click. Each pad remembers both its bar length and
  source BPM when switching between them, including after saving a project.
  A pad with no previous BPM starts at 120. Set the length or source BPM
  correctly before judging stretch quality.
- **Automatic source BPM.** Put exactly three digits at the start or end of
  the file name, separated by a space, `_` or `-`: `095_guitar.wav` or
  `guitar_095.wav` (020 to 300). Conflicting values at both ends are ignored.
  Without one, Timestretch suggests a length from the duration and project
  tempo; it does not detect beats.
- **Replacing a sample** keeps the pad's pitch, reverse, volume, loop/play and
  mode. A BPM in the new file name updates the source BPM; otherwise the
  previous BPM or length stays. An empty pad starts with loop, repitch, zero
  pitch and the suggested length; a pad without saved settings plays one bar.
- **Recording.** A recording made with the stock recorder is assigned to the
  selected pad like a file, keeping a replaced pad's settings. Its source BPM
  is the project tempo at assignment, including its decimal digit.
- **Trim.** Playback follows the outer start/end markers, which new
  assignments set to the whole sample; the sampler's hidden loop markers do
  not apply.
- **Slice and copy pads.** Hold a musical key and press M1 for the stock
  slicer (transient, even or tap), M2 to copy that pad, or M3 to paste onto
  the held key. Slices inherit the source pad's loop/play, mode and
  BPM-or-length setting. With a bar length, each slice plays for that length;
  use source BPM to keep slices at the source tempo. Copy/paste within
  Timestretch also preserves trim, tuning, reverse and volume. Pasting from
  another engine uses Timestretch's default loop/play, mode and length;
  pasting into a stock sampler clears Timestretch's private settings.
  Lazy chop auditions the source once, forwards, at the current playback speed.
  Taps use the same tempo and pitch clock: a 99 BPM source at 50 master BPM
  advances through the waveform at 50/99 speed in repitch mode with zero pitch.
  The audition freezes its source bounds, controls and tempo when it starts;
  later edits apply on the next audition. Project transpose and keyboard routing
  do not substitute another source pad.
  Wait for analysis to finish before chopping in timestretch mode to hear the
  whole source. If analysis is still loading, the audition advances silently
  and resumes at the matching source position when ready; it does not restart
  behind the tap clock.
  Each new note starts at its start marker (or end when reversed), including
  loop mode while the sequencer runs; project tempo still controls playback speed.
- **Edit multiple.** Hold a musical key and press M4, then select the pads to
  edit. Pitch, loop/play, source BPM or length, and repitch/timestretch use the
  same controls as single-pad editing, including encoder clicks. Each selected
  pad keeps its own base values and receives the adjustment; the displayed
  selected pad updates with its saved settings.
- **Pitch.** Pad pitch, live pitch (another ±12 semitones), the octave
  buttons and the track transpose (±36) add up, limited to ±60 semitones. In
  timestretch mode pitch is independent of loop speed; in repitch mode it
  also changes speed. Retriggering a pad, at any octave, chokes its previous
  voice; other pads and tracks keep playing.
- **Live controls** start from the pad's settings. Start and end move the
  markers toward the sample's ends; play, mode and reverse have a middle `pad`
  position that keeps the saved choice. Reverse's outer positions force
  forward (`off`) or backward (`on`) playback;
  volume scales the saved volume. Tempo offsets the source BPM by up to ±140
  BPM, or the length by up to ±5 steps. The bundled preset centers them.
- **Analysis.** Timestretch mode needs an analysis of the sample, prepared on
  the stock file worker after assignment; the progress overlay reports it.
  Repitch plays meanwhile; timestretch stays silent until it completes. The
  stock loader's limit applies: 882,000 frames (20 seconds at 44.1 kHz).
- New presets use zero amp decay and full release. Load the preset on a fresh
  track when testing; saved tracks keep their stored parameter values.

## How it uses the sample API

[`sample.cpp`](sample.cpp) renders through the
[sample engine API](https://github.com/etcetc000/open-xy/blob/staging/docs/api.md#sample-engines). `definition.json`
declares stereo output, same-key choke, pad settings with drum controls, and a
2 MiB analysis budget. `sliceTiming` reports the source-frame clock used by lazy chop.

- [`controls.h`](controls.h) packs loop/play, mode and BPM-or-length into the
  pad settings, and implements `sample_defaults`, `sample_assign`,
  `drum_control` and `drum_panel`. [`assignment.h`](assignment.h) reads the
  file-name BPM and suggests a length.
- [`analysis.h`](analysis.h) prepares a keyframe bitmap per channel: positions
  where the derivative changes sign, at most 256 frames apart. Playback reads
  the original PCM; keyframes only time grain splices.
  [`stretch.h`](stretch.h) plays grains with linear interpolation and shares
  playheads and crossfades between channels. Incoming grains that advance
  faster than the tempo clock start behind it by 75% of their predicted
  drift during the fade, reducing early attacks without shifting the loop
  clock. This offset is bounded to a quarter of the trimmed loop and cannot
  precede initial playback.

See the [upstream revision and AGPL notices](vendor/README.md).
