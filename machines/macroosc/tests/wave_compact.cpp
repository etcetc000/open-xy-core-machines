#include "optimized/wave_compact.hpp"
#include "optimized/wave_compact.cpp"
#include "vendor/braids/resources.cc"
#include <cstdio>

static int32_t floor16(int64_t value) {
  return static_cast<int32_t>(value < 0 ? -((-value+65535)/65536) : value/65536);
}
static int16_t reference(unsigned wave,uint32_t phase) {
  const unsigned index=phase>>24;
  const int32_t a=braids::wt_waves[wave*129+index];
  const int32_t b=braids::wt_waves[wave*129+index+1];
  return static_cast<int16_t>((a<<8)-32768+floor16(int64_t(b-a)*(phase&0xffffff)));
}
int main() {
  const uint32_t fractions[]={0,1,255,65535,65536,0x7fffff,0x800000,0xfffffe,0xffffff};
  for(unsigned wave=0;wave<256;++wave) {
    for(unsigned segment=0;segment<128;++segment) {
      for(unsigned f=0;f<sizeof(fractions)/sizeof(fractions[0]);++f) {
        const uint32_t phase=(segment<<24)|fractions[f];
        const int16_t expected=reference(wave,phase);
        const uint32_t* packed=macroosc::wave_compact+wave*128;
        if(macroosc::wave_sample(packed,phase)!=expected) return 1;
        if(macroosc::wave_sample_wrapped(packed,phase)!=expected) return 10;
        if(fractions[f]==0 && macroosc::wave_grid_sample(packed,phase)!=expected) return 7;
      }
    }
  }
  uint32_t rng=0x1b27f3d9;
  for(unsigned test=0;test<1000000;++test) {
    rng=rng*1664525u+1013904223u;const unsigned a=rng&255,b=(rng>>8)&255;
    const uint16_t balance=rng>>16;
    rng=rng*1664525u+1013904223u;const uint32_t phase=rng&0x7fffffff;
    const int32_t left=reference(a,phase),right=reference(b,phase);
    const int16_t expected=static_cast<int16_t>(left+floor16(int64_t(right-left)*balance));
    const uint32_t* pa=macroosc::wave_compact+a*128;const uint32_t* pb=macroosc::wave_compact+b*128;
    const int16_t value=macroosc::wave_crossfade(pa,pb,phase,balance);
    if(value!=expected)return 3;
    if(macroosc::wave_crossfade_wrapped(pa,pb,phase,balance)!=expected)return 11;
    if(macroosc::wave_sample_crossfade(left,right,balance)!=expected)return 8;
    const uint32_t grid_phase=phase&0x7f000000;
    const int32_t grid_left=reference(a,grid_phase),grid_right=reference(b,grid_phase);
    const int16_t grid_expected=static_cast<int16_t>(grid_left+floor16(int64_t(grid_right-grid_left)*balance));
    if(macroosc::wave_grid_crossfade(pa,pb,grid_phase,balance)!=grid_expected)return 9;
  }
  static_assert(sizeof(macroosc::wave_line_indices)==65,"Wave Line needs a terminal guard wave");
  // Independent original selection; the final duplicate preserves the existing
  // defined upper-endpoint guard. All shared wave samples were checked above.
  const uint8_t line_waves[]={187,179,154,155,135,134,137,19,24,3,8,66,79,25,180,174,
      64,127,198,15,10,7,11,0,191,192,115,238,237,236,241,47,70,76,235,26,133,208,34,
      175,183,146,147,148,150,151,152,153,117,138,32,33,35,125,199,201,30,31,193,27,
      29,21,18,182,182};
  for(unsigned i=0;i<65;++i)if(macroosc::wave_line_indices[i]!=line_waves[i])return 4;
  // Independent copy of the original MIT-licensed mini_wave_line selection.
  const unsigned chord_waves[]={157,161,171,188,189,191,192,193,196,198,201,234,232,
      229,226,224,1,2,3,4,5,8,12,32,36,42,47,252,254,141,139,135,174};
  const uint16_t balances[]={0,1,255,32767,32768,65534,65535};
  for(unsigned pair=0;pair<32;++pair) {
    const unsigned a=chord_waves[pair],b=chord_waves[pair+1];
    for(unsigned segment=0;segment<128;++segment) {
      for(unsigned f=0;f<sizeof(fractions)/sizeof(fractions[0]);++f) {
        const uint32_t phase=(segment<<24)|fractions[f];
        const int32_t left=reference(a,phase),right=reference(b,phase);
        for(unsigned k=0;k<sizeof(balances)/sizeof(balances[0]);++k) {
          const int16_t expected=static_cast<int16_t>(left+floor16(int64_t(right-left)*balances[k]));
          const uint32_t* pa=macroosc::wave_compact+a*128;
          const uint32_t* pb=macroosc::wave_compact+b*128;
          const int16_t actual=balances[k]==0 ? macroosc::wave_sample(pa,phase)
              : macroosc::wave_crossfade(pa,pb,phase,balances[k]);
          if(actual!=expected) return 5;
        }
      }
    }
  }
  for(unsigned test=0;test<1000000;++test) {
    rng=rng*1664525u+1013904223u;const unsigned pair=rng&31;
    const uint16_t balance=rng>>16;
    rng=rng*1664525u+1013904223u;const uint32_t phase=rng&0x7fffffff;
    const unsigned a=chord_waves[pair],b=chord_waves[pair+1];
    const int32_t left=reference(a,phase),right=reference(b,phase);
    const int16_t expected=static_cast<int16_t>(left+floor16(int64_t(right-left)*balance));
    const uint32_t* pa=macroosc::wave_compact+a*128;
    const uint32_t* pb=macroosc::wave_compact+b*128;
    const int16_t actual=balance==0 ? macroosc::wave_sample(pa,phase)
        : macroosc::wave_crossfade(pa,pb,phase,balance);
    if(actual!=expected)return 6;
  }
  std::puts("Aligned wavetable interpolation, chord endpoints and endpoint guard pass");
}
