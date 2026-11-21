// test_slew_latch_regression.cpp — CC_PROMPT_slew_latch_regression_START: the slew-latch
// contract. Moving the slew knob (rhythm/melody/qmix — macro OR east; both feed the same
// paramManager getters that populate PatternInput.*Slew) must change the drawn
// probabilities at the next phrase-boundary roll WHETHER OR NOT the ABMix lock is engaged.
//
// ROOT CAUSE (trunk, pinned in the prompt): *SlewLatched was latched ONLY inside
// PatternEngine::latchMix, and MonsoonModeController::updatePatternInput called latchMix
// ONLY when (abR || abM || abQ) — the ABMix LOCK liveness. ABMix locked → latchMix never
// called → *SlewLatched STALE → the phrase-boundary roll re-derived the slewed buffers
// with the OLD slew → moving the knob changed nothing. That violates the code's OWN spec
// (MonsoonSandsManager::remapSigChanged_ comment): slew latches EVERY CONTROL-RATE CYCLE;
// the draw-counter advance at the roll is the re-derive trigger. Slew has NO own Lock
// control (Control enum = Spread/ABMix/… — no Slew) → it is meant to latch UNCONDITIONALLY.
//
// MonsoonModeController.cpp is rig-invisible (it needs Module/paramManager — only the
// plugin build compiles it), so this test drives PatternEngine directly:
//   - controllerLatchCycle() is a HAND-KEPT REPLICA of the ModeController's latch-block
//     call shape (updatePatternInput's if (abR||abM||abQ) block);
//   - the roll is driven via redrawRhythm/Melody/Qmix — exactly what the phrase boundary
//     does: advance the draw counter → recomputeEffective* reads *SlewLatched via
//     slewKnobToR → new r → ZCache miss → re-derive.
// It also pins the CPU contract: a mid-phrase slew move does NOT recompute (the buffers
// hold until the roll) — the roll is the ONLY re-derive trigger.
#include "test_stubs.hpp"
#include "PatternEngine.hpp"
#include <iostream>
#include <cstring>
#include <string>

static int s_pass=0, s_fail=0;
#define TEST(desc,...) do{ \
    bool _ok=true; std::string _msg; \
    try{__VA_ARGS__;}catch(const std::exception& _e){_ok=false;_msg=_e.what();} \
    if(_ok){++s_pass;std::cout<<"  \033[32m✓\033[0m "<<(desc)<<"\n";} \
    else{++s_fail;std::cout<<"  \033[31m✗\033[0m "<<(desc); \
         if(!_msg.empty())std::cout<<" — "<<_msg;std::cout<<"\n";} \
}while(0)
#define EXPECT(e) do{if(!(e))throw std::runtime_error("EXPECT(" #e ") failed");}while(0)

// ── ModeController latch-block replica (hand-kept; sync with MonsoonModeController.cpp) ──
// TRUNK SHAPE (the bug this test runs RED against): the whole latchMix — SLEW INCLUDED —
// sits inside the ABMix-liveness gate:
//     if (abR || abM || abQ)
//         engine.pe.latchMix(rhythmMix, melodyMix, qmixMix,
//                            rhythmSlew, melodySlew, qmixSlew,
//                            /*applyRhythm=*/abR, /*applyMelody=*/abM, /*applyQmix=*/abQ);
// With ABMix locked (abR=abM=abQ=false) NOTHING latches → *SlewLatched stale. Per-stream
// too: a frozen stream (apply false) does not re-latch its slew even when another axis
// is live.
// FIXED SHAPE (what updatePatternInput does after the fix — mirrored here):
//     engine.pe.latchSlew(rhythmSlew, melodySlew, qmixSlew);          // ALWAYS
//     if (abR || abM || abQ)
//         engine.pe.latchMix(rhythmMix, melodyMix, qmixMix,
//                            /*applyRhythm=*/abR, /*applyMelody=*/abM, /*applyQmix=*/abQ);
static void controllerLatchCycle(PatternEngine& pe,
                                 float rMix, float mMix, float qMix,
                                 float rSlew, float mSlew, float qSlew,
                                 bool abR, bool abM, bool abQ) {
    pe.latchSlew(rSlew, mSlew, qSlew);          // ALWAYS — never behind the ABMix gate
    if (abR || abM || abQ)
        pe.latchMix(rMix, mMix, qMix, abR, abM, abQ);
}

