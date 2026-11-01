"""Generate the Chords page's retained keyboard artwork (480 x 220)."""
from pathlib import Path

WHITE = (0, 2, 4, 6, 7, 9, 11)  # F-to-E key positions
BLACK = (1, 3, 5, 8, 10)
parts = ['<g id="chords_panel">', '<rect x="0" y="20" width="480" height="200" fill="#000000"/>']


def rect(x, y, w, h, fill):
    parts.append(f'<rect x="{x:g}" y="{y:g}" width="{w:g}" height="{h:g}" fill="{fill}" stroke="#000000" stroke-width="1"/>')


def text(x, y, value, size=18):
    parts.append(f'<text transform="matrix(1 0 0 1 {x:g} {y:g})" fill="#D2D2D2" font-family="UniversNextPro-Light" font-size="{size}">{value}</text>')


def dot(identity, x, y, black, radius=4):
    fill = '#F0F0F0' if black else '#000000'
    parts.append(f'<circle id="{identity}" transform="matrix(1 0 0 1 {x:g} {y:g})" cx="0" cy="0" r="{radius}" fill="{fill}" stroke="{fill}" stroke-width="0.5"/>')


width = 444 / 14
for octave in range(2):
    for i in range(7):
        rect(18 + (octave * 7 + i) * width, 91, width, 51, '#B4B4B4' if octave == 0 else '#F0F0F0')
    for i in (1, 2, 3, 5, 6):
        rect(18 + (octave * 7 + i - 0.3) * width, 91, width * 0.6, 30, '#000000')

for i, label in enumerate(('6', 'm7', 'M7', '9', '11', '13', 'b9')):
    x, y = 18 + (i + 0.5) * width, 164 if i % 2 == 0 else 188
    if i % 2:
        parts.append(f'<path d="M{x:g} 145 V168" fill="none" stroke="#777777" stroke-width="0.5"/>')
    text(x - len(label) * 5, y, label, 20)
for column, (i, label) in enumerate(zip((1, 2, 3, 5), ('dim', 'min', 'maj', 'sus'))):
    x, y = 18 + i * width, 57 if column % 2 == 0 else 80
    if column % 2 == 0:
        parts.append(f'<path d="M{x:g} 61 V88" fill="none" stroke="#777777" stroke-width="0.5"/>')
    text(x - len(label) * 4.5, y, label)

# Root bits 0..11 occupy the right half; modifier bits 12..23 the left.
for key in range(24):
    pc, octave = key % 12, 1 if key < 12 else 0
    black = pc in BLACK
    position = (1, 2, 3, 5, 6)[BLACK.index(pc)] if black else WHITE.index(pc) + 0.5
    dot(f'chords_dot_{key}', 18 + (octave * 7 + position) * width, 113 if black else 133, black, 4)
parts.append('<text id="chords_text" transform="matrix(1 0 0 1 240 215)" fill="#F2F2F2" font-family="UniversNextPro-Light" font-size="22">chords</text>')
parts.append('</g>')
Path(__file__).with_name('panel.svg.fragment').write_text('\n'.join(parts) + '\n', encoding='utf-8', newline='\n')
