#include "../player.cpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <string>
#define require(x) do { if (!(x)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
using namespace opxy;
using namespace opxy::player;
namespace {
const i32 centre = 0x40000000, top = 0x7fffffff;
// Orchid order: upper/black Dim Min Maj Sus; lower/white 6 m7 M7 9.
enum Key { Six=53, Dim=54, Minor7=55, Minor=56, Major7=57, Major=58,
           Nine=59, Simple=60, Sus=61, Advanced=62, Addition=63, Free=64, C=72 };
struct Rig {
    chords::State state = {};
    Controls controls = {{0, 0, centre, 0}, 53};
    Plan plan = {};
    Update changed = {};
    void on(u32 note, u32 velocity=90) { require(chords::press(state, controls, note, velocity, plan)); }
    bool off(u32 note) { return chords::release(state, note); }
    bool next() { return chords::update(state, controls, changed); }
    void setting(u32 note) { on(note); require(plan.count == 0); require(off(note)); }
};
void notes(const Plan& plan, std::initializer_list<int> pitches, int velocity=90) {
    require(plan.count == pitches.size());
    unsigned i=0;
    for (int p : pitches) { require(plan.tones[i].pitch == p); require(plan.tones[i++].velocity == velocity); }
}
void base_and_extensions() {
    const int types[] = {Dim, Minor, Major, Sus};
    const int middle[] = {3,3,4,5}, fifth[] = {6,7,7,7};
    const int extensions[] = {Six, Minor7, Major7, Nine}, intervals[] = {9,10,11,14};
    for (unsigned t=0; t<4; ++t) {
        Rig r;
        r.on(types[t]); require(r.plan.count == 0);
        r.on(C); notes(r.plan, {C,C+middle[t],C+fifth[t]});
        require(!r.off(C));
        for (unsigned e=0; e<4; ++e) {
            r.on(extensions[e]); require(r.plan.count == 0);
            r.on(C); notes(r.plan, {C,C+middle[t],C+fifth[t],C+intervals[e]});
            require(!r.off(C)); require(r.off(extensions[e]));
        }
        require(r.off(types[t]));
    }
    Rig r;
    r.on(Major); for (int k : extensions) r.on(k);
    r.on(C); notes(r.plan,{72,76,79,81,82,83,86});
    r.off(C); r.off(Major); r.on(C); notes(r.plan,{72});
}
void styles() {
    for (int style : {Simple,Advanced,Free}) {
        Rig r; r.setting(style);
        r.on(C,77); notes(r.plan,{72},77);
        r.on(Major); require(r.plan.count==0);
        if (style==Simple) require(!r.next());
        else { require(r.next()); require(r.changed.root==C && r.changed.mode==Replace); notes(r.changed.plan,{72,76,79},77); }
        require(!r.next());
        r.off(Major); require(!r.next());
        r.on(Minor);
        if (style==Free) { require(r.next()); notes(r.changed.plan,{72,75,79},77); }
        else require(!r.next());
        r.off(Minor); r.on(Minor);
        require(r.next()==(style==Free));
        require(!r.off(C)); r.off(Minor); r.on(Major); require(!r.next());
        r.on(C); notes(r.plan,{72,76,79});
        r.off(Major); require(!r.next());
        r.on(Minor); require(r.next()==(style==Free));
        require(!r.off(C)); r.off(Minor);
    }
}
void additions() {
    for (int style : {Simple,Advanced,Free}) for (bool replay : {false,true}) {
        Rig r; r.setting(style); if(replay) r.setting(Addition);
        r.on(Major); r.on(C); notes(r.plan,{72,76,79});
        r.on(Minor7);
        if (style==Simple) require(!r.next());
        else {
            require(r.next()); require(r.changed.mode==(replay ? Replace:Add));
            if(replay) notes(r.changed.plan,{72,76,79,82}); else notes(r.changed.plan,{82});
        }
        r.off(Minor7); require(!r.next());
        r.on(Minor7);
        if (style!=Simple && replay) { require(r.next()); notes(r.changed.plan,{72,76,79,82}); }
        else require(!r.next());
        r.on(Nine);
        if (style!=Simple) {
            require(r.next());
            if(replay) notes(r.changed.plan,{72,76,79,82,86}); else notes(r.changed.plan,{86});
        } else require(!r.next());
        r.off(C); r.off(Major); r.off(Minor7); r.off(Nine);
        r.on(C); r.on(Nine); require(!r.next());
        r.on(Minor);
        if(style!=Simple) { require(r.next()); notes(r.changed.plan,{72,75,79,86}); }
        else require(!r.next());
    }
}
void roots_and_octaves() {
    for(int octave=-3; octave<=3; ++octave) {
        Rig r; const u32 low=53+12*octave; r.controls.lowest_key=low;
        for(u32 key=low+12;key<low+24;++key) { r.on(key); notes(r.plan,{static_cast<int>(key)}); require(!r.off(key)); }
        r.on(low+3); require(r.plan.count==0);
        r.on(low+19); notes(r.plan,{static_cast<int>(low+19),static_cast<int>(low+22),static_cast<int>(low+26)});
        r.controls.lowest_key+=12; require(r.off(low+3)); require(!r.off(low+19));
    }
    Rig r; r.setting(Free);
    for(int key=65;key<73;++key) r.on(key);
    r.on(73); require(r.plan.count==0);
    r.on(Minor);
    for(int key=65;key<73;++key) { require(r.next()); require(r.changed.root==static_cast<u32>(key)); }
    require(!r.next());
    r.off(65); r.on(73); notes(r.plan,{73,76,80});
    r.setting(Addition); chords::reset(r.state);
    require(r.state.style==chords::kFree && r.state.replay==1);
    r.on(Major); require(!r.next()); r.on(C); notes(r.plan,{72,76,79});
}
void voicing_timing_and_display() {
    Rig r; r.on(Minor); r.on(Minor7); r.on(Nine); r.on(C);
    notes(r.plan,{72,75,79,82,86});
    Display d={}; chords::display(r.state,r.controls,d);
    require(std::string(d.text)=="simple/add  C minor 9");
    require(d.lit==((1u<<7)|(1u<<(12+3))|(1u<<(12+2))|(1u<<(12+6))|(1u<<(12+7))));
    require(std::string(d.values[0])=="0" && std::string(d.values[1])=="up" && std::string(d.values[2])=="0" && std::string(d.values[3])=="off");
    r.off(Minor); r.on(Major); chords::display(r.state,r.controls,d);
    require(std::string(d.text)=="simple/add  C minor 9");
    r.off(C); r.off(Minor7); r.off(Nine);
    r.controls.value[2]=top; r.on(C); notes(r.plan,{96,100,103}); r.off(C);
    r.controls.value[2]=0; r.on(C); notes(r.plan,{48,52,55}); r.off(C);
    r.controls.value[2]=centre; r.controls.value[3]=top; r.on(C); notes(r.plan,{72,79,88}); r.off(C);
    r.controls.value[3]=0; r.controls.value[0]=top; r.on(C);
    require(r.plan.tones[0].delay==0 && r.plan.tones[1].delay==63 && r.plan.tones[2].delay==126);
    r.off(C); r.controls.value[1]=0x60000000;
    r.on(C); require(r.plan.count==3); unsigned mask=0;
    for(unsigned i=0;i<3;++i) mask|=1u<<(r.plan.tones[i].pitch-C);
    require(mask==((1u<<0)|(1u<<4)|(1u<<7)));
    r.off(C); r.controls.value[2]=top; r.on(120); require(r.plan.count==0);
}
}
int main() {
    base_and_extensions(); styles(); additions(); roots_and_octaves(); voicing_timing_and_display();
    std::puts("Orchid modifier order, play styles, additions, octave shifts, held roots and display passed");
}
