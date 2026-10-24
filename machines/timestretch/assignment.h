// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <opxy/types.hpp>

namespace timestretch {
// Opt-in filename convention, not a general sample-pack naming heuristic.
// Three ASCII digits at either end of the basename, separated by space, '-'
// or '_'; e.g. 095_guitar.wav or guitar_095.wav. Ignore directory names.
inline bool tempo_separator(char c) { return c == ' ' || c == '_' || c == '-'; }
inline bool tempo_digit(char c) { return c >= '0' && c <= '9'; }
inline opxy::u32 three_digits(const char* p) {
    if (!tempo_digit(p[0]) || !tempo_digit(p[1]) || !tempo_digit(p[2])) return 0;
    return (p[0] - '0') * 100u + (p[1] - '0') * 10u + (p[2] - '0');
}
inline opxy::u32 filename_bpm(const char* path, opxy::u32 size) {
    if (!path || size < 3 || size > 1024) return 0;
    opxy::u32 begin = 0, end = size;
    for (opxy::u32 i = 0; i < size; ++i) {
        if (!path[i]) return 0; // Require the complete length-delimited name.
        if (path[i] == '/' || path[i] == '\\') begin = i + 1;
    }
    // Only strip a known audio extension, never a decimal tempo suffix.
    for (opxy::u32 i = begin; i < size; ++i) {
        if (path[i] != '.') continue;
        const opxy::u32 n = size - i - 1;
        if (n != 3 && n != 4) continue;
        const char a = path[i+1] | 32, b = path[i+2] | 32, c = path[i+3] | 32;
        if ((n == 3 && a == 'w' && b == 'a' && c == 'v') ||
            (a == 'a' && b == 'i' && c == 'f' &&
             (n == 3 || (path[i+4] | 32) == 'f'))) end = i;
    }
    if (end - begin < 3) return 0;
    const opxy::u32 length = end - begin;
    const bool first_token = (length == 3 || tempo_separator(path[begin+3])) &&
        tempo_digit(path[begin]) && tempo_digit(path[begin+1]) && tempo_digit(path[begin+2]);
    const bool last_token = (length == 3 || tempo_separator(path[end-4])) &&
        tempo_digit(path[end-3]) && tempo_digit(path[end-2]) && tempo_digit(path[end-1]);
    const opxy::u32 first = first_token ? three_digits(path + begin) : 0;
    const opxy::u32 last = last_token ? three_digits(path + end - 3) : 0;
    if ((first_token && (first < 20 || first > 300)) ||
        (last_token && (last < 20 || last > 300)) || (first && last && first != last)) return 0;
    return first ? first : last;
}

// Guess the closest supported musical length from duration and project tempo.
// This is not onset detection: recordings at another tempo may need correction.
// Return the index of 1, 2, 4, 8, 16 or 32 beats.
inline opxy::u32 suggested_length(opxy::u32 frames, opxy::u32 rate,
                                opxy::u32 bpm_x10) {
    if (frames < 2 || frames >= (1u << 20) || rate < 8000 || rate > 192000 ||
        bpm_x10 < 200 || bpm_x10 > 3000) return 2; // one-bar fallback
    // frames*bpm_x10 < 3,145,728,000. The largest midpoint is 24 beats:
    // rate*900*16 <= 2,764,800,000. All intermediates fit unsigned 32 bits.
    const opxy::u32 duration = frames * bpm_x10;
    opxy::u32 midpoint = rate * 900u;
    for (opxy::u32 index = 0; index < 5; ++index) {
        if (duration < midpoint) return index;
        if (index < 4) midpoint <<= 1;
    }
    return 5;
}
}
