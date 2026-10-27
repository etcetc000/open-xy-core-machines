# Karplus

A plucked-string synth for the OP-XY, built with the
[Open-XY SDK](https://github.com/etcetc000/open-xy): the example of a machine
that lives in its own repository and reaches the Open-XY builder through the
catalog.

Each note fills a delay line one period long with a pluck. It circulates,
losing a little treble and energy each pass, like a string (Karplus-Strong
synthesis), and a pickup at the pick point hears it. The OP-XY's own envelope and level shape the note;
the controls shape the string.

| Control | Effect |
| --- | --- |
| Bright | The pluck: a soft finger pulling the string aside, up to a hard, bright pick |
| Pick | Where the string is plucked and heard: thin and twangy at the bridge, nasal, then round and hollow at the middle, for the whole note |
| Damp | Treble lost on every pass: a glassy, metallic ring to a dull, felt-muted string |
| Spread | A second string up to about 25 cents apart, for a chorused, twelve-string shimmer |

Control captions show whole numbers from 0–100; Shift + turn retains fine
adjustment.

The screen shows the string between its pegs. Each note sends a small wave
out both ways from the Pick position; the waves bounce off the pegs and fade
faster the more the string is damped, and Bright sets their height. The waves
of up to twelve notes ring at once and add up. Spread
fans out companion strings above and below, darker the further out they are.

| File | Owns |
| --- | --- |
| [definition.json](definition.json), [machine.json](machine.json) | Controls, presets, resources, SDK version and identity |
| [presets](presets) | The device presets the builder installs in `content/presets/karplus/`: `default.json`, the starting sound, plus `harp.json` and `chorus.json` |
| [core.h](core.h), [engine.cpp](engine.cpp) | Voice state and sound |
| [display.h](display.h), [display.cpp](display.cpp), [panel.svg.fragment](panel.svg.fragment) | The screen |
| [pick.h](pick.h) | Pick's sweep and the pickup's level, shared by the sound and the screen |
| [tests/karplus.cpp](tests/karplus.cpp) | Sound checks: tuning at every Damp and Pick, ringing and fading, Damp's darkening, Pick's harmonics, Spread and the declared peak |

With the SDK cloned next to this repository:

```sh
python ../open-xy/sdk.py test --machine machines/karplus --cxx clang++ --host-cxx g++
python ../open-xy/sdk.py preview --machine machines/karplus --out ../previews/karplus-001
python ../open-xy/tools/serve_harness.py --preview ../previews/karplus-001
```

Released versions are tags; the Open-XY catalog pins one of them.
