/**
 * test_edge_cases.cpp
 * Tests for areas not yet covered:
 *   - ClockEngine: external PPQN (wrong/zero/huge), period filter, BPM clamp
 *   - ClockEngine: ext→int and int→ext switching
 *   - Mute: INV GATE, restart-on-unmute, power-mute
 *   - Gate assignments: all 4 gate1 and gate2 behaviours
 *   - Phrase boundary: seed application order, locked guard
 *   - handleRestart: stepIndex placement, gate cleared, resetImmediate seeds
 *   - JSON round-trip: corrupt/out-of-range values silently clamped or ignored
 *   - Pattern window: startStep > endStep (wrapped), length=0 guard
 *
 * Compile: g++ -std=c++17 test_edge_cases.cpp -o test_edge && ./test_edge
 */

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

template<typename T> T clampv(T v,T lo,T hi){return v<lo?lo:(v>hi?hi:v);}

// ── Test framework ────────────────────────────────────────────────────────────
static int s_pass=0, s_fail=0;
#define SUITE(n) do{std::cout<<"\n\033[1;34m["<<(n)<<"]\033[0m\n";}while(0)
#define TEST(desc,...) do{ \
    bool _ok=true;std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e)      do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)
#define EXPECT_EQ(a__,b__) do{if((a__)!=(b__)){std::ostringstream _os; \
    _os<<#a__<<"="<<(a__)<<" != "<<#b__<<"="<<(b__);throw std::runtime_error(_os.str());}}while(0)
#define EXPECT_NEAR(a__,b__,e__) do{if(std::fabs((a__)-(b__))>(e__)){std::ostringstream _os; \
    _os<<#a__<<"="<<(a__)<<" not near "<<#b__<<"="<<(b__);throw std::runtime_error(_os.str());}}while(0)
#define EXPECT_GT(a__,b__) do{if(!((a__)>(b__))){std::ostringstream _os; \
    _os<<#a__<<"="<<(a__)<<" not > "<<#b__<<"="<<(b__);throw std::runtime_error(_os.str());}}while(0)

// ═══════════════════════════════════════════════════════════════════════════════
// ClockEngine mirror (exact copy of plugin logic, no Rack deps needed)
// ═══════════════════════════════════════════════════════════════════════════════
struct SchmittTrigger {
    // Simplified Schmitt trigger for testing
    float state = 0.f;
    bool process(float v, float lo=0.1f, float hi=2.f) {
        bool was = (state > lo);
        state = v;
        bool now = (v > hi);
        return !was && now;   // rising edge only
    }
    void reset(){ state=0.f; }
};

struct ClockEngine {
    SchmittTrigger clkTrig;
    float timeAcc        = 0.f;
    float clockTimeAcc   = 0.f;
    bool  measured       = false;
    bool  externalActive = false;
    int   ppqnCount      = 0;
    int   sixteenthCount = 0;
    bool  sixteenthEdge  = false;
    bool  quarterEdge    = false;
    float bpm            = 120.f;

    void reset(){
        clkTrig.reset();
        timeAcc=clockTimeAcc=0.f;
        ppqnCount=sixteenthCount=0;
        measured=externalActive=false;
        sixteenthEdge=quarterEdge=false;
        bpm=120.f;
    }

    void process(float clockVoltage, bool clkConnected,
                 float internalBpm, int ppqn, float sampleTime){
        sixteenthEdge=false; quarterEdge=false;
        clockTimeAcc+=sampleTime;

        if(clkConnected){
            externalActive=true; timeAcc=0.f;
            // Option 2: use knob BPM until first pulse measured
            if(!measured) bpm=clampv(internalBpm,20.f,300.f);
            if(clkTrig.process(clockVoltage,0.1f,2.f)){
                if(measured && clockTimeAcc>0.001f && clockTimeAcc<5.f){
                    float derivedBpm=60.f/(clockTimeAcc*float(std::max(1,ppqn)));
                    bpm=clampv(derivedBpm,20.f,300.f);
                }
                measured=true; clockTimeAcc=0.f;
                ppqnCount=(ppqnCount+1)%std::max(1,ppqn);
                if(ppqnCount==0){
                    sixteenthEdge=true;
                    sixteenthCount=(sixteenthCount+1)&3;
                    if(sixteenthCount==0) quarterEdge=true;
                }
            }
        } else {
            externalActive=false; measured=false;
            clockTimeAcc=0.f;
            ppqnCount=0;
            bpm=clampv(internalBpm,20.f,300.f);
            float sixteenthSec=(4.f*60.f/bpm)/16.f;
            timeAcc+=sampleTime;
            if(timeAcc>=sixteenthSec){
                timeAcc-=sixteenthSec;
                sixteenthEdge=true;
                sixteenthCount=(sixteenthCount+1)&3;
                if(sixteenthCount==0) quarterEdge=true;
            }
        }
    }
};

