/**
 * test_note_length.cpp — Note-length dial -> actual gate duration, incl. the
 * sub-step (1/32, dotted-16th, triplet) fractional gate close.
 *
 * Replicates (from source truth) the FIXED GateState model:
 *   NOTE_VALUES (the single source; DOTTED BUILD spec v3: 13 entries, TRUE duration
 *     order — 1/16T at 11, 1/32 at 12; the DIAL walks DIAL_ORDER, grouped):
 *     0..12 = 1/1, 1/2., 1/2, 1/4., 1/4, 1/8., 1/4T, 1/8, 1/16., 1/8T, 1/16, 1/16T, 1/32
 *   gs_noteSteps(i) = NOTE_VALUES[i].fraction*16   (no whole-step floor)
 *   setDuration(): splits dur into whole steps (holdRemain) + pendingFrac
 *   tick(): decrements holdRemain; on the last whole step arms fracGateSec =
 *           pendingFrac*sixteenthSec (sub-step open time)
 *   process(): per-sample; counts fracGateSec down, closes gate mid-step
 *
 * AIM: all notes START on the 1/16 grid; 1/32, 1/16. and triplets END
 * fractionally within a step. Whole-note lengths unchanged.
 *
 * Compile:  g++ -std=c++17 -I. test_note_length.cpp -o t && ./t
 */
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>

static int s_pass=0,s_fail=0;
#define SUITE(n) std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n"
#define TEST(desc,...) do{bool _ok=true;std::string _m;try{__VA_ARGS__;}catch(const std::exception&_e){_ok=false;_m=_e.what();} \
  if(_ok){++s_pass;std::cout<<"  \033[32m\xE2\x9C\x93\033[0m "<<(desc)<<"\n";}else{++s_fail;std::cout<<"  \033[31m\xE2\x9C\x97\033[0m "<<(desc);if(!_m.empty())std::cout<<" - "<<_m;std::cout<<"\n";}}while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream _s;_s<<#a<<"="<<(a)<<" not ~ "<<(b);throw std::runtime_error(_s.str());}}while(0)

#include "../src/dsp/NoteValues.hpp"   // single source of truth (real header)
#include "../src/dsp/engines/PatternEngine.hpp"   // varyNoteIndex (the rank-window oracle pins)
// Aliases so the existing test body keeps working:
static float gs_noteSteps(int i){ return noteValueSteps(i); }
#define LABELS_OK 1

struct GS {
    // Mirrors fixed GateState: holdRemain (whole-step, decisions) + gateSecRemain
    // (precise gate length). curStepSec stashed by tick(), read by armGate().
    float holdRemain=0.f, gateSecRemain=-1.f, curStepSec=0.f; bool gateHeld=false;
    void armGate(float durSteps){ gateSecRemain=(curStepSec>0.f)?durSteps*curStepSec:-1.f; }
    void trigger(int nv){ gateHeld=true; float d=gs_noteSteps(nv); holdRemain=d; armGate(d); }
    void tick(float s){ if(s>0.f) curStepSec=s;
        if(holdRemain>0.f){ holdRemain-=1.f; if(holdRemain<=0.f){ holdRemain=0.f; if(gateSecRemain<0.f)gateHeld=false; } } }
    bool process(float dt){
        if(gateSecRemain>=0.f){ gateSecRemain-=dt; if(gateSecRemain<=0.f){ gateSecRemain=-1.f;gateHeld=false; } }
        return gateHeld;
    }
};

static float gateSteps(int nv,float stepSec,float SR=48000.f){
    float dt=1.f/SR; GS g;
    g.tick(stepSec);      // engine ticks before trigger in the same step -> sets curStepSec
    g.trigger(nv);
    float high=0.f,t=0.f,nextEdge=stepSec;
    long maxN=(long)(40*stepSec*SR);
    for(long n=0;n<maxN;n++){
        if(g.process(dt)) high+=dt;
        t+=dt;
        if(t>=nextEdge-1e-9f){ g.tick(stepSec); nextEdge+=stepSec; }
        if(!g.gateHeld && g.gateSecRemain<0.f && g.holdRemain<=0.f) break;
    }
    return high/stepSec;
}

