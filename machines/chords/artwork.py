"""Generate the Chords page's retained keyboard artwork (480 x 220)."""
from pathlib import Path

WHITE = (0, 2, 4, 6, 7, 9, 11)  # F-to-E key positions
BLACK = (1, 3, 5, 8, 10)
parts = ['<g id="chords_panel">', '<rect x="0" y="20" width="480" height="200" fill="#000000"/>']


def rect(x, y, w, h, fill):
    parts.append(f'<rect x="{x:g}" y="{y:g}" width="{w:g}" height="{h:g}" fill="{fill}" stroke="#000000" stroke-width="1"/>')


def text(x, y, value, size=13):
    parts.append(f'<text transform="matrix(1 0 0 1 {x:g} {y:g})" fill="#D2D2D2" font-family="UniversNextPro-Light" font-size="{size}">{value}</text>')


def dot(identity, x, y, black, radius=4):
    fill = '#F0F0F0' if black else '#000000'
    parts.append(f'<circle id="{identity}" transform="matrix(1 0 0 1 {x:g} {y:g})" cx="0" cy="0" r="{radius}" fill="{fill}" stroke="{fill}" stroke-width="0.5"/>')


width = 444 / 14
for octave in range(2):
    for i in range(7):
        rect(18 + (octave * 7 + i) * width, 50, width, 43, '#B4B4B4' if octave == 0 else '#F0F0F0')
    for i in (1, 2, 3, 5, 6):
        rect(18 + (octave * 7 + i - 0.3) * width, 50, width * 0.6, 26, '#000000')

for i, label in enumerate(('6', 'm7', 'M7', '9', 'simp', 'adv', 'free')):
    text(18 + (i + 0.5) * width - len(label) * 2.5, 110, label, 11)
for i, label in zip((1, 2, 3, 5, 6), ('dim', 'min', 'maj', 'sus', 'add/full')):
    text(18 + i * width - len(label) * (2 if i == 6 else 2.5), 42, label, 8 if i == 6 else 11)

for x, w in ((18, 219), (243, 219)):
    parts.append(f'<path d="M{x} 119 H{x+w}" fill="none" stroke="#B4B4B4" stroke-width="0.5"/>')
text(18, 136, 'modifier')
text(439, 136, 'play')

# Root bits 0..11 occupy the right half; modifier bits 12..23 the left.
for key in range(24):
    pc, octave = key % 12, 1 if key < 12 else 0
    black = pc in BLACK
    position = (1, 2, 3, 5, 6)[BLACK.index(pc)] if black else WHITE.index(pc) + 0.5
    dot(f'chords_dot_{key}', 18 + (octave * 7 + position) * width, 67 if black else 84, black)
parts.append('<text id="chords_text" transform="matrix(1 0 0 1 240 157)" fill="#F2F2F2" font-family="UniversNextPro-Light" font-size="18">simple/add</text>')

# Every MIDI pitch has a retained key and activity dot. The SDK fits the
# visible range and positions these; white keys precede black keys in paint order.
def black(note): return note % 12 in (1, 3, 6, 8, 10)
for is_black in (False, True):
    for note in range(128):
        if black(note) != is_black:
            continue
        w, h = (6, 27) if is_black else (10, 44)
        fill = '#000000' if is_black else '#F0F0F0'
        parts.append(f'<path id="chords_key_{note}" transform="matrix(1 0 0 1 1024 174)" d="M0 0 H{w} V{h} H0 Z" fill="{fill}" stroke="#000000" stroke-width="0.5"/>')
for note in range(128):
    dot(f'chords_played_{note}', 1024, 0, black(note), 2.5)
parts.append('</g>')
Path(__file__).with_name('panel.svg.fragment').write_text('\n'.join(parts) + '\n', encoding='utf-8', newline='\n')
