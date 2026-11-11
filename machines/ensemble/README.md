# Ensemble

A stereo chorus for the OP-XY, built with the
[Open-XY SDK](https://github.com/etcetc000/open-xy): the example of a send
effect with eight controls on a page of its own, drawn as stock's FX page
(SDK 1.14, for the send's meters).

One LFO sweeps a short delay on each side; Stereo offsets the right side's
phase, up to half a cycle. The page reads as stock's FX page does: the send's
badge and the effect's name, both rows of controls with a level bar each, and
the send's input and return meters at the ends of the band. A press slides the
band to the other row.

| Control | Page | What it does |
| --- | --- | --- |
| Rate | 1 | LFO speed, 0.1 to 5 Hz |
| Depth | 1 | How far the LFO sweeps the delay, up to 5 ms |
| Delay | 1 | The centre of the sweep, 5 to 25 ms |
| Feedback | 1 | How much of the chorus returns to its own line, up to 75% |
| Shape | 2 | The LFO's shape: sine or triangle |
| Stereo | 2 | Phase offset between the sides, up to 180° |
| Level | 2 | The chorus's level, glided so changes don't click |
| Dry | 2 | How much of the source the stock mixer keeps at full send |

Each side's delay line is 2048 frames in the send slot's scratch memory,
declared as `bufferBytes`.

## Files

| File | Contents |
| --- | --- |
| [machine.json](machine.json), [definition.json](definition.json) | Kind, SDK version, controls, the buffer size and the display's budget |
| [core.h](core.h), [fx.cpp](fx.cpp) | The delay lines, the LFO and the feedback |
| [effect_page.h](effect_page.h) | The page: stock's FX page for eight controls, which an effect copies with the machine |
| [display.h](display.h), [display.cpp](display.cpp) | The effect page, its titles and captions |
| [panel.svg.fragment](panel.svg.fragment) | The page's artwork, the block `effect_page.h` draws over |
| [tests/sound.cpp](tests/sound.cpp) | Sound checks: a stale buffer never heard, the repeat arriving after its delay, silence without a buffer |

The buffer is never cleared, so the effect reads only what it has written
since it started; outside a send slot it has no buffer and stays silent.

## Make your own

Copy it under your own ID and name, then change the sound and the titles:

```sh
python ../open-xy/sdk.py new --from machines/ensemble --id <id> --name "<Name>" --out machines/<id>
```

## Checks

```sh
python ../open-xy/sdk.py test --machine machines/ensemble --firmware
```

CI runs the same on every push. MIT licensed.