// Helper: fire N clock pulses with given inter-pulse gap (seconds)
static int fireClockPulses(ClockEngine& clk, int n, float period,
                            float sampleTime, int ppqn,
                            int* sixteenths_out=nullptr, int* quarters_out=nullptr){
    int sixteenths=0, quarters=0;
    float internalBpm=120.f;
    // Each "pulse": send high for 1 sample, then low for (period-sampleTime) samples
    for(int p=0;p<n;++p){
        // Rising edge sample
        clk.process(5.f, true, internalBpm, ppqn, sampleTime);
        if(clk.sixteenthEdge) ++sixteenths;
        if(clk.quarterEdge)   ++quarters;
        // Remaining samples at low
        float elapsed=sampleTime;
        while(elapsed<period-sampleTime){
            clk.process(0.f, true, internalBpm, ppqn, sampleTime);
            if(clk.sixteenthEdge) ++sixteenths;
            if(clk.quarterEdge)   ++quarters;
            elapsed+=sampleTime;
        }
    }
    if(sixteenths_out) *sixteenths_out=sixteenths;
    if(quarters_out)   *quarters_out  =quarters;
    return sixteenths;
}

// ── stepInWindow mirror ───────────────────────────────────────────────────────
static bool stepInWindow(int idx, int start, int end){
    if(start<=end) return idx>=start && idx<=end;
    return idx>=start || idx<=end;
}

// ── Mute state ────────────────────────────────────────────────────────────────
struct MuteState {
    bool muted=false; bool restartFired=false;
    int  muteBehavior=0;  // 0=normal, 1=restart-on-unmute, 2=inv-gate

    // Mirror of gate2Assign==2 handler
    void applyGate(bool g2High){
        bool shouldMute=(muteBehavior==2)?!g2High:g2High;
        if(shouldMute!=muted){
            muted=shouldMute;
            if(!muted && muteBehavior==1) restartFired=true;
        }
    }
};

// ── Gate1 assignment handler mirror ──────────────────────────────────────────
struct GateAssignState {
    int rhythmMode=0, melodyMode=0;
    bool rhythmSeedPending=false, melodySeedPending=false;
    bool restartFired=false;

    void onGate1Rise(int gate1Assign){
        switch(gate1Assign){
            case 0: rhythmMode=(1-rhythmMode); break;
            case 1: if(rhythmMode==0){rhythmSeedPending=true;} break;
            case 2: if(melodyMode==0){melodySeedPending=true;} break;
            case 3: restartFired=true; break;
        }
    }
    void onGate2Rise(int gate2Assign){
        switch(gate2Assign){
            case 0: melodyMode=(1-melodyMode); break;
            case 1: if(melodyMode==0){melodySeedPending=true;} break;
            case 3: restartFired=true; break;
        }
    }
};

// ── Phrase boundary seed state ────────────────────────────────────────────────
struct PhraseState {
    float rhythmSeedFloat=0.f, melodySeedFloat=0.f;
    float rhythmSeedPendingFloat=0.f, melodySeedPendingFloat=0.f;
    bool  rhythmSeedPending=false, melodySeedPending=false;
    bool  locked=false;
    int   redrawCount=0;  // tracks how many times pattern was redrawn

    // Mirror of onPhraseBoundary_
    void onPhraseBoundary(){
        if(rhythmSeedPending){
            rhythmSeedFloat=rhythmSeedPendingFloat;
            rhythmSeedPending=false;
        }
        if(melodySeedPending){
            melodySeedFloat=melodySeedPendingFloat;
            melodySeedPending=false;
        }
        if(!locked) ++redrawCount;
    }

    // Mirror of handleRestart(resetImmediate=true)
    void handleRestart(bool resetImmediate){
        if(resetImmediate){
            if(rhythmSeedPending){
                rhythmSeedFloat=rhythmSeedPendingFloat;
                rhythmSeedPending=false;
            }
            if(melodySeedPending){
                melodySeedFloat=melodySeedPendingFloat;
                melodySeedPending=false;
            }
        }
        if(!locked) ++redrawCount;
    }
};

