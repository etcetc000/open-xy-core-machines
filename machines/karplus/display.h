#pragma once
#include <opxy/draw.hpp>

namespace karplus {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// The string is four overlapping outlines (paths 0 to 3), then its two pegs,
// the pick marker and four companion strings for Detune.
using Frame = opxy::draw::Frame<4, 64, 11, 0, 4>;
// Each note's pluck: its waves' height, time since the note and pick point.
const unsigned kPlucks = 12;
struct Pluck {
    opxy::i32 amplitude;     // Q10 pixels; 0 when the pluck has faded
    opxy::i32 time;          // since the note, Q10 seconds
    opxy::i32 pick;          // Q10 pixels from the left peg
};
struct DisplayState {
    Pluck plucks[kPlucks];   // the plucks still ringing, which add up
    opxy::u32 serial;        // the last note start seen
    opxy::u32 track;
    opxy::u32 initialized;
    opxy::u32 key;           // the outlines' geometry key
};
}
