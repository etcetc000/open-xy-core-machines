# Open-XY core machines

The machines and firmware extensions Open-XY maintains itself, for the OP-XY on OS 1.2.1. They are
built with the [Open-XY SDK](https://github.com/etcetc000/open-xy) and reach the
[builder](https://open-xy.com) through its catalog, which pins a tagged commit
of this repository. They also serve as worked examples for writing your own.

| Machine | Kind | What it shows |
| --- | --- | --- |
| [2op FM](machines/fm2op) | Synth | A small fixed-point FM voice with an animated orbit display |
| [Macro Osc](machines/macroosc) | Synth | A port of Mutable Instruments Braids, built from vendored sources |
| [Karplus](machines/karplus) | Synth | A plucked string with an animated screen and host sound tests |
| [Timestretch](machines/timestretch) | Sample engine | Tempo-locked, pitch-preserving loops on the stock drum page |
| [Ring Mod](machines/ringmod) | Send effect | Ring modulation with the stock mixer handling Dry |
| [Ping Pong](machines/pingpong) | Send effect | A ping-pong delay with stock delay's tempo-synced controls, in the send slot's scratch memory (SDK 1.2) |

Each folder is one machine: `machine.json` (kind, SDK version and the numbers it
prefers), `definition.json` (controls), its C++ and, for a synth, its artwork.
Some have their own sound tests in `tests/`.

| Extension | What it shows |
| --- | --- |
| [Retrospective capture](extensions/capture) | Double-tap Record to replace the sequence with what you just played: claims a key, replaces notes with Undo, tests its gesture with fake actions |

Each extension folder holds `extension.json` (its ID, the builder's text, and the
claims and actions it uses from the SDK's registry), `extension.cpp` and its
tests in `tests/`. Extensions use only the public `<opxy/extension.hpp>` events and
actions; see the SDK's [extensions guide](https://github.com/etcetc000/open-xy/blob/staging/docs/extensions.md).

To write a machine of your own, start from the
[machine template](https://github.com/etcetc000/open-xy-machine-template), not
from this repository.

## Checks

Clone the SDK next to this repository, then from here:

```sh
python ../open-xy/sdk.py test --machine machines/fm2op --machine machines/karplus
python ../open-xy/sdk.py test --extension extensions/capture
python ../open-xy/sdk.py preview --machine machines/fm2op --out ../previews/fm2op-001
python ../open-xy/tools/serve_harness.py --preview ../previews/fm2op-001
```

CI checks every machine and extension and compiles the firmware module with all
of them on each push. Releases are tags; the SDK's `catalog/machines.json` and
`catalog/extensions.json` pin one commit per machine or extension.

## Licences

MIT ([LICENSE](LICENSE)), except vendored code under its own terms:

- Macro Osc's `vendor/` holds Mutable Instruments Braids and stmlib, MIT
  ([vendor/LICENSE](machines/macroosc/vendor/LICENSE)).
- Timestretch's `analysis.h` and `stretch.h` adapt Capicola, which is GNU AGPL
  version 3 ([vendor/README.md](machines/timestretch/vendor/README.md)).
