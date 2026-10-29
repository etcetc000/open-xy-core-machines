#include "controls.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
using namespace timestretch;
using namespace opxy;
void check(bool ok) { if (!ok) std::abort(); }
// The SDK tests the storage codec; this machine tests its semantic settings.
PadSettings checked_settings(PadSettings value) { check(value <= 262143u); return value; }
int main() {
    const DrumControl click{1,0,true,true,false};
    const DrumControl up{1,1,false,true,false};
    const DrumControl down{1,-1,false,true,false};
    for (u32 bpm=200;bpm<=3000;++bpm) for(u32 bars=0;bars<6;++bars) for(u32 flags=0;flags<4;++flags) {
        auto value=encode_controls(Controls{bool(flags&1),bool(flags&2),bpm,bars,bpm});
        check(value<=262143);
        value=checked_settings(value);
        drum_control(value,click);
        auto c=decode_controls(checked_settings(value));
        check(!c.bpm_x10 && c.length==bars && c.remembered_bpm_x10==bpm);
        drum_control(value,up);
        c=decode_controls(checked_settings(value));
        check(!c.bpm_x10 && c.length==(bars<5?bars+1:5));
        drum_control(value,click);
        c=decode_controls(checked_settings(value));
        check(c.bpm_x10==bpm && c.once==bool(flags&1) && c.stretch==bool(flags&2));
        drum_control(value,down);
        drum_control(value,click);
        drum_control(value,click);
        check(decode_controls(checked_settings(value)).bpm_x10==(bpm>200?bpm-1:200));
    }
    // Previously saved BPM settings migrate without losing decimal precision.
    for(u32 bpm=200;bpm<=3000;++bpm) {
        PadSettings old=1u|((bpm-194u)<<4);
        check(decode_controls(old).bpm_x10==bpm);
        drum_control(old,click); drum_control(old,click);
        check(decode_controls(checked_settings(old)).bpm_x10==bpm);
    }
    for(u32 bars=0;bars<6;++bars) {
        PadSettings old=1u|(bars<<4);
        drum_control(old,click); drum_control(old,click);
        check(decode_controls(checked_settings(old)).length==bars);
    }
    const char* file="090 take my hand.wav";
    SampleAssignment input{file,u32(std::strlen(file)),1000,44100,1200,false,0,false};
    auto value=sample_assign(input);
    check(decode_controls(value).bpm_x10==900);
    drum_control(value,up); drum_control(value,click);
    input.replacing=true; input.previous=checked_settings(value); input.filename=nullptr; input.filename_bytes=0;
    value=sample_assign(input); drum_control(value,click);
    check(decode_controls(value).bpm_x10==901);
    SampleRegion saved{0,1000,sample_unity_level,false,0}; DrumPanel panel{};
    drum_panel(value,saved,nullptr,panel);
    check(!std::strcmp(panel.shifted[0],"90.1 bpm"));
    const DrumControl stretch{2,1,false,true,false}; drum_control(value,stretch);
    drum_panel(value,saved,nullptr,panel);
    check(!std::strcmp(panel.shifted[1],"timestretch"));
    std::puts("PASS: every BPM/bar/flag combination, legacy migration, assignment and panel captions");
}
