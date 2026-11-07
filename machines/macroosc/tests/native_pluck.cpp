// native/pluck.hpp: the reference against the Braids plucked-string loop it replaces (per
// output pair, three strings in their state records, stmlib's random state in a global,
// Interpolate1022 on the line), over random lines, phases, increments, write positions
// (including word 0 and runs past the line's end), excitation, losses and update
// probabilities, with strings 40 bytes apart as in a larger record; and its range.
// The routine (native/pluck.s) was matched against this reference sample for sample on
// the BF707 functional simulator; a host build, as here, runs the reference.
#include "native/pluck.hpp"
using namespace opxy;
static u32 r = 11;
static u32 next() { r = r * 1103515245u + 12345u; return (r >> 8) ^ (r << 13); }
static u32 rng;
static u32 word() { rng = rng * 1664525u + 1013904223u; return rng; }
struct Record { macroosc::native::PluckString s; u32 pad[2]; };
static void original(macroosc::native::PluckString* p0, unsigned count, i16* buffer, unsigned size, u32 update_probability,
                     i16 loss, i16& previous) {
    i16 previous_sample = previous;
    while (size) {
        i32 sample = 0;
        for (unsigned i = 0; i < count; ++i) {
            macroosc::native::PluckString* p = &reinterpret_cast<Record*>(p0)[i].s;
            i16* dl = p->line;
            if (p->excite) {
                --p->excite;
                i32 excitation_sample = (dl[p->excite] + 3 * static_cast<i16>(word() >> 16)) >> 2;
                dl[p->excite] = static_cast<i16>(excitation_sample);
                sample += excitation_sample;
            } else {
                p->phase += p->increment;
                u32 read_ptr = ((p->phase >> (22 + p->shift)) + 2) & p->mask;
                u32 write_ptr = p->write;
                while (write_ptr != read_ptr) {
                    u32 next_ptr = (write_ptr + 1) & p->mask;
                    i32 a = dl[write_ptr], b = dl[next_ptr];
                    u32 probability = word();
                    if ((probability & 0xffff) <= update_probability) {
                        i32 sum = a + b;
                        sum = sum < 0 ? -(-sum >> 1) : (sum >> 1);
                        if (loss) sum = sum * (32768 - loss) >> 15;
                        dl[write_ptr] = static_cast<i16>(sum);
                    }
                    if (write_ptr == 0) dl[p->size] = dl[0];
                    write_ptr = next_ptr;
                }
                p->write = write_ptr;
                const u32 phase = p->phase >> p->shift;
                const i32 a = dl[phase >> 22], b = dl[(phase >> 22) + 1];
                // Interpolate1022, its product wrapping as on the device.
                sample += static_cast<i16>(a + (static_cast<i32>(static_cast<u32>(b - a) * ((phase >> 6) & 0xffff)) >> 16));
            }
        }
        if (sample < -32767) sample = -32767;
        if (sample > 32767) sample = 32767;
        *buffer++ = static_cast<i16>((previous_sample + sample) >> 1);
        *buffer++ = static_cast<i16>(sample);
        previous_sample = static_cast<i16>(sample);
        size -= 2;
    }
    previous = previous_sample;
}
static i16 la[3][1025], lb[3][1025];
int main() {
    for (unsigned t = 0; t < 4000; ++t) {
        Record a[3], b[3];
        for (unsigned i = 0; i < 3; ++i) {
            macroosc::native::PluckString& s = a[i].s;
            const u32 size = next() % 8 == 0 ? 0 : 1u << (next() % 11);
            s.size = size; s.mask = size ? size - 1 : 0; s.shift = next() % 10;
            s.phase = next() ^ (next() << 16); s.write = next() & s.mask;
            const u32 k = next() % 4;
            s.increment = k == 0 ? 0 : k == 1 ? (next() % (1u << 24)) << s.shift : (next() ^ (next() << 12)) >> (next() % 8);
            s.excite = next() % 3 == 0 ? next() % (size + 1) : 0;
            for (unsigned j = 0; j < 1025; ++j) la[i][j] = lb[i][j] = static_cast<i16>(next());
            if (next() % 2) la[i][size] = lb[i][size] = la[i][0];
            s.line = la[i]; b[i] = a[i]; b[i].s.line = lb[i];
        }
        const unsigned pairs = next() % 16 == 0 ? next() % 256 + 1 : next() % 12 + 1;
        const i16 loss = static_cast<i16>(next() % 3 == 0 ? 0 : next() % 4097);
        const u32 up = next() % 3 == 0 ? 65535u : next() % 3 == 0 ? 131072u - (next() % 4096) * 31 : next() % 65536;
        u32 random = next(); rng = random;
        i16 pa = static_cast<i16>(next()), pb = pa;
        i16 oa[512], ob[512];
        if (!macroosc::native::pluck(&a[0].s, 3, sizeof(Record), oa, pairs, random, up, loss, pa)) return 1;
        original(&b[0].s, 3, ob, 2 * pairs, up, loss, pb);
        if (random != rng || pa != pb) return 2;
        for (unsigned j = 0; j < 2 * pairs; ++j) if (oa[j] != ob[j]) return 3;
        for (unsigned i = 0; i < 3; ++i) {
            if (a[i].s.phase != b[i].s.phase || a[i].s.write != b[i].s.write || a[i].s.excite != b[i].s.excite) return 4;
            for (unsigned j = 0; j < 1025; ++j) if (la[i][j] != lb[i][j]) return 5;
        }
    }
    // Outside the range: nothing rendered, nothing changed.
    Record s[1] = {};
    s[0].s.size = 3; s[0].s.mask = 2; s[0].s.line = la[0];
    u32 random = 1; i16 previous = 0; i16 out[4] = {7, 7, 7, 7};
    if (macroosc::native::pluck(&s[0].s, 1, sizeof(Record), out, 2, random, 65535, 0, previous) || out[0] != 7 || random != 1) return 6;
    s[0].s.size = 4; s[0].s.mask = 3; s[0].s.shift = 10;
    if (macroosc::native::pluck_in_range(&s[0].s, 1, sizeof(Record), 2, 0)) return 7;
    s[0].s.shift = 0;
    if (!macroosc::native::pluck_in_range(&s[0].s, 1, sizeof(Record), 2, 0) || macroosc::native::pluck_in_range(&s[0].s, 1, 30, 2, 0) ||
        macroosc::native::pluck_in_range(&s[0].s, 1, sizeof(Record), 2, -1) || macroosc::native::pluck_in_range(&s[0].s, 5, sizeof(Record), 2, 0))
        return 8;
    return 0;
}
