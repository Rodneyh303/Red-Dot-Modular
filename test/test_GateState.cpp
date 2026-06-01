#include "test_stubs.hpp"
/**
 * test_GateState.cpp
 * Compile: g++ -std=c++17 -I/home/claude test_GateState.cpp -o test_gs && ./test_gs
 */
#include "GateState.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e)       do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a,b)  do{if((a)!=(b)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" != "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)
#define EXPECT_NEAR(a,b,e) do{if(std::fabs((a)-(b))>(e)){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" not near "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)
#define EXPECT_GT(a,b)  do{if(!((a)>(b))){std::ostringstream s; \
    s<<#a<<"="<<(a)<<" not > "<<#b<<"="<<(b);throw std::runtime_error(s.str());}}while(0)

static GateState fresh() { GateState g; g.reset(); return g; }

int main(){
    std::cout<<"\033[1mGateState Tests\033[0m\n";
    std::cout<<std::string(50,'=')<<"\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("triggerNote — Normal Note");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("triggerNote: gate opens", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        EXPECT(g.gateHeld);
    });
    TEST("triggerNote: pitch set correctly", {
        auto g=fresh(); g.triggerNote(1.5f,5,3);
        EXPECT_NEAR(g.currentPitchV, 1.5f, 1e-5f);
    });
    TEST("triggerNote: lastSemitone updated", {
        auto g=fresh(); g.triggerNote(1.f,7,3);
        EXPECT_EQ(g.lastSemitone, 7);
    });
    TEST("triggerNote: holdRemain = noteSteps(nvIdx)", {
        auto g=fresh(); g.triggerNote(1.f,5,3);  // idx 3 = 1/8 = 2 steps
        EXPECT_NEAR(g.holdRemain, 2.f, 1e-5f);
    });
    TEST("triggerNote: gatePulse fired (high immediately)", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        EXPECT_GT(g.process(0.f), 5.f);  // pulse high
    });
    TEST("triggerNote: semiPlayRemain updated", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        EXPECT_GT(g.semiPlayRemain[5], 0.f);
    });
    TEST("triggerNote: semiPlayRemain for other semitones untouched", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        EXPECT_NEAR(g.semiPlayRemain[4], 0.f, 1e-5f);
        EXPECT_NEAR(g.semiPlayRemain[6], 0.f, 1e-5f);
    });
    TEST("triggerNote twice: holdRemain SET not accumulated", {
        auto g=fresh();
        g.triggerNote(1.f,5,1);  // 1/4 = 4 steps
        g.triggerNote(1.f,5,3);  // 1/8 = 2 steps
        EXPECT_NEAR(g.holdRemain, 2.f, 1e-5f);  // reset to 2, not 4+2
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("slideNote — Legato");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("slideNote: pitch changes", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        g.slideNote(2.f,7,3);
        EXPECT_NEAR(g.currentPitchV, 2.f, 1e-5f);
    });
    TEST("slideNote while held: holdRemain extended", {
        auto g=fresh(); g.triggerNote(1.f,5,1);  // 4 steps
        float before=g.holdRemain;
        g.slideNote(2.f,7,3);  // extend by 2
        EXPECT_NEAR(g.holdRemain, before+2.f, 1e-4f);
    });
    TEST("slideNote while held: NO new gatePulse (no retrigger)", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        g.gatePulse.reset();     // clear initial pulse
        g.slideNote(2.f,7,3);
        // gatePulse should NOT have been triggered again
        EXPECT(!g.gatePulse.isHigh());
    });
    TEST("slideNote from cold (gate not held): opens gate with pulse", {
        auto g=fresh();  // gate not held
        g.slideNote(1.5f,3,3);
        EXPECT(g.gateHeld);
        EXPECT(g.gatePulse.isHigh());  // first note of legato run
    });
    TEST("slideNote from cold: holdRemain SET to dur", {
        auto g=fresh();
        g.slideNote(1.5f,3,3);
        EXPECT_NEAR(g.holdRemain, 2.f, 1e-4f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("slideMax — Legato Maximum");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("slideMax: gate held", {
        auto g=fresh(); g.slideMax(1.f,5,3);
        EXPECT(g.gateHeld);
    });
    TEST("slideMax: no retrigger pulse", {
        auto g=fresh(); g.slideMax(1.f,5,3);
        EXPECT(!g.gatePulse.isHigh());
    });
    TEST("slideMax: holdRemain set to exact dur (not extended)", {
        auto g=fresh(); g.triggerNote(1.f,5,1);  // 4 steps
        g.slideMax(2.f,7,3);  // 2 steps
        EXPECT_NEAR(g.holdRemain, 2.f, 1e-4f);  // SET not +=
    });
    TEST("slideMax: pitch updated", {
        auto g=fresh(); g.slideMax(2.5f,9,3);
        EXPECT_NEAR(g.currentPitchV, 2.5f, 1e-5f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("extendHold — Tie");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("extendHold: holdRemain increases by dur", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        float before=g.holdRemain;
        g.extendHold(5,3);
        EXPECT_NEAR(g.holdRemain, before*2.f, 1e-4f);  // 2+2=4
    });
    TEST("extendHold: pitch unchanged", {
        auto g=fresh(); g.triggerNote(1.5f,5,3);
        g.extendHold(5,3);
        EXPECT_NEAR(g.currentPitchV, 1.5f, 1e-5f);
    });
    TEST("extendHold: no retrigger", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        g.gatePulse.reset();
        g.extendHold(5,3);
        EXPECT(!g.gatePulse.isHigh());
    });
    TEST("extendHold multiple times: accumulates correctly", {
        auto g=fresh(); g.triggerNote(1.f,5,3);  // 2 steps
        g.extendHold(5,3);  // +2 = 4
        g.extendHold(5,3);  // +2 = 6
        EXPECT_NEAR(g.holdRemain, 6.f, 1e-3f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("rest()");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("rest (no tie): holdRemain unchanged", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        float before=g.holdRemain;
        g.rest(false, 3);
        EXPECT_NEAR(g.holdRemain, before, 1e-5f);
    });
    TEST("rest (no tie): gate stays held", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        g.rest(false, 3);
        EXPECT(g.gateHeld);
    });
    TEST("rest (tie + held): holdRemain extended", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        float before=g.holdRemain;
        g.rest(true, 3);
        EXPECT_NEAR(g.holdRemain, before+2.f, 1e-4f);
    });
    TEST("rest (tie + NOT held): holdRemain unchanged (can't extend closed gate)", {
        auto g=fresh();  // gate not held
        g.rest(true, 3);
        EXPECT_NEAR(g.holdRemain, 0.f, 1e-5f);
    });
    TEST("rest (tie + held): pitch unchanged", {
        auto g=fresh(); g.triggerNote(1.5f,5,3);
        g.rest(true,3);
        EXPECT_NEAR(g.currentPitchV, 1.5f, 1e-5f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("tick() — Step Edge Countdown");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("tick: holdRemain decrements by 1", {
        auto g=fresh(); g.triggerNote(1.f,5,1);  // 4 steps
        g.tick();
        EXPECT_NEAR(g.holdRemain, 3.f, 1e-5f);
    });
    TEST("tick N times: gate closes when holdRemain hits 0", {
        auto g=fresh(); g.triggerNote(1.f,5,3);  // 2 steps
        g.tick(); g.tick();
        EXPECT(!g.gateHeld);
        EXPECT_NEAR(g.holdRemain, 0.f, 1e-5f);
    });
    TEST("tick: holdRemain never goes below 0", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        for(int i=0;i<10;++i) g.tick();
        EXPECT(g.holdRemain >= 0.f);
    });
    TEST("tick when not held: no state change", {
        auto g=fresh();  // gate not held
        g.tick();
        EXPECT(!g.gateHeld);
        EXPECT_NEAR(g.holdRemain, 0.f, 1e-5f);
    });
    TEST("tick: semiPlayRemain decays", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        float before=g.semiPlayRemain[5];
        g.tick();
        EXPECT(g.semiPlayRemain[5] < before);
    });
    TEST("tick: semiPlayRemain clamps at 0", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        for(int i=0;i<10;++i) g.tick();
        EXPECT(g.semiPlayRemain[5] >= 0.f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("process() — Gate Voltage Output");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("process: 10V when gate held", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        g.gatePulse.reset();  // clear pulse, leave gateHeld
        EXPECT_NEAR(g.process(1.f/44100.f), 10.f, 1e-5f);
    });
    TEST("process: 0V when neither held nor pulse", {
        auto g=fresh();
        EXPECT_NEAR(g.process(1.f/44100.f), 0.f, 1e-5f);
    });
    TEST("process: 10V during gatePulse even if not held", {
        auto g=fresh(); g.gatePulse.trigger(0.001f);
        EXPECT_NEAR(g.process(0.f), 10.f, 1e-5f);
    });
    TEST("process: after gate expires and pulse gone, returns 0V", {
        auto g=fresh(); g.triggerNote(1.f,5,3);
        for(int i=0;i<2;++i) g.tick();         // hold expires
        g.process(1.f);  // first call still "on" (Rack PulseGenerator returns true while draining)
        float v=g.process(1.f);  // second call: pulse fully drained
        EXPECT_NEAR(v, 0.f, 1e-5f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("semiLedBrightness — LED Output");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("brightness 0 before any note", {
        auto g=fresh();
        EXPECT_NEAR(g.semiLedBrightness(5), 0.f, 1e-5f);
    });
    TEST("brightness near 1 immediately after 1/4 note", {
        auto g=fresh(); g.triggerNote(1.f,5,1);  // 1/4=4 steps; 4*0.25=1.0
        EXPECT_NEAR(g.semiLedBrightness(5), 1.f, 1e-3f);
    });
    TEST("brightness clamped to 0..1", {
        auto g=fresh(); g.triggerNote(1.f,5,0);  // 1/2=8 steps; 8*0.25=2.0 → clamped 1
        EXPECT(g.semiLedBrightness(5) <= 1.f);
        EXPECT(g.semiLedBrightness(5) >= 0.f);
    });
    TEST("brightness decays after ticks", {
        auto g=fresh(); g.triggerNote(1.f,5,1);  // 4 steps
        float b0=g.semiLedBrightness(5);
        g.tick(); g.tick();
        EXPECT(g.semiLedBrightness(5) < b0);
    });
    TEST("out-of-range semitone: returns 0 (no crash)", {
        auto g=fresh();
        EXPECT_NEAR(g.semiLedBrightness(-1),  0.f, 1e-5f);
        EXPECT_NEAR(g.semiLedBrightness(12),  0.f, 1e-5f);
        EXPECT_NEAR(g.semiLedBrightness(100), 0.f, 1e-5f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("reset()");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("reset: gateHeld cleared", {
        auto g=fresh(); g.triggerNote(1.f,5,3); g.reset();
        EXPECT(!g.gateHeld);
    });
    TEST("reset: holdRemain = 0", {
        auto g=fresh(); g.triggerNote(1.f,5,3); g.reset();
        EXPECT_NEAR(g.holdRemain, 0.f, 1e-5f);
    });
    TEST("reset: currentPitchV = 0", {
        auto g=fresh(); g.triggerNote(2.5f,5,3); g.reset();
        EXPECT_NEAR(g.currentPitchV, 0.f, 1e-5f);
    });
    TEST("reset: lastSemitone = -1", {
        auto g=fresh(); g.triggerNote(1.f,7,3); g.reset();
        EXPECT_EQ(g.lastSemitone, -1);
    });
    TEST("reset: semiPlayRemain all zero", {
        auto g=fresh(); g.triggerNote(1.f,5,3); g.reset();
        for(int i=0;i<12;++i) EXPECT_NEAR(g.semiPlayRemain[i], 0.f, 1e-5f);
    });
    TEST("reset: process() returns 0V", {
        auto g=fresh(); g.triggerNote(1.f,5,3); g.reset();
        EXPECT_NEAR(g.process(1.f), 0.f, 1e-5f);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("noteSteps — Duration Mapping");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("1/4 note = 4 steps", {
        EXPECT_NEAR(gs_noteSteps(1), 4.f, 1e-5f);
    });
    TEST("1/8 note = 2 steps", {
        EXPECT_NEAR(gs_noteSteps(3), 2.f, 1e-5f);
    });
    TEST("1/2 note = 8 steps", {
        EXPECT_NEAR(gs_noteSteps(0), 8.f, 1e-5f);
    });
    TEST("1/32 note = 0.5 steps (minimum)", {
        EXPECT_NEAR(gs_noteSteps(7), 0.5f, 1e-5f);
    });
    TEST("out-of-range nvIdx: returns 1.0 (safe default)", {
        EXPECT_NEAR(gs_noteSteps(-1), 1.f, 1e-5f);
        EXPECT_NEAR(gs_noteSteps(99), 1.f, 1e-5f);
    });
    TEST("all valid indices return > 0", {
        for(int i=0;i<=8;++i) EXPECT(gs_noteSteps(i) > 0.f);
    });
    TEST("durations strictly decrease idx 0→7", {
        for(int i=1;i<=7;++i) EXPECT(gs_noteSteps(i-1) > gs_noteSteps(i));
    });

    std::cout<<"\n"<<std::string(50,'=')<<"\n";
    std::cout<<"\033[32m"<<s_pass<<" passed\033[0m  ";
    if(s_fail>0)std::cout<<"\033[31m"<<s_fail<<" failed\033[0m";
    else        std::cout<<"\033[32m0 failed\033[0m";
    std::cout<<"  ("<<(s_pass+s_fail)<<" total)\n\n";
    return s_fail>0?1:0;
}
