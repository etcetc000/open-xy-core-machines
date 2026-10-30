# Chords note player

Hold modifiers with your left hand, then press a root with your right hand.
Each root captures the modifiers held when it is pressed. Changing or releasing
modifiers affects the next root, not a chord already sounding. Releasing the
root releases its chord. This Simple behavior is always active; there are no
play-style or extension-addition mode controls.

At the default keyboard octave:

| Keys, left to right | Role |
| --- | --- |
| F♯3 G♯3 A♯3 C♯4 | Types: dim, min, maj, sus4 |
| F3 G3 A3 B3 C4 D4 E4 | Extensions: 6, m7, M7, 9, 11, 13, b9 |
| D♯4 | Unused; silent |
| F4..E5, and notes outside F3..E4 | Roots |

The four types and first four extensions retain Orchid's row order, adapted
to black and white keys. The former Simple/Advanced/Free keys are now 11,
13 and flat 9. The former Add/Full key is unused. Previously recorded mode-key
events now act as extensions and must be removed or remapped in old sequences.

Modifiers alone are silent. Extensions require a type; without one, a root
plays alone. The most recently pressed held type wins. Multiple extensions
combine: 6, m7, M7, 9, 11, 13 and b9 add 9, 10, 11, 14, 17, 21 and 13
semitones respectively. They add only the selected intervals, not an implied
stack of all lower extensions. Secret Chords combinations are not implemented.

If 6 and 13 are both held, 13 takes precedence. At most eight notes can sound
per root; the fullest combinations omit the fifth to keep the root, third or
suspension, and every remaining selected extension. Up to eight roots can be
held together. Notes outside MIDI 0..127 after voicing/spread are omitted.

Octave shifts preserve physical key roles. Held inputs release by their
original notes even after a shift. Stop/all-notes-off clears held inputs.

## Encoders and screen

| Encoder | Caption | Control | Range |
| --- | --- | --- | --- |
| 1 | str | Strum | 0..63 player steps between tones |
| 2 | dir | Direction | up, dn, alt or rnd |
| 3 | voi | Voicing | -6..+6 inversion steps |
| 4 | spr | Spread | off/on; every second tone rises one octave |

Voicing moves the lowest note up an octave per positive step, or the highest
down per negative step. With a single note, each step shifts it a full octave.
Defaults are str 0, dir up, voi 0 and spr off.

The upper keyboard shows held modifiers on the left and the last root on the
right. The chord-name line describes that root's captured chord without mode
labels. The bottom keyboard shows every emitted note across held chords,
including voicing and spread. Strummed notes light as they sound; root release
clears that root's notes. The output row expands to fit the note range.

## Build and tests

Requires SDK 1.7. Build with
`sdk.py build --machine <folder> --stock-os <stock>/os.ldr --out <build>`
and select **chords** as the track's player. Preserve the resulting
`layout.json` for future builds. Stock firmware loads an unknown saved player
as Arpeggio. For the original experimental player PR's projects, use
`--layout <this-folder>/legacy-layout.json` on the first updated build. This
preserves player identity, not the earlier modifier-note mapping.

[tests/gestures.cpp](tests/gestures.cpp) covers every type/extension, all 128
extension combinations, Simple input/release behavior, capacity, octave shifts,
voicing, strum and captured display state. Run with `sdk.py test --machine <folder>`.
The engine is [player.cpp](player.cpp). Regenerate
[panel.svg.fragment](panel.svg.fragment) with `python artwork.py`.
