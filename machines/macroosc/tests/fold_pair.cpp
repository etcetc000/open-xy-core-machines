#include "optimized/division_table.cpp"
#include "optimized/analog_oscillator.cpp"
#include "optimized/chunk_math.cpp"
#include "optimized/packed_tables.cpp"
#include "optimized/pitch_table.cpp"
#include "optimized/resources.cpp"
#include <cassert>

// Compare the fused pair with independent reference rendering, including
// unequal parameters, phase histories, rates and one-sided shape resets.
int main() {
  braids::AnalogOscillator ref[2]={}, fused[2]={};
  // Braids' empty constructor/Init leave some history members untouched.
  // The adapter zeroes voice storage before construction; reproduce that here.
  std::memset(static_cast<void*>(ref),0,sizeof(ref));
  std::memset(static_cast<void*>(fused),0,sizeof(fused));
  for (unsigned i=0;i<2;++i) {ref[i].Init();fused[i].Init();}
  uint8_t sync[24]={};
  int16_t a[24],b[24],actual[24];
  unsigned seed=17;
  int previous_balance=0;
  for (unsigned block=0;block<12288;++block) {
    seed=seed*1664525u+1013904223u;
    const unsigned count=8*(1+block%3);
    const int pitch=(seed>>16)%17000;
    const int timbre=(seed>>1)&32767;
    for (unsigned i=0;i<2;++i) {
      const int rate=pitch+((block%11==0)?int(i)*137:0);
      const int parameter=(block%7==0 && i)?32767-timbre:timbre;
      const braids::AnalogOscillatorShape shape=i?braids::OSC_SHAPE_TRIANGLE_FOLD:braids::OSC_SHAPE_SINE_FOLD;
      ref[i].set_pitch(rate);fused[i].set_pitch(rate);
      ref[i].set_parameter(parameter);fused[i].set_parameter(parameter);
      ref[i].set_shape(shape);fused[i].set_shape(shape);
    }
    const int balance=(seed>>16)&32767;
    ref[0].RenderNoSync(sync,a,nullptr,count);
    ref[1].RenderNoSync(sync,b,nullptr,count);
    fused[0].RenderFoldPair(fused[1],actual,count,previous_balance,balance-previous_balance);
    const int step=32767/count;
    for (unsigned i=0;i<count;++i) {
      const uint16_t mix=(previous_balance+(((balance-previous_balance)*int((i+1)*step))>>15))<<1;
      assert(actual[i]==stmlib::Mix(a[i],b[i],mix));
    }
    previous_balance=balance;
    if (block%13==0) {
      const unsigned i=(block/13)%2;
      ref[i].set_shape(braids::OSC_SHAPE_SAW);
      fused[i].set_shape(braids::OSC_SHAPE_SAW);
      ref[i].RenderNoSync(sync,a,nullptr,count);
      fused[i].RenderNoSync(sync,b,nullptr,count);
      for(unsigned j=0;j<count;++j) assert(a[j]==b[j]);
    }
  }
  // Exercise every gain control with a ramp followed by an unchanged block,
  // covering the constant-control path as well as transitions into it.
  std::memset(static_cast<void*>(ref),0,sizeof(ref));
  std::memset(static_cast<void*>(fused),0,sizeof(fused));
  for(unsigned i=0;i<2;++i) {ref[i].Init();fused[i].Init();}
  previous_balance=0;
  const int balances[]={0,8192,16384,32767};
  for(int parameter=0;parameter<32768;++parameter) {
    const int balance=balances[(parameter/64)%4];
    for(unsigned repeat=0;repeat<2;++repeat) {
      for(unsigned i=0;i<2;++i) {
        const braids::AnalogOscillatorShape shape=i?braids::OSC_SHAPE_TRIANGLE_FOLD:braids::OSC_SHAPE_SINE_FOLD;
        ref[i].set_shape(shape);fused[i].set_shape(shape);
        ref[i].set_pitch(7680);fused[i].set_pitch(7680);
        ref[i].set_parameter(parameter);fused[i].set_parameter(parameter);
      }
      ref[0].RenderNoSync(sync,a,nullptr,8);
      ref[1].RenderNoSync(sync,b,nullptr,8);
      fused[0].RenderFoldPair(fused[1],actual,8,previous_balance,balance-previous_balance);
      for(unsigned j=0;j<8;++j) {
        const uint16_t mix=(previous_balance+(((balance-previous_balance)*int((j+1)*4095))>>15))<<1;
        assert(actual[j]==stmlib::Mix(a[j],b[j],mix));
      }
      previous_balance=balance;
    }
  }
}
