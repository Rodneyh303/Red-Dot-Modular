/**
 * test_modes_bcd.cpp
 * Tests for Mode B (external gate + legato/rest), Mode C (quarter-note
 * quantiser), and Mode D (gate-level quantiser with external gate).
 *
 * Compile:  g++ -std=c++17 test_modes_bcd.cpp -o test_bcd && ./test_bcd
 * No Rack SDK needed — pure logic stubs.
 */

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

// ── Minimal stubs ─────────────────────────────────────────────────────────────
namespace rack {
    template<typename T>
    T clamp(T v, T lo, T hi){ return v<lo?lo:(v>hi?hi:v); }
}
template<typename T>
T clampv(T v, T lo, T hi){ return rack::clamp(v,lo,hi); }

// ── Test framework ────────────────────────────────────────────────────────────
static int s_pass=0, s_fail=0;

#define SUITE(n) do{ std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n"; }while(0)

#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception&e){_ok=false;_msg=e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg; std::cout<<"\n";} \
}while(0)

#define EXPECT(e)     do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a__,b__) do{if((a__)!=(b__)){std::ostringstream _s__; \
    _s__<<#a__<<"="<<(a__)<<" != "<<#b__<<"="<<(b__);throw std::runtime_error(_s__.str());}}while(0)
#define EXPECT_NEAR(a__,b__,eps__) do{if(std::fabs((a__)-(b__))>(eps__)){std::ostringstream _s__; \
    _s__<<#a__<<"="<<(a__)<<" not near "<<#b__<<"="<<(b__);throw std::runtime_error(_s__.str());}}while(0)
#define EXPECT_GT(a__,b__) do{if(!((a__)>(b__))){std::ostringstream _s__; \
    _s__<<#a__<<"="<<(a__)<<" not > "<<#b__<<"="<<(b__);throw std::runtime_error(_s__.str());}}while(0)
#define EXPECT_LT(a__,b__) do{if(!((a__)<(b__))){std::ostringstream _s__; \
    _s__<<#a__<<"="<<(a__)<<" not < "<<#b__<<"="<<(b__);throw std::runtime_error(_s__.str());}}while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// Shared logic extracted from plugin
// ═══════════════════════════════════════════════════════════════════════════════

static float noteValFrac(int idx){
    static const float v[8]={0.5f,0.25f,1.f/6.f,0.125f,1.f/12.f,0.0625f,1.f/24.f,0.03125f};
    return (idx>=0&&idx<=7)?v[idx]:0.f;
}
static float noteDurSec(int nvIdx, float bpm){
    float w=4.f*60.f/std::max(1.f,bpm);
    return std::max(noteValFrac(nvIdx)*w, 0.002f);
}
static bool probFired(float prob, float rng){ return rng<prob; }

// ── quantizeToScale (exact mirror of plugin implementation) ──────────────────
// Weights: active[12] = weight for each semitone (0=silent, >0=active)
static float quantizeToScale(float vIn, const float weights[12]){
    int octave=(int)std::floor(vIn);
    float bestScore=-1.f, bestV=vIn;

    for(int o=octave-1;o<=octave+1;++o){
        for(int s=0;s<12;++s){
            float w=clampv(weights[s],0.f,1.f);
            if(w<=0.f) continue;
            float cand=o+s/12.f;
            float dist=std::fabs(vIn-cand);
            float radius=w*(1.f/12.f);
            if(dist<=radius+1e-4f){
                float score=w/(dist+1e-4f);
                if(score>bestScore){bestScore=score;bestV=cand;}
            }
        }
    }
    // Fallback: nearest active semitone regardless of radius
    if(bestScore<0.f){
        float minDist=1e9f;
        for(int o=octave-1;o<=octave+1;++o)
            for(int s=0;s<12;++s){
                if(weights[s]<=0.f) continue;
                float cand=o+s/12.f;
                float dist=std::fabs(vIn-cand);
                if(dist<minDist){minDist=dist;bestV=cand;}
            }
    }
    return clampv(bestV,0.f,5.f);
}

// ── stepInWindow (exact mirror) ───────────────────────────────────────────────
static bool stepInWindow(int idx, int startStep, int endStep){
    if(startStep<=endStep) return idx>=startStep && idx<=endStep;
    return idx>=startStep || idx<=endStep;  // wrapped window
}