// ── Per-stream plumbing ───────────────────────────────────────────────────────
enum Stream { S_RHYTHM, S_MELODY, S_QMIX };
static const char* streamName(Stream s) {
    switch (s) { case S_RHYTHM: return "rhythm"; case S_MELODY: return "melody"; default: return "qmix"; }
}
// Same seeds for every engine in a scenario — determinism between module/reference engines.
static void seedStreams(PatternEngine& pe) {
    pe.seedRhythmPhilox(0.42f); pe.seedBridgeRhythmPhilox(0.42f);
    pe.seedMelodyPhilox(0.17f); pe.seedBridgeMelodyPhilox(0.17f);
    pe.seedQmixPhilox(0.9f);    pe.seedBridgeQmixPhilox(0.9f);
}
static void rollStream(PatternEngine& pe, Stream s, const PatternInput& in) {
    switch (s) {
    case S_RHYTHM: pe.redrawRhythm(in); break;
    case S_MELODY: pe.redrawMelody(in); break;
    default:       pe.redrawQmix(in);   break;
    }
}
static float slewLatched(const PatternEngine& pe, Stream s) {
    switch (s) {
    case S_RHYTHM: return pe.rhythmSlewLatched;
    case S_MELODY: return pe.melodySlewLatched;
    default:       return pe.qmixSlewLatched;
    }
}
// Voice-0 effective (slewed) buffers for one stream — the drawn probabilities the roll
// re-derives. Compared BITWISE: the re-derive is deterministic in (seed, counter, mix, slew).
struct StreamSnap { float rhythm[16], variation[16], legato[16], accent[16],
                           melody[16], octave[16], qmix[16]; };
static void snapStream(const PatternEngine& pe, Stream s, StreamSnap& o) {
    switch (s) {
    case S_RHYTHM:
        for (int i=0;i<16;++i) { o.rhythm[i]=pe.slewedRhythm[0][i]; o.variation[i]=pe.slewedVariation[0][i];
                                 o.legato[i]=pe.slewedLegato[0][i];  o.accent[i]=pe.slewedAccent[0][i]; }
        break;
    case S_MELODY:
        for (int i=0;i<16;++i) { o.melody[i]=pe.slewedMelody[0][i]; o.octave[i]=pe.slewedOctave[0][i]; }
        break;
    default:
        for (int i=0;i<16;++i) { o.qmix[i]=pe.slewedQmix[0][i]; }
        break;
    }
}
static bool snapEq(const PatternEngine& pe, Stream s, const StreamSnap& o) {
    switch (s) {
    case S_RHYTHM:
        for (int i=0;i<16;++i) {
            if (std::memcmp(&pe.slewedRhythm[0][i],   &o.rhythm[i],   sizeof(float))) return false;
            if (std::memcmp(&pe.slewedVariation[0][i],&o.variation[i],sizeof(float))) return false;
            if (std::memcmp(&pe.slewedLegato[0][i],   &o.legato[i],   sizeof(float))) return false;
            if (std::memcmp(&pe.slewedAccent[0][i],   &o.accent[i],   sizeof(float))) return false;
        }
        return true;
    case S_MELODY:
        for (int i=0;i<16;++i) {
            if (std::memcmp(&pe.slewedMelody[0][i], &o.melody[i], sizeof(float))) return false;
            if (std::memcmp(&pe.slewedOctave[0][i], &o.octave[i], sizeof(float))) return false;
        }
        return true;
    default:
        for (int i=0;i<16;++i)
            if (std::memcmp(&pe.slewedQmix[0][i], &o.qmix[i], sizeof(float))) return false;
        return true;
    }
}
static bool snapAnyDiff(const StreamSnap& a, const StreamSnap& b, Stream s) {
    StreamSnap dummy{}; (void)dummy;   // silence unused-field concerns; fields read via s below
    switch (s) {
    case S_RHYTHM:
        for (int i=0;i<16;++i) {
            if (std::memcmp(&a.rhythm[i],   &b.rhythm[i],   sizeof(float))) return true;
            if (std::memcmp(&a.variation[i],&b.variation[i],sizeof(float))) return true;
            if (std::memcmp(&a.legato[i],   &b.legato[i],   sizeof(float))) return true;
            if (std::memcmp(&a.accent[i],   &b.accent[i],   sizeof(float))) return true;
        }
        return false;
    case S_MELODY:
        for (int i=0;i<16;++i) {
            if (std::memcmp(&a.melody[i], &b.melody[i], sizeof(float))) return true;
            if (std::memcmp(&a.octave[i], &b.octave[i], sizeof(float))) return true;
        }
        return false;
    default:
        for (int i=0;i<16;++i)
            if (std::memcmp(&a.qmix[i], &b.qmix[i], sizeof(float))) return true;
        return false;
    }
}

