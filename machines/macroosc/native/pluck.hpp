#pragma once
#include <opxy/native.hpp>
#include <opxy/types.hpp>

namespace macroosc { namespace native {
using namespace opxy;

// Macro Osc's native routine: Karplus-Strong plucked strings (the Plucked model
// of Mutable Instruments Braids), rendered at half rate in output pairs. A
// firmware build runs native/pluck.s (opxy/native.hpp); elsewhere, and for
// arguments outside the routine's range, the portable reference below runs. The
// reference defines the routine: both give the same samples.
//
// A string's line holds `size` (a power of two, mask = size - 1) words plus a
// guard word line[size] that mirrors line[0] for interpolation. The random
// state is stmlib's: each draw is state = state * 1664525 + 1013904223.
struct PluckString {
    u32 phase;      // advanced by the call
    u32 increment;
    u32 write;      // the next word the string updates, below size
    u32 shift;      // phase >> shift is the line position (0..9)
    u32 mask;
    u32 size;
    u32 excite;     // words of excitation left to write, from the top down
    i16* line;
};

namespace pluck_detail {
inline u32 draw(u32& state) { state = state * 1664525u + 1013904223u; return state; }
// The strings lie `stride` bytes apart (a string may begin a larger record).
inline PluckString& at(PluckString* strings, unsigned i, unsigned stride) {
    return *reinterpret_cast<PluckString*>(reinterpret_cast<unsigned char*>(strings) + i * stride);
}
}

// Per output pair, each string in turn either writes one excitation word
// (when `excite` is left: (line[excite - 1] + 3 * the draw's high half) >> 2,
// also the string's sample) or advances its phase and averages words from
// `write` up to the position two past the phase, each kept unless the draw's
// low half is at most `update_probability`, halved toward zero and, with a
// `loss`, scaled by (32768 - loss) / 32768; the string's sample interpolates its
// line at the phase. The sum of the strings, clipped to +-32767, is the pair's
// second sample; the first is its average with the previous pair's second.
inline void pluck_reference(PluckString* strings, unsigned count, unsigned stride, i16* out, unsigned pairs,
                            u32& random, u32 update_probability, i32 loss, i16& previous) {
    using namespace pluck_detail;
    i32 last = previous;
    while (pairs--) {
        i32 sample = 0;
        for (unsigned i = 0; i < count; ++i) {
            PluckString& p = at(strings, i, stride);
            i16* dl = p.line;
            if (p.excite) {
                --p.excite;
                const i32 excitation = (dl[p.excite] + 3 * static_cast<i16>(draw(random) >> 16)) >> 2;
                dl[p.excite] = static_cast<i16>(excitation);
                sample += excitation;
                continue;
            }
            p.phase += p.increment;
            const u32 read = ((p.phase >> (22 + p.shift)) + 2) & p.mask;
            u32 w = p.write;
            while (w != read) {
                const u32 next = (w + 1) & p.mask;
                const i32 a = dl[w], b = dl[next];
                if ((draw(random) & 0xffff) <= update_probability) {
                    i32 sum = a + b;
                    sum = sum < 0 ? -(-sum >> 1) : (sum >> 1);
                    if (loss) sum = sum * (32768 - loss) >> 15;
                    dl[w] = static_cast<i16>(sum);
                }
                if (w == 0) dl[p.size] = dl[0];
                w = next;
            }
            p.write = w;
            const u32 position = p.phase >> p.shift;
            const i32 l = dl[position >> 22], r = dl[(position >> 22) + 1];
            const u32 product = static_cast<u32>(r - l) * ((position >> 6) & 0xffff);
            sample += static_cast<i16>(l + (static_cast<i32>(product) >> 16));
        }
        if (sample < -32767) sample = -32767;
        if (sample > 32767) sample = 32767;
        *out++ = static_cast<i16>((last + sample) >> 1);
        *out++ = static_cast<i16>(sample);
        last = sample;
    }
    previous = static_cast<i16>(last);
}

// The device routine's range: 1..4 strings at a stride of a whole number of
// words from 32 to 1024 bytes, 1..256 pairs, loss 0..32767, and
// each string's size a power of two up to 65536 with write below it, or a
// string never set up (size, mask and write 0: it never steps), and shift at
// most 9.
inline bool pluck_in_range(PluckString* strings, unsigned count, unsigned stride, unsigned pairs, i32 loss) {
    if (count - 1u >= 4u || pairs - 1u >= 256u || static_cast<u32>(loss) > 32767u) return false;
    if (stride < 32u || stride > 1024u || (stride & 3u)) return false;
    for (unsigned i = 0; i < count; ++i) {
        const PluckString& p = pluck_detail::at(strings, i, stride);
        if (p.shift > 9u || p.write > p.mask) return false;
        if (p.size == 0 ? p.mask != 0 : p.size > 65536u || p.mask != p.size - 1u || (p.size & p.mask)) return false;
    }
    return true;
}

// Renders and returns true when the call is in the routine's range; otherwise
// renders nothing and returns false, so the caller can run its own loop.
struct PluckCall {
    u32 pairs; u32 random; u32 update_probability; i32 keep; i32 previous; u32 multiplier; u32 addend; u32 stride;
};
// native/pluck.s on the device, and its definition: the reference, with the
// call's loss as 32767 - keep.
extern "C" void macroosc_native_pluck(PluckString* strings, unsigned count, i16* out, PluckCall* call);
inline void pluck_reference_entry(PluckString* strings, unsigned count, i16* out, PluckCall* call) {
    i16 previous = static_cast<i16>(call->previous);
    pluck_reference(strings, count, call->stride, out, call->pairs, call->random, call->update_probability,
                    32767 - call->keep, previous);
    call->previous = previous;
}
inline bool pluck(PluckString* strings, unsigned count, unsigned stride, i16* out, unsigned pairs, u32& random,
                  u32 update_probability, i32 loss, i16& previous) {
    if (!pluck_in_range(strings, count, stride, pairs, loss)) return false;
    PluckCall call = {pairs, random, update_probability, 32767 - loss, previous, 1664525u, 1013904223u, stride};
    OPXY_NATIVE(macroosc_native_pluck, pluck_reference_entry)(strings, count, out, &call);
    random = call.random;
    previous = static_cast<i16>(call.previous);
    return true;
}
}} // namespace macroosc::native
