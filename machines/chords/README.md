# Chords note player

Hold modifiers on the lower left octave and play roots on the upper right.
The eight modifiers follow Orchid's order: **Dim, Min, Maj, Sus** in the
upper row, **6, m7, M7, 9** below. On OP-XY, types use black keys and
extensions use white keys. Four spare keys select performance settings,
keeping the existing str/dir/voi/spr header.

At the default keyboard octave:

| Keys, left to right | Role |
| --- | --- |
| F♯3 G♯3 A♯3 C♯4 | Chord types: dim, min, maj, sus4 |
| F3 G3 A3 B3 | Extensions: 6, m7, M7, 9 |
| C4 D4 E4 | Select Simple, Advanced, Free |
| D♯4 | Toggle Add Note / Play Chord |
| F4..E5, and notes outside F3..E4 | Roots |

Modifiers alone are silent. Extensions require a chord type. With no type
held, a root plays alone. Multiple extensions combine; the last pressed type
wins when several types are held. This corresponds to Orchid with Secret
Chords disabled; its additional multi-type combinations are not implemented.

Keyboard octave shifts preserve physical roles. Held inputs release by their
original notes even after a shift. Existing sequences made with the earlier
modifier layout need their modifier notes remapped.

## Play styles

| Gesture | Simple | Advanced | Free |
| --- | --- | --- | --- |
| Hold type, then root | Chord | Chord | Chord |
| Hold root, then type | Keeps single note | Starts chord | Starts chord |
| Release/repress type while root stays down | Keeps captured chord | Keeps captured chord | Retriggers or changes chord |
| Release root | Releases tones | Releases tones | Releases tones |

Simple is the default. Advanced allows one chord activation per root press.
Free allows repeated changes while roots stay down. Changes apply to all held
roots, up to eight roots and eight tones per root.

In Advanced and Free, adding an extension uses the selected addition mode:

- **Add Note** (default): starts only the newly requested extension, preserving
  existing notes, their attacks and queued strum tones.
- **Play Chord**: releases and retriggers the expanded chord.

Releasing modifiers does not remove captured tones. They remain until root
release or chord replacement. Repressing a captured extension in Add Note
mode does not duplicate it; Play Chord retriggers it. Changing a setting
alone does not retrigger a chord.

Settings survive stop/all-notes-off but reset to Simple/Add when the player
object is recreated. They are not saved as project parameters; recorded
setting-key events can select them during playback.

## Encoders and screen

| Encoder | Caption | Control | Range |
| --- | --- | --- | --- |
| 1 | str | Strum | 0..63 player steps between tones |
| 2 | dir | Direction | up, dn, alt or rnd |
| 3 | voi | Voicing | -6..+6 inversion steps |
| 4 | spr | Spread | off/on; every second tone rises one octave |

Voicing moves the lowest note up an octave per positive step, or the highest
down per negative step. With a single note, each step shifts that note a full
octave. Defaults are str 0, dir up, voi 0 and spr off.

The upper keyboard shows held modifiers on the left, the selected mode and
the last root on the right. The line beneath shows the mode and that root's
captured chord, which stays correctly named while preparing other modifiers
in Simple mode. The bottom keyboard shows all emitted notes across held
chords, fitting the range across octaves. Queued strum tones stay dark until
they sound; added extensions appear when emitted.

## Reference and verification

Behavior follows Telepathic's official
[Playstyles guide](https://support.telepathicinstruments.com/hc/en-us/articles/15280843614863-What-s-the-Difference-Between-Playstyles),
[extension guide](https://support.telepathicinstruments.com/hc/en-us/articles/16576229505167-Chord-Extensions-Explained),
and [Options reference](https://support.telepathicinstruments.com/hc/en-us/articles/15280847893263-Navigating-the-Options-Menu-A-Complete-Overview).

These describe press orders and addition modes, but do not fully specify
extension releases or every Advanced/Free transition. Retaining extensions
until root release and allowing one chord activation per held root in
Advanced are explicit implementation choices, not verified Orchid behavior.
This implements the documented modifier gestures, not complete Orchid firmware
equivalence. Hardware comparison is still required.

[tests/gestures.cpp](tests/gestures.cpp) covers types/extensions, both press
orders, all styles, addition modes, releases, simultaneous roots, octave
shifts, voicing, strum and display state. The SDK separately tests native
record updates, capacity, scheduled tones, reset callbacks and stale updates.

## Build

Requires SDK 1.7 with the optional live-update/reset callbacks.
Build with `sdk.py build --machine <folder> --stock-os <stock>/os.ldr --out <build>`
and select **chords** as the track's player. Preserve the resulting
`layout.json` for future builds. Stock firmware loads an unknown saved player
as Arpeggio.

For the original experimental player PR's projects, use
`--layout <this-folder>/legacy-layout.json` on the first updated build. This
preserves player identity, not the earlier modifier-note mapping.

The engine is [player.cpp](player.cpp). Regenerate the retained
[panel.svg.fragment](panel.svg.fragment) with `python artwork.py`.
