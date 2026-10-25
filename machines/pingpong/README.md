# Ping Pong

A ping-pong delay for the OP-XY, built with the
[Open-XY SDK](https://github.com/etcetc000/open-xy): the example of a send
effect with delay memory, published through the catalog.

The input repeats on the left, then on the right, then on the left again, each
repeat a little quieter and darker. Its controls work like the OP-XY's own
delay: Size picks the same tempo-synced note lengths, and the send page names
them the same way. Each side keeps a one-second line in the send slot's
scratch memory, the same memory the OP-XY's own delay uses, which the effect
declares as `bufferBytes`; it reads the tempo through `transport` (SDK 1.2).

| Control | What it does |
| --- | --- |
| Size | Time between repeats in note lengths at the project tempo: 1/32, 1/32 dotted, 1/16, 1/16 dotted, 1/8, 1/8 dotted, 1/4 or 1/2, as stock delay; a new length glides like tape |
| Fine | Moves the time up to 50 ms either way |
| Feedback | How much of each repeat crosses to the other side, up to 95% |
| Dry | How much of the source the stock mixer keeps at full send |

Repeats stay between about 12 ms and 1 s, as stock delay's do; a half note at
a slow tempo stops at one second.

## Files

| File | Contents |
| --- | --- |
| [machine.json](machine.json), [definition.json](definition.json) | Kind, SDK version, controls, the buffer size and the tempo |
| [core.h](core.h), [fx.cpp](fx.cpp) | The two lines, the crossing feedback and the tone filters |
| [tests/sound.cpp](tests/sound.cpp) | Sound checks: repeat timing and sides, every note length and Fine, a stale buffer never heard, silence without a buffer, decay at full feedback |

The buffer is never cleared, so the effect reads only what it has written
since it started; outside a send slot it has no buffer and stays silent.

## Checks

```sh
python ../open-xy/sdk.py test --machine machines/pingpong --firmware
```

CI runs the same on every push. MIT licensed.
