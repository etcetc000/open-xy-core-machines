# Ping Pong

A ping-pong delay for the OP-XY, built with the
[Open-XY SDK](https://github.com/etcetc000/open-xy): the example of a send
effect with delay memory, published through the catalog.

The input repeats on the left, then on the right, then on the left again, each
repeat a little quieter and darker. Each side keeps a one-second line in the
send slot's scratch memory, the same memory the OP-XY's own delay uses, which
the effect declares as `bufferBytes` (SDK 1.1).

| Control | What it does |
| --- | --- |
| Time | 10 ms to 1 s between repeats; a new time glides like tape |
| Feedback | How much of each repeat crosses to the other side, up to 95% |
| Tone | Bright repeats, down to dark and muffled ones |
| Dry | How much of the source the stock mixer keeps at full send |

## Files

| File | Contents |
| --- | --- |
| [machine.json](machine.json), [definition.json](definition.json) | Kind, SDK version, controls and the buffer size |
| [core.h](core.h), [fx.cpp](fx.cpp) | The two lines, the crossing feedback and the tone filters |
| [tests/sound.cpp](tests/sound.cpp) | Sound checks: repeat timing and sides, a stale buffer never heard, silence without a buffer, decay at full feedback |

The buffer is never cleared, so the effect reads only what it has written
since it started; outside a send slot it has no buffer and stays silent.

## Checks

```sh
python ../open-xy/sdk.py test --machine machines/pingpong --firmware
```

CI runs the same on every push. MIT licensed.