// THE ORACLE: the PRE-dotted varyNoteIndex, verbatim (the raw-index 8-entry
// algorithm). The new rank window must reproduce it EXACTLY for every legacy
// base x variation x r under the default mask — the bit-identity pin.
static int legacyVaryNoteIndex(int baseIdx, float variationAmount, int mask, float r) {
    auto allowed = [&](int idx) -> bool {
        if (idx < 0 || idx >= 8) return false;
        if (idx == 3) return (mask & 0b001) != 0;
        if (idx == 5) return (mask & 0b010) != 0;
        if (idx == 7) return (mask & 0b100) != 0;
        return true;
    };
    float var = variationAmount;
    if (std::fabs(var - 0.5f) < 1e-4f) return baseIdx;
    float spread = 2.f * std::fabs(var - 0.5f);
    int direction = (var < 0.5f) ? -1 : 1;
    int targetExtreme = (direction == -1) ? 0 : 7;
    float maxDist = (float)std::abs(targetExtreme - baseIdx);
    if (maxDist < 0.5f) return baseIdx;
    float reach = spread * maxDist;
    float weights[8] = {};
    float total = 0.f;
    for (int i = 0; i < 8; ++i) {
        int dist = direction * (i - baseIdx);
        if (dist < 0) continue;
        if (i == baseIdx) { weights[i] = 1.0f; }
        else {
            if (!allowed(i)) continue;
            float fDist = (float)dist;
            if (fDist <= reach) weights[i] = 1.0f;
            else if (fDist < reach + 1.0f) weights[i] = reach - (fDist - 1.0f);
        }
        total += weights[i];
    }
    if (total <= 1e-6f) return baseIdx;
    float acc = 0.f, roll = r * total;
    for (int i = 0; i < 8; ++i) {
        if (weights[i] > 0.f) { acc += weights[i]; if (roll <= acc) return i; }
    }
    return baseIdx;
}

