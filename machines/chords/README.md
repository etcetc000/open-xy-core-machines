# Chords note player

Hold chord types and extensions on the lower left octave with your left hand
and play roots on the upper right octave with your right hand; every root
sounds the held chord. It plays whichever instrument
the track uses, live or from the sequencer.

At the default keyboard octave the OP-XY's keys run F3 to E5:

| Keys | Role |
| --- | --- |
| F4..E5, and every key outside F3..E4 | Chord roots |
| F3 G3 A3 B3 C4 D4 E4 | Chord types: dim, aug, min, maj, sus (sus4), dom (dominant 7) and half (half-diminished 7). The last one pressed wins |
| F♯3 G♯3 A♯3 C♯4 D♯4 | Extensions 6, m7, M7, 9 and 11 (+9, +10, +11, +14 and +17 semitones). They stack |

With no chord type held, a root plays alone. Extensions only apply with a
chord type. Combinations build richer chords: maj + m7 is a dominant 7th,
dim + 6 a diminished 7th, min + m7 + 9 a minor 9th, dom + 9 + 11 an 11th.
A tone that two keys add sounds once, so a chord holds at most eight tones.
Modifier keys are ordinary notes, so sequencer steps on that octave change the
chord during playback. Keyboard octave shifts preserve the left-hand modifier
and right-hand root roles. Patterns recorded with the previous reversed split
need their modifier and root notes moved to the corresponding new octaves.

| Encoder | Caption | Control | Range |
| --- | --- | --- | --- |
| 1 | str | Strum | 0..63: the delay between chord tones, in player steps |
| 2 | dir | Direction | up, dn, alt (alternating each chord) or rnd (random first tone) |
| 3 | voi | Voicing | -6..+6 inversion steps: each step up moves the lowest note an octave up, each step down the highest an octave down |
| 4 | spr | Spread | off or on: every second note rises an octave |

The upper keyboard shows held chord types and extensions on the left and the
last root on the right, with the chord name beneath it. The lower keyboard
shows every emitted note across held chords, including voicing and spread.
Strummed notes light as they sound and disappear when their root is released.
The lower row expands to fit the full note range, keeping octaves distinct.
The header retains strum, direction, voicing and spread.

Requires the SDK note-player interface, version 1.7. Build with this machine's
folder using `sdk.py build --machine <folder> --stock-os <stock>/os.ldr --out <build>`;
select **chords** as a track's player. Preserve the build's layout for future
builds. Firmware without this player loads its saved type as Arpeggio.

Defaults are str 0, dir up, voi 0 and spr off.

The chord engine and page logic are [`player.cpp`](player.cpp);
[`tests/chords.cpp`](tests/chords.cpp) holds their vectors and
[`panel.svg.fragment`](panel.svg.fragment) the page artwork.

Regenerate the retained artwork with `python artwork.py`.

For projects made with the original experimental player PR, pass
`--layout <this-folder>/legacy-layout.json` on the first updated build. It
preserves Chords' original player type. After that, pass the resulting build's
`layout.json`. Fresh builds otherwise assign a new player type by name.
