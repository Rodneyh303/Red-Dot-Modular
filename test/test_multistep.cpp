#include "test_stubs.hpp"
/**
 * test_multistep.cpp
 * Tests for multi-step note/rest spanning (the MeloDicer "secret sauce").
 *
 * Compile: g++ -std=c++17 -I. test_multistep.cpp -o test_ms && ./test_ms
 */
#include "GateState.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

static int s_pass=0, s_fail=0;
#define SUITE(n) std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n"
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a,b) do{if((a)!=(b)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" != "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" not near "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)
#define EXPECT_GT(a,b) do{if(!((a)>(b))){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" not > "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)

// ── Mirror of triggerStepEventForOffsetStep_ with full multi-step logic ──────
// Self-contained so it can be tested without Rack.
struct StepEngine {
    float holdRemain   = 0.f;
    bool  gateHeld     = false;
    float currentPitch = 0.f;
    int   lastSemitone = -1;
    bool  gateTriggered= false;  // set on any gatePulse.trigger() call
    float semiRemain[12] = {};

    // Fixed RNG sequences for deterministic testing
    float rngSeq[64] = {};
    int   rngIdx     = 0;
    float nextRng()  { float v=rngSeq[rngIdx%64]; rngIdx++; return v; }

    void reset() {
        holdRemain=0.f; gateHeld=false; currentPitch=0.f;
        lastSemitone=-1; gateTriggered=false; rngIdx=0;
        std::fill(semiRemain,semiRemain+12,0.f);
    }

    // Mirror of triggerStepEventForOffsetStep_ exact logic
    void step(int sem, float pitchV, float restProb, float legatoProb, float dur) {
        gateTriggered = false;  // reset pulse flag each step

        // Max-legato shortcut
        if (legatoProb >= 0.999f) {
            currentPitch = pitchV; lastSemitone = sem;
            gateHeld = true; holdRemain = dur;
            if (sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
            return;
        }

        // Mid-note: inside a multi-step note
        if (holdRemain >= 1.f) {
            if (gateHeld) {
                if (nextRng() < legatoProb) {
                    holdRemain += dur;
                    if (sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],holdRemain);
                }
            } else {
                // Mid-rest
                if (nextRng() < restProb)
                    holdRemain += dur;
            }
            return;
        }

        // Note boundary — full decision
        bool doRest = nextRng() < restProb;
        if (doRest) {
            gateHeld   = false;
            holdRemain = dur;
            return;
        }
        bool doLegato = nextRng() < legatoProb;
        bool doTie    = (!doLegato) && (sem==lastSemitone) && gateHeld
                        && (nextRng() < legatoProb);

        if (doLegato) {
            currentPitch = pitchV; lastSemitone = sem;
            if (gateHeld) holdRemain += dur;
            else { gateTriggered=true; holdRemain=dur; gateHeld=true; }
            if (sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
            return;
        }
        if (doTie) {
            holdRemain += dur;
            if (sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
            return;
        }
        // Normal note
        currentPitch = pitchV; lastSemitone = sem;
        gateTriggered = true; gateHeld = true; holdRemain = dur;
        if (sem>=0&&sem<12) semiRemain[sem]=std::max(semiRemain[sem],dur);
    }

    // Simulate gs.tick() — decrement holdRemain by 1 per step edge
    void tick() {
        if (!gateHeld) { holdRemain=std::max(0.f,holdRemain-1.f); return; }
        holdRemain -= 1.f;
        if (holdRemain <= 0.f) { holdRemain=0.f; gateHeld=false; }
        for (int i=0;i<12;++i) { semiRemain[i]-=1.f; if(semiRemain[i]<0) semiRemain[i]=0; }
    }
};

// Helper: run N steps with given note length (no rest, no legato)
static StepEngine runSteps(int nSteps, float dur, float restProb=0.f, float legatoProb=0.f) {
    StepEngine e; e.reset();
    // Set rng to all-high so rest/legato never fire by default
    std::fill(e.rngSeq, e.rngSeq+64, 0.99f);
    for (int s=0; s<nSteps; ++s) {
        e.tick();
        e.step(5, 1.5f, restProb, legatoProb, dur);
    }
    return e;
}

int main() {
    std::cout<<"\033[1mMulti-Step Note/Rest Tests\033[0m\n";
    std::cout<<std::string(54,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mid-Note: Gate Holds Across Steps");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Quarter note (4 steps): gate held through all 4 steps", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f); // no rest, no legato
        // Step 1: note fires (holdRemain=0 at entry)
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f); // dur=4 steps (quarter)
        EXPECT(e.gateHeld); EXPECT_NEAR(e.holdRemain,4.f,1e-4f);
        // Steps 2,3,4: mid-note, gate stays held, no retrigger
        for (int i=0;i<3;++i) {
            bool wasTriggered = false;
            e.tick();
            e.step(5,1.5f,0.f,0.f,4.f);
            EXPECT(e.gateHeld);
            EXPECT(!e.gateTriggered);  // no retrigger on mid-note steps
        }
    });

    TEST("Quarter note (4 steps): holdRemain counts down correctly", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f);
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f);
        EXPECT_NEAR(e.holdRemain,4.f,1e-4f);  // after trigger
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f); EXPECT_NEAR(e.holdRemain,3.f,1e-4f);
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f); EXPECT_NEAR(e.holdRemain,2.f,1e-4f);
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f); EXPECT_NEAR(e.holdRemain,1.f,1e-4f);
        // Step 5: holdRemain<=1 at entry → new note fires
        e.tick(); // holdRemain → 0, gate closes
        EXPECT(!e.gateHeld);
    });

    TEST("Quarter note followed by eighth: new attack fires at step 5", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f);
        // Quarter (4 steps)
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f);
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f);
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f);
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f);
        // Step 5: note expired, new note fires
        e.tick();
        EXPECT(!e.gateHeld); // gate closed by tick
        e.step(7,2.0f,0.f,0.f,2.f); // eighth (2 steps)
        EXPECT(e.gateTriggered);
        EXPECT(e.gateHeld);
        EXPECT_NEAR(e.holdRemain,2.f,1e-4f);
    });

    TEST("Half note (8 steps): gate held all 8 steps, no retriggering", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f);
        e.tick(); e.step(5,1.5f,0.f,0.f,8.f);
        int retrigs=0;
        for (int i=1;i<8;++i) {
            e.tick(); e.step(5,1.5f,0.f,0.f,8.f);
            if (e.gateTriggered) ++retrigs;
        }
        EXPECT_EQ(retrigs,0);
        EXPECT(e.gateHeld);
    });

    TEST("1/16 note (1 step): every step fires a new note", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f);
        int retrigs=0;
        for (int i=0;i<8;++i) {
            e.tick(); e.step(5,1.5f,0.f,0.f,1.f);
            if (e.gateTriggered) ++retrigs;
        }
        EXPECT_EQ(retrigs,8); // every step fires
    });

    TEST("Pitch unchanged on mid-note steps (no pitch theft)", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f);
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f);
        float pitchAfterTrigger = e.currentPitch;
        // Steps 2,3,4 with different pitch in melodySemitone slot — pitch should NOT change
        e.tick(); e.step(7,2.0f,0.f,0.f,4.f); // different semitone/pitch
        EXPECT_NEAR(e.currentPitch,pitchAfterTrigger,1e-5f); // pitch locked during note body
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mid-Note Extension: Probabilistic Tie");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Mid-note with legatoProb=1: every intermediate step extends hold", {
        StepEngine e; e.reset();
        // rng all low → legato always fires
        std::fill(e.rngSeq,e.rngSeq+64,0.01f);
        e.tick(); e.step(5,1.5f,0.f,1.f,4.f); // trigger (legatoProb=1 shortcut)
        float h0 = e.holdRemain;
        e.tick(); e.step(5,1.5f,0.f,0.5f,4.f); // mid-note, legato fires (rng=0.01<0.5)
        EXPECT_GT(e.holdRemain, h0-1.f+3.9f); // extended: ticked -1 + added 4
    });

    TEST("Mid-note with legatoProb=0: no extension, hold counts down", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f); // rng high → legato never fires
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f);
        float h0 = e.holdRemain; // 4.0
        e.tick(); e.step(5,1.5f,0.f,0.f,4.f); // mid-note, no extension
        EXPECT_NEAR(e.holdRemain, h0-1.f, 1e-4f); // just counted down
    });

    TEST("Mid-note extension accumulates over multiple steps", {
        StepEngine e; e.reset();
        // rng all low → extension fires on every mid-note step
        std::fill(e.rngSeq,e.rngSeq+64,0.01f);
        e.tick(); e.step(5,1.5f,0.f,1.f,4.f); // trigger
        // 3 more mid-note steps, each extending by 4
        for (int i=0;i<3;++i) { e.tick(); e.step(5,1.5f,0.f,0.5f,4.f); }
        EXPECT_GT(e.holdRemain,8.f); // well extended
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mid-Rest: Rests Span Multiple Steps");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Rest fires, sets holdRemain=dur, gate closes", {
        StepEngine e; e.reset();
        // rng low → rest fires
        std::fill(e.rngSeq,e.rngSeq+64,0.01f);
        e.tick(); e.step(5,1.5f,0.5f,0.f,4.f); // restProb=0.5, rng=0.01 → rest
        EXPECT(!e.gateHeld);
        EXPECT_NEAR(e.holdRemain,4.f,1e-4f);
    });

    TEST("Mid-rest: no new note fires while rest is counting down", {
        StepEngine e; e.reset();
        e.rngSeq[0]=0.01f; // step 1: rest fires (rng < restProb=0.5)
        std::fill(e.rngSeq+1,e.rngSeq+64,0.99f); // steps 2-4: rng high
        e.tick(); e.step(5,1.5f,0.5f,0.f,4.f); // rest, holdRemain=4
        EXPECT(!e.gateHeld);
        int attacks=0;
        for (int i=1;i<4;++i) {
            e.tick();
            e.step(5,1.5f,0.5f,0.f,4.f);
            if (e.gateHeld) ++attacks;
        }
        EXPECT_EQ(attacks,0); // gate never opened mid-rest
    });

    TEST("Mid-rest extension with restProb=1: rest extends every step", {
        StepEngine e; e.reset();
        // rng low → rest fires and extends every mid-rest step
        std::fill(e.rngSeq,e.rngSeq+64,0.01f);
        e.tick(); e.step(5,1.5f,0.5f,0.f,4.f); // rest fires
        float h0 = e.holdRemain;
        e.tick(); e.step(5,1.5f,1.0f,0.f,4.f); // mid-rest, restProb=1, rng<1 → extends
        EXPECT_GT(e.holdRemain, h0-1.f+3.9f);
    });

    TEST("Mid-rest with restProb=0: no extension, rest ends at boundary", {
        StepEngine e; e.reset();
        e.rngSeq[0]=0.01f; // rest fires
        std::fill(e.rngSeq+1,e.rngSeq+64,0.99f);
        e.tick(); e.step(5,1.5f,0.5f,0.f,4.f); // rest
        float h0 = e.holdRemain;
        e.tick(); e.step(5,1.5f,0.0f,0.f,4.f); // mid-rest, restProb=0 → no extension
        EXPECT_NEAR(e.holdRemain,h0-1.f,1e-4f);
    });

    TEST("Note fires at rest boundary (holdRemain <= 1)", {
        StepEngine e; e.reset();
        e.rngSeq[0]=0.01f;  // step1: rest fires with dur=2
        e.rngSeq[1]=0.99f;  // step2 mid-rest: no extension
        e.rngSeq[2]=0.99f;  // step3 boundary: no rest (rng > restProb)
        e.rngSeq[3]=0.99f;  // step3: no legato
        e.rngSeq[4]=0.99f;  // step3: no tie
        e.tick(); e.step(5,1.5f,0.5f,0.f,2.f); // rest, hold=2
        EXPECT(!e.gateHeld);
        e.tick(); e.step(5,1.5f,0.5f,0.f,2.f); // mid-rest, no extension, hold=1
        EXPECT(!e.gateHeld);
        e.tick(); e.step(5,1.5f,0.1f,0.f,2.f); // rest ended (hold<=1 after tick), note fires
        EXPECT(e.gateHeld);
        EXPECT(e.gateTriggered);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Phrase Rhythm Integrity");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("16 steps of 1/4 notes: exactly 4 attack points", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f); // no rest, no legato
        int attacks=0;
        for (int s=0;s<16;++s) {
            e.tick();
            e.step(5,1.5f,0.f,0.f,4.f);
            if (e.gateTriggered) ++attacks;
        }
        EXPECT_EQ(attacks,4); // quarter notes: attacks at steps 0,4,8,12
    });

    TEST("16 steps of 1/2 notes: exactly 2 attack points", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f);
        int attacks=0;
        for (int s=0;s<16;++s) {
            e.tick();
            e.step(5,1.5f,0.f,0.f,8.f);
            if (e.gateTriggered) ++attacks;
        }
        EXPECT_EQ(attacks,2);
    });

    TEST("16 steps of 1/8 notes: exactly 8 attack points", {
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f);
        int attacks=0;
        for (int s=0;s<16;++s) {
            e.tick();
            e.step(5,1.5f,0.f,0.f,2.f);
            if (e.gateTriggered) ++attacks;
        }
        EXPECT_EQ(attacks,8);
    });

    TEST("Mixed quarter+eighth: 4+2 steps = 6 step phrase spans correctly", {
        // Quarter(4) + Eighth(2) + Quarter(4) + Eighth(2) + Quarter(4) = 16
        StepEngine e; e.reset();
        std::fill(e.rngSeq,e.rngSeq+64,0.99f);
        float durs[16]={4,4,4,4, 2,2, 4,4,4,4, 2,2, 4,4,4,4};
        int attacks=0;
        for (int s=0;s<16;++s) {
            e.tick();
            e.step(5,1.5f,0.f,0.f,durs[s]);
            if (e.gateTriggered) ++attacks;
        }
        // Attacks at steps 0, 4, 6, 10, 12 = 5
        EXPECT_EQ(attacks,5);
    });

    TEST("No mid-note restarts ever produce holdRemain < 0", {
        StepEngine e; e.reset();
        // Random mix of rng values
        for (int i=0;i<64;++i) e.rngSeq[i]=float(i%7)/7.f;
        for (int s=0;s<64;++s) {
            e.tick();
            e.step(5,1.5f,0.3f,0.4f,4.f);
            EXPECT(e.holdRemain>=0.f);
        }
    });

    std::cout<<"\n"<<std::string(54,'=')<<"\n";
    std::cout<<"\033[32m"<<s_pass<<" passed\033[0m  ";
    if(s_fail>0)std::cout<<"\033[31m"<<s_fail<<" failed\033[0m";
    else        std::cout<<"\033[32m0 failed\033[0m";
    std::cout<<"  ("<<(s_pass+s_fail)<<" total)\n\n";
    return s_fail>0?1:0;
}
