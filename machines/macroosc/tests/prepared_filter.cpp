#include "braids/svf.h"
#include "vendor/braids/resources.cc"
#include "optimized/packed_tables.cpp"
#include <cassert>
#include <cstring>
#define Svf OriginalSvf
#define SvfMode OriginalSvfMode
#define SVF_MODE_LP ORIGINAL_SVF_MODE_LP
#define SVF_MODE_BP ORIGINAL_SVF_MODE_BP
#define SVF_MODE_HP ORIGINAL_SVF_MODE_HP
#include "vendor/braids/svf.h"
#undef Svf
#undef SvfMode
#undef SVF_MODE_LP
#undef SVF_MODE_BP
#undef SVF_MODE_HP

static_assert(sizeof(braids::Svf) == sizeof(braids::OriginalSvf), "Filter layout grew");
template<braids::SvfMode Mode> void check() {
  braids::OriginalSvf original;
  braids::Svf candidate, local;
  // Match the adapter's zeroed voice storage. Init marks coefficients dirty;
  // the first Process/Prepare still has to calculate the original values.
  std::memset(static_cast<void*>(&original),0,sizeof original);
  std::memset(static_cast<void*>(&candidate),0,sizeof candidate);
  std::memset(static_cast<void*>(&local),0,sizeof local);
  original.Init();candidate.Init();local.Init();
  original.set_mode(static_cast<braids::OriginalSvfMode>(Mode));candidate.set_mode(Mode);local.set_mode(Mode);
  unsigned seed=12345;
  int current_resonance=16384;
  for (unsigned block=0;block<4096;++block) {
    const int pitch=(block*719u)%28000;
    const int resonance=(block*179u)%32768;
    // Cover frequency-only, resonance-only, both and neither invalidation,
    // including init before either setter and repeated Prepare calls.
    if (block & 1) {
      original.set_frequency(pitch);candidate.set_frequency(pitch);local.set_frequency(pitch);
    }
    if (block & 2) {
      current_resonance=resonance;
      original.set_resonance(resonance);candidate.set_resonance(resonance);local.set_resonance(resonance);
    }
    candidate.Prepare();local.Prepare();
    if (block & 4) { candidate.Prepare();local.Prepare(); }
    if (block & 16) {
      original.set_resonance(current_resonance);
      candidate.set_resonance(current_resonance);local.set_resonance(current_resonance);
      candidate.Prepare();local.Prepare();
    }
    for (unsigned i=0;i<24;++i) {
      seed=seed*1664525u+1013904223u;
      // Include excitation beyond the audio range: Snare's initial pulse
      // exceeds 16 bits and its original wrapping products must survive.
      const int in=(i%7)==0 ? 15*32768 : ((i%7)==1 ? -15*32768 : static_cast<short>(seed>>16));
      const int expected=original.Process(in);
      // Exercise both general and prepared users of the shared dirty flags.
      assert(expected==((block & 8) ? candidate.Process(in) : candidate.ProcessNoPunch<Mode>(in)));
      assert(expected==local.ProcessNoPunchLocal<Mode>(in));
    }
  }
}
int main(){check<braids::SVF_MODE_BP>();check<braids::SVF_MODE_HP>();check<braids::SVF_MODE_LP>();}