// ── The regression scenario, one stream ───────────────────────────────────────
// Phrase 1 runs UNLOCKED at slewA (both latches live), then ABMix locks with the given
// per-axis liveness while the user moves the slew knob to slewB; locked control cycles
// run; the phrase-boundary roll re-derives. Asserts:
//   (1) pre-roll buffers unchanged — CPU contract: slew-only moves do NOT recompute
//       mid-phrase (the roll is the only re-derive trigger);
//   (2) *SlewLatched == slewB — the latch contract: slew latches under the ABMix lock;
//   (3) post-roll buffers == the reference that latched slewB cleanly — the drawn
//       probabilities CHANGED with the knob at the phrase boundary.
static void slewMoveUnderABMix(Stream s, bool abR, bool abM, bool abQ) {
    const float slewA = 0.f;    // r = 0 (independent/raw)
    const float slewB = 0.9f;   // strongly correlated — audibly different probabilities
    // The regression state: module locked, dice opted live (the roll still fires), ABMix
    // axes frozen per the flags. This is exactly what the ModeController computes from
    // LockManager::liveNow(Control::ABMix, …) + the scopeLiveMask bits.
    PatternInput in;
    in.locked = true; in.diceLiveR = in.diceLiveM = in.diceLiveQ = true;

    // The module under test.
    PatternEngine pe; seedStreams(pe);
    controllerLatchCycle(pe, 0.f,0.f,0.f, slewA,slewA,slewA, true,true,true);  // unlocked cycle
    rollStream(pe, s, in);                                                     // roll #1 (ctr 0)
    StreamSnap preLock; snapStream(pe, s, preLock);
    for (int k=0;k<3;++k)                                                      // locked control cycles,
        controllerLatchCycle(pe, 0.f,0.f,0.f, slewB,slewB,slewB, abR,abM,abQ); // knob now at slewB
    // (1) CPU contract: the mid-phrase slew move did NOT recompute.
    if (!snapEq(pe, s, preLock))
        throw std::runtime_error(std::string(streamName(s)) + ": mid-phrase slew move recomputed (CPU contract)");
    rollStream(pe, s, in);                                                     // roll #2 (ctr 1) — the re-derive

    // Reference: same seeds, slewB latched throughout, same two rolls.
    PatternEngine ref; seedStreams(ref);
    controllerLatchCycle(ref, 0.f,0.f,0.f, slewB,slewB,slewB, true,true,true);
    rollStream(ref, s, in); rollStream(ref, s, in);
    // Old-slew reference (non-vacuity guard: slew must MATTER at this seed/counter).
    PatternEngine refOld; seedStreams(refOld);
    controllerLatchCycle(refOld, 0.f,0.f,0.f, slewA,slewA,slewA, true,true,true);
    rollStream(refOld, s, in); rollStream(refOld, s, in);
    StreamSnap refNewSnap, refOldSnap; snapStream(ref, s, refNewSnap); snapStream(refOld, s, refOldSnap);
    if (!snapAnyDiff(refNewSnap, refOldSnap, s))
        throw std::runtime_error(std::string(streamName(s)) + ": slewA/slewB references identical — vacuous scenario");

    // (2) the latch contract: slew latched under the ABMix lock.
    if (slewLatched(pe, s) != slewB)
        throw std::runtime_error(std::string(streamName(s)) + "SlewLatched stale under the ABMix lock");
    // (3) the roll re-derived with the NEW slew — the probabilities changed.
    if (!snapEq(pe, s, refNewSnap))
        throw std::runtime_error(std::string(streamName(s)) + ": rolled buffers did not follow the slew move");
}

