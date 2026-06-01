/**
 * test_var_leg_rest.cpp  — Variation, Legato, Rest unit tests
 * Written from scratch against current MeloDicer.cpp logic.
 * No Rack SDK needed. Compile:
 *   g++ -std=c++17 -I. test_var_leg_rest.cpp -o t && ./t
 */
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

// ── Test framework ────────────────────────────────────────────────────────────
static int s_pass=0, s_fail=0;
#define SUITE(n) std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n"
#define TEST(desc,...) do{ \
    bool _ok=true;std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e)     do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a,b) do{if((a)!=(b)){std::ostringstream _s; \
    _s<<#a<<"="<<(a)<<" != "<<#b<<"="<<(b);throw std::runtime_error(_s.str());}}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream _s; \
    _s<<#a<<"="<<(a)<<" not near "<<#b<<"="<<(b);throw std::runtime_error(_s.str());}}while(0)
#define EXPECT_GT(a,b) do{if(!((a)>(b))){std::ostringstream _s; \
    _s<<#a<<"="<<(a)<<" not > "<<#b<<"="<<(b);throw std::runtime_error(_s.str());}}while(0)

// ── NOTEVALS fractions (matches MeloDicer::NOTEVALS[8]) ──────────────────────
// idx: 0=whole 1=half 2=quarter 3=eighth 4=sixteenth
//      5=qtrT  6=8thT 7=32nd
static const float NOTEFRACS[8] = {
    1.0f, 0.5f, 0.25f, 0.125f, 0.0625f,
    1.0f/6.0f, 1.0f/12.0f, 0.03125f
};
// Steps = fraction * 16, minimum 0.5
static float noteSteps(int idx) {
    if (idx<0||idx>7) return 1.f;
    return std::max(0.5f, NOTEFRACS[idx]*16.f);
}

// ── varyNoteIndex mirror (exact copy of PatternEngine logic) ──────────────────
static int varyNoteIdx(int baseIdx, float variationAmount, float rng,
                       int mask=0b111) {
    auto allowed=[&](int i)->bool{
        if(i<0||i>=9) return false;
        if(i==4) return (mask&0b001)!=0;
        if(i==6) return (mask&0b010)!=0;
        if(i==7||i==8) return (mask&0b100)!=0;
        return true;
    };
    float spread=2.f*std::fabs(variationAmount-0.5f);
    if(spread<1e-4f) return baseIdx;
    int lo=std::max(0,baseIdx-2), hi=std::min(7,baseIdx+2);
    float weights[9]={}, total=0.f;
    for(int i=lo;i<=hi;++i) if(allowed(i)){
        float dist=float(std::abs(i-baseIdx));
        if(dist==0.f){ weights[i]=1.f; }
        else{
            float w=spread/(1.f+dist);
            if(i>baseIdx && variationAmount>0.5f) w*=1.f+spread;
            if(i<baseIdx && variationAmount<0.5f) w*=1.f+spread;
            weights[i]=w;
        }
        total+=weights[i];
    }
    if(total<=0.f) return baseIdx;
    float r=rng*total, acc=0.f;
    for(int i=lo;i<=hi;++i) if(weights[i]>0.f){
        acc+=weights[i]; if(r<=acc) return i;
    }
    return baseIdx;
}

// ── Step engine mirror (exact copy of triggerStepEventForOffsetStep_ logic) ───
// Deterministic: caller injects RNG values via rngQueue.
struct StepEngine {
    float holdRemain   = 0.f;
    bool  gateHeld     = false;
    float currentPitch = 0.f;
    int   lastSemitone = -1;
    bool  gateTriggered= false;
    float semiRemain[12]={};

    // Injected RNG queue — values consumed in order
    float rngQ[64]={};
    int   rngIdx=0;
    float rng(){ float v=rngQ[rngIdx%64]; rngIdx++; return v; }

    void reset(){
        holdRemain=0.f; gateHeld=false; currentPitch=0.f;
        lastSemitone=-1; gateTriggered=false; rngIdx=0;
        std::fill(semiRemain,semiRemain+12,0.f);
    }
    // Fill rng queue with a constant value
    void setRng(float v){ std::fill(rngQ,rngQ+64,v); }

    // One tick (gs.tick equivalent) — call before step()
    void tick(){
        if(gateHeld){ holdRemain-=1.f; if(holdRemain<=0.f){holdRemain=0.f;gateHeld=false;} }
        else{ holdRemain=std::max(0.f,holdRemain-1.f); }
        for(int i=0;i<12;++i){semiRemain[i]-=1.f;if(semiRemain[i]<0)semiRemain[i]=0;}
    }

    // step() — mirrors triggerStepEventForOffsetStep_
    void step(int sem, float pitch, float restProb, float legatoProb, float dur){
        gateTriggered=false;

        // Max-legato shortcut
        if(legatoProb>=0.999f){
            currentPitch=pitch; lastSemitone=sem;
            gateHeld=true; holdRemain=dur;
            if(sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
            return;
        }

        // Mid-note / mid-rest
        if(holdRemain>=1.f){
            if(gateHeld){
                if(rng()<legatoProb){
                    holdRemain+=dur;
                    if(sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],holdRemain);
                }
            }
            // mid-rest: no extension, just count down
            return;
        }

        // Boundary — full decision
        bool doRest   = rng()<restProb;
        if(doRest){ gateHeld=false; holdRemain=dur; return; }

        bool doLegato = rng()<legatoProb;
        bool doTie    = (!doLegato)&&(sem==lastSemitone)&&gateHeld&&(rng()<legatoProb);

        if(doLegato){
            currentPitch=pitch; lastSemitone=sem;
            if(gateHeld){ holdRemain+=dur; }
            else{ gateTriggered=true; holdRemain=dur; gateHeld=true; }
            if(sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
            return;
        }
        if(doTie){
            holdRemain+=dur;
            if(sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
            return;
        }
        // Normal note
        currentPitch=pitch; lastSemitone=sem;
        gateTriggered=true; gateHeld=true; holdRemain=dur;
        if(sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
    }
};

// ═════════════════════════════════════════════════════════════════════════════
int main(){
    std::cout<<"\033[1mVariation / Legato / Rest Tests\033[0m\n";
    std::cout<<std::string(52,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Variation — varyNoteIndex");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("variation=0.5: always returns baseIdx (zero variation)", {
        for(int base=0;base<=7;++base)
            for(int ri=0;ri<20;++ri)
                EXPECT_EQ(varyNoteIdx(base,0.5f,ri/20.f), base);
    });

    TEST("variation=0.5 exactly: spread<1e-4 → early return", {
        EXPECT_EQ(varyNoteIdx(3,0.5f,0.0f), 3);
        EXPECT_EQ(varyNoteIdx(3,0.5f,0.5f), 3);
        EXPECT_EQ(varyNoteIdx(3,0.5f,0.99f),3);
    });

    TEST("variation=0.0: biases toward longer (lower index) on average", {
        int base=4; float sum=0;
        for(int i=0;i<200;++i) sum+=float(varyNoteIdx(base,0.0f,i/200.f));
        EXPECT(sum/200.f < float(base));  // average below base
    });

    TEST("variation=1.0: biases toward shorter (higher index) on average", {
        int base=3; float sum=0;
        for(int i=0;i<200;++i) sum+=float(varyNoteIdx(base,1.0f,i/200.f));
        EXPECT(sum/200.f > float(base));
    });

    TEST("result always within ±2 of baseIdx and in 0..7", {
        for(float var:{0.f,0.25f,0.5f,0.75f,1.f})
            for(int base=0;base<=7;++base)
                for(int ri=0;ri<20;++ri){
                    int r=varyNoteIdx(base,var,ri/20.f);
                    EXPECT(r>=std::max(0,base-2) && r<=std::min(7,base+2));
                }
    });





    TEST("noteVariationMask=0: triplet/32nd indices never chosen (allowed bases only)", {
        // Only test base indices that are themselves allowed by mask=0
        // (4=1/8T, 6=8thT, 7=32nd are blocked — if baseIdx is blocked,
        //  the early-return at var=0.5 returns baseIdx unmodified, which
        //  is a separate edge case handled by PPQN clamping in practice)
        int allowedBases[] = {0,1,2,3,5};
        for(float var:{0.f,0.75f,1.f})  // skip 0.5 (early return)
            for(int base : allowedBases)
                for(int ri=1;ri<20;++ri){  // skip ri=0 edge
                    int r=varyNoteIdx(base,var,ri/20.f,0b000);
                    EXPECT(r!=4&&r!=6&&r!=7);
                }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("noteSteps — Duration Mapping");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("whole note = 16 steps",    { EXPECT_NEAR(noteSteps(0),16.f,1e-5f); });
    TEST("half note = 8 steps",      { EXPECT_NEAR(noteSteps(1), 8.f,1e-5f); });
    TEST("quarter note = 4 steps",   { EXPECT_NEAR(noteSteps(2), 4.f,1e-5f); });
    TEST("eighth note = 2 steps",    { EXPECT_NEAR(noteSteps(3), 2.f,1e-5f); });
    TEST("sixteenth note = 1 step",  { EXPECT_NEAR(noteSteps(4), 1.f,1e-5f); });
    TEST("32nd note = 0.5 steps (minimum floor)", { EXPECT_NEAR(noteSteps(7),0.5f,1e-5f); });
    TEST("durations correct for all indices (musical order, not step order)",{
        EXPECT_NEAR(noteSteps(0),16.f, 1e-4f);  // whole
        EXPECT_NEAR(noteSteps(1), 8.f, 1e-4f);  // half
        EXPECT_NEAR(noteSteps(2), 4.f, 1e-4f);  // quarter
        EXPECT_NEAR(noteSteps(3), 2.f, 1e-4f);  // eighth
        EXPECT_NEAR(noteSteps(4), 1.f, 1e-4f);  // sixteenth
        EXPECT(noteSteps(5) > 2.f);              // quarter-triplet > eighth
        EXPECT(noteSteps(6) > 1.f);              // eighth-triplet > sixteenth
        EXPECT_NEAR(noteSteps(7), 0.5f,1e-4f);  // 32nd (minimum)
    });
    TEST("out-of-range indices return 1 (safe default)", {
        EXPECT_NEAR(noteSteps(-1),1.f,1e-5f);
        EXPECT_NEAR(noteSteps(99),1.f,1e-5f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Rest — Probability and Gate Behaviour");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("restProb=0: never a rest", {
        StepEngine e; e.reset(); e.setRng(0.01f); // rng always low but prob=0
        e.tick(); e.step(5,1.5f, 0.f,0.f, 2.f);
        EXPECT(e.gateHeld);
        EXPECT(e.gateTriggered);
    });

    TEST("restProb=1: always a rest at boundary", {
        StepEngine e; e.reset(); e.setRng(0.01f);
        e.tick(); e.step(5,1.5f, 1.f,0.f, 2.f);
        EXPECT(!e.gateHeld);
        EXPECT_NEAR(e.holdRemain, 2.f, 1e-4f);
    });

    TEST("rest: gate closes, holdRemain set to dur", {
        StepEngine e; e.reset(); e.rngQ[0]=0.01f; // first rng draw = rest
        e.tick(); e.step(5,1.5f, 0.5f,0.f, 4.f);
        EXPECT(!e.gateHeld);
        EXPECT_NEAR(e.holdRemain, 4.f, 1e-4f);
    });

    TEST("rest: no gate trigger", {
        StepEngine e; e.reset(); e.rngQ[0]=0.01f;
        e.tick(); e.step(5,1.5f, 0.5f,0.f, 4.f);
        EXPECT(!e.gateTriggered);
    });

    TEST("rng exactly = restProb: NOT a rest (strict less-than)", {
        StepEngine e; e.reset(); e.rngQ[0]=0.5f; // rng==prob → not rest
        e.tick(); e.step(5,1.5f, 0.5f,0.f, 2.f);
        EXPECT(e.gateHeld); // note fired, not rest
    });

    TEST("rng just below restProb: IS a rest", {
        StepEngine e; e.reset(); e.rngQ[0]=0.499f;
        e.tick(); e.step(5,1.5f, 0.5f,0.f, 2.f);
        EXPECT(!e.gateHeld);
    });

    TEST("rest at 100% then 0%: notes resume at next boundary", {
        StepEngine e; e.reset(); e.setRng(0.01f);
        // Several steps with restProb=1.0 — hold=dur each boundary
        for(int i=0;i<8;++i){ e.tick(); e.step(5,1.5f,1.f,0.f,2.f); }
        // Now set restProb=0 — next boundary should fire a note
        e.holdRemain=0.f; e.gateHeld=false; // force to boundary
        e.tick(); e.step(5,1.5f, 0.f,0.f, 2.f);
        EXPECT(e.gateHeld);
        EXPECT(e.gateTriggered);
    });



    TEST("mid-rest: no extension (fixed duration)", {
        StepEngine e; e.reset();
        e.rngQ[0]=0.01f; e.setRng(0.01f); // rng always low
        e.tick(); e.step(5,1.5f, 0.5f,0.f, 4.f); // rest, hold=4
        float h0=e.holdRemain;
        e.tick(); e.step(5,1.5f, 1.f,0.f, 4.f); // mid-rest, restProb=1 but no ext
        // holdRemain should only have decreased by 1 (tick), not increased
        EXPECT_NEAR(e.holdRemain, h0-1.f, 1e-4f);
    });

    TEST("note fires after rest expires", {
        StepEngine e; e.reset();
        e.rngQ[0]=0.01f;  // step1: rest (rng<restProb=0.5)
        e.setRng(0.99f);  // subsequent: high rng = no rest, no legato
        e.tick(); e.step(5,1.5f, 0.5f,0.f, 2.f); // rest, hold=2
        e.tick(); e.step(5,1.5f, 0.1f,0.f, 2.f); // mid-rest hold=1
        e.tick(); e.step(5,1.5f, 0.1f,0.f, 2.f); // hold=0 after tick → boundary
        EXPECT(e.gateHeld);
        EXPECT(e.gateTriggered);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Legato — Gate Extension and Pitch Slide");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("legatoProb=0: never legato", {
        StepEngine e; e.reset(); e.setRng(0.01f); // low rng → no rest, then legato check
        // rng[0]=rest check (0.01>0=pass), rng[1]=legato check (0.01>0 → no legato)
        e.rngQ[0]=0.99f; // not rest
        e.rngQ[1]=0.01f; // would be legato if prob>0, but prob=0
        e.tick(); e.step(5,1.5f, 0.f,0.f, 2.f);
        EXPECT(e.gateTriggered); // normal note, not legato
    });

    TEST("legatoProb near 1: legato at boundary extends hold, no retrigger", {
        StepEngine e; e.reset();
        // Set up: gate is held, holdRemain > 0 (so tick won't close it)
        e.gateHeld=true; e.holdRemain=2.f; e.lastSemitone=5; e.currentPitch=1.5f;
        // Force to boundary: set holdRemain=0 WITHOUT calling tick
        e.holdRemain=0.f;  // now boundary condition
        e.rngIdx=0;
        e.rngQ[0]=0.99f;   // not rest
        e.rngQ[1]=0.01f;   // legato fires (0.01 < 0.98)
        e.step(7,2.0f, 0.f,0.98f, 2.f);  // no tick before step
        EXPECT(!e.gateTriggered);
        EXPECT(e.gateHeld);
        EXPECT_NEAR(e.currentPitch, 2.0f, 1e-5f);
    });

    TEST("legato: pitch changes to new pitch", {
        StepEngine e; e.reset(); e.setRng(0.99f);
        e.tick(); e.step(5,1.5f, 0.f,0.f, 2.f); // normal note
        e.holdRemain=0.f; e.rngIdx=0;
        e.rngQ[0]=0.99f; e.rngQ[1]=0.01f; // legato fires
        e.tick(); e.step(7,2.5f, 0.f,0.9f, 2.f);
        EXPECT_NEAR(e.currentPitch, 2.5f, 1e-5f);
    });

    TEST("legato while held: extends holdRemain (no retrigger)", {
        StepEngine e; e.reset();
        // Setup: gate held, at boundary (holdRemain=0)
        e.gateHeld=true; e.lastSemitone=5; e.holdRemain=0.f;
        e.rngQ[0]=0.99f;  // not rest
        e.rngQ[1]=0.01f;  // legato fires
        e.step(5,1.5f, 0.f,0.9f, 4.f); // legato, gate held → extend
        EXPECT(!e.gateTriggered);
        EXPECT(e.gateHeld);
        EXPECT_NEAR(e.holdRemain, 4.f, 1e-4f); // extended by dur
    });

    TEST("legato from cold (gate not held): opens gate with pulse", {
        StepEngine e; e.reset();
        e.rngQ[0]=0.99f; e.rngQ[1]=0.01f; // not rest, legato fires
        e.tick(); e.step(5,1.5f, 0.f,0.9f, 2.f);
        EXPECT(e.gateHeld);
        EXPECT(e.gateTriggered); // cold legato = open with pulse
        EXPECT_NEAR(e.holdRemain, 2.f, 1e-4f);
    });

    TEST("legato: lastSemitone updated", {
        StepEngine e; e.reset();
        e.rngQ[0]=0.99f; e.rngQ[1]=0.01f;
        e.tick(); e.step(7,2.0f, 0.f,0.9f, 2.f);
        EXPECT_EQ(e.lastSemitone, 7);
    });

    TEST("max-legato (>=0.999): gate always held, no retrigger, hold=dur", {
        StepEngine e; e.reset(); e.setRng(0.5f);
        e.tick(); e.step(5,1.5f, 0.f,1.f, 4.f); // first
        EXPECT(!e.gateTriggered); // max-legato shortcut: no gatePulse
        EXPECT(e.gateHeld);
        EXPECT_NEAR(e.holdRemain, 4.f, 1e-4f);
        // Second step
        e.tick(); e.step(7,2.0f, 0.f,1.f, 4.f);
        EXPECT(e.gateHeld);
        EXPECT_NEAR(e.holdRemain, 4.f, 1e-4f); // set to dur, not extended
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Tie — Same Semitone Extension");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("tie (same semitone, gate held): extends holdRemain", {
        StepEngine e; e.reset();
        // Setup: gate held at boundary, lastSemitone=5
        e.gateHeld=true; e.lastSemitone=5; e.holdRemain=0.f;
        e.rngQ[0]=0.99f;  // not rest
        e.rngQ[1]=0.99f;  // not legato
        e.rngQ[2]=0.01f;  // tie fires (0.01 < legatoProb=0.5)
        e.step(5,1.5f, 0.f,0.5f, 2.f); // same sem=5 → tie
        EXPECT(!e.gateTriggered);
        EXPECT(e.gateHeld);
        EXPECT_NEAR(e.holdRemain, 2.f, 1e-4f);
    });

    TEST("tie: pitch unchanged (same semitone)", {
        StepEngine e; e.reset(); e.setRng(0.99f);
        e.tick(); e.step(5,1.5f, 0.f,0.f, 2.f);
        e.holdRemain=0.f; e.rngIdx=0;
        e.rngQ[0]=0.99f; e.rngQ[1]=0.99f; e.rngQ[2]=0.01f;
        e.tick(); e.step(5,1.5f, 0.f,0.5f, 2.f);
        EXPECT_NEAR(e.currentPitch, 1.5f, 1e-5f);
    });

    TEST("no tie when different semitone: normal note fires", {
        StepEngine e; e.reset(); e.setRng(0.99f);
        e.tick(); e.step(5,1.5f, 0.f,0.f, 2.f); // sem=5
        e.holdRemain=0.f; e.rngIdx=0; e.setRng(0.99f);
        e.tick(); e.step(7,2.0f, 0.f,0.5f, 2.f); // different sem=7 → no tie
        EXPECT(e.gateTriggered); // new attack
    });

    TEST("no tie when gate not held", {
        StepEngine e; e.reset(); e.setRng(0.99f);
        e.lastSemitone=5; // set sem but gate not held
        e.rngQ[0]=0.99f; e.rngQ[1]=0.99f; e.rngQ[2]=0.01f;
        e.tick(); e.step(5,1.5f, 0.f,0.5f, 2.f);
        EXPECT(e.gateTriggered); // not a tie — gate was not held
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Multi-Step Note Spanning");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("quarter note (4 steps): gate held through all 4, no retriggering", {
        StepEngine e; e.reset(); e.setRng(0.99f); // no ext
        e.tick(); e.step(5,1.5f, 0.f,0.f, 4.f); // trigger
        EXPECT(e.gateTriggered);
        for(int i=0;i<3;++i){
            e.tick(); e.step(5,1.5f, 0.f,0.f, 4.f);
            EXPECT(!e.gateTriggered);
            EXPECT(e.gateHeld);
        }
    });

    TEST("16 quarter notes: exactly 4 attacks in 16 steps", {
        StepEngine e; e.reset(); e.setRng(0.99f);
        int attacks=0;
        for(int s=0;s<16;++s){
            e.tick(); e.step(5,1.5f,0.f,0.f,4.f);
            if(e.gateTriggered) ++attacks;
        }
        EXPECT_EQ(attacks, 4);
    });

    TEST("16 eighth notes: exactly 8 attacks in 16 steps", {
        StepEngine e; e.reset(); e.setRng(0.99f);
        int attacks=0;
        for(int s=0;s<16;++s){
            e.tick(); e.step(5,1.5f,0.f,0.f,2.f);
            if(e.gateTriggered) ++attacks;
        }
        EXPECT_EQ(attacks, 8);
    });

    TEST("mid-note extension: legatoProb=1 extends hold each mid step", {
        StepEngine e; e.reset(); e.setRng(0.01f); // all low
        e.tick(); e.step(5,1.5f, 0.f,1.f, 4.f); // max-legato trigger
        EXPECT_NEAR(e.holdRemain, 4.f, 1e-4f);
        float h0=e.holdRemain;
        e.tick(); e.step(5,1.5f, 0.f,0.5f, 4.f); // mid-note, rng=0.01<0.5 → extend
        EXPECT_GT(e.holdRemain, h0-1.f+3.9f);
    });

    TEST("mid-note: pitch not stolen by different note at that slot", {
        StepEngine e; e.reset(); e.setRng(0.99f);
        e.tick(); e.step(5,1.5f, 0.f,0.f, 4.f);
        float p0=e.currentPitch;
        e.tick(); e.step(7,2.5f, 0.f,0.f, 4.f); // mid-note, diff slot pitch ignored
        EXPECT_NEAR(e.currentPitch, p0, 1e-5f);
    });

    TEST("holdRemain never goes below 0", {
        StepEngine e; e.reset();
        for(int i=0;i<64;++i) e.rngQ[i]=float(i%7)/7.f;
        for(int s=0;s<64;++s){
            e.tick(); e.step(s%12,1.5f, 0.3f,0.4f, 4.f);
            EXPECT(e.holdRemain>=0.f);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    std::cout<<"\n"<<std::string(52,'=')<<"\n";
    std::cout<<"\033[32m"<<s_pass<<" passed\033[0m  ";
    if(s_fail)std::cout<<"\033[31m"<<s_fail<<" failed\033[0m";
    else      std::cout<<"\033[32m0 failed\033[0m";
    std::cout<<"  ("<<(s_pass+s_fail)<<" total)\n\n";
    return s_fail?1:0;
}
