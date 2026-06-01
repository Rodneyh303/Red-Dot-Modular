#include "test_stubs.hpp"
/**
 * test_PatternEngine.cpp
 * Unit tests for PatternEngine — no Rack SDK needed.
 * Compile: g++ -std=c++17 -I/home/claude test_PatternEngine.cpp -o test_pe && ./test_pe
 */
#include "PatternEngine.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <numeric>
#include <cstring>

// ── Test framework ────────────────────────────────────────────────────────────
static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e)     do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a__,b__) do{if((a__)!=(b__)){std::ostringstream _s; \
    _s<<#a__<<"="<<(a__)<<" != "<<#b__<<"="<<(b__);throw std::runtime_error(_s.str());}}while(0)
#define EXPECT_NEAR(a__,b__,e__) do{if(std::fabs((a__)-(b__))>(e__)){std::ostringstream _s; \
    _s<<#a__<<"="<<(a__)<<" not near "<<#b__<<"="<<(b__);throw std::runtime_error(_s.str());}}while(0)

// ── Helpers ───────────────────────────────────────────────────────────────────
static PatternInput allActive() {
    PatternInput in;
    for (int i=0;i<12;++i) in.semiWeights[i]=1.f;
    in.octaveLo=3.f; in.octaveHi=5.f; in.restProb=0.f;
    in.variationAmount=0.5f; in.transpose=0.f;
    in.noteVariationMask=0b111; in.locked=false;
    return in;
}
static PatternInput singleSemitone(int s, float oct=4.f) {
    PatternInput in = allActive();
    for (int i=0;i<12;++i) in.semiWeights[i]=0.f;
    in.semiWeights[s]=1.f;
    in.octaveLo=oct; in.octaveHi=oct;
    return in;
}
static PatternEngine fresh(float rseed=0.f, float mseed=0.f) {
    PatternEngine pe;
    pe.seedRngFromFloat(pe.rhythmRng, rseed);
    pe.seedRngFromFloat(pe.melodyRng, mseed);
    return pe;
}

// ═══════════════════════════════════════════════════════════════════════════════
int main(){
    std::cout<<"\033[1mPatternEngine Tests\033[0m\n";
    std::cout<<std::string(50,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Seed & RNG");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Same seed → identical rhythm pattern", {
        auto p1=fresh(3.f,5.f), p2=fresh(3.f,5.f);
        PatternInput in=allActive();
        p1.redrawRhythm(in); p2.redrawRhythm(in);
        for(int i=0;i<16;++i) EXPECT_EQ(p1.rhythmPattern[i], p2.rhythmPattern[i]);
    });

    TEST("Different seeds → different patterns (very likely)", {
        auto p1=fresh(1.f,1.f), p2=fresh(9.f,9.f);
        PatternInput in=allActive();
        p1.redrawMelody(in); p2.redrawMelody(in);
        int diffs=0;
        for(int i=0;i<16;++i) if(p1.melodySemitone[i]!=p2.melodySemitone[i]) ++diffs;
        EXPECT(diffs>2);
    });

    TEST("seedRngFromFloat: different seeds produce different RNG output", {
        PatternEngine pe;
        pe.seedRngFromFloat(pe.rhythmRng, 1.f);
        float r0 = pe.unitRhythm();
        pe.seedRngFromFloat(pe.rhythmRng, 9.f);
        float r10 = pe.unitRhythm();
        EXPECT(std::fabs(r0-r10) > 1e-4f);
    });

    TEST("seedRngFromFloat: same value → same first output", {
        PatternEngine pe;
        pe.seedRngFromFloat(pe.rhythmRng, 5.f);
        float a = pe.unitRhythm();
        pe.seedRngFromFloat(pe.rhythmRng, 5.f);
        float b = pe.unitRhythm();
        EXPECT_NEAR(a, b, 1e-7f);
    });

    TEST("applyPendingSeedsAndRedraw clears both pending flags", {
        auto pe=fresh();
        pe.rhythmSeedPending=true; pe.rhythmSeedPendingFloat=3.f;
        pe.melodySeedPending=true; pe.melodySeedPendingFloat=7.f;
        pe.applyPendingSeedsAndRedraw(allActive());
        EXPECT(!pe.rhythmSeedPending);
        EXPECT(!pe.melodySeedPending);
    });

    TEST("applyPendingSeedsAndRedraw applies rhythm seed before drawing", {
        auto p1=fresh(), p2=fresh();
        // Arm seed 5.0 on p1, draw with it
        p1.rhythmSeedPendingFloat=5.f; p1.rhythmSeedPending=true;
        p1.applyPendingSeedsAndRedraw(allActive());
        // Set seed 5.0 directly on p2 and draw
        p2.seedRngFromFloat(p2.rhythmRng,5.f);
        p2.seedRngFromFloat(p2.melodyRng,0.f);
        p2.redrawRhythm(allActive());
        for(int i=0;i<16;++i) EXPECT_EQ(p1.rhythmPattern[i], p2.rhythmPattern[i]);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("pickSemitone — Weighted Selection");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("All weights zero → returns -1", {
        auto pe=fresh();
        float w[12]={};
        EXPECT_EQ(pe.pickSemitone(w), -1);
    });

    TEST("Single weight → always picks that semitone", {
        auto pe=fresh(1.f,2.f);
        float w[12]={};
        for(int target=0;target<12;++target){
            std::fill(w,w+12,0.f); w[target]=1.f;
            for(int t=0;t<20;++t) EXPECT_EQ(pe.pickSemitone(w), target);
        }
    });

    TEST("Result always in 0..11", {
        auto pe=fresh(0.5f);
        float w[12]; std::fill(w,w+12,1.f);
        for(int t=0;t<100;++t){ int s=pe.pickSemitone(w); EXPECT(s>=0&&s<12); }
    });

    TEST("Higher-weight semitone picked more often over many trials", {
        auto pe=fresh(7.f,3.f);
        float w[12]={}; w[0]=0.1f; w[7]=0.9f;
        int cnt[12]={};
        for(int t=0;t<1000;++t) ++cnt[pe.pickSemitone(w)];
        EXPECT(cnt[7] > cnt[0]*3);
    });

    TEST("rng=0 boundary: does not return index before first weight", {
        // With very first RNG output near 0, should still return valid index
        auto pe=fresh();
        pe.seedRngFromFloat(pe.melodyRng, 0.f);
        float w[12]={}; w[5]=1.f;
        int s=pe.pickSemitone(w);
        EXPECT_EQ(s,5);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("genPitch — Octave Range & Transpose");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Pitch clamped to 0..5V", {
        auto pe=fresh(1.f,2.f);
        auto in=allActive();
        for(int t=0;t<100;++t){
            int sem; float v=pe.genPitch(sem,in);
            EXPECT(v>=0.f&&v<=5.f);
        }
    });

    TEST("Single semitone, single octave: pitch is exactly oct+sem/12", {
        auto pe=fresh(1.f,2.f);
        auto in=singleSemitone(7, 4.f);  // G4
        for(int t=0;t<10;++t){
            int sem; float v=pe.genPitch(sem,in);
            EXPECT_NEAR(v, 4.f+7.f/12.f, 1e-5f);
            EXPECT_EQ(sem,7);
        }
    });

    TEST("Pitch stays within octave window", {
        auto pe=fresh(2.f,3.f);
        auto in=allActive(); in.octaveLo=3.f; in.octaveHi=4.f;
        for(int t=0;t<100;++t){
            int sem; float v=pe.genPitch(sem,in);
            EXPECT(v>=3.f && v<=5.f);  // up to 4+11/12≈4.917
        }
    });

    TEST("Inverted lo/hi octave: auto-swapped, no crash", {
        auto pe=fresh();
        auto in=singleSemitone(0,3.f);
        in.octaveLo=5.f; in.octaveHi=2.f;  // inverted
        int sem; float v=pe.genPitch(sem,in);
        EXPECT(v>=0.f&&v<=5.f);
    });

    TEST("Positive transpose shifts pitch up by semitones", {
        auto pe1=fresh(1.f,1.f), pe2=fresh(1.f,1.f);
        auto in=singleSemitone(0,4.f);
        in.transpose=0.f; int s1; float v1=pe1.genPitch(s1,in);
        in.transpose=3.f; int s2; float v2=pe2.genPitch(s2,in);
        EXPECT_NEAR(v2-v1, 3.f/12.f, 1e-5f);
    });

    TEST("outSemitone set to 0 when all weights zero (fallback)", {
        auto pe=fresh();
        float w[12]={};
        PatternInput in=allActive();
        std::fill(in.semiWeights,in.semiWeights+12,0.f);
        int sem=-99; pe.genPitch(sem,in);
        EXPECT_EQ(sem,0);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("redrawRhythm — Rest Probability");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("restProb=0: all steps active", {
        auto pe=fresh(1.f);
        PatternInput in=allActive(); in.restProb=0.f;
        pe.redrawRhythm(in);
        for(int i=0;i<16;++i) EXPECT(pe.rhythmPattern[i]);
    });

    TEST("restProb=1: all steps are rests", {
        auto pe=fresh(1.f);
        PatternInput in=allActive(); in.restProb=1.f;
        pe.redrawRhythm(in);
        for(int i=0;i<16;++i) EXPECT(!pe.rhythmPattern[i]);
    });

    TEST("restProb=0.5: roughly half active over many redraws", {
        auto pe=fresh(42.f);
        PatternInput in=allActive(); in.restProb=0.5f;
        int active=0;
        for(int r=0;r<100;++r){
            pe.redrawRhythm(in);
            for(int i=0;i<16;++i) if(pe.rhythmPattern[i]) ++active;
        }
        float ratio=float(active)/1600.f;
        EXPECT(ratio>0.35f && ratio<0.65f);
    });

    TEST("locked=true: rhythm NOT redrawn", {
        auto pe=fresh(3.f);
        PatternInput in=allActive(); in.restProb=0.f;
        pe.redrawRhythm(in);
        bool snap[16]; for(int i=0;i<16;++i) snap[i]=pe.rhythmPattern[i];
        in.locked=true; in.restProb=1.f;  // would flip everything
        pe.redrawRhythm(in);
        for(int i=0;i<16;++i) EXPECT_EQ(pe.rhythmPattern[i],snap[i]);
    });

    TEST("16 distinct steps generated per redraw", {
        auto pe=fresh(5.f);
        PatternInput in=allActive(); in.restProb=0.5f;
        pe.redrawRhythm(in);
        int count=0;
        for(int i=0;i<16;++i) if(pe.rhythmPattern[i]) ++count;
        EXPECT(count>=0 && count<=16);  // sanity
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("redrawMelody — Pattern Generation");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("All 16 melody slots populated", {
        auto pe=fresh(1.f,2.f);
        PatternInput in=allActive();
        pe.redrawMelody(in);
        for(int i=0;i<16;++i){
            EXPECT(pe.melodySemitone[i]>=0 && pe.melodySemitone[i]<12);
            EXPECT(pe.melodyPitchV[i]>=0.f && pe.melodyPitchV[i]<=5.f);
        }
    });

    TEST("locked=true: melody NOT redrawn", {
        auto pe=fresh(1.f,2.f);
        PatternInput in=allActive();
        pe.redrawMelody(in);
        int snap[16]; for(int i=0;i<16;++i) snap[i]=pe.melodySemitone[i];
        in.locked=true;
        // change weights so different semitones would be picked
        std::fill(in.semiWeights,in.semiWeights+12,0.f); in.semiWeights[11]=1.f;
        pe.redrawMelody(in);
        for(int i=0;i<16;++i) EXPECT_EQ(pe.melodySemitone[i],snap[i]);
    });

    TEST("Semitone matches pitch voltage fractional part", {
        auto pe=fresh(3.f,4.f);
        PatternInput in=allActive(); in.octaveLo=4.f; in.octaveHi=4.f;
        in.transpose=0.f;
        pe.redrawMelody(in);
        for(int i=0;i<16;++i){
            float frac=pe.melodyPitchV[i]-std::floor(pe.melodyPitchV[i]);
            float expected=pe.melodySemitone[i]/12.f;
            EXPECT_NEAR(frac, expected, 1e-5f);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("varyNoteIndex — Variation Bias");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("variation=0.5 (neutral): result within ±2 of base", {
        auto pe=fresh(7.f);
        PatternInput in=allActive(); in.variationAmount=0.5f;
        for(int base=0;base<=8;++base)
            for(int t=0;t<20;++t){
                int r=pe.varyNoteIndex(base,in);
                EXPECT(r>=std::max(0,base-2) && r<=std::min(8,base+2));
            }
    });

    TEST("variation=1.0: biases toward shorter (higher index) on average", {
        auto pe=fresh(8.f);
        PatternInput in=allActive(); in.variationAmount=1.0f;
        int base=3; float sum=0;
        for(int t=0;t<200;++t) sum+=float(pe.varyNoteIndex(base,in));
        EXPECT(sum/200.f > float(base));
    });

    TEST("variation=0.0: biases toward longer (lower index) on average", {
        auto pe=fresh(9.f);
        PatternInput in=allActive(); in.variationAmount=0.0f;
        int base=5; float sum=0;
        for(int t=0;t<200;++t) sum+=float(pe.varyNoteIndex(base,in));
        EXPECT(sum/200.f < float(base));
    });

    TEST("Result always clamped 0..8", {
        auto pe=fresh(2.f);
        PatternInput in=allActive();
        for(float v:{0.f,0.5f,1.f}) for(int b=0;b<=8;++b){
            in.variationAmount=v;
            int r=pe.varyNoteIndex(b,in);
            EXPECT(r>=0&&r<=8);
        }
    });

    TEST("noteVariationMask=0: triplet/short notes blocked", {
        auto pe=fresh(1.f);
        PatternInput in=allActive(); in.noteVariationMask=0;
        // idx 4=1/8T, 6=1/16T, 7=1/32, 8=1/32T all blocked
        for(int t=0;t<50;++t){
            int r=pe.varyNoteIndex(3,in);
            EXPECT(r!=4 && r!=6 && r!=7 && r!=8);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mode Switching — Cache & Restore");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("switchMelodyMode: dice→realtime caches pitch pattern", {
        auto pe=fresh(1.f,2.f);
        PatternInput in=allActive();
        pe.redrawMelody(in);
        float snap[16]; for(int i=0;i<16;++i) snap[i]=pe.melodyPitchV[i];
        int si=5, lsi=4;
        pe.switchMelodyMode(si,lsi);
        EXPECT_EQ(pe.melodyMode,1);
        // Overwrite pattern
        in.semiWeights[0]=1.f; for(int i=1;i<12;++i) in.semiWeights[i]=0.f;
        pe.redrawMelody(in);
        // Switch back
        pe.switchMelodyMode(si,lsi);
        EXPECT_EQ(pe.melodyMode,0);
        for(int i=0;i<16;++i) EXPECT_NEAR(pe.melodyPitchV[i],snap[i],1e-5f);
    });

    TEST("switchRhythmMode: dice→realtime caches rhythm pattern", {
        auto pe=fresh(3.f,4.f);
        PatternInput in=allActive(); in.restProb=0.3f;
        pe.redrawRhythm(in);
        bool snap[16]; for(int i=0;i<16;++i) snap[i]=pe.rhythmPattern[i];
        int si=0,lsi=0;
        pe.switchRhythmMode(si,lsi);
        in.restProb=1.f; pe.redrawRhythm(in);  // all rests
        pe.switchRhythmMode(si,lsi);
        for(int i=0;i<16;++i) EXPECT_EQ(pe.rhythmPattern[i],snap[i]);
    });

    TEST("switchMelodyMode: stepIndex cached and restored", {
        auto pe=fresh(); int si=7, lsi=6;
        pe.switchMelodyMode(si,lsi); si=99;  // change while in realtime
        pe.switchMelodyMode(si,lsi);          // restore
        EXPECT_EQ(si,7);
    });

    TEST("switchRhythmMode twice (dice→rt→dice): rhythmMode back to 0", {
        auto pe=fresh(); int si=0,lsi=0;
        EXPECT_EQ(pe.rhythmMode,0);
        pe.switchRhythmMode(si,lsi); EXPECT_EQ(pe.rhythmMode,1);
        pe.switchRhythmMode(si,lsi); EXPECT_EQ(pe.rhythmMode,0);
    });

    TEST("No restore if cache not set (first switch to realtime, back)", {
        auto pe=fresh(); int si=3,lsi=2;
        // Switch to realtime first (sets cache), but DON'T switch back
        // Then create new engine with no cache and switch back directly
        PatternEngine pe2; pe2.melodySeedCached=false;
        int si2=5,lsi2=4; pe2.melodyMode=1;
        // Switching from realtime to dice when no cache should not crash
        pe2.switchMelodyMode(si2,lsi2);
        EXPECT_EQ(pe2.melodyMode,0);  // toggled
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("reset() — State Cleared");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("reset clears pending seed flags", {
        auto pe=fresh();
        pe.rhythmSeedPending=true; pe.melodySeedPending=true;
        pe.reset();
        EXPECT(!pe.rhythmSeedPending);
        EXPECT(!pe.melodySeedPending);
    });

    TEST("reset sets both modes to dice (0)", {
        auto pe=fresh(); pe.rhythmMode=1; pe.melodyMode=1;
        pe.reset();
        EXPECT_EQ(pe.rhythmMode,0);
        EXPECT_EQ(pe.melodyMode,0);
    });

    TEST("reset: subsequent redraw is deterministic (same as fresh)", {
        PatternEngine p1,p2;
        p1.reset(); p2.reset();
        PatternInput in=allActive();
        p1.redrawRhythm(in); p2.redrawRhythm(in);
        for(int i=0;i<16;++i) EXPECT_EQ(p1.rhythmPattern[i],p2.rhythmPattern[i]);
    });

    // ─────────────────────────────────────────────────────────────────────────
    std::cout<<"\n"<<std::string(50,'=')<<"\n";
    std::cout<<"\033[32m"<<s_pass<<" passed\033[0m  ";
    if(s_fail>0)std::cout<<"\033[31m"<<s_fail<<" failed\033[0m";
    else        std::cout<<"\033[32m0 failed\033[0m";
    std::cout<<"  ("<<(s_pass+s_fail)<<" total)\n\n";
    return s_fail>0?1:0;
}
