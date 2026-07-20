// test_StepGate.cpp — the STEP GATE + STEP LEGATO (SLEG) invariants, scope-verified on
// the build (2026-07) and locked here against regression.
//
// STEP is a MIRROR GateState (gsStep) that receives the SAME ticks/pulses as the fused
// gate (gs) but RE-STRIKES every continuation instead of fusing it. The properties that
// must hold, independent of the surrounding engine:
//   1. On a fresh/isolated note, STEP == GATE (nothing to un-fuse).
//   2. Across a legato/tie chain, GATE stays HIGH once; STEP falls+rises at every
//      sub-note boundary (one strike per played step).
//   3. A tie re-strikes at the SAME pitch (STEP edge, CV flat).
//   4. Rest closes both.
//   5. SLEG = STEP masked by slur membership: silent on isolated notes, high on every
//      note of a slur (lead + all continuations).
//
// This is a focused replica (cf. test_fractional_tail): it models the gate arming/tick/
// process and the articulation-vs-mirror decision, which is exactly where STEP differs
// from GATE. The real gsStep is driven by SequencerEngine at the sites this replicates;
// test_per_voice_articulation (2440 assertions) covers that the engine reaches them.
#include <cstdio>
#include <vector>
#include <string>

static int passed = 0, failed = 0;
static void chk(bool c, const std::string& m) {
    if (c) { ++passed; } else { ++failed; std::printf("  FAIL: %s\n", m.c_str()); }
}

// ── Minimal gate: arm on trigger/extend, count down in process, edge on re-strike ──────
// Models the retrigger PULSE the real GateState carries: a fresh strike forces a brief
// gate-low notch (retrigPulse) even when the gate was already high, so downstream envelopes
// re-trigger. This is why gsStep.process advances a pulse and why a re-struck legato chain
// shows separate edges while the fused gate stays continuously high.
struct Gate {
    float gateSecRemain = -1.f, curStepSec = 0.f;
    float retrigRemain = 0.f;   // >0 = in the low notch of a fresh strike
    bool  gateHeld = false;
    float pitchV = 0.f;
    int   strikes = 0;
    bool  prevHigh = false;

    void tick(float ss) { if (ss > 0) curStepSec = ss; }
    void arm(float durSteps) { gateSecRemain = (curStepSec > 0) ? durSteps * curStepSec : -1.f; }
    void retrig() { retrigRemain = 1e-3f; }   // 1ms notch, like PulseGenerator's default

    // FUSED path (gs): a legato/tie continuation SLIDES/EXTENDS — NO retrigger notch.
    void slide(float durSteps, float pv) { pitchV = pv; gateHeld = true; arm(durSteps); }
    void extend(float durSteps)          { gateHeld = true; arm(durSteps); }
    // MIRROR path (gsStep): every continuation RE-STRIKES — fresh edge (notch), own length.
    void trigger(float durSteps, float pv) { pitchV = pv; gateHeld = true; arm(durSteps); retrig(); }
    void rest() { gateHeld = false; gateSecRemain = -1.f; retrigRemain = 0.f; }

    // effective gate output this sample: low during the retrigger notch
    bool out() const { return gateHeld && retrigRemain <= 0.f; }

    void process(float dt) {
        if (gateSecRemain >= 0.f) { gateSecRemain -= dt; if (gateSecRemain <= 0.f) { gateSecRemain = -1.f; gateHeld = false; } }
        if (retrigRemain > 0.f) retrigRemain -= dt;
        bool high = out();
        if (high && !prevHigh) ++strikes;
        prevHigh = high;
    }
};

// A played step in a phrase the test drives through both gates in lockstep.
enum class Art { Fresh, Legato, Tie, Rest };
struct Step { Art art; float dur; float pitch; bool slurMember; };

// Drive gs (fused) and gsStep (mirror) through a phrase; report per-stream gate-high
// length (in steps) and strike counts, plus SLEG-high length.
struct Result { float gateHigh=0, stepHigh=0, slegHigh=0; int gateStrikes=0, stepStrikes=0; };

static Result run(const std::vector<Step>& phrase) {
    const float ss = 0.125f, SR = 48000.f, dt = 1.f / SR;
    Gate gs, step; gs.tick(ss); step.tick(ss);
    Result r;
    bool firstOfChain = true;
    for (const Step& s : phrase) {
        // Arm both gates for this step per articulation.
        switch (s.art) {
            case Art::Fresh:
                gs.trigger(s.dur, s.pitch);   step.trigger(s.dur, s.pitch); firstOfChain = false; break;
            case Art::Legato:
                gs.slide(s.dur, s.pitch);     step.trigger(s.dur, s.pitch); break;   // gs fuses, step re-strikes
            case Art::Tie:
                gs.extend(s.dur);             step.trigger(s.dur, gs.pitchV); break; // same pitch, step edge
            case Art::Rest:
                gs.rest();                    step.rest(); firstOfChain = true; break;
        }
        // Play the step out (one step-worth of samples).
        for (int n = 0; n < (int)(ss * SR); ++n) {
            // length uses gateHeld (envelope sustain); the retrigger notch is a sub-ms
            // edge artifact, not a gap in the note — so it must not shorten the measured
            // high-length. Edges (strikes) are counted inside process() off out().
            bool gHi = gs.gateHeld, sHi = step.gateHeld;
            gs.process(dt); step.process(dt);
            if (gHi) r.gateHigh += dt;
            if (sHi) r.stepHigh += dt;
            if (sHi && s.slurMember) r.slegHigh += dt;   // SLEG = STEP masked by membership
        }
        step.tick(ss); gs.tick(ss);
    }
    r.gateStrikes = gs.strikes; r.stepStrikes = step.strikes;
    (void)firstOfChain;
    return r;
}

