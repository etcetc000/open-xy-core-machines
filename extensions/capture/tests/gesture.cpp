// Capture's gesture end to end: its callbacks driven as the SDK runtime calls them,
// with a fake action table standing in for the SDK's note writes and popups.
#include "extension.cpp"

using namespace opxy;
using namespace opxy::extensions;
#define CHECK(value) do { if (!(value)) return __LINE__; } while (false)

namespace {
u32 replaces, popups, written_track, written_count, refuse;
Note written[120];
u32 replace(const Context&, u32 track, const Note* notes, u32 count) {
    if (refuse) return 0;
    ++replaces; written_track = track; written_count = count;
    for (u32 i = 0; i < count; ++i) written[i] = notes[i];
    return count;
}
u32 insert(const Context&, u32, const Note*, u32) { return 0; }
void notify(const char*) { ++popups; }
const Actions actions = {insert, notify, replace};

Context context(bool playing, u32 pattern = 2) {
    Context c = {};
    c.recording = {1, pattern, 3, 16 * 480, 0, 0, true, true};
    c.playing = playing;
    return c;
}
Time now;
void key(const Context& c, bool pressed, bool accepted = true, bool unmodified = true, Time elapsed = 50000) {
    now += elapsed;
    const Control control = {keys::record, pressed, accepted, unmodified, now};
    capture::hooks.queued_control(c, control, actions);
}
void double_tap(const Context& c) { key(c, true); key(c, false); key(c, true); key(c, false); }
void play_note(const Context& c, u32 pitch, i32 position, u32 ticks) {
    const LiveNote note = {3, pitch, 90, position, 180};
    capture::hooks.recordable_note_on(c, note);
    capture::hooks.recording_tick(c, Tick{ticks << 8, false});
    capture::hooks.selected_note_off(c, NoteOff{3, pitch});
}
}

int main() {
    const Context playing = context(true), stopped = context(false);
    capture::hooks.context(stopped);
    // Nothing played: a double-tap writes nothing.
    double_tap(stopped); CHECK(replaces == 0 && popups == 0);

    // Play two notes, stop, double-tap: the take replaces the sequence, then a popup.
    capture::hooks.context(playing);
    play_note(playing, 60, 0, 120);
    play_note(playing, 64, 480, 60);
    capture::hooks.context(stopped);
    double_tap(stopped);
    CHECK(replaces == 1 && popups == 1 && written_track == 3 && written_count == 2);
    CHECK(written[0].pitch == 60 && written[0].length == 120 && written[1].position == 480);
    // The take is spent: another double-tap writes nothing.
    double_tap(stopped); CHECK(replaces == 1);

    // A refused write keeps the take for another try, without a popup.
    capture::hooks.context(playing);
    play_note(playing, 67, 0, 30);
    capture::hooks.context(stopped);
    refuse = 1; double_tap(stopped); CHECK(replaces == 1 && popups == 1);
    refuse = 0; double_tap(stopped); CHECK(replaces == 2 && popups == 2 && written[0].pitch == 67);

    // A chord or a release stock did not accept never captures, and a stuck tap
    // does not eat the next double-tap.
    capture::hooks.context(playing);
    play_note(playing, 69, 0, 30);
    capture::hooks.context(stopped);
    key(stopped, true, true, false); key(stopped, false); key(stopped, true, true, false); key(stopped, false);
    CHECK(replaces == 2);
    key(stopped, true); key(stopped, false, false);
    double_tap(stopped); CHECK(replaces == 3 && written[0].pitch == 69);

    // A context change between the taps cancels the gesture.
    capture::hooks.context(playing);
    play_note(playing, 71, 0, 30);
    capture::hooks.context(stopped);
    key(stopped, true); key(stopped, false);
    const Context moved = context(false, 5);
    capture::hooks.context(moved);
    key(moved, true); key(moved, false);
    CHECK(replaces == 3);
    return 0;
}
