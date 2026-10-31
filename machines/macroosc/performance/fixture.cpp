// Portable model corpus shared by the host comparison and native cost probe.
// No stock voice/envelope/mixer code is included in this isolated DSP fixture.
#include "core.h"
#include <cstring>
namespace {
macroosc::Voice voice;
macroosc::Shared shared;
int32_t parameters[8],output[64];uint32_t pitch[64];
unsigned model,scenario;
const uint32_t increments[]={6369993u,25479972u,101919888u,407679552u};
}
extern "C" void macro_prepare(unsigned chosen,unsigned mode){
  std::memset(&voice,0,sizeof voice);std::memset(&shared,0,sizeof shared);
  macroosc::initialize(voice,shared,1);macroosc::start(voice,shared,1);
  model=chosen;scenario=mode;
}
extern "C" void macro_advance(unsigned block){
  const unsigned shape=scenario==5?(model+block/8)%47:model;
  parameters[0]=static_cast<int32_t>((shape*32768u+16384u)/47)<<16;
  parameters[1]=scenario>=2?static_cast<int32_t>((block*13638149u)&0x7fffffff):0x40000000;
  parameters[2]=scenario>=2?static_cast<int32_t>((block*23219453u+0x20000000u)&0x7fffffff):0x40000000;
  parameters[3]=0x7fffffff;
  parameters[4]=scenario==1||scenario==2?0x7fffffff:0;
  parameters[5]=scenario==4?0x40000000:0x7fffffff;
  parameters[6]=scenario==4?0x40000000:0x7fffffff;
  parameters[7]=scenario==4?0x7fffffff:0;
  const unsigned base=increments[(scenario==3?block/8:scenario)%4];
  for(unsigned i=0;i<64;++i)pitch[i]=base+(scenario==2?(i+block*64)*173u:0);
  if(block && !(block%31))macroosc::start(voice,shared,1);
}
extern "C" void macro_render(){macroosc::render(voice,shared,1,output,parameters,pitch);}
extern "C" const int32_t* macro_output(){return output;}
extern "C" unsigned macro_hash(){
  unsigned hash=2166136261u;
  for(unsigned i=0;i<64;++i)hash=(hash^static_cast<uint32_t>(output[i]))*16777619u;
  return hash;
}