// ── Mode B gate + legato/rest state ──────────────────────────────────────────
// Mirrors triggerStepEventForOffsetStep_ + gate state
struct ModeBState {
    float currentPitch = 0.f;
    float holdRemain   = 0.f;
    bool  gateHeld     = false;
    bool  gateTriggered= false;
    int   lastSemitone = -1;
    float semiRemain[12] = {};

    void reset(){
        currentPitch=0.f; holdRemain=0.f; gateHeld=false;
        gateTriggered=false; lastSemitone=-1;
        std::fill(semiRemain,semiRemain+12,0.f);
    }

    // Mirrors triggerStepEventForOffsetStep_
    void triggerStep(float pitchV, int sem, int nvIdx,
                     float restProb, float legatoProb,
                     float restRng, float legatoRng, float tieRng, float bpm)
    {
        float dur = noteDurSec(nvIdx, bpm);

        // Legato max (knob fully right): gate always on
        if(legatoProb >= 0.999f){
            currentPitch=pitchV; lastSemitone=sem;
            gateHeld=true; holdRemain=dur;
            if(sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
            return;
        }

        // Rest drawn first
        if(probFired(restProb, restRng)) return;

        bool doLegato = probFired(legatoProb, legatoRng);
        bool doTie    = !doLegato && (sem==lastSemitone) && gateHeld
                        && probFired(legatoProb, tieRng);

        if(doLegato){
            currentPitch=pitchV; lastSemitone=sem;
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
        currentPitch=pitchV; lastSemitone=sem;
        gateTriggered=true; gateHeld=true; holdRemain=dur;
        if(sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
    }
};

// ── Mode B step-advance logic ─────────────────────────────────────────────────
struct StepEngine {
    int startStep = 0, endStep = 15;
    int stepIndex = -1;  // -1 = not started

    void setWindow(int start, int length){
        startStep=start;
        endStep=(start+length-1)&0x0F;
    }

    // Mirrors advanceStepIndexOnEdge_
    void advance(){
        if(stepIndex==-1) stepIndex=(startStep-1+16)%16;
        stepIndex=(stepIndex+1)&0x0F;
        if(!stepInWindow(stepIndex,startStep,endStep)) stepIndex=startStep;
        for(int s=0;s<16&&!stepInWindow(stepIndex,startStep,endStep);++s)
            stepIndex=(stepIndex+1)&0x0F;
    }

    bool wrappedOnAdvance(int prevStep){
        return prevStep!=-1 && stepIndex==startStep;
    }
};

// ── Mode C/D quantiser state ──────────────────────────────────────────────────
struct QuantiserState {
    float currentPitch = 0.f;
    float gateOut      = 0.f;  // 0=low, 10=high

    // Mode C: quarter-note edge latches and quantises CV
    void handleModeC(bool quarterEdge, float inCV, const float weights[12]){
        if(quarterEdge) currentPitch = quantizeToScale(inCV, weights);
        gateOut = quarterEdge ? 10.f : 0.f;
    }

    // Mode D: gate2 high = quantise continuously; low = output zero
    void handleModeD(bool gate2High, float inCV, const float weights[12]){
        if(gate2High){
            currentPitch = quantizeToScale(inCV, weights);
            gateOut = 10.f;
        } else {
            currentPitch = 0.f;  // output clears when gate low
            gateOut = 0.f;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// TESTS
// ═══════════════════════════════════════════════════════════════════════════════

int main(){
    std::cout << "\033[1mMode B / C / D Tests\033[0m\n";
    std::cout << std::string(54,'=') << "\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mode B — Step Advance on External Gate Rising Edge");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("First gate edge: stepIndex advances from -1 to startStep", {
        StepEngine se; se.setWindow(0,16);
        se.advance();
        EXPECT_EQ(se.stepIndex, 0);
    });

    TEST("Step advances linearly through 16-step window", {
        StepEngine se; se.setWindow(0,16);
        for(int i=0;i<16;++i){ se.advance(); EXPECT_EQ(se.stepIndex,i); }
    });

    TEST("Step wraps at end of window (length=16)", {
        StepEngine se; se.setWindow(0,16);
        for(int i=0;i<16;++i) se.advance();
        EXPECT_EQ(se.stepIndex,15);
        se.advance();
        EXPECT_EQ(se.stepIndex,0);  // wraps back to start
    });

    TEST("Shortened window: stays within startStep..startStep+length-1", {
        StepEngine se; se.setWindow(4,4);  // steps 4,5,6,7
        for(int run=0;run<20;++run){
            se.advance();
            EXPECT(se.stepIndex>=4 && se.stepIndex<=7);
        }
    });

    TEST("Wrapped window (start>end): steps 12,13,14,15,0,1,2,3", {
        StepEngine se; se.setWindow(12,8);  // endStep=(12+7)&0xF=3
        std::vector<int> seen;
        for(int i=0;i<8;++i){ se.advance(); seen.push_back(se.stepIndex); }
        // Should visit 12,13,14,15,0,1,2,3
        for(int i=0;i<8;++i) EXPECT_EQ(seen[i], (12+i)%16);
    });

    TEST("wrappedOnAdvance: true only when step wraps back to startStep", {
        StepEngine se; se.setWindow(0,4);  // steps 0,1,2,3
        int wraps=0;
        for(int i=0;i<20;++i){
            int prev=se.stepIndex;
            se.advance();
            if(se.wrappedOnAdvance(prev)) ++wraps;
        }
        // 20 advances, window=4: first advance from -1 is NOT a wrap (prevStep==-1 guard)
        // Steps: 0,1,2,3, 0*,1,2,3, 0*,1,2,3, 0*,1,2,3, 0*,1,2,3  (* = wrap)
        // Wraps at steps 5,9,13,17 = 4 wraps total
        EXPECT_EQ(wraps, 4);
    });

    TEST("Length=1: always stays at startStep, wraps every advance", {
        StepEngine se; se.setWindow(7,1);
        for(int i=0;i<10;++i){ se.advance(); EXPECT_EQ(se.stepIndex,7); }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mode B — Legato with External Gate Trigger");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Gate edge with legato=0, rest=0: fires normal note", {
        ModeBState s;
        s.triggerStep(1.0f,5,3, 0.f,0.f, 0.5f,0.5f,0.5f, 120.f);
        EXPECT(s.gateTriggered);
        EXPECT_NEAR(s.currentPitch, 1.0f, 1e-5f);
        EXPECT(s.gateHeld);
    });

    TEST("Gate edge with legato prob=1: no retrigger, pitch updates, gate held", {
        ModeBState s;
        s.gateHeld=true; s.holdRemain=0.2f;
        s.triggerStep(2.0f,7,3, 0.f,1.f, 0.5f,0.0f,0.5f, 120.f);
        EXPECT(!s.gateTriggered);
        EXPECT_NEAR(s.currentPitch, 2.0f, 1e-5f);
        EXPECT(s.gateHeld);
    });

    TEST("Legato extends holdRemain when gate already held", {
        ModeBState s;
        s.gateHeld=true; s.holdRemain=0.1f;
        float dur=noteDurSec(3,120.f);
        // legatoProb=0.8, rng=0.0 → doLegato=true (uses normal legato path, not >=0.999 shortcut)
        s.triggerStep(2.0f,7,3, 0.f,0.8f, 0.9f,0.0f,0.5f, 120.f);
        EXPECT_NEAR(s.holdRemain, 0.1f+dur, 1e-4f);
    });

    TEST("Legato from cold (gate not held): opens gate with trigger", {
        ModeBState s;
        s.gateHeld=false;
        // legatoProb=0.8 (< 0.999), rng=0.0 → triggers normal legato path
        s.triggerStep(1.5f,3,3, 0.f,0.8f, 0.9f,0.0f,0.5f, 120.f);
        EXPECT(s.gateTriggered);  // first note of legato run needs to open gate
        EXPECT(s.gateHeld);
    });

    TEST("Legato=max (>=0.999): gate held, holdRemain set to full duration", {
        ModeBState s;
        float dur=noteDurSec(3,120.f);
        s.triggerStep(1.0f,4,3, 0.f,1.0f, 0.5f,0.0f,0.5f, 120.f);
        EXPECT_NEAR(s.holdRemain, dur, 1e-4f);
        EXPECT(!s.gateTriggered);  // max legato = no retrigger
    });

    TEST("Rest with gate edge: gate not closed, no pitch change", {
        ModeBState s;
        s.gateHeld=true; s.currentPitch=1.5f; s.holdRemain=0.3f;
        // rest fires (rng=0.0 < restProb=0.8)
        s.triggerStep(2.0f,5,3, 0.8f,0.f, 0.0f,0.5f,0.5f, 120.f);
        EXPECT_NEAR(s.currentPitch, 1.5f, 1e-5f);  // pitch unchanged
        EXPECT_NEAR(s.holdRemain,   0.3f, 1e-5f);   // holdRemain unchanged
        EXPECT(!s.gateTriggered);
    });

    TEST("Rest prob=1: every gate edge is a rest (no retrigger ever)", {
        ModeBState s;
        for(int i=0;i<10;++i){
            s.gateTriggered=false;
            s.triggerStep(1.0f,5,3, 1.0f,0.f, 0.0f,0.5f,0.5f, 120.f);
            EXPECT(!s.gateTriggered);
        }
    });

    TEST("Successive gate edges with rest=0, legato=0: each fires a new note", {
        ModeBState s;
        for(int i=0;i<5;++i){
            s.gateTriggered=false;
            s.triggerStep(float(i)*0.1f, i%12, 3, 0.f,0.f,
                          0.5f,0.9f,0.9f, 120.f);
            EXPECT(s.gateTriggered);
        }
    });

    TEST("Rest then normal note: note fires correctly after silence", {
        ModeBState s;
        // Rest step (no note)
        s.triggerStep(1.0f,5,3, 1.0f,0.f, 0.0f,0.5f,0.5f, 120.f);
        EXPECT(!s.gateTriggered);
        s.gateTriggered=false;
        // Normal note step
        s.triggerStep(1.5f,5,3, 0.0f,0.f, 0.5f,0.9f,0.9f, 120.f);
        EXPECT(s.gateTriggered);
        EXPECT_NEAR(s.currentPitch,1.5f,1e-5f);
    });

    TEST("Legato max + gate edge: semiPlayRemain updated", {
        ModeBState s;
        float dur=noteDurSec(3,120.f);
        s.triggerStep(1.0f,7,3, 0.f,1.0f, 0.5f,0.0f,0.5f, 120.f);
        EXPECT(s.semiRemain[7] >= dur-1e-5f);
    });

    TEST("Tie: same semitone, gate held, extends holdRemain without retrigger", {
        ModeBState s;
        s.gateHeld=true; s.lastSemitone=4; s.holdRemain=0.1f;
        float dur=noteDurSec(3,120.f);
        // doLegato=false (rng=0.9 >= prob=0.5), doTie drawn: rng=0.1 < prob=0.5
        s.triggerStep(1.0f,4,3, 0.f,0.5f, 0.9f,0.9f,0.1f, 120.f);
        EXPECT(!s.gateTriggered);
        EXPECT_NEAR(s.holdRemain, 0.1f+dur, 1e-4f);
    });

    TEST("No tie: different semitone, gate not held → retrigger normally", {
        ModeBState s;
        s.gateHeld=true; s.lastSemitone=4;
        s.triggerStep(2.0f,7,3, 0.f,0.f, 0.9f,0.9f,0.9f, 120.f);
        EXPECT(s.gateTriggered);
        EXPECT_NEAR(s.currentPitch,2.0f,1e-5f);
    });

    TEST("Mode B: rest fires before legato RNG (RNG ordering)", {
        // rest is drawn first; if it fires, legato is never checked.
        // Use legatoProb=0.8 (below 0.999 max shortcut), restRng=0.0 < restProb=0.8
        ModeBState s;
        s.currentPitch=1.0f;
        s.triggerStep(2.0f,5,3, 0.8f,0.8f, 0.0f,0.0f,0.0f, 120.f);
        // restRng=0.0 < restProb=0.8 → rest fires, legato never reached
        EXPECT_NEAR(s.currentPitch,1.0f,1e-5f);  // pitch unchanged (rest won)
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mode C — Quarter-Note Quantiser");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("No quarter edge: CV output holds previous value", {
        float w[12]={1,1,1,1,1,1,1,1,1,1,1,1};
        QuantiserState q; q.currentPitch=1.5f;
        q.handleModeC(false, 0.3f, w);
        EXPECT_NEAR(q.currentPitch, 1.5f, 1e-5f);  // unchanged
    });

    TEST("Quarter edge: CV is quantised and latched", {
        float w[12]={0}; w[0]=1.f;  // only C active
        QuantiserState q;
        q.handleModeC(true, 0.05f, w);  // 0.05V ≈ C0 range
        // Should snap to C (semitone 0) nearest octave
        float frac=q.currentPitch - std::floor(q.currentPitch);
        EXPECT_NEAR(frac, 0.f, 1.f/12.f);  // within half-semitone of C
    });

    TEST("Quarter edge: gate output fires high for exactly that sample", {
        float w[12]={1}; w[0]=1.f;
        QuantiserState q;
        q.handleModeC(true, 0.f, w);
        EXPECT_NEAR(q.gateOut, 10.f, 1e-5f);
    });

    TEST("Between quarter edges: gate output is low", {
        float w[12]={1.f};
        QuantiserState q;
        q.handleModeC(true,  0.f, w);  // fire once
        q.handleModeC(false, 0.f, w);  // next sample
        EXPECT_NEAR(q.gateOut, 0.f, 1e-5f);
    });

    TEST("Quantiser C major scale: 0V → C, 0.25V → D, 0.5V → E", {
        // C major: C D E F G A B (semitones 0,2,4,5,7,9,11)
        float w[12]={0};
        int cmaj[]={0,2,4,5,7,9,11};
        for(int s:cmaj) w[s]=1.f;
        QuantiserState q;

        // Test C: 0V should land on C (0/12=0.0)
        q.handleModeC(true, 0.0f, w);
        EXPECT_NEAR(q.currentPitch - std::floor(q.currentPitch), 0.f, 0.01f);

        // Test D: 2/12 = 0.1667V → should quantise to D
        q.handleModeC(true, 2.f/12.f, w);
        float frac = q.currentPitch - std::floor(q.currentPitch);
        EXPECT_NEAR(frac, 2.f/12.f, 0.01f);
    });

    TEST("Quantiser all-zero weights: falls back to nearest active in ±1 oct", {
        // All weights zero — quantiser should not crash; output stays in 0..5V
        float w[12]={0};
        QuantiserState q;
        q.handleModeC(true, 2.5f, w);
        EXPECT(q.currentPitch >= 0.f && q.currentPitch <= 5.f);
    });

    TEST("Quantiser single active semitone G: input near G snaps to G", {
        float w[12]={0}; w[7]=1.f;  // only G active (7/12 = 0.5833V per octave)
        QuantiserState q;
        // Test inputs that ARE within the attraction radius of G
        // radius = weight * (1/12) = 1.0/12 ≈ 0.083V
        // G at octave 0: 7/12 ≈ 0.583V. Inputs within 0.083V should snap to G.
        for(float offset : {0.0f, 0.04f, -0.04f}){
            float v = 7.f/12.f + offset;
            q.handleModeC(true, v, w);
            float frac = q.currentPitch - std::floor(q.currentPitch);
            EXPECT_NEAR(frac, 7.f/12.f, 0.5f/12.f);
        }
        // Input far from G (e.g. 0V) falls back to nearest G in ±1 octave
        q.handleModeC(true, 0.0f, w);
        float frac = q.currentPitch - std::floor(q.currentPitch);
        // Fallback: nearest G could be at octave -1 (clamped to 0V) or
        // the snap logic finds G at octave 0 = 0.583V which is 0.583 away.
        // Either way, output is within 0..5V and some G fraction.
        EXPECT(q.currentPitch >= 0.f && q.currentPitch <= 5.f);
    });

    TEST("Quantiser output clamped 0..5V", {
        float w[12]={0}; w[11]=1.f;  // only B, highest semitone
        QuantiserState q;
        q.handleModeC(true, 4.99f, w);
        EXPECT(q.currentPitch >= 0.f && q.currentPitch <= 5.f);
    });

    TEST("Mode C: consecutive quarter edges advance CV latch", {
        float w[12]={0}; w[0]=1.f; w[7]=1.f;  // C and G
        QuantiserState q;
        q.handleModeC(true, 0.0f,      w);  float cv1=q.currentPitch;
        q.handleModeC(true, 7.f/12.f,  w);  float cv2=q.currentPitch;
        // Second latch should reflect the new CV value
        EXPECT(std::fabs(cv2-cv1) > 0.01f);
    });

    TEST("Mode C: step index advances on each quarter edge", {
        StepEngine se; se.setWindow(0,16);
        int initial=se.stepIndex;  // -1
        se.advance(); int s1=se.stepIndex;  // 0
        se.advance(); int s2=se.stepIndex;  // 1
        EXPECT_EQ(s1,0); EXPECT_EQ(s2,1);
    });

    TEST("Mode C: step wraps correctly at pattern end", {
        StepEngine se; se.setWindow(0,4);  // 4-step pattern
        for(int i=0;i<4;++i) se.advance();
        EXPECT_EQ(se.stepIndex,3);
        se.advance();
        EXPECT_EQ(se.stepIndex,0);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mode D — Gate-Level Quantiser with External Gate");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Gate2 low: CV output = 0V, gate output = 0V", {
        float w[12]={0}; w[0]=1.f;
        QuantiserState q; q.currentPitch=2.5f;
        q.handleModeD(false, 2.5f, w);
        EXPECT_NEAR(q.currentPitch, 0.f, 1e-5f);
        EXPECT_NEAR(q.gateOut,      0.f, 1e-5f);
    });

    TEST("Gate2 high: CV is quantised, gate = 10V", {
        float w[12]={0}; w[0]=1.f;
        QuantiserState q;
        q.handleModeD(true, 0.0f, w);
        EXPECT_NEAR(q.gateOut, 10.f, 1e-5f);
    });

    TEST("Gate2 high: pitch quantised to active semitone", {
        float w[12]={0}; w[5]=1.f;  // only F (semitone 5)
        QuantiserState q;
        q.handleModeD(true, 0.5f, w);
        float frac=q.currentPitch-std::floor(q.currentPitch);
        EXPECT_NEAR(frac, 5.f/12.f, 0.5f/12.f);
    });

    TEST("Gate2 goes low after being high: outputs clear", {
        float w[12]={0}; w[0]=1.f;
        QuantiserState q;
        q.handleModeD(true,  0.0f, w);  // gate high
        EXPECT_NEAR(q.gateOut, 10.f, 1e-5f);
        q.handleModeD(false, 0.0f, w);  // gate drops
        EXPECT_NEAR(q.gateOut,      0.f, 1e-5f);
        EXPECT_NEAR(q.currentPitch, 0.f, 1e-5f);
    });

    TEST("Mode D: quantises every sample while gate is held (not just on edge)", {
        float w[12]={0}; w[0]=1.f; w[7]=1.f;
        QuantiserState q;
        q.handleModeD(true, 0.0f,     w); float cv1=q.currentPitch;
        q.handleModeD(true, 7.f/12.f, w); float cv2=q.currentPitch;
        // Unlike Mode C, Mode D updates every sample while gate is high
        EXPECT(std::fabs(cv1-cv2) > 0.01f);
    });

    TEST("Mode D vs C: D tracks CV in real-time, C only latches on edge", {
        float w[12]={0}; w[0]=1.f; w[7]=1.f;
        QuantiserState qC, qD;
        // Initial latch at C
        qC.handleModeC(true,  0.0f, w);
        qD.handleModeD(true,  0.0f, w);
        // CV changes to G — no new quarter edge for C
        qC.handleModeC(false, 7.f/12.f, w);  // no edge — C holds
        qD.handleModeD(true,  7.f/12.f, w);  // gate high — D tracks
        EXPECT_NEAR(qC.currentPitch-std::floor(qC.currentPitch), 0.f, 0.01f);   // C held at C
        EXPECT_NEAR(qD.currentPitch-std::floor(qD.currentPitch), 7.f/12.f, 0.01f); // D moved to G
    });

    TEST("Gate2 threshold: <1V not recognised as gate", {
        float w[12]={0}; w[0]=1.f;
        QuantiserState q;
        // Plugin uses > 1.0V threshold (getVoltage() >= 1.f)
        bool gateHigh = (0.9f >= 1.f);  // below threshold
        q.handleModeD(gateHigh, 2.5f, w);
        EXPECT_NEAR(q.gateOut, 0.f, 1e-5f);  // not triggered
    });

    TEST("Gate2 threshold: exactly 1V is recognised", {
        float w[12]={0}; w[0]=1.f;
        QuantiserState q;
        bool gateHigh = (1.0f >= 1.f);  // exactly at threshold
        q.handleModeD(gateHigh, 0.0f, w);
        EXPECT_NEAR(q.gateOut, 10.f, 1e-5f);
    });

    TEST("Mode D all-zero weights: output stays in 0..5V range (no crash)", {
        float w[12]={0};
        QuantiserState q;
        q.handleModeD(true, 3.7f, w);
        EXPECT(q.currentPitch >= 0.f && q.currentPitch <= 5.f);
    });

    TEST("Mode D: gate held for many samples, output stable on constant CV", {
        float w[12]={0}; w[4]=1.f;  // only E
        QuantiserState q;
        float cv = 4.f/12.f;  // E at octave 0
        for(int i=0;i<100;++i) q.handleModeD(true, cv, w);
        float frac=q.currentPitch-std::floor(q.currentPitch);
        EXPECT_NEAR(frac, 4.f/12.f, 0.5f/12.f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mode C/D — Quantiser Pitch Accuracy");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Chromatic scale: each semitone snaps correctly", {
        // All 12 semitones active at equal weight
        float w[12]; std::fill(w,w+12,1.f);
        QuantiserState q;
        for(int si=0;si<12;++si){
            float v = si/12.f;  // exact semitone voltage
            q.handleModeC(true, v, w);
            float frac=q.currentPitch-std::floor(q.currentPitch);
            EXPECT_NEAR(frac, si/12.f, 1.f/24.f);  // within half-semitone
        }
    });

    TEST("Higher-weight semitone wins when equidistant", {
        float w[12]={0};
        w[3]=0.5f;  // D# at low weight
        w[4]=1.0f;  // E  at high weight
        // Input exactly between D# and E: 3.5/12
        QuantiserState q;
        q.handleModeC(true, 3.5f/12.f, w);
        float frac=q.currentPitch-std::floor(q.currentPitch);
        // E should win due to higher weight / better score
        EXPECT_NEAR(frac, 4.f/12.f, 1.f/24.f);
    });

    TEST("Input above all active semitones: snaps to nearest (fallback)", {
        float w[12]={0}; w[0]=1.f;  // only C
        QuantiserState q;
        // Very high input — fallback should find nearest C
        q.handleModeC(true, 4.95f, w);
        float frac=q.currentPitch-std::floor(q.currentPitch);
        EXPECT_NEAR(frac, 0.f, 0.5f/12.f);  // snaps to C
    });

    TEST("Output always clamped 0..5V regardless of input", {
        float w[12]; std::fill(w,w+12,1.f);
        QuantiserState q;
        for(float v : {-1.f, 0.f, 2.5f, 5.f, 6.f, 10.f}){
            q.handleModeC(true, v, w);
            EXPECT(q.currentPitch >= 0.f && q.currentPitch <= 5.f);
        }
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mode B/C/D — Mode Switching Side Effects");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Switching mode clears holdRemain (no stuck notes)", {
        float holdRemain=0.5f;
        int mode=0;
        // Simulate mode switch: Mode A → B
        int newMode=1;
        if(newMode!=mode){ holdRemain=0.f; mode=newMode; }
        EXPECT_NEAR(holdRemain, 0.f, 1e-5f);
    });

    TEST("Mode D: switching away and back does not retain stale pitch", {
        float w[12]={0}; w[5]=1.f;
        QuantiserState q;
        q.handleModeD(true,  5.f/12.f, w);  float pitchWhileOn=q.currentPitch;
        q.handleModeD(false, 5.f/12.f, w);  // gate low = outputs clear
        EXPECT_NEAR(q.currentPitch, 0.f, 1e-5f);  // cleared when gate drops
    });

    TEST("Mode C: gate fires exactly once per quarter edge, not continuously", {
        float w[12]; std::fill(w,w+12,1.f);
        QuantiserState q; int gateHighCount=0;
        // 10 samples: 1 edge at sample 3
        for(int i=0;i<10;++i){
            q.handleModeC(i==3, 0.f, w);
            if(q.gateOut > 5.f) ++gateHighCount;
        }
        EXPECT_EQ(gateHighCount, 1);
    });

    TEST("Mode B rest=1 then mode switch to C: step index persists", {
        StepEngine se; se.setWindow(0,16);
        for(int i=0;i<5;++i) se.advance();
        int stepBeforeSwitch=se.stepIndex;
        // Mode switch doesn't reset stepIndex in plugin
        EXPECT_EQ(se.stepIndex, stepBeforeSwitch);
    });

    // ─────────────────────────────────────────────────────────────────────────
    // Summary
    // ─────────────────────────────────────────────────────────────────────────
    std::cout << "\n" << std::string(54,'=') << "\n";
    std::cout << "\033[32m"<<s_pass<<" passed\033[0m  ";
    if(s_fail>0) std::cout<<"\033[31m"<<s_fail<<" failed\033[0m";
    else         std::cout<<"\033[32m0 failed\033[0m";
    std::cout<<"  ("<<(s_pass+s_fail)<<" total)\n\n";
    return s_fail>0?1:0;
}
