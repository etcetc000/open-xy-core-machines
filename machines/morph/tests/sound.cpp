// Morph is silent: it only steers other tracks.
#include "../engine.cpp"
#include <cassert>
int main() {
    morph::Voice voice = {};
    morph::Shared shared;
    opxy::i32 parameters[8] = {};
    opxy::u32 pitch[opxy::block_frames] = {};
    opxy::i32 output[opxy::block_frames];
    for (auto& sample : output) sample = 123;
    morph::initialize(voice, shared, 1);
    morph::start(voice, shared, 1);
    morph::render(voice, shared, 1, output, parameters, pitch);
    for (auto sample : output) assert(sample == 0);
    morph::destroy(voice, shared, 1);
}