// ── Mix-latch semantics under the ABMix lock (the no-regression pin) ──────────
// A frozen axis holds its *MixLatched, a live axis follows its mix knob — AND every
// axis's *SlewLatched follows the slew knob regardless (slew is NOT an A/B-mix property).
static void mixLatchSemantics(bool abR, bool abM, bool abQ) {
    PatternEngine pe; seedStreams(pe);
    controllerLatchCycle(pe, 0.25f,0.30f,0.35f, 0.f,0.f,0.f, true,true,true);   // unlocked: latch all
    for (int k=0;k<3;++k)                                                       // locked cycles, knobs moved
        controllerLatchCycle(pe, 0.70f,0.75f,0.80f, 0.9f,0.9f,0.9f, abR,abM,abQ);
    EXPECT(pe.rhythmMixLatched == (abR ? 0.70f : 0.25f));
    EXPECT(pe.melodyMixLatched == (abM ? 0.75f : 0.30f));
    EXPECT(pe.qmixMixLatched   == (abQ ? 0.80f : 0.35f));
    EXPECT(pe.rhythmSlewLatched == 0.9f);
    EXPECT(pe.melodySlewLatched == 0.9f);
    EXPECT(pe.qmixSlewLatched   == 0.9f);
}

int main(){
    std::cout<<"\033[1mSlew latch regression (ABMix-lock decoupling) Tests\033[0m\n"<<std::string(50,'=')<<"\n";

    // (a) THE REGRESSION: ABMix LOCKED (all axes frozen), slew moved → the next roll's
    // probabilities change. RED on trunk: nothing latched under lock → *SlewLatched stale.
    TEST("ABMix locked (all axes): slew move changes rolled probabilities — rhythm",
         { slewMoveUnderABMix(S_RHYTHM, false,false,false); });
    TEST("ABMix locked (all axes): slew move changes rolled probabilities — melody",
         { slewMoveUnderABMix(S_MELODY, false,false,false); });
    TEST("ABMix locked (all axes): slew move changes rolled probabilities — qmix",
         { slewMoveUnderABMix(S_QMIX,   false,false,false); });

    // (a2) Per-stream: rhythm axis live, melody+qmix axes frozen — the FROZEN streams'
    // slew must still latch (slew ≠ an A/B-mix property; it has no own lock control).
    // RED on trunk for melody/qmix: latchMix ran (abR true) but applyMelody/applyQmix
    // =false skipped their slew latch.
    TEST("ABMix partial (rhythm live only): frozen melody slew still latches at the roll",
         { slewMoveUnderABMix(S_MELODY, true,false,false); });
    TEST("ABMix partial (rhythm live only): frozen qmix slew still latches at the roll",
         { slewMoveUnderABMix(S_QMIX,   true,false,false); });

    // (b) no-regression: ABMix live → slew latches and the roll follows (worked on trunk too).
    TEST("ABMix live: slew move changes rolled probabilities — all 3 streams", {
        slewMoveUnderABMix(S_RHYTHM, true,true,true);
        slewMoveUnderABMix(S_MELODY, true,true,true);
        slewMoveUnderABMix(S_QMIX,   true,true,true);
    });

    // (c) mix-latch semantics: frozen axes hold their mix, live axes follow, ALL slews
    // follow (the split's per-stream contract: frozen MIX, live SLEW).
    TEST("ABMix locked (all axes): mix frozen, slew follows — all streams",
         { mixLatchSemantics(false,false,false); });
    TEST("ABMix partial (rhythm live): per-stream mix gating, all slews follow",
         { mixLatchSemantics(true,false,false); });

    std::cout<<"\n"<<s_pass<<" passed, "<<s_fail<<" failed\n";
    return s_fail ? 1 : 0;
}
