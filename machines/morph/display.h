#pragma once
#include <opxy/draw.hpp>
namespace morph {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
using Frame = opxy::draw::Frame<0, 0, 15, 0, 16>;
struct DisplayState { opxy::u32 unused; };
}
