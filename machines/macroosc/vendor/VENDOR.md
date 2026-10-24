# Vendored Braids DSP

Source: [pichenettes/eurorack](https://github.com/pichenettes/eurorack) at
`08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4`, Braids and stmlib, MIT
([LICENSE](LICENSE), copyright Emilie Gillet). Only the macro-oscillator DSP,
its headers and its regenerated tables are vendored; the module firmware (UI,
drivers, settings storage, envelope, quantizer) is not.

[VENDOR.json](VENDOR.json) records the SHA-256 of every file and whether it is
byte-identical to upstream (line endings aside) or patched. Patched files carry
`OP-XY:` comments.

| File | Patch |
| --- | --- |
| `braids/digital_oscillator.h` | Delay lengths halved for 44.1 kHz; delay-line union made a public type held by pointer (`set_delay_lines`) so lines come from a shared pool; `HarmonicsState` holds a cached spectrum. |
| `braids/digital_oscillator.cc` | Delay-line member accesses use `->` for the pointer; `RenderHarmonics` recomputes its spectrum only when the parameters change (identical output). |
| `braids/macro_oscillator.h` | Forwards `set_delay_lines`; adds a `shape()` accessor. |
| `braids/resources.cc`, `braids/resources.h` | Regenerated with the upstream table scripts at 44,100 Hz instead of 96,000 Hz. |

To refresh from a newer upstream, copy the same files, reapply the patches
above, regenerate the tables at 44,100 Hz with the upstream scripts, rebuild
`VENDOR.json` and rerun `sdk.py compile`.
