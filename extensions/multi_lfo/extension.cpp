// Four LFOs per instrument track, all running at once, each edited on stock's
// own LFO page.
//
// On the LFO page, Shift + turning encoder 1 shows the next or previous LFO
// (1-4, round). The SDK swaps it into stock's LFO storage, so stock's page, its
// encoders and clicks, M4, the Shift + M4 type list and the live display edit
// it, and the LFO it replaces runs on as a copy. A short popup names the LFO
// now shown. Everything else on the page stays stock's.
#include <opxy/extension.hpp>

namespace opxy { namespace extensions { namespace multi_lfo {
namespace {
bool layer(const Context&, const Layer& event, const Actions& actions) {
    // Stock's LFO page does nothing with Shift + encoder 1: take only that.
    if (event.kind != Layer::turn || !event.shift || event.key != 0) return false;
    const u32 shown = actions.shown_lfo(event.track);
    const u32 next = (shown + (event.delta > 0 ? 1u : lfo_count - 1)) % lfo_count;
    if (actions.show_lfo(event.track, next)) {
        static char text[] = "LFO 1";
        text[4] = static_cast<char>('1' + next);
        actions.notify(text);
    }
    return true;
}
}
extern constexpr Hooks hooks = Hooks().with_layer(layer);
}}}
