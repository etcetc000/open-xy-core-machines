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
           Nine=59, Eleven=60, Sus=61, Thirteen=62, Unused=63, Flat9=64, C=72 };
struct Rig {
    chords::State state = {};
    Controls controls = {{0, 0, centre, 0}, 53};
    Plan plan = {};
    void on(u32 note, u32 velocity=90) { require(chords::press(state, controls, note, velocity, plan)); }
    bool off(u32 note) { return chords::release(state, note); }
};
void notes(const Plan& plan, std::initializer_list<int> pitches, int velocity=90) {
    require(plan.count == pitches.size());
    unsigned i=0;
    for (int p : pitches) { require(plan.tones[i].pitch == p); require(plan.tones[i++].velocity == velocity); }
}
void base_and_extensions() {
    const int types[] = {Dim, Minor, Major, Sus};
    const int middle[] = {3,3,4,5}, fifth[] = {6,7,7,7};
    const int extensions[] = {Six, Minor7, Major7, Nine, Eleven, Thirteen, Flat9}, intervals[] = {9,10,11,14,17,21,13};
    for (unsigned t=0; t<4; ++t) {
        Rig r;
        r.on(types[t]); require(r.plan.count == 0);
        r.on(C); notes(r.plan, {C,C+middle[t],C+fifth[t]});
        require(!r.off(C));
        for (unsigned e=0; e<7; ++e) {
            r.on(extensions[e]); require(r.plan.count == 0);
            r.on(C); notes(r.plan, {C,C+middle[t],C+fifth[t],C+intervals[e]});
            require(!r.off(C)); require(r.off(extensions[e]));
        }
        require(r.off(types[t]));
    }
    Rig r;
    r.on(Major); for (int k : extensions) r.on(k);
    r.on(C); notes(r.plan,{72,76,82,83,85,86,89,93});
    r.off(C); r.off(Major); r.on(C); notes(r.plan,{72});
}
void simple_only() {
    Rig r;
    r.on(C,77); notes(r.plan,{72},77);
    r.on(Major); require(r.plan.count==0 && r.state.roots[0].type==chords::kNone);
    r.off(C); r.on(C); notes(r.plan,{72,76,79});
    r.off(Major); r.on(Minor); r.on(Eleven); r.on(Thirteen); r.on(Flat9);
    require(r.plan.count==0 && r.state.roots[0].type==chords::kMaj && r.state.roots[0].extensions==0);
    Display shown={}; chords::display(r.state,r.controls,shown);
    require(std::string(shown.text)=="C major");
    r.off(C); r.on(C); notes(r.plan,{72,75,79,85,89,93});
    chords::display(r.state,r.controls,shown);
    require(std::string(shown.text)=="C minor add 11 add 13 b9");
    r.on(Unused); require(r.plan.count==0); require(r.off(Unused));
    require((shown.lit & (1u<<(12+10)))==0);
    r.off(Minor); r.off(Eleven); r.off(Thirteen); r.off(Flat9);
    require(r.state.roots[0].type==chords::kMin);
    r.off(C); r.on(C); notes(r.plan,{72});
}
void extension_combinations() {
    const int keys[]={Six,Minor7,Major7,Nine,Eleven,Thirteen,Flat9};
    const int pitches[]={81,82,83,86,89,93,85};
    for(unsigned mask=0;mask<128;++mask) {
        Rig r; r.on(Major);
        for(unsigned i=0;i<7;++i) if(mask & (1u<<i)) r.on(keys[i]);
        r.on(C); require(r.plan.count<=8);
        for(unsigned i=0;i<7;++i) if((mask & (1u<<i)) && !(i==0 && (mask & 32))) {
            bool found=false;
            for(unsigned j=0;j<r.plan.count;++j) if(r.plan.tones[j].pitch==pitches[i]) found=true;
            require(found);
        }
        for(unsigned i=1;i<r.plan.count;++i) require(r.plan.tones[i-1].pitch<r.plan.tones[i].pitch);
    }
    Rig r; r.on(Major); r.on(Six); r.on(Thirteen); r.on(C);
    notes(r.plan,{72,76,79,93}); // 13 replaces the same pitch class in the sixth's register
}
void roots_and_octaves() {
    for(int octave=-3; octave<=3; ++octave) {
        Rig r; const u32 low=53+12*octave; r.controls.lowest_key=low;
        for(u32 key=low+12;key<low+24;++key) { r.on(key); notes(r.plan,{static_cast<int>(key)}); require(!r.off(key)); }
        r.on(low+3); require(r.plan.count==0);
        r.on(low+19); notes(r.plan,{static_cast<int>(low+19),static_cast<int>(low+22),static_cast<int>(low+26)});
        r.controls.lowest_key+=12; require(r.off(low+3)); require(!r.off(low+19));
    }
    Rig r;
    for(int key=65;key<73;++key) r.on(key);
    r.on(73); require(r.plan.count==0);
    r.on(Minor);
    for(const auto& root : r.state.roots) require(root.type==chords::kNone);
    r.off(65); r.on(73); notes(r.plan,{73,76,80});
    chords::reset(r.state);
    for(const auto& root : r.state.roots) require(root.note==0);
    r.on(Major); r.on(C); notes(r.plan,{72,76,79});
}
void voicing_timing_and_display() {
    Rig r; r.on(Minor); r.on(Minor7); r.on(Nine); r.on(C);
    notes(r.plan,{72,75,79,82,86});
    Display d={}; chords::display(r.state,r.controls,d);
    require(std::string(d.text)=="C minor 9");
    require(d.lit==((1u<<7)|(1u<<(12+3))|(1u<<(12+2))|(1u<<(12+6))));
    require(std::string(d.values[0])=="0" && std::string(d.values[1])=="up" && std::string(d.values[2])=="0" && std::string(d.values[3])=="off");
    r.off(Minor); r.on(Major); chords::display(r.state,r.controls,d);
    require(std::string(d.text)=="C minor 9");
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
void overlapping_roles_after_octave_shift() {
    // A sounding root later receives a modifier press at the same pitch.
    Rig r;
    r.on(66); notes(r.plan,{66});
    r.controls.lowest_key=65;
    r.on(66); require(r.plan.count==0 && r.state.held!=0);
    require(!r.off(66)); // must forward stock note-off, despite modifier ownership
    require(r.state.held==0 && r.state.notes[1]==0);
    for (const auto& root : r.state.roots) require(root.note==0);
    // The opposite order must also clear both owners with one release.
    r.on(66); require(r.plan.count==0);
    r.controls.lowest_key=53;
    r.on(66); notes(r.plan,{66,69,72});
    require(!r.off(66));
    require(r.state.held==0 && r.state.notes[1]==0);
    for (const auto& root : r.state.roots) require(root.note==0);
    // The same contract holds for extensions, including the unused key.
    for (u32 key : {65u,67u,69u,71u,72u,74u,75u,76u}) {
        Rig ext;
        ext.on(key); require(ext.plan.count==1);
        ext.controls.lowest_key=65;
        ext.on(key); require(ext.plan.count==0);
        require(!ext.off(key));
        require(ext.state.extensions==0);
        for (const auto& root : ext.state.roots) require(root.note==0);
    }
}
void empty_retrigger_preserves_sounding_root() {
    Rig r;
    r.on(120); notes(r.plan,{120});
    r.on(Major);
    r.controls.value[2]=top;
    r.on(120); require(r.plan.count==0);
    require(r.state.roots[0].note==121 && r.state.roots[0].type==chords::kNone);
    Display shown={}; chords::display(r.state,r.controls,shown);
    require(std::string(shown.text)=="C");
    r.controls.lowest_key=109;
    r.on(120); require(r.plan.count==0);
    require(!r.off(120)); // the original stock record still needs releasing
    require(r.state.roots[0].note==0 && r.state.extensions==0);
    Rig full;
    for (u32 note=100;note<108;++note) full.on(note);
    full.controls.value[2]=top;
    full.on(100); require(full.plan.count==0);
    full.controls.value[2]=centre;
    full.on(108); require(full.plan.count==0); // empty retrigger freed no stock slot
    for (u32 i=0;i<8;++i) require(full.state.roots[i].note==101+i);
}
}
int main() {
    base_and_extensions(); simple_only(); extension_combinations(); roots_and_octaves(); voicing_timing_and_display();
    overlapping_roles_after_octave_shift();
    empty_retrigger_preserves_sounding_root();
    std::puts("Simple gestures, all seven extensions and combinations, octave shifts, held roots and display passed");
}
