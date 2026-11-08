// Four LFOs per instrument track: stock's own (LFO 1) and the SDK's three
// copies of it (LFOs 2-4), which the audio role runs through stock's LFO code.
//
// On the LFO page, Shift + turning encoder 1 picks LFO 1-4. LFO 1 is stock's
// page, untouched. With LFO 2-4 picked, this layer paints over the page and
// takes its controls, laid out as stock's LFO page lays them out:
// - encoders 1-4 turn the LFO's parameters (1% a detent),
// - clicking an encoder steps a stepped parameter (destination, source, ...),
// - M4 turns the LFO on or off, and Shift + M4 steps its type.
// An LFO picked for the first time starts as a copy of LFO 1, off.
#include <opxy/extension.hpp>

namespace opxy { namespace extensions { namespace multi_lfo {
namespace {
const i32 kTop = 0x7fffffff;
const i32 kDetent = kTop / 100;
const u32 kTypes[] = {lfo_types::tremolo, lfo_types::value, lfo_types::random, lfo_types::element,
                      lfo_types::duck};
const u32 kTypeCount = sizeof(kTypes) / sizeof(kTypes[0]);

// Layer nodes, in extension.json order.
enum Node : u32 { panel, title, kind, state, value0, hint = value0 + 4 };

u32 track;     // the LFO page's instrument track
u32 selected;  // 0: stock's LFO; 1-3: its copies
Lfo shown;     // the selected copy, as last read or set

const char* type_name(u32 type) {
    switch (type) {
    case lfo_types::tremolo: return "tremolo";
    case lfo_types::value: return "value";
    case lfo_types::random: return "random";
    case lfo_types::element: return "element";
    case lfo_types::duck: return "duck";
    }
    return "?";
}

// The word (0-7, of words 24-31) an encoder turns, as stock's LFO page maps
// it; -1 for none.
i32 turn_word(u32 type, u32 encoder, bool shift) {
    if (!shift) return type == lfo_types::random && encoder == 3 ? 7 : static_cast<i32>(encoder);
    if (type == lfo_types::tremolo && encoder == 3) return 7;
    if (type == lfo_types::random && encoder == 2) return 3;
    return -1;
}

// The word an encoder click steps, and its steps (0: not stepped), as stock.
struct Click { i32 word; u32 steps; };
Click click(u32 type, u32 encoder) {
    static const Click table[5][4] = {
        {{4, 0}, {1, 0}, {2, 0}, {7, 5}},  // tremolo
        {{4, 4}, {1, 0}, {2, 6}, {3, 4}},  // value
        {{4, 0}, {1, 0}, {2, 6}, {7, 4}},  // random
        {{0, 4}, {1, 0}, {2, 4}, {3, 4}},  // element
        {{4, 2}, {1, 0}, {2, 0}, {3, 0}},  // duck
    };
    for (u32 t = 0; t < kTypeCount; ++t)
        if (kTypes[t] == type && encoder < 4) return table[t][encoder];
    return {-1, 0};
}

// A stepped word holds its step in equal parts of the positive range.
u32 step_of(i32 word, u32 steps) {
    const u32 step = static_cast<u32>((static_cast<__UINT64_TYPE__>(word < 0 ? 0 : word) * steps) >> 31);
    return step < steps ? step : steps - 1;
}
i32 step_word(u32 step, u32 steps) {
    return static_cast<i32>((static_cast<__UINT64_TYPE__>(2 * step + 1) << 31) / (2 * steps));
}

i32 add(i32 word, i32 delta) {
    const __INT64_TYPE__ next = static_cast<__INT64_TYPE__>(word) + static_cast<__INT64_TYPE__>(delta) * kDetent;
    return next < 0 ? 0 : next > kTop ? kTop : static_cast<i32>(next);
}

bool pristine(const Lfo& lfo) {
    if (lfo.on || lfo.type) return false;
    for (u32 i = 0; i < 8; ++i)
        if (lfo.words[i]) return false;
    return true;
}

bool store(const Actions& actions) { return actions.set_lfo(track, selected - 1, shown); }

void pick(const Actions& actions, i32 delta) {
    selected = (selected + (delta > 0 ? 1u : 3u)) % 4;
    static char text[] = "LFO 1";
    text[4] = static_cast<char>('1' + selected);
    if (selected && actions.get_lfo(track, selected, shown) && pristine(shown)) {
        Lfo stock = {};
        if (actions.get_lfo(track, 0, stock)) {
            shown = stock;
            shown.on = false;
            store(actions);
        }
    }
    actions.notify(text);
}

bool layer(const Context&, const Layer& event, const Actions& actions) {
    switch (event.kind) {
    case Layer::open:
        track = event.track;
        selected = 0;
        return false;
    case Layer::close:
        selected = 0;
        return false;
    case Layer::frame:
        // A project load may have changed it.
        if (selected) actions.get_lfo(track, selected, shown);
        return false;
    case Layer::turn: {
        if (event.shift && event.key == 0) {
            pick(actions, event.delta);
            return true;
        }
        if (!selected) return false;
        const i32 word = turn_word(shown.type, event.key, event.shift);
        if (word >= 0) {
            shown.words[word] = add(shown.words[word], event.delta);
            store(actions);
        }
        return true;
    }
    case Layer::keypress: {
        if (!selected) return false;
        if (event.key == keys::m4) {
            if (!event.pressed) return true;
            if (event.shift) {
                u32 t = 0;
                while (t < kTypeCount && kTypes[t] != shown.type) ++t;
                shown.type = kTypes[(t + 1) % kTypeCount];
            } else {
                shown.on = !shown.on;
            }
            store(actions);
            return true;
        }
        if (event.key >= keys::encoder_click && event.key < keys::encoder_click + 4) {
            if (!event.pressed || event.shift) return true;
            const Click c = click(shown.type, event.key - keys::encoder_click);
            if (c.word >= 0 && c.steps) {
                shown.words[c.word] = step_word((step_of(shown.words[c.word], c.steps) + 1) % c.steps, c.steps);
                store(actions);
            }
            return true;
        }
        return false;
    }
    }
    return false;
}

// "42%", or "2/4" for a stepped word.
void describe(char* out, i32 word, u32 steps) {
    u32 n = 0;
    char digits[4];
    u32 count = 0;
    u32 value = steps ? step_of(word, steps) + 1 : static_cast<u32>((static_cast<__UINT64_TYPE__>(word < 0 ? 0 : word) * 100 + (1u << 30)) >> 31);
    do { digits[count++] = static_cast<char>('0' + value % 10); value /= 10; } while (value && count < 4);
    while (count) out[n++] = digits[--count];
    if (steps) {
        out[n++] = '/';
        out[n++] = static_cast<char>('0' + steps % 10);
    } else {
        out[n++] = '%';
    }
    out[n] = 0;
}

void draw(const Canvas& canvas) {
    if (!selected) {
        for (u32 node = panel; node <= hint; ++node) canvas.hide(node);
        return;
    }
    for (u32 node = panel; node <= hint; ++node) canvas.show(node);
    static char name[] = "LFO 1";
    name[4] = static_cast<char>('1' + selected);
    canvas.text(title, name);
    canvas.text(kind, type_name(shown.type));
    canvas.text(state, shown.on ? "on" : "off");
    for (u32 e = 0; e < 4; ++e) {
        char text[8];
        const i32 word = turn_word(shown.type, e, false);
        const Click c = click(shown.type, e);
        describe(text, shown.words[word], c.word == word ? c.steps : 0);
        canvas.text(value0 + e, text);
    }
    canvas.text(hint, "M4 on/off, Shift+M4 type");
}
}
extern constexpr Hooks hooks = Hooks().with_layer(layer).with_draw(draw);
}}}
