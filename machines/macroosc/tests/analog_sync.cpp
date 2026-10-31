#include "optimized/division_table.cpp"
#include "optimized/analog_oscillator.cpp"
#include "optimized/chunk_math.cpp"
#include "optimized/packed_tables.cpp"
#include "optimized/pitch_table.cpp"
#include "optimized/resources.cpp"
#include <cassert>

// Compare specialized sync paths with the complete generic renderer,
// including pulse/saw switching and independently changing master/slave pitch.
int main() {
  braids::AnalogOscillator reference[2],candidate[2];
  // Match the adapter's intentionally zeroed storage before Braids Init().
  std::memset(static_cast<void*>(reference),0,sizeof(reference));
  std::memset(static_cast<void*>(candidate),0,sizeof(candidate));
  for(unsigned i=0;i<2;++i){reference[i].Init();candidate[i].Init();}
  uint8_t silent[24]={},expected_sync[24],actual_sync[24];
  int16_t expected[24],actual[24];
  unsigned seed=42;
  for(unsigned block=0;block<12288;++block) {
    seed=seed*1664525u+1013904223u;
    const unsigned count=8*(1+block%3);
    const braids::AnalogOscillatorShape shape=(block/17)%2?braids::OSC_SHAPE_SQUARE:braids::OSC_SHAPE_SAW;
    for(unsigned i=0;i<2;++i) {
      // DualSync's timbre adds up to 8191 pitch units to the slave, including
      // pitches above the table clamp. Exercise that complete offset range.
      const int pitch=int((seed>>16)&16383)+int(i)*int(seed&8191);
      reference[i].set_parameter(0);candidate[i].set_parameter(0);
      reference[i].set_pitch(pitch);candidate[i].set_pitch(pitch);
      reference[i].set_shape(shape);candidate[i].set_shape(shape);
    }
    reference[0].Render(silent,expected,expected_sync,count);
    if(block%3==0) candidate[0].RenderSyncMaster(silent,actual,actual_sync,count);
    else if(shape==braids::OSC_SHAPE_SQUARE)
      candidate[0].RenderSquareSyncMaster(silent,actual,actual_sync,count);
    else candidate[0].RenderSawSyncMaster(silent,actual,actual_sync,count);
    for(unsigned i=0;i<count;++i) {
      assert(expected[i]==actual[i]);assert(expected_sync[i]==actual_sync[i]);
    }
    reference[1].Render(expected_sync,expected,nullptr,count);
    if(block%3==0) candidate[1].RenderSyncSlave(actual_sync,actual,nullptr,count);
    else if(shape==braids::OSC_SHAPE_SQUARE)
      candidate[1].RenderSquareSyncSlave(actual_sync,actual,nullptr,count);
    else candidate[1].RenderSawSyncSlave(actual_sync,actual,nullptr,count);
    for(unsigned i=0;i<count;++i)assert(expected[i]==actual[i]);
  }
}