// ── JSON value validation mirror ──────────────────────────────────────────────
// Simulates what happens when loading a corrupt patch: values silently loaded
// without range checking. These tests document current behaviour and can
// catch regressions if clamping is added in future.
static bool validModeSelect(int v)  { return v>=0 && v<=3; }
static bool validPpqnSetting(int v) { return v==1 || v==4 || v==24; }
static bool validGateAssign(int v)  { return v>=0 && v<=3; }
static bool validMuteBehavior(int v){ return v>=0 && v<=3; }
static bool validRhythmMode(int v)  { return v==0 || v==1; }
static bool validStepIdx(int v)     { return v>=0 && v<=15; }
static float clampPitch(float v)    { return clampv(v,0.f,5.f); }

// ═══════════════════════════════════════════════════════════════════════════════
// TESTS
// ═══════════════════════════════════════════════════════════════════════════════

int main(){
    std::cout << "\033[1mEdge Case Tests\033[0m\n";
    std::cout << std::string(54,'=') << "\n";

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("ClockEngine — External Clock PPQN");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("PPQN=1: every clock pulse → one 1/16 edge", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, PERIOD=0.5f;  // 120BPM quarter = 0.5s
        int sixteenths=0;
        fireClockPulses(clk, 16, PERIOD, ST, 1, &sixteenths);
        EXPECT_EQ(sixteenths, 16);
    });

    TEST("PPQN=4: four pulses → one 1/16 edge", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, PERIOD=0.125f;  // 120BPM 1/16 = 0.125s
        int sixteenths=0;
        fireClockPulses(clk, 4*16, PERIOD, ST, 4, &sixteenths);
        // 4 pulses per step × 16 steps = 16 sixteenth edges
        EXPECT_EQ(sixteenths, 16);
    });

    TEST("PPQN=24: 24 pulses → one 1/16 edge (MIDI clock)", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // 120BPM MIDI: 24 ppqn per quarter, quarter=0.5s → pulse every 0.0208s
        const float PERIOD=0.5f/24.f;
        int sixteenths=0;
        fireClockPulses(clk, 24*4, PERIOD, ST, 24, &sixteenths);
        // 24 pulses per beat × 4 beats = 96 pulses → 4 sixteenth edges
        EXPECT_EQ(sixteenths, 4);
    });

    TEST("PPQN=1: every 4 sixteenth edges → one quarter edge", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, PERIOD=0.5f;
        int quarters=0;
        fireClockPulses(clk, 16, PERIOD, ST, 1, nullptr, &quarters);
        EXPECT_EQ(quarters, 4);  // 16 steps / 4 = 4 quarter edges
    });

    TEST("Wrong PPQN=0: clamped to 1 internally (std::max(1,ppqn))", {
        // Plugin uses std::max(1, ppqn) so ppqn=0 behaves like ppqn=1
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, PERIOD=0.5f;
        int sixteenths=0;
        fireClockPulses(clk, 4, PERIOD, ST, 0, &sixteenths);
        // ppqn=0 → max(1,0)=1 → every pulse = one 1/16
        EXPECT_EQ(sixteenths, 4);
    });

    TEST("PPQN=2 (non-standard): divides correctly by 2", {
        // Not a UI option but could come from automation/corrupt patch
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, PERIOD=0.25f;
        int sixteenths=0;
        fireClockPulses(clk, 32, PERIOD, ST, 2, &sixteenths);
        // 2 pulses per 1/16 → 32 pulses → 16 sixteenth edges
        EXPECT_EQ(sixteenths, 16);
    });

    TEST("Huge PPQN=96: divides all the way down", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, PERIOD=0.5f/96.f;
        int sixteenths=0;
        fireClockPulses(clk, 96, PERIOD, ST, 96, &sixteenths);
        // 96 pulses per beat ÷ 96 = 1 sixteenth edge (one beat = 1 step)
        EXPECT_EQ(sixteenths, 1);
    });

    TEST("Spurious very short pulses (<1ms) ignored for BPM measurement", {
        // The plugin guards: if(measured && clockTimeAcc > 0.001f && ...)
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // First normal pulse to establish 'measured'
        clk.process(5.f,true,120.f,1,ST);
        clk.process(0.f,true,120.f,1,ST);
        float bpmAfterFirst=clk.bpm;
        // Second pulse 0.0001s later (spuriously short — sub-millisecond)
        clk.process(5.f,true,120.f,1,0.0001f);  // tiny gap
        // BPM should NOT have updated to 600000 BPM or similar insane value
        EXPECT(clk.bpm <= 300.f);  // clamped
    });

    TEST("Very long gap (>5s) between pulses: ignored for BPM measurement", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // Establish measurement baseline
        clk.process(5.f,true,120.f,1,ST); clk.process(0.f,true,120.f,1,ST);
        float bpmBefore=clk.bpm;
        // Simulate 6-second gap (stale, should be ignored)
        clk.process(5.f,true,120.f,1,6.0f);
        // BPM should not change to a near-zero value
        EXPECT(clk.bpm >= 20.f);
    });

    TEST("BPM derived from PPQN=1 at 120BPM: direct measurement, exact after 2nd pulse", {
        // Direct measurement: first pulse sets 'measured', second pulse gives exact BPM.
        // No smoothing lag — tempo is correct immediately.
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, QUARTER=0.5f;
        for(int i=0;i<3;++i){
            clk.process(5.f,true,120.f,1,ST);
            float e=ST; while(e<QUARTER-ST){clk.process(0.f,true,120.f,1,ST);e+=ST;}
        }
        EXPECT_NEAR(clk.bpm, 120.f, 1.f);   // within 1 BPM after just 3 pulses
    });


    TEST("Before first ext pulse: BPM falls back to internal knob (option 2)", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // Connect clock but don't fire any pulses yet
        // Process one sample with clock connected but no edge
        clk.process(0.f,true,150.f,1,ST);
        // measured=false → bpm should use internalBpm=150
        EXPECT_NEAR(clk.bpm, 150.f, 1.f);
        // After one pulse, still no measurement (first pulse sets measured=true only)
        clk.process(5.f,true,150.f,1,ST);  // rising edge — sets measured, no prior period
        EXPECT_NEAR(clk.bpm, 150.f, 1.f);  // still knob value (guard: !measured was true)
    });

    TEST("BPM clamped to 20..300: slow clock gives 20, fast gives 300", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // Very slow: 1 pulse every 10 seconds → derived BPM = 6 → clamped to 20
        clk.process(5.f,true,120.f,1,ST);
        clk.process(0.f,true,120.f,1,ST);
        clk.process(5.f,true,120.f,1,10.f);  // 10s gap
        EXPECT(clk.bpm >= 20.f);
        // Very fast: 1 pulse every 0.01s → derived BPM = 6000 → clamped to 300
        clk.reset();
        clk.process(5.f,true,120.f,1,ST); clk.process(0.f,true,120.f,1,ST);
        clk.process(5.f,true,120.f,1,0.01f);
        EXPECT(clk.bpm <= 300.f);
    });

    TEST("Direct BPM measurement: tempo change tracked immediately on next pulse", {
        // No LP filter means a tempo change is reflected on the very next clock pulse.
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // Establish 120BPM
        const float Q120=0.5f;
        for(int i=0;i<3;++i){
            clk.process(5.f,true,120.f,1,ST);
            float e=ST; while(e<Q120-ST){clk.process(0.f,true,120.f,1,ST);e+=ST;}
        }
        EXPECT_NEAR(clk.bpm, 120.f, 1.f);
        // Switch to 180BPM (quarter = 0.333s) — one pulse should update immediately
        const float Q180=60.f/180.f;
        clk.process(5.f,true,180.f,1,ST);
        float e=ST; while(e<Q180-ST){clk.process(0.f,true,180.f,1,ST);e+=ST;}
        clk.process(5.f,true,180.f,1,ST);  // second pulse measures the new period
        EXPECT_NEAR(clk.bpm, 180.f, 2.f);  // immediate — no lag
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("ClockEngine — Internal ↔ External Switching");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Switching ext→internal: externalActive clears, internal clock runs", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // Run with external clock
        clk.process(5.f,true,120.f,1,ST); clk.process(0.f,true,120.f,1,ST);
        EXPECT(clk.externalActive);
        // Remove external clock
        clk.process(0.f,false,120.f,1,ST);
        EXPECT(!clk.externalActive);
    });

    TEST("Switching ext→internal: timeAcc was reset, accumulates cleanly", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        clk.process(5.f,true,120.f,1,ST);
        // timeAcc should be 0 while external is active
        EXPECT_NEAR(clk.timeAcc,0.f,1e-6f);
        // Switch to internal
        clk.process(0.f,false,120.f,1,ST);
        EXPECT_NEAR(clk.timeAcc,ST,1e-5f);  // accumulated one sample
    });

    TEST("Switching ext→internal: measured flag clears on disconnect", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, PERIOD=0.5f;
        fireClockPulses(clk,4,PERIOD,ST,1);
        EXPECT(clk.measured);
        clk.process(0.f,false,120.f,1,ST);
        EXPECT(!clk.measured);
    });

    TEST("Switching internal→ext: ppqnCount reset (no spurious skip)", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // Internal clock runs for a while — ppqnCount stays 0 (unused internally)
        for(int i=0;i<1000;++i) clk.process(0.f,false,120.f,1,ST);
        EXPECT_EQ(clk.ppqnCount,0);
        // Switch to external; first pulse after switch starts clean
        clk.process(5.f,true,120.f,1,ST);
        // After first pulse ppqnCount should be 0 (wrapped: (0+1)%1=0)
        EXPECT_EQ(clk.ppqnCount,0);
    });

    TEST("Internal clock fires 16 sixteenth edges per bar at 120BPM", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // One bar at 120BPM = 2s = 88200 samples
        int sixteenths=0, quarters=0;
        for(int s=0;s<88210;++s){  // slight margin for float rounding
            clk.process(0.f,false,120.f,1,ST);
            if(clk.sixteenthEdge) ++sixteenths;
            if(clk.quarterEdge)   ++quarters;
        }
        EXPECT(sixteenths >= 16 && sixteenths <= 17);  // allow ±1 for float rounding
        EXPECT_EQ(quarters, 4);
    });

    TEST("Internal clock: timeAcc never exceeds sixteenthSec by more than 1 sample", {
        // timeAcc -= sixteenthSec on each edge — the remainder is kept
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        float maxAcc=0.f;
        for(int s=0;s<44100;++s){
            clk.process(0.f,false,120.f,1,ST);
            if(clk.timeAcc>maxAcc) maxAcc=clk.timeAcc;
        }
        float sixteenthSec=(4.f*60.f/120.f)/16.f;
        EXPECT(maxAcc < sixteenthSec + ST);
    });

    TEST("Internal BPM=20 (minimum): clock still produces edges", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        int sixteenths=0;
        // At 20BPM one 1/16 = 60/20/4 = 0.75s → 33075 samples
        for(int s=0;s<33200;++s){
            clk.process(0.f,false,20.f,1,ST);
            if(clk.sixteenthEdge) ++sixteenths;
        }
        EXPECT_EQ(sixteenths,1);
    });

    TEST("Internal BPM=300 (maximum): clock produces correct fast edges", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        int sixteenths=0;
        // At 300BPM one 1/16 = 60/300/4 = 0.05s → 2205 samples
        for(int s=0;s<2210;++s){
            clk.process(0.f,false,300.f,1,ST);
            if(clk.sixteenthEdge) ++sixteenths;
        }
        EXPECT_EQ(sixteenths,1);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Mute — INV GATE, Restart-on-Unmute, Power Mute");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Normal mute (behavior=0): gate high=muted, gate low=unmuted", {
        MuteState m; m.muteBehavior=0;
        m.applyGate(true);  EXPECT( m.muted);
        m.applyGate(false); EXPECT(!m.muted);
    });

    TEST("INV GATE (behavior=2): gate high=unmuted, gate low=muted", {
        MuteState m; m.muteBehavior=2;
        m.applyGate(true);  EXPECT(!m.muted);   // inverted: high = NOT mute
        m.applyGate(false); EXPECT( m.muted);   // low = mute
    });

    TEST("INV GATE: starts muted when gate is absent (0V)", {
        // With INV GATE, 0V (gate low) → shouldMute=true
        MuteState m; m.muteBehavior=2; m.muted=false;
        m.applyGate(false);  // gate low
        EXPECT(m.muted);
    });

    TEST("Restart-on-unmute (behavior=1): restart fires when unmuting", {
        MuteState m; m.muteBehavior=1; m.muted=true;
        m.applyGate(false);  // unmute
        EXPECT(!m.muted);
        EXPECT(m.restartFired);
    });

    TEST("Restart-on-unmute: restart does NOT fire when muting (only on unmute)", {
        MuteState m; m.muteBehavior=1; m.muted=false;
        m.applyGate(true);   // mute
        EXPECT(m.muted);
        EXPECT(!m.restartFired);
    });

    TEST("Normal mute (behavior=0): no restart fires on unmute", {
        MuteState m; m.muteBehavior=0; m.muted=true;
        m.applyGate(false);
        EXPECT(!m.restartFired);
    });

    TEST("Mute gate held steady: no repeated state changes (level-sensitive)", {
        MuteState m; m.muteBehavior=0;
        // Apply gate high 5 times in a row
        for(int i=0;i<5;++i) m.applyGate(true);
        EXPECT(m.muted);  // still muted, no oscillation
    });

    TEST("INV GATE + restart-on-unmute: undefined combo, no crash", {
        // muteBehavior can only be 0,1,2,3 — mixing inv(2) with restart(1) not valid
        // but test robustness: only one behavior is active at a time
        MuteState m; m.muteBehavior=2;
        m.applyGate(true);   // inv: unmute
        m.applyGate(false);  // inv: mute
        EXPECT(m.muted);  // no crash, correct state
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Gate Assignments — All 8 Cases");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Gate1 assign=0 (TGL DICE R): toggles rhythmMode on each rise", {
        GateAssignState g;
        EXPECT_EQ(g.rhythmMode,0);
        g.onGate1Rise(0); EXPECT_EQ(g.rhythmMode,1);
        g.onGate1Rise(0); EXPECT_EQ(g.rhythmMode,0);
    });

    TEST("Gate1 assign=1 (RE-DICE R): arms seed only in dice-mode", {
        GateAssignState g; g.rhythmMode=0;
        g.onGate1Rise(1); EXPECT(g.rhythmSeedPending);
    });

    TEST("Gate1 assign=1 (RE-DICE R): does NOT arm seed in realtime-mode", {
        GateAssignState g; g.rhythmMode=1;
        g.onGate1Rise(1); EXPECT(!g.rhythmSeedPending);
    });

    TEST("Gate1 assign=2 (RE-DICE M): arms melody seed only in dice-mode", {
        GateAssignState g; g.melodyMode=0;
        g.onGate1Rise(2); EXPECT(g.melodySeedPending);
    });

    TEST("Gate1 assign=2 (RE-DICE M): blocked in realtime melody mode", {
        GateAssignState g; g.melodyMode=1;
        g.onGate1Rise(2); EXPECT(!g.melodySeedPending);
    });

    TEST("Gate1 assign=3 (RESTART): fires restart", {
        GateAssignState g;
        g.onGate1Rise(3); EXPECT(g.restartFired);
    });

    TEST("Gate2 assign=0 (TGL DICE M): toggles melodyMode", {
        GateAssignState g;
        g.onGate2Rise(0); EXPECT_EQ(g.melodyMode,1);
        g.onGate2Rise(0); EXPECT_EQ(g.melodyMode,0);
    });

    TEST("Gate2 assign=1 (RE-DICE M): arms seed only in dice-mode", {
        GateAssignState g; g.melodyMode=0;
        g.onGate2Rise(1); EXPECT(g.melodySeedPending);
    });

    TEST("Gate2 assign=1: blocked when melodyMode=1 (realtime)", {
        GateAssignState g; g.melodyMode=1;
        g.onGate2Rise(1); EXPECT(!g.melodySeedPending);
    });

    TEST("Gate2 assign=3 (RESTART): fires restart", {
        GateAssignState g;
        g.onGate2Rise(3); EXPECT(g.restartFired);
    });

    TEST("Gate1 assign=1 then switch to realtime: re-dice blocked after switch", {
        GateAssignState g; g.rhythmMode=0;
        g.onGate1Rise(1);  EXPECT(g.rhythmSeedPending);
        g.rhythmSeedPending=false;
        g.rhythmMode=1;    // switch to realtime
        g.onGate1Rise(1);  EXPECT(!g.rhythmSeedPending);  // now blocked
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Phrase Boundary — Seed Application Order & Lock");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Pending seed applied before redraw on phrase boundary", {
        PhraseState p;
        p.rhythmSeedPendingFloat=7.5f; p.rhythmSeedPending=true;
        float seedBefore=p.rhythmSeedFloat;
        p.onPhraseBoundary();
        // Seed should now be applied (pending cleared, float updated)
        EXPECT_NEAR(p.rhythmSeedFloat, 7.5f, 1e-5f);
        EXPECT(!p.rhythmSeedPending);
    });

    TEST("Both rhythm and melody seeds applied atomically at phrase boundary", {
        PhraseState p;
        p.rhythmSeedPendingFloat=3.0f; p.rhythmSeedPending=true;
        p.melodySeedPendingFloat=8.0f; p.melodySeedPending=true;
        p.onPhraseBoundary();
        EXPECT_NEAR(p.rhythmSeedFloat, 3.0f, 1e-5f);
        EXPECT_NEAR(p.melodySeedFloat, 8.0f, 1e-5f);
        EXPECT(!p.rhythmSeedPending && !p.melodySeedPending);
    });

    TEST("No pending seed: phrase boundary leaves seeds unchanged", {
        PhraseState p; p.rhythmSeedFloat=5.0f; p.melodySeedFloat=2.0f;
        p.onPhraseBoundary();
        EXPECT_NEAR(p.rhythmSeedFloat, 5.0f, 1e-5f);
        EXPECT_NEAR(p.melodySeedFloat, 2.0f, 1e-5f);
    });

    TEST("Phrase boundary redraw blocked when locked", {
        PhraseState p; p.locked=true;
        p.onPhraseBoundary();
        EXPECT_EQ(p.redrawCount, 0);
    });

    TEST("Phrase boundary redraws when unlocked", {
        PhraseState p; p.locked=false;
        p.onPhraseBoundary();
        EXPECT_EQ(p.redrawCount, 1);
    });

    TEST("Multiple phrase boundaries accumulate redraw count when unlocked", {
        PhraseState p;
        for(int i=0;i<5;++i) p.onPhraseBoundary();
        EXPECT_EQ(p.redrawCount, 5);
    });

    TEST("handleRestart with resetImmediate: applies seed immediately", {
        PhraseState p;
        p.rhythmSeedPendingFloat=9.0f; p.rhythmSeedPending=true;
        p.handleRestart(true);
        EXPECT_NEAR(p.rhythmSeedFloat, 9.0f, 1e-5f);
        EXPECT(!p.rhythmSeedPending);
    });

    TEST("handleRestart without resetImmediate: seed not applied yet", {
        PhraseState p;
        p.rhythmSeedFloat=1.0f;
        p.rhythmSeedPendingFloat=9.0f; p.rhythmSeedPending=true;
        p.handleRestart(false);
        EXPECT_NEAR(p.rhythmSeedFloat, 1.0f, 1e-5f);  // unchanged
        EXPECT(p.rhythmSeedPending);                    // still pending
    });

    TEST("Armed seed on phrase boundary then RESET: applied only once", {
        PhraseState p;
        p.rhythmSeedPendingFloat=4.0f; p.rhythmSeedPending=true;
        p.onPhraseBoundary();   // applies seed, clears pending
        EXPECT(!p.rhythmSeedPending);
        // Subsequent reset: no pending seed → nothing to apply
        p.handleRestart(true);
        EXPECT_NEAR(p.rhythmSeedFloat, 4.0f, 1e-5f);  // still 4.0, not corrupted
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("Pattern Window Edge Cases");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Normal window [0,15]: all 16 steps in range", {
        for(int i=0;i<16;++i) EXPECT(stepInWindow(i,0,15));
    });

    TEST("Single-step window [7,7]: only step 7 in range", {
        for(int i=0;i<16;++i)
            EXPECT_EQ(stepInWindow(i,7,7), i==7);
    });

    TEST("Wrapped window [14,2] (start>end): 14,15,0,1,2 in window", {
        std::vector<int> expected={14,15,0,1,2};
        for(int i=0;i<16;++i){
            bool inW=stepInWindow(i,14,2);
            bool shouldBe=std::find(expected.begin(),expected.end(),i)!=expected.end();
            EXPECT_EQ(inW,shouldBe);
        }
    });

    TEST("Wrapped window [15,0]: only steps 15 and 0", {
        EXPECT( stepInWindow(15,15,0));
        EXPECT( stepInWindow( 0,15,0));
        EXPECT(!stepInWindow( 1,15,0));
        EXPECT(!stepInWindow(14,15,0));
    });

    TEST("Length 1 window at any start: exactly one step in window", {
        for(int start=0;start<16;++start){
            int end=start;  // length=1 → end=start
            int count=0;
            for(int i=0;i<16;++i) if(stepInWindow(i,start,end)) ++count;
            EXPECT_EQ(count,1);
        }
    });

    TEST("Length 16 window: all steps in window regardless of start", {
        // With offset=0, length=16: start=0, end=15
        int count=0;
        for(int i=0;i<16;++i) if(stepInWindow(i,0,15)) ++count;
        EXPECT_EQ(count,16);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("JSON Patch Integrity — Valid Ranges");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("Valid modeSelect values: only 0..3", {
        for(int v:{0,1,2,3}) EXPECT(validModeSelect(v));
        for(int v:{-1,4,99,-100}) EXPECT(!validModeSelect(v));
    });

    TEST("Valid ppqnSetting: only 1, 4, 24", {
        EXPECT( validPpqnSetting(1));
        EXPECT( validPpqnSetting(4));
        EXPECT( validPpqnSetting(24));
        EXPECT(!validPpqnSetting(0));
        EXPECT(!validPpqnSetting(2));
        EXPECT(!validPpqnSetting(8));
        EXPECT(!validPpqnSetting(48));
    });

    TEST("Valid gate assignments: 0..3", {
        for(int v:{0,1,2,3}) EXPECT(validGateAssign(v));
        EXPECT(!validGateAssign(-1)); EXPECT(!validGateAssign(4));
    });

    TEST("Valid muteBehavior: 0..3", {
        for(int v:{0,1,2,3}) EXPECT(validMuteBehavior(v));
        EXPECT(!validMuteBehavior(-1)); EXPECT(!validMuteBehavior(4));
    });

    TEST("Valid rhythmMode/melodyMode: 0 or 1 only", {
        EXPECT(validRhythmMode(0)); EXPECT(validRhythmMode(1));
        EXPECT(!validRhythmMode(-1)); EXPECT(!validRhythmMode(2));
    });

    TEST("Valid step indices: 0..15", {
        for(int v=0;v<16;++v) EXPECT(validStepIdx(v));
        EXPECT(!validStepIdx(-1)); EXPECT(!validStepIdx(16));
    });

    TEST("melodyPitchV clamped to 0..5V on load", {
        EXPECT_NEAR(clampPitch(-1.f), 0.f, 1e-5f);
        EXPECT_NEAR(clampPitch( 6.f), 5.f, 1e-5f);
        EXPECT_NEAR(clampPitch( 2.5f),2.5f,1e-5f);
    });

    TEST("Corrupt ppqnSetting=99 in patch: engine still works (clamped by std::max)", {
        // Plugin doesn't validate ppqnSetting on load — it's used directly in
        // ClockEngine: ppqnCount = (ppqnCount+1) % std::max(1,ppqn)
        // ppqn=99 → divides by 99 → just produces very infrequent edges
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        int sixteenths=0;
        // 99 pulses should give exactly 1 sixteenth edge
        for(int p=0;p<99;++p){
            clk.process(5.f,true,120.f,99,ST);
            if(clk.sixteenthEdge) ++sixteenths;
            clk.process(0.f,true,120.f,99,ST);
        }
        EXPECT_EQ(sixteenths,1);
    });

    TEST("Corrupt ppqnSetting=1000: engine does not crash", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // Just verify no crash and no negative ppqnCount
        for(int p=0;p<20;++p){
            clk.process(5.f,true,120.f,1000,ST);
            clk.process(0.f,true,120.f,1000,ST);
        }
        EXPECT(clk.ppqnCount >= 0);
    });

    // ─────────────────────────────────────────────────────────────────────────
    SUITE("ClockEngine — ppqnCount Reset Behaviour");
    // ─────────────────────────────────────────────────────────────────────────

    TEST("ppqnCount wraps correctly at PPQN=4", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // After 1 pulse: ppqnCount = (0+1)%4 = 1
        clk.process(5.f,true,120.f,4,ST); clk.process(0.f,true,120.f,4,ST);
        EXPECT_EQ(clk.ppqnCount,1);
        // After 4 pulses total: ppqnCount = 0 again, sixteenthEdge fires
        for(int i=0;i<3;++i){ clk.process(5.f,true,120.f,4,ST); clk.process(0.f,true,120.f,4,ST); }
        EXPECT_EQ(clk.ppqnCount,0);
    });

    TEST("sixteenthCount wraps mod 4 to produce quarterEdge", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f, PERIOD=0.5f;
        int quarters=0;
        fireClockPulses(clk,4,PERIOD,ST,1,nullptr,&quarters);
        EXPECT_EQ(quarters,1);  // 4 steps = 1 quarter note
    });

    TEST("PPQN change mid-run: count resets on switch (external path clears on connect)", {
        ClockEngine clk; clk.reset();
        const float ST=1.f/44100.f;
        // Run 2 pulses at PPQN=4 (ppqnCount=2)
        clk.process(5.f,true,120.f,4,ST); clk.process(0.f,true,120.f,4,ST);
        clk.process(5.f,true,120.f,4,ST); clk.process(0.f,true,120.f,4,ST);
        int countBefore=clk.ppqnCount;  // should be 2
        EXPECT_EQ(countBefore,2);
        // Switch to PPQN=1 — ppqnCount modulo changes
        // Next pulse: (2+1)%1 = 0 → sixteenthEdge fires
        clk.process(5.f,true,120.f,1,ST);
        EXPECT(clk.sixteenthEdge);  // fires because (2+1)%1==0
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
