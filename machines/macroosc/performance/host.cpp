#include <cstdio>
#include <cstdint>
#include <chrono>
extern "C" void macro_prepare(unsigned,unsigned);
extern "C" void macro_advance(unsigned);
extern "C" void macro_render();
extern "C" const int32_t* macro_output();
int main(int argc,char** argv){
  if(argc!=2)return 1;FILE* out=std::fopen(argv[1],"wb");if(!out)return 2;
  for(unsigned model=0;model<47;++model)for(unsigned mode=0;mode<6;++mode){
    macro_prepare(model,mode);
    long long duration=0;
    for(unsigned block=0;block<128;++block){
      macro_advance(block);
      auto before=std::chrono::steady_clock::now();macro_render();
      duration+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-before).count();
      if(std::fwrite(macro_output(),4,64,out)!=64)return 3;
    }
    std::printf("%u %u %lld\n",model,mode,duration);
  }
  return std::fclose(out)!=0;
}