#define NEAR(a,b) (((a)-(b) < 0.03f) && ((b)-(a) < 0.03f))

int main() {
    const float S = 0.125f;   // one step in seconds, for length↔steps conversion

    // 1. Isolated notes: STEP == GATE, strike for strike.
    {
        auto r = run({ {Art::Fresh,1,1.0f,false}, {Art::Rest,1,0,false},
                       {Art::Fresh,1,2.0f,false}, {Art::Rest,1,0,false} });
        chk(NEAR(r.gateHigh, r.stepHigh), "isolated: STEP high-length == GATE");
        chk(r.gateStrikes == 2 && r.stepStrikes == 2, "isolated: 2 strikes each");
        chk(r.slegHigh == 0.f, "isolated: SLEG silent");
    }

    // 2. Legato chain: GATE one long high, STEP three edges of one step each.
    {
        // 1/4 lead (4 steps) then two 1/16 legato continuations — but as a CHAIN the fused
        // gate stays high across all three; STEP re-strikes each.
        auto r = run({ {Art::Fresh,1,1.0f,true}, {Art::Legato,1,2.0f,true}, {Art::Legato,1,3.0f,true},
                       {Art::Rest,1,0,false} });
        chk(r.gateStrikes == 1, "legato: GATE strikes once for the whole chain");
        chk(r.stepStrikes == 3, "legato: STEP strikes 3x (one per played step)");
        chk(NEAR(r.gateHigh, 3.f * S), "legato: GATE high across all 3 steps");
        chk(NEAR(r.slegHigh, r.stepHigh), "legato: SLEG == STEP (all members)");
        chk(r.slegHigh > 0.f, "legato: SLEG fires inside the slur");
    }

    // 3. Tie re-strikes at the SAME pitch: STEP edges, pitch constant.
    {
        const float dt = 1.f / 48000.f;
        Gate gs, step; gs.tick(S); step.tick(S);
        gs.trigger(1, 5.0f); step.trigger(1, 5.0f);
        for (int n = 0; n < (int)(S * 48000.f); ++n) { gs.process(dt); step.process(dt); }
        float pitchBefore = step.pitchV;
        // tie: gs.extend (pitch unchanged, no notch), step.trigger(gs.pitchV) (re-strike)
        gs.extend(1); step.trigger(1, gs.pitchV);
        for (int n = 0; n < (int)(S * 48000.f); ++n) { gs.process(dt); step.process(dt); }
        chk(step.pitchV == pitchBefore, "tie: STEP re-strike keeps the held pitch");
        chk(step.strikes == 2, "tie: STEP shows a second strike (re-articulation)");
        chk(gs.strikes == 1, "tie: GATE does not re-strike (fused)");
    }

    // 4. Rest closes both.
    {
        Gate gs, step; gs.tick(S); step.tick(S);
        gs.trigger(4, 1.0f); step.trigger(4, 1.0f);
        gs.rest(); step.rest();
        chk(!gs.gateHeld && !step.gateHeld, "rest: both gates closed");
    }

    // 5. SLEG is a strict subset — mixed phrase: isolated note then a slur.
    {
        auto r = run({ {Art::Fresh,1,1.0f,false},                       // isolated: not a member
                       {Art::Rest,1,0,false},
                       {Art::Fresh,1,2.0f,true}, {Art::Legato,1,3.0f,true} });  // slur: members
        // SLEG high only over the 2 slur steps; STEP high over 3 played steps.
        chk(NEAR(r.slegHigh, 2.f * S), "SLEG high only over slur members (2 steps)");
        chk(NEAR(r.stepHigh, 3.f * S), "STEP high over all played steps (3)");
        chk(r.slegHigh < r.stepHigh, "SLEG is a strict subset of STEP");
    }

    // 6. Degenerate: an all-legato phrase still re-strikes per step on STEP while GATE holds.
    {
        std::vector<Step> chain = { {Art::Fresh,2,1.0f,true} };
        for (int i = 0; i < 5; ++i) chain.push_back({Art::Legato,2,(float)(i+2),true});
        auto r = run(chain);
        chk(r.gateStrikes == 1, "long chain: GATE one strike");
        chk(r.stepStrikes == 6, "long chain: STEP six strikes");
    }

    std::printf(failed ? "\n%d passed, %d FAILED\n" : "\n%d passed, 0 failed\n", passed, failed);
    return failed ? 1 : 0;
}
