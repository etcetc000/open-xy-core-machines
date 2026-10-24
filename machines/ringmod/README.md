# Ring Mod send effect

A stereo ring modulator: multiply incoming audio by a bipolar oscillator for
slow pulses, clangorous percussion and metallic tones. Both channels share the
carrier phase, preserving their stereo relationship without cross-channel mixing.

| Encoder | Control | Mapping |
| --- | --- | --- |
| 1 | Freq | Carrier frequency, approximately 0.25 Hz to 4096 Hz, exponential |
| 2 | Shape | Smoothly morphs the carrier from sine to square |
| 3 | Low Cut | Removes bass before modulation; zero bypasses the filter |
| 4 | Dry | Restores source dry signal through the stock mixer; wet level stays unchanged |

The FX page uses stock percentage readouts. Frequency follows approximately
`0.25 * 2^(14 * value / 100)` Hz: 0 is 0.25 Hz, 25 is 2.8 Hz, 50 is 32 Hz,
75 is 362 Hz and 100 is 4096 Hz. Defaults are Freq 75, Shape 0, Low Cut 0, Dry 0.
Shift + turn provides fine adjustment.

Start with a drum or sustained sound sent to Ring Mod. Low frequency settings
produce pulses and polarity reversals; higher settings add metallic sidebands.
Raise Shape for a more hollow, buzzy sound. Low Cut keeps bass out of the
modulated return. With one send fully up, Dry zero gives only the effect and
Dry 100 retains the original alongside it. Raising Dry does not reduce the wet
return. Track send and return levels still determine the amount of effect.

Low Cut is a first-order input high-pass, approximately
`10000 * (value / 100)^2` Hz at 44.1 kHz, with a 6 dB/octave slope. A value of
10 is about 100 Hz, 25 is 625 Hz, and 50 is 2.5 kHz. Modulation can create
frequencies below the cutoff because the filter runs before the multiplier.

## Implementation

`fx.cpp` implements the [effect interface](https://github.com/etcetc000/open-xy/blob/staging/docs/api.md#effects) in portable
C++11 with 32-bit integer arithmetic and 28 bytes of state. A quarter-wave sine
table and a square carrier with polynomial edge correction avoid a large audio
buffer. The edge correction reduces square-wave aliasing; high-frequency input
and modulation sidebands can still alias. Frequency, Shape and Low Cut changes
are smoothed over roughly 1.5 ms, and the oscillator phase continues across blocks.
There is no allocation, I/O or gain normalization in the callback.

The optional `dryParameter` declaration delegates Dry to the stock mixer.
The DSP writes only the modulated return and never adds clean signal itself.
See [native builds](https://github.com/etcetc000/open-xy/blob/staging/docs/build.md) for collection verification.