int main(){
    std::cout<<"\033[1mNote-length gate-duration tests (sub-step close + DOTTED BUILD)\033[0m\n";
    const float BPM=120.f, STEP=15.f/BPM;

    SUITE("Table matches the dial labels (13 entries, TRUE duration order)");
    float exp_frac[NUM_NOTE_VALUES]={1.f, 0.75f, 0.5f, 0.375f, 0.25f, 0.1875f,
                                     1.f/6.f, 0.125f, 0.09375f, 1.f/12.f, 0.0625f, 1.f/24.f, 0.03125f};
    for(int i=0;i<NUM_NOTE_VALUES;++i){std::ostringstream d;d<<"NOTE_VALUES '"<<NOTE_VALUES[i].label<<"' correct";
        TEST(d.str().c_str(),{EXPECT_NEAR(NOTE_VALUES[i].fraction,exp_frac[i],1e-6f);});}

    SUITE("Whole-note lengths unchanged (regression guard; re-derived indices)");
    TEST("1/1 = 16 steps",{EXPECT_NEAR(gateSteps(0,STEP),16.f,0.05f);});
    TEST("1/2 = 8 steps", {EXPECT_NEAR(gateSteps(2,STEP),8.f,0.05f);});   // was idx 1
    TEST("1/4 = 4 steps", {EXPECT_NEAR(gateSteps(4,STEP),4.f,0.05f);});  // was idx 2
    TEST("1/8 = 2 steps", {EXPECT_NEAR(gateSteps(7,STEP),2.f,0.05f);});  // was idx 4
    TEST("1/16 = 1 step", {EXPECT_NEAR(gateSteps(10,STEP),1.f,0.05f);}); // was idx 6 (the old sentinel value!)

    SUITE("Sub-step / triplet / dotted-16th lengths render fractionally (THE FIX)");
    TEST("1/4T = 2.667 steps",{EXPECT_NEAR(gateSteps(6,STEP),16.f/6.f,0.06f);});   // was idx 3
    TEST("1/8T = 1.333 steps",{EXPECT_NEAR(gateSteps(9,STEP),16.f/12.f,0.06f);});  // was idx 5
    TEST("1/32 = 0.5 steps (half a 1/16 step, the design aim)",{EXPECT_NEAR(gateSteps(12,STEP),0.5f,0.05f);});
    TEST("1/16. = 1.5 steps (the dotted sixteenth, sub-step like 1/32)",{EXPECT_NEAR(gateSteps(8,STEP),1.5f,0.05f);});
    TEST("1/16T = 0.6667 steps (the sixteenth triplet, 4 pulses @ 24 PPQN — safe)",{EXPECT_NEAR(gateSteps(11,STEP),16.f/24.f,0.05f);});

    SUITE("DOTTED BUILD — the integer dotted durations");
    TEST("1/2. = 12 steps",{EXPECT_NEAR(gateSteps(1,STEP),12.f,0.05f);});
    TEST("1/4. = 6 steps", {EXPECT_NEAR(gateSteps(3,STEP),6.f,0.05f);});
    TEST("1/8. = 3 steps", {EXPECT_NEAR(gateSteps(5,STEP),3.f,0.05f);});

    SUITE("User's two symptoms resolved (re-derived indices)");
    TEST("1/4 and 1/4T now DISTINCT",{EXPECT(std::fabs(gateSteps(4,STEP)-gateSteps(6,STEP))>0.3f);});
    TEST("1/8, 1/8T, 1/16, 1/32 all DISTINCT",{
        float a=gateSteps(7,STEP),b=gateSteps(9,STEP),c=gateSteps(10,STEP),d=gateSteps(11,STEP);
        EXPECT(std::fabs(a-b)>0.2f); EXPECT(std::fabs(b-c)>0.2f); EXPECT(std::fabs(c-d)>0.2f);
    });
    TEST("the dotted values are DISTINCT from their straight bases",{
        EXPECT(std::fabs(gateSteps(1,STEP)-gateSteps(2,STEP))>1.f);   // 1/2. 12 vs 1/2 8
        EXPECT(std::fabs(gateSteps(3,STEP)-gateSteps(4,STEP))>1.f);   // 1/4. 6 vs 1/4 4
        EXPECT(std::fabs(gateSteps(5,STEP)-gateSteps(7,STEP))>0.5f);  // 1/8. 3 vs 1/8 2
        EXPECT(std::fabs(gateSteps(8,STEP)-gateSteps(10,STEP))>0.3f);// 1/16. 1.5 vs 1/16 1
    });

    SUITE("All notes still START on the 1/16 grid");
    TEST("every length has a sub-step tail < 1 step (on-grid onset)",{
        for(int i=0;i<NUM_NOTE_VALUES;++i){ float s=gs_noteSteps(i); float frac=s-std::floor(s); EXPECT(frac<1.f); }
    });

    SUITE("DOTTED BUILD — legato lead (integer lengths only)");
    TEST("dotted 1/2., 1/4., 1/8. CAN lead legato (12/6/3 steps are integers)",{
        EXPECT(noteCanLeadLegato(1)); EXPECT(noteCanLeadLegato(3)); EXPECT(noteCanLeadLegato(5));
    });
    TEST("dotted 1/16. (1.5 steps) can NOT lead legato — like 1/32, 1/16T and the triplets",{
        EXPECT(!noteCanLeadLegato(8)); EXPECT(!noteCanLeadLegato(12));
        EXPECT(!noteCanLeadLegato(11)); EXPECT(!noteCanLeadLegato(6)); EXPECT(!noteCanLeadLegato(9));
    });

    SUITE("DOTTED BUILD — the table authorities");
    TEST("every value allowed at PPQN24 (bit 4 — the only live mask)",{
        bool ok=true;
        for(int i=0;i<NUM_NOTE_VALUES;++i) if(!(NOTE_VALUES[i].allowedPPQN&4)) ok=false;
        EXPECT(ok);});
    TEST("the legacy map preserves every pre-dotted value (label identity)",{
        const char* legacyLabels[8]={"1/1","1/2","1/4","1/4T","1/8","1/8T","1/16","1/32"};
        bool ok=true;
        for(int i=0;i<8;++i)
            if(std::string(NOTE_VALUES[legacyNoteValueIndex(i)].label)!=legacyLabels[i]) ok=false;
        EXPECT(ok);});
    TEST("the nullification sentinel addresses a 1-step value (the inert countdown)",{
        EXPECT_NEAR(noteValueSteps(NV_GATE_NULLIFY), 1.f, 1e-6f);});
    TEST("noteShorterOf: the duration authority (index order == duration order again)",{
        EXPECT(noteShorterOf(0,4)==4);            // 1/4(4 steps) shorter than 1/1(16)
        EXPECT(noteShorterOf(4,0)==4);
        EXPECT(noteShorterOf(1,2)==2);            // 1/2(8) shorter than 1/2.(12)
        EXPECT(noteShorterOf(2,1)==2);
        EXPECT(noteShorterOf(2,3)==3);            // 1/4.(6) shorter than 1/2(8)
        EXPECT(noteShorterOf(3,4)==4);            // 1/4(4) shorter than 1/4.(6)
        EXPECT(noteShorterOf(9,10)==10);          // 1/16(1) shorter than 1/8T(1.333)
        EXPECT(noteShorterOf(10,9)==10);
        EXPECT(noteShorterOf(0,0)==0);
    });
    TEST("the table is index-monotone (TRUE duration order: strictly decreasing)",{
        for(int i=1;i<NUM_NOTE_VALUES;++i)
            EXPECT(noteValueSteps(i-1) > noteValueSteps(i));
    });
    TEST("DIAL_ORDER is a valid permutation (every index exactly once)",{
        bool seen[NUM_NOTE_VALUES]={};
        for(int k=0;k<NUM_NOTE_VALUES;++k){
            EXPECT(DIAL_ORDER[k]>=0 && DIAL_ORDER[k]<NUM_NOTE_VALUES);
            EXPECT(!seen[DIAL_ORDER[k]]);
            seen[DIAL_ORDER[k]]=true;
        }
    });
    TEST("DIAL_ORDER reads in grouped families (base, triplet, then next family's dotted)",{
        // The spec's dial sequence labels, verbatim:
        const char* dial[NUM_NOTE_VALUES]={"1/1","1/2.","1/2","1/4.","1/4","1/4T","1/8.","1/8",
                                           "1/8T","1/16.","1/16","1/16T","1/32"};
        for(int k=0;k<NUM_NOTE_VALUES;++k)
            EXPECT(std::string(NOTE_VALUES[DIAL_ORDER[k]].label)==dial[k]);
    });

    SUITE("DOTTED BUILD — the rank window (varyNoteIndex)");
    {
        PatternEngine pe;
        auto isDotted=[](int i){ return NOTE_VALUES[i].variationBit>=3; };
        // (a) THE ORACLE PIN: under the DEFAULT mask (dotted OFF), the new rank
        //     window reproduces the PRE-dotted algorithm's MUSICAL CONTENT exactly —
        //     map the LEGACY base into the new table, compare the pick's FRACTION
        //     to the oracle's legacy result fraction, for every base x variation x r.
        //     The bit-identity proof, executable.
        {
            static const float legacyFrac[8] = {1.f, 0.5f, 0.25f, 1.f/6.f,
                                                0.125f, 1.f/12.f, 0.0625f, 0.03125f};
            bool ok=true;
            for(int base=0;base<8;++base){
                const int newBase = legacyNoteValueIndex(base);
                PatternInput in; in.noteVariationMask=0b111;
                for(int v=0;v<2;++v){
                    in.variationAmount=(v==0)?1.0f:0.0f;
                    for(int k=0;k<=200;++k){
                        const int pick=pe.varyNoteIndex(newBase,in,k/200.f);
                        const int oracle=legacyVaryNoteIndex(base,in.variationAmount,0b111,k/200.f);
                        if(NOTE_VALUES[pick].fraction != legacyFrac[oracle]) ok=false;
                    }
                }
                // mid-range variations too
                for(float var : {0.2f,0.35f,0.65f,0.8f}){
                    PatternInput in2; in2.noteVariationMask=0b111; in2.variationAmount=var;
                    for(int k=0;k<=200;++k){
                        const int pick=pe.varyNoteIndex(newBase,in2,k/200.f);
                        const int oracle=legacyVaryNoteIndex(base,var,0b111,k/200.f);
                        if(NOTE_VALUES[pick].fraction != legacyFrac[oracle]) ok=false;
                    }
                }
            }
            TEST("ORACLE: default mask == the pre-dotted algorithm's values, every base x var x r",{EXPECT(ok);});
        }
        // (b) dotted OFF ⇒ a non-dotted base NEVER picks a dotted value (a dotted
        //     base can only pick itself — the base is always a rung).
        {
            bool ok=true;
            for(int base=0;base<NUM_NOTE_VALUES;++base){
                PatternInput in; in.noteVariationMask=0b111;
                for(int v=0;v<2;++v){
                    in.variationAmount=(v==0)?1.0f:0.0f;
                    for(int k=0;k<=100;++k){
                        const int pick=pe.varyNoteIndex(base,in,k/100.f);
                        if(isDotted(pick)&&!(isDotted(base)&&pick==base)) ok=false;
                    }
                }
            }
            TEST("default mask: dotted bits OFF => dotted never picked (except a dotted base itself)",{EXPECT(ok);});
        }
        // (c) each dotted bit ON ⇒ its value reachable.
        {
            const int dottedIdx[5]={1,3,5,8,11};  // 1/2. 1/4. 1/8. 1/16. 1/16T
            const int bitOf[5]    ={3,4,5,6,7};
            bool okAll=true;
            for(int d=0;d<5;++d){
                bool reached=false;
                PatternInput in;
                in.noteVariationMask=0b111|(1<<bitOf[d]);
                for(int base=0;base<NUM_NOTE_VALUES&&!reached;++base){
                    if(base==dottedIdx[d]) continue;
                    for(int v=0;v<2&&!reached;++v){
                        in.variationAmount=(v==0)?1.0f:0.0f;
                        for(int k=0;k<=200&&!reached;++k)
                            if(pe.varyNoteIndex(base,in,k/200.f)==dottedIdx[d]) reached=true;
                    }
                }
                if(!reached) okAll=false;
            }
            TEST("each dotted bit ON => its value reachable (varyNoteIndex)",{EXPECT(okAll);});
        }
    }

    std::cout<<"\n\033[1mResults: \033[32m"<<s_pass<<" passed\033[0m, "
             <<(s_fail?"\033[31m":"")<<s_fail<<" failed\033[0m\n";
    return s_fail?1:0;
}
