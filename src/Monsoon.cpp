// src/Monsoon.cpp
// Monsoon - full implementation with clean Mode A stepping logic
// C++11 compatible
//
// Patched: separate Philox streams for melody & rhythm (counter-based, stateless),
// SEED CV input/output, RESET input, deferred reseed at phrase boundary, JSON serialization,
// two-part 64-bit seeding, widget ports for RESET/SEED, and preserved behavior.

#include <rack.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstring>
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
  #include <pmmintrin.h>   // _MM_SET_DENORMALS_ZERO_MODE (SSE3)
  #include <xmmintrin.h>   // _MM_SET_FLUSH_ZERO_MODE (SSE)
#endif

//#include "MonsoonSandsExpander.hpp"
#include "MonsoonInterchangeExpander.hpp"
#include "MonsoonStraitsExpander.hpp"
#include "MonsoonCausewayPolyExpander.hpp"
#include "MonsoonChangiExpander.hpp"
#include "MonsoonShophouseExpander.hpp"
// West retired (Straits redesign): #include "MonsoonStraitWestExpander.hpp"
//#include "MonsoonStraitsSands.hpp"            // NEW (Macro): global DNA controls
//#include "MonsoonDeepStraitsSands.hpp"        // NEW (Deep): per-voice DNA controls
#include "StraitsEastSandsVisual.hpp"         // Visual DNA editor (East)
//#include "StraitsWestSandsVisual.hpp"         // Visual DNA editor (West)
#include "StraitsSandsMacroVisual.hpp"        // Visual DNA editor (Macro)
#include "MonsoonWidget.hpp"
#include "Monsoon.hpp"
#include "dsp/managers/MonsoonConfigurator.hpp"
#include "dsp/engines/PatternEngine.hpp"
#include "dsp/gates/GateState.hpp"

using namespace rack;
using namespace MonsoonIds;

Plugin* pluginInstance = nullptr;

void Monsoon::onSampleRateChange(const SampleRateChangeEvent& e) {
    lightDivider.setDivision(std::max(1, (int)std::round(e.sampleRate / 90.f)));
    controlDivider.setDivision(std::max(1, (int)std::round(e.sampleRate / 1500.f)));
}

Monsoon::Monsoon() {
        MonsoonConfigurator::setup(this);

        // Seed RNGs with a random value — safe to call here (uses rack::random, not inputs[])
        rhythmSeedFloat = rack::random::uniform() * 10.f;
        melodySeedFloat = rack::random::uniform() * 10.f;
        engine.pe.seedRhythmPhilox(rhythmSeedFloat);
        engine.pe.seedMelodyPhilox(melodySeedFloat);
        rhythmSeedPendingFloat = rhythmSeedFloat;
        melodySeedPendingFloat = melodySeedFloat;

        // Initialize managers
        scaleManager = std::unique_ptr<ScaleManager>(new ScaleManager(this));
        paramManager = std::unique_ptr<ParameterManager>(new ParameterManager(this, &expanderManager.cachedScaleExpander, &expanderManager.cachedPolyVoiceExpander));
        modeController = std::unique_ptr<ModeController>(new ModeController(this, engine, clock, *paramManager));
        uiManager = std::unique_ptr<UIManager>(new UIManager(this, lightDivider));
        timingController = std::unique_ptr<TimingController>(new TimingController(this));
        cvRouter = std::unique_ptr<CVRouter>(new CVRouter());
        outputGenerator = std::unique_ptr<OutputGenerator>(new OutputGenerator());

        // Initialize dividers
        onSampleRateChange({APP->engine->getSampleRate(), APP->engine->getSampleRate()});

        initialize();
    }

void Monsoon::updateExpanderPointers() {
    expanderManager.update(this);
}

  void Monsoon::initialize(){
        cv1Mode = 0;
        cv2Mode = 0;
        gate1Assign = 0;
        gate2Assign = 1;
        invertMuteLogic = false;
        restartOnUnmute = false;
        reseedOnRoll = false;
        reseedOnRestart = false;
        lastModeSelect = -1;
        
        if (scaleManager) {
            scaleManager->reset();
        }

        engine.reset();

        bpm = 120.f;
        clock.reset();
        prevExtGate = false;
        currentPitchV = 0.f;
        melodySeedCached = false;
        rhythmSeedCached = false;
        updateExpanderPointers();
  }

    // --- serialization ---
    json_t* Monsoon::dataToJson() {
        return PersistenceManager::toJson(this);
    }
  
    void Monsoon::dataFromJson(json_t* root) {
        PersistenceManager::fromJson(this, root);
    // Finalize state. Seeding re-keys Philox and ZEROES the draw counter — so snapshot
    // the counter the persistence layer restored, seed, then put it back, then regenerate
    // candidate B at that exact stream position (Option 3: A restored, B regenerated).
    int64_t savedR = engine.pe.rhythmDrawCtr, savedM = engine.pe.melodyDrawCtr;
    engine.pe.seedRhythmPhilox(rhythmSeedFloat);
    engine.pe.seedMelodyPhilox(melodySeedFloat);
    if (pendingRegenB) {
        engine.pe.rhythmDrawCtr = savedR;
        engine.pe.melodyDrawCtr = savedM;
        engine.pe.regenerateRhythmB();
        engine.pe.regenerateMelodyB();
        pendingRegenB = false;
    }
    // scaleManager is guaranteed to be valid after construction
    scaleManager->updateScaleMask();
    }

//return semitone parameter value with CV input added (if connected)
    float Monsoon::getSemitoneParam(int sem) {
        return (scaleManager && paramManager) ? scaleManager->getSemitoneWeight(sem, *paramManager) : 0.f;
    }

    //return octave parameter value with CV input added (if connected)
    // Now delegated to ParameterManager - see wrapper functions below

// ── Parameter getters (delegated to ParameterManager) ──────────────────────
// These wrapper functions maintain backward compatibility while delegating
// to the centralized ParameterManager.

float Monsoon::getNoteValueParam()  { return paramManager->getNoteValue(); }
float Monsoon::getVariationParam()  { return paramManager->getVariation(); }
float Monsoon::getLegatoParam()     { return paramManager->getLegato(); }
float Monsoon::getRestParam()       { return paramManager->getRest(); }
float Monsoon::getAccentParam()     { return paramManager->getAccent(); }

float Monsoon::getOctaveLoParam()   { return paramManager->getOctaveLo(); }
float Monsoon::getOctaveHiParam()   { return paramManager->getOctaveHi(); }

float Monsoon::getPolyRestParam(int voiceIdx) {
    return paramManager->getPolyRest(voiceIdx);
}

// ── Effective per-voice poly rest/accent — SINGLE SOURCE OF TRUTH ────────────
// Every consumer (the engine's per-voice decision, the Straits mod arcs, and any future
// reader) pulls the effective value from HERE. There are no cached "effective" copies to
// keep in sync — that push-to-multiple-copies pattern caused three separate drift/clobber
// bugs (scale-lock faders, dice-scale-gate, Causeway CV). Effective = base knob (on Straits,
// via paramManager) + Causeway per-voice CV × (per-voice att + global att), clamped 0..1.
// Base-only accessors give the arcs their "set" reference without a second copy.
float Monsoon::getBasePolyRest(int voiceIdx) {
    return paramManager ? paramManager->getPolyRest(voiceIdx) : 0.f;
}
float Monsoon::getBasePolyAccent(int voiceIdx) {
    return paramManager ? paramManager->getPolyAccent(voiceIdx) : 0.f;
}
float Monsoon::getEffectivePolyRest(int voiceIdx) {
    float base = getBasePolyRest(voiceIdx);
    auto* cway = expanderManager.cachedCausewayPolyExpander;
    if (cway && voiceIdx >= 0 && voiceIdx < 15) {
        const int ch = voiceIdx + 1;   // poly voice → poly-cable channel (ch0 = mono)
        auto& in = cway->inputs[POLY_REST_CV_INPUT];
        if (in.isConnected() && ch < in.getChannels()) {
            float att = cway->params[POLY_REST_MOD_ATT_1 + voiceIdx].getValue()
                      + cway->params[POLY_REST_MOD_ATT_GLOBAL].getValue();
            base += in.getVoltage(ch) * att * 0.1f;
        }
    }
    return math::clamp(base, 0.f, 1.f);
}
float Monsoon::getEffectivePolyAccent(int voiceIdx) {
    float base = getBasePolyAccent(voiceIdx);
    auto* cway = expanderManager.cachedCausewayPolyExpander;
    if (cway && voiceIdx >= 0 && voiceIdx < 15) {
        const int ch = voiceIdx + 1;
        auto& in = cway->inputs[POLY_ACCENT_CV_INPUT];
        if (in.isConnected() && ch < in.getChannels()) {
            float att = cway->params[POLY_ACCENT_MOD_ATT_1 + voiceIdx].getValue()
                      + cway->params[POLY_ACCENT_MOD_ATT_GLOBAL].getValue();
            base += in.getVoltage(ch) * att * 0.1f;
        }
    }
    return math::clamp(base, 0.f, 1.f);
}

// --- switch melody/rhythm mode (dice/realtime), caching/restoring state as needed ---    
void Monsoon::switchMelodyMode() { engine.pe.switchMelodyMode(stepIndex, lastStepIndex); }

// --- switch melody/rhythm mode (dice/realtime), caching/restoring state as needed ---
void Monsoon::switchRhythmMode() { engine.pe.switchRhythmMode(stepIndex, lastStepIndex); }


// pick a semitone (0..11) using weighted random, or -1 if no semitone is selected
// uses melody RNG
// returns -1 if no semitone is selected (all weights zero)
int Monsoon::pickSemitoneWeighted() {
    float w[12];
    for(int i=0; i<12; ++i) w[i] = getSemitoneParam(i);
    int idx = engine.getMelodyStep();
    return engine.pe.pickSemitone(w, melodyRandom[idx]);
}

// generate a pitch V in 1V/oct format, returnh semitone via out parameter
//applies octave range and transpose
// uses melody RNG
// returns 0V and -1 semitone if no semitone is selected
// (should be rare, only if all semitone weights are zero)
float Monsoon::genPitchV(int& outSemitone) {
    return engine.pe.genPitchLive(outSemitone, modeController->currentPatternInput, melodyRandom[engine.getMelodyStep()], octaveRandom[engine.getOctaveStep()]);
}

    // allowed note lengths depend on noteVariationMask - note we defined NoteLength enum and allowednoteLengths function
    //so this could be changed to use that
    // (bit0: 1/8T, bit1: 1/16T, bit2: 1/32 & 1/32T)
    // e.g. mask=0b101 allows 1/8T, 1/4, 1/2, 1, 2, 4, 1/32, 1/32T
    //      mask=0b010 allows 1/16T, 1/4, 1/2, 1, 2, 4
    //      mask=0b000 allows only 1/4, 1/2, 1, 2, 4
    //      mask=0b111 allows all note lengths

// Convert semitone (0..11) to volts (1V/oct)
// 12 semitones per octave
float Monsoon::semitoneToVolts(int semitone) {
    return semitone / 12.f;
}

    // regenerate rhythm pattern (uses Philox rhythm stream) — skipped when locked
    // Build a PatternInput snapshot from current params/CV state
    // This method is no longer needed as PatternInput is cached in ModeController
    // PatternInput Monsoon::makePatternInput() { ... }

    void Monsoon::redrawRhythmPattern() { engine.pe.redrawRhythm(modeController->currentPatternInput); }
    void  Monsoon::redrawMelodyPattern() { engine.pe.redrawMelody(modeController->currentPatternInput); }

    void Monsoon::rotateRhythm(int steps) {
        engine.pe.rotateRhythm(steps);
        engine.pe.refreshPatternCache(modeController->currentPatternInput);
    }

    void Monsoon::rotateRhythmPattern(int steps) {
        engine.pe.rotateRhythmPattern(steps);
        engine.pe.refreshPatternCache(modeController->currentPatternInput);
    }
    
    void Monsoon::rotateVariation(int steps) {
        engine.pe.rotateVariation(steps);
        engine.pe.refreshPatternCache(modeController->currentPatternInput);
    }

    void Monsoon::rotateLegato(int steps) {
        engine.pe.rotateLegato(steps);
        engine.pe.refreshPatternCache(modeController->currentPatternInput);
    }

    void Monsoon::rotateMelody(int steps) {
        engine.pe.rotateMelody(steps);
        engine.pe.refreshPatternCache(modeController->currentPatternInput);
    }

    void Monsoon::rotateMelodyPattern(int steps) {
        engine.pe.rotateMelodyPattern(steps);
        engine.pe.refreshPatternCache(modeController->currentPatternInput);
    }
    
    void Monsoon::rotateOctave(int steps) {
        engine.pe.rotateOctave(steps);
        engine.pe.refreshPatternCache(modeController->currentPatternInput);
    }

    void Monsoon::rebuildSemiCache_() {
        float weights[12];
        for (int i = 0; i < 12; ++i) weights[i] = scaleManager->getSemitoneWeight(i, *paramManager);
        engine.rebuildScaleCache(weights);
    }

    float Monsoon::quantizeToScale(float vIn) { return engine.quantize(vIn); }

    // handle manual restart: place index so next increment lands on startStep, optionally redraw realtime patterns
    // If there are pending seeds and resetImmediate==true, apply them immediately (for RESET triggered reseed)
    void Monsoon::handleRestart(bool manual, bool resetImmediate) {
        stepIndex = (startStep - 1 + 16) % 16;
        engine.totalStepsElapsed = 0; // Sync polymeters to "Beat 1" on hard reset
        engine.gs.reset();          // clears gate, hold, pitch, pulse, semiPlayRemain
        prevExtGate = false;

        if (!locked) {
            if (reseedOnRestart) {
                // Only use the SEED-CV (reproducible, A=B) path when the SEED
                // input is actually present. Unpatched → internal entropy via the
                // morph-preserving reseed-roll path (no A=B collapse).
                if (inputs[SEED_INPUT].isConnected()) {
                    engine.pe.setPendingRhythmSeed(sampleSeedFromSource());
                    engine.pe.setPendingMelodySeed(sampleSeedFromSource());
                } else {
                    engine.pe.setPendingRhythmReseedRoll(0.f, /*full=*/true);
                    engine.pe.setPendingMelodyReseedRoll(0.f, /*full=*/true);
                }
            }
            if (resetImmediate) {
                engine.pe.applyPendingSeedsAndRedraw(modeController->currentPatternInput);
            }
        }
        resetArmed = false;
    }

    // Helper to sample a seed value (float 0..10) from either SEED input (if connected) or internal RNG.
    // Helper to sample a seed value (float 0..10) from either SEED input (if connected) or internal RNG.
    // The action of sampling is performed when user presses DICE (or dice is triggered via gate assignment).
    float Monsoon::sampleSeedFromSource() {
        // If SEED_INPUT is connected, use it (clamped) as sample-and-hold.
        if (inputs[SEED_INPUT].isConnected()) {
            float v = inputs[SEED_INPUT].getVoltage();
            v = clampv<float>(v, 0.f, 10.f);
            return v;
        }
        // Otherwise fall back to internal RNG. 
        float u = rack::random::uniform(); //  default internal RNG
        // scale to 0..10
        return (float)(u * 10.0);
    }

    // A MAIN dice gesture. Whether it RESEEDS is governed solely by the
    // "Reseed on roll" option — NOT by the SEED cable. The SEED cable only
    // determines the seed SOURCE when we do reseed (CV if patched, else full
    // 64-bit internal entropy). Neither source is privileged.
    //   reseedOnRoll OFF → plain roll (advance stream, no reseed), cable or not.
    //   reseedOnRoll ON  → reseed-roll (promote B→A, reseed, keep morph), source
    //                      = CV if patched else internal.
    // Shared by the panel dice buttons and gate "Re-dice". TRIAL dice never come
    // here (setPending*Trial directly) and never reseed.
    void Monsoon::diceRhythm() {
        rhythmMode = 0;
        if (reseedOnRoll) {
            const bool sc = inputs[SEED_INPUT].isConnected();
            engine.pe.setPendingRhythmReseedRoll(sc ? sampleSeedFromSource() : 0.f, /*full=*/!sc);
        } else {
            engine.pe.setPendingRhythmRoll();
        }
    }
    void Monsoon::diceMelody() {
        melodyMode = 0;
        if (reseedOnRoll) {
            const bool sc = inputs[SEED_INPUT].isConnected();
            engine.pe.setPendingMelodyReseedRoll(sc ? sampleSeedFromSource() : 0.f, /*full=*/!sc);
        } else {
            engine.pe.setPendingMelodyRoll();
        }
    }

    // Single definition of every die-action. Fired by G3 (menu-routed) and by
    // Raffles's dedicated gates (and any future source) — DRY.
    void Monsoon::fireDieAction(int a) {
        switch (a) {
            case DA_TRIAL_R:       rhythmMode = 0; engine.pe.setPendingRhythmTrial(); break;
            case DA_TRIAL_M:       melodyMode = 0; engine.pe.setPendingMelodyTrial(); break;
            case DA_REDICE_R:      diceRhythm(); break;
            case DA_REDICE_M:      diceMelody(); break;
            case DA_LIVESRC_R:     rhythmLiveTrial = !rhythmLiveTrial; break;
            case DA_LIVESRC_M:     melodyLiveTrial = !melodyLiveTrial; break;
            case DA_LIVESTATIC_R:  rhythmMode = 1 - rhythmMode; break;
            case DA_LIVESTATIC_M:  melodyMode = 1 - melodyMode; break;
            case DA_RESEED_ROLL:   reseedOnRoll    = !reseedOnRoll;    break;
            case DA_RESEED_RESTART:reseedOnRestart = !reseedOnRestart; break;
            case DA_LASTDICE_R:    rhythmMode = 0; engine.pe.setPendingRhythmLastRoll();  break;
            case DA_LASTDICE_M:    melodyMode = 0; engine.pe.setPendingMelodyLastRoll();  break;
            case DA_LASTTRIAL_R:   rhythmMode = 0; engine.pe.setPendingRhythmLastTrial(); break;
            case DA_LASTTRIAL_M:   melodyMode = 0; engine.pe.setPendingMelodyLastTrial(); break;
        }
    }



// ---------------- Helper: phrase boundary hook -------------------------------
// Called at phrase boundary (stepIndex wraps from endStep back to startStep).
// Seeds are applied FIRST so the subsequent redraw uses the new RNG state.
void Monsoon::applyReversibleModeChange_() {
    // Per-stream Normal/Reversible, with entry-transition handling. On ENTERING
    // reversible for a stream: reseedOnModeChange → reseed that stream's Philox (also
    // zeroes the index); else if resetIndexOnModeChange → just zero the index (keep
    // key); else keep both (seamless "reversible from here", nonzero origin allowed).
    bool rRev = (rhythmReversibleMode != 0);
    bool mRev = (melodyReversibleMode != 0);
    if (rRev && !rhythmReversiblePrev_) {
        if (reseedOnModeChange)          engine.pe.seedRhythmPhilox(rhythmSeedFloat);
        else if (resetIndexOnModeChange) engine.pe.zeroRhythmIndex();
    }
    if (mRev && !melodyReversiblePrev_) {
        if (reseedOnModeChange)          engine.pe.seedMelodyPhilox(melodySeedFloat);
        else if (resetIndexOnModeChange) engine.pe.zeroMelodyIndex();
    }
    rhythmReversiblePrev_ = rRev;
    melodyReversiblePrev_ = mRev;
    engine.pe.setRhythmReversible(rRev);
    engine.pe.setMelodyReversible(mRev);
}

void Monsoon::onPhraseBoundary_() {
    // Per-stream behavior is handled inside the draw path via rhythmDrawDir()/
    // melodyDrawDir(): a REVERSIBLE stream under backward phase steps its signed index
    // -1 (regenerating the previous draw); a NORMAL stream always steps +1 ("keeps
    // rolling" even in reverse — no backward draw-tracking). So the same redraw path is
    // correct in both directions; we just drive it every boundary that has a pending
    // dice action, exactly as forward. Modes A-D always run forward.
    engine.pe.onPhraseBoundary(modeController->currentPatternInput);
}

// ---------------- Helper: expander change hook -------------------------------
//
// Expander topology:
//   [ScaleExpander] — [Monsoon] — [DNAExpander] — [PolyVoiceExpander]
//
// The left expander is always the scale/CV expander.
// The right side is a chain: Monsoon checks its immediate right for DNA or
// PolyVoice.  If DNA is present, PolyVoice is expected to the right of DNA
// and is reached by walking one step further.  If no DNA is present,
// PolyVoice may attach directly to Monsoon's right.
void Monsoon::onExpanderChange(const ExpanderChangeEvent& e) {
    updateExpanderPointers();
}

// ---------------- Helper: reset hook -----------------------------------------
void Monsoon::onReset() {
    Module::onReset();
    initialize();   // resets all module state to defaults
    clock.reset();  // resets ClockEngine timing
}


// Returns note length index: NOTE VALUE sets the base, VARIATION randomly biases it.
// NOTEVALS.allowedPPQN bitmask: 1=PPQN1, 2=PPQN4, 4=PPQN24
// ppqnSetting raw values: 1, 4, 24 — must be converted to bitmask before use.
int Monsoon::computeNoteLengthIdx(int requestedIdx, int ppqnMask) { return engine.computeNoteLengthIdx(requestedIdx, ppqnMask); }


// ---------------- Replacement for process() ---------------------------------
// Full logic for all modes inline here
// Calls helper functions as needed
void Monsoon::process(const ProcessArgs& args) {
    // Flush denormals to zero on the audio thread. Decaying float state (pulses,
    // smoothing, pitch CV) can drift into the denormal range, where FPU ops cost
    // ~10-100x normal — a classic cause of sudden CPU spikes that scale with the
    // number of active voices. With -ffast-math and no software flushing, this
    // guard is the standard Rack-plugin fix and costs nothing in the normal case.
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

    // --- Audio-rate Input Fetching ---
    InputState input;
    input.clk   = cachedClkConnected ? inputs[CLK_INPUT].getVoltage() : 0.f;
    input.gate1 = cachedGate1Connected ? inputs[GATE1_INPUT].getVoltage() : 0.f;
    input.gate2 = cachedGate2Connected ? inputs[GATE2_INPUT].getVoltage() : 0.f;
    input.gate3 = cachedGate3Connected ? inputs[GATE3_MOD_INPUT].getVoltage() : 0.f;
    input.run   = cachedRunConnected ? inputs[RUN_GATE_INPUT].getVoltage() : 0.f;
    input.reset = cachedResetConnected ? inputs[RESET_TRIGGER_INPUT].getVoltage() : 0.f;
    input.cv1   = cachedCv1Connected ? inputs[CV1_INPUT].getVoltage() : 0.f;
    input.cv2   = (modeSelect >= 2 && cachedCv2Connected) ? inputs[CV2_INPUT].getVoltage() : 0.f;

    // Logic References (Eliminate pointer indirection in hot path)
    TimingController& tc = *timingController;
    ModeController& mc = *modeController;
    CVRouter& cvr = *cvRouter;

    // ── Centralised clock tick (processes CLK IN once, before all mode handlers) ──
    // Derives bpm from external clock period or BPM knob, emits sixteenthEdge + quarterEdge.
    clock.process(
        input.clk,
        cachedClkConnected,
        cachedBpmParam,
        ppqnSetting,
        args.sampleTime
    );
    bpm = clock.bpm;  // keep module-level bpm in sync for note duration calculations

    // ── Mode E: external PHASE ramp on CV1 drives the pulse grid ──
    // In Mode E, CV1 is EXCLUSIVELY the phase input (it takes over CV1's meaning,
    // mirroring how Mode B repurposes gate1). The PhaseEngine emits the same
    // pulseEdge/sixteenthEdge/quarterEdge contract as the clock, plus a reverse flag.
    if (modeSelect == 4) {
        phase.process(input.cv1, cachedCv1Connected, args.sampleTime, ppqnSetting);
        if (cachedCv1Connected) bpm = phase.bpm;   // tempo follows the ramp's velocity
        modeController->setPhaseReverse(phase.reverse);
        engine.pe.setReverseActive(phase.reverse);
        applyReversibleModeChange_();
    } else {
        engine.pe.setReverseActive(false);
        applyReversibleModeChange_();
    }

    // ── Run/Reset Gate Processing ──
    runGateActive = tc.processRunGate(
        runGateActive,
        input.run,
        cachedRunBtn
    );
    
    tc.processResetGate(
        input.reset,
        cachedResetBtn
    );
    
    resetArmed = tc.isResetArmed();
    outputs[RESET_TRIGGER_OUTPUT].setVoltage(tc.getResetPulseOutput(args.sampleTime));
    
    // ── Handle Reset Trigger ──
    if (resetArmed && runGateActive) {
        handleRestart(/*manual=*/true, /*resetImmediate=*/true);
        tc.clearReset();
    } else if (!runGateActive) {
        tc.clearReset();
    }

    // ── Gate Edge Detection ──
    auto gateEdges = tc.processGateEdges(input.gate1, input.gate2);
    input.gate1Rise = gateEdges.gate1Rise;
    input.gate2Rise = gateEdges.gate2Rise;

    // ── Gate 3 Assignment Handling (Audio Rate for Consistency) ──
    if (cachedGate3Connected && gate3Trig.process(input.gate3, 0.1f, 1.f)) {
        static const int g3map[] = { DA_TRIAL_R, DA_TRIAL_M, DA_RESEED_ROLL,
            DA_RESEED_RESTART, DA_LIVESRC_R, DA_LIVESRC_M };
        if (gate3Target >= 0 && gate3Target < (int)(sizeof(g3map)/sizeof(g3map[0]))) {
            fireDieAction(g3map[gate3Target]);
        }
    }

    // ── Gate Assignment Handling ──
    if (modeSelect != 1) { // Mode B uses Gate 1 for input driving
        tc.handleGate1Assignment(gate1Assign, input.gate1Rise);
    }
    if (modeSelect != 3) { // Mode D uses Gate 2 for input driving
        tc.handleGate2Assignment(gate2Assign, input.gate2Rise, tc.getGate2High(), invertMuteLogic);
    }

    // --- Shophouse scale expander: sync + apply the ACTIVE front's scale EVERY process frame,
    //     independent of the clock (so editing a scale knob dims the faders immediately, and it
    //     works even when the sequencer is stopped). The boundary-quantised FRONT SWITCH (CV) is
    //     handled separately inside the clock-edge path. Recompute the mask only when it changes. ---
    if (auto* shop = expanderManager.cachedShophouseExpander) {
        shop->syncEntriesFromParams();
        scaleManager->lockScaleNotes =
            shop->params[ShophouseIds::CONSERVATION_PARAM].getValue() > 0.5f;
        const auto& e = shop->list.activeEntry();
        if (scaleManager->lastSelectedScale != e.scaleIdx
            || scaleManager->scaleRoot != e.root) {
            scaleManager->lastSelectedScale = e.scaleIdx;
            scaleManager->scaleRoot         = e.root;
            scaleManager->updateScaleMask();
        }
    }

    // --- Mode dispatch (only if running) ---
    if (runGateActive) {
        // Gate-close on the PPQN grid pulse. In Mode E the pulse source is the phase
        // ramp; otherwise the internal/external clock.
        bool gridPulse = (modeSelect == 4) ? phase.pulseEdge : clock.pulseEdge;
        if (gridPulse) {
            engine.gs.tickPulse();
            for (int i = 0; i < engine.numPolyVoices; ++i)
                engine.voices[i].gs.tickPulse();
        }
        // Optimization: Only execute mode logic if a relevant trigger/state is active.
        // This avoids calling executeMode and its internal switch every sample for Modes A, B, C.
        bool gate1High = input.gate1 >= 1.0f;
        bool shouldExecute = (modeSelect == 3); // Mode D is continuous
        if (!shouldExecute) {
            if (modeSelect == 0) shouldExecute = clock.sixteenthEdge;
            else if (modeSelect == 1) shouldExecute = input.gate1Rise || (gate1High && engine.stepIndex == -1);
            else if (modeSelect == 2) shouldExecute = clock.quarterEdge;
            else if (modeSelect == 4) shouldExecute = phase.sixteenthEdge; // Mode E: phase 1/16 grid
        }

        if (shouldExecute) {
            mc.executeMode(modeSelect, input, tc.getGate2High());

            // ── Shophouse scale expander: boundary-quantised scale/root ──
            // Drive the attached Shophouse's ScaleList. The index CV is sampled at the phrase
            // boundary (wrapped) to pick the pending front; commitAtBoundary() then makes it the
            // active (scale, root), which we write into the existing ScaleManager slot. All scale
            // changes therefore land on the loop edge — never mid-phrase. If no Shophouse is
            // attached, the context-menu scale is left untouched.
            if (auto* shop = expanderManager.cachedShophouseExpander) {
                // Boundary-quantised FRONT SWITCH only (needs the phrase-boundary edge, which is why
                // this part stays inside the clock-edge path). Sampling the index CV picks the
                // pending front; commitAtBoundary() makes it active on the loop edge. The active
                // front's scale is applied every process frame in the block below (not here), so
                // editing a scale knob responds immediately without waiting for a boundary.
                if (engine.lastStepResult.wrapped) {
                    auto& cv = shop->inputs[ShophouseIds::INDEX_CV_INPUT];
                    int prevActive = shop->list.active();
                    if (cv.isConnected()) {
                        int n = shop->list.size();
                        float att = shop->params[ShophouseIds::INDEX_CV_ATT_PARAM].getValue();
                        float norm = clampv<float>((cv.getVoltage() / 10.f) * att, 0.f, 0.999f);
                        int idx = (int)std::floor(norm * n);
                        shop->list.setPending(idx);
                        INFO("[Shophouse] WRAP cv=%.2fV att=%.2f n=%d -> idx=%d (pending=%d active=%d)",
                             cv.getVoltage(), att, n, idx, shop->list.pending(), prevActive);
                    } else {
                        INFO("[Shophouse] WRAP but INDEX_CV NOT CONNECTED (active=%d)", prevActive);
                    }
                    bool changed = shop->list.commitAtBoundary();
                    if (changed || shop->list.active() != prevActive)
                        INFO("[Shophouse] COMMIT changed=%d active %d->%d scaleIdx=%d root=%d",
                             (int)changed, prevActive, shop->list.active(),
                             shop->list.activeEntry().scaleIdx, shop->list.activeEntry().root);
                }
            }
        }

        // ── Mode E jump/scrub: replay the event chain over the jumped 1/16 steps ──
        // A jump moved the phase by >1 step in one sample. We replay executeModeE for
        // each crossed 1/16 in the jump direction, reusing the verified real-time path.
        // Modulation is frozen automatically (whole replay is one process() call).
        // WITHIN-DRAW: bounded to the current phrase (cap at 16 steps); a jump that
        // would cross the phrase boundary is clamped — cross-draw regeneration is a
        // later refinement.
        if (modeSelect == 4 && phase.jumped && phase.jumpSixteenths != 0) {
            bool jumpReverse = (phase.jumpSixteenths < 0);
            mc.setPhaseReverse(jumpReverse);
            // Bridge the jump DIRECTION to the draw-index direction: a boundary crossed
            // mid-jump fires onPhraseBoundary_ (inside executeModeE→postExecute_), which
            // for a reversible stream steps the index +1/-1 per reverseActive. Set it
            // from the jump sign for the replay, restore after. Live mode always rolls
            // at the crossing; armed dice rolls once; unarmed = reuse — all handled by
            // the existing boundary path. (Cap = 16 steps = at most one boundary.)
            bool savedReverse = engine.pe.reverseActive;
            engine.pe.setReverseActive(jumpReverse);
            int steps = jumpReverse ? -phase.jumpSixteenths : phase.jumpSixteenths;
            if (steps > 16) steps = 16;                 // within-draw cap (one phrase)
            int p16 = ClockEngine::pulsesPer16th(ppqnSetting);
            for (int s = 0; s < steps; ++s) {
                mc.executeMode(4, input, tc.getGate2High());
                // executeModeA advances holdRemain (step decisions) internally, but the
                // gate-CLOSE counter (gatePulseRemain) is normally driven by the pulse
                // burst we skip on a jump. Tick it p16 times per replayed 1/16 so a gate
                // that should have closed during the jumped region does so, keeping the
                // post-jump gate state aligned with the landing position.
                for (int pu = 0; pu < p16; ++pu) {
                    engine.gs.tickPulse();
                    for (int i = 0; i < engine.numPolyVoices; ++i)
                        engine.voices[i].gs.tickPulse();
                }
            }
            engine.pe.setReverseActive(savedReverse);
        }

    }

    // --- CV Routing (via CVRouter) ---
    // In Mode E, CV1 is EXCLUSIVELY the phase input (handled above), so its normal
    // pitch/BPM routing is suppressed — mirrors Mode B fully repurposing gate1.
    float cvOutVoltage = currentPitchV;
    bool cv1IsPhase = (modeSelect == 4);
    if (!cv1IsPhase && cachedCv1Connected && input.cv1 != 0.f && (cv1Mode == 0 || cv1Mode == 1)) {
        cvOutVoltage = cvr.processCV1Input(cv1Mode, input.cv1, *paramManager, currentPitchV, true);
    } else if (!cv1IsPhase && cachedCv1Connected && cv1Mode == 4) { // BPM Mod
        cvr.processCV1Input(cv1Mode, input.cv1, *paramManager, currentPitchV, true);
    } else {
        paramManager->clearCv1BpmOffset(); // Clear BPM offset if CV1 is not connected or not in BPM mode
    }
    if (outputs[CV_OUTPUT].isConnected()) outputs[CV_OUTPUT].setVoltage(cvOutVoltage);

    // --- Output Generation (Delegated to OutputGenerator) ---
    outputGenerator->drive(engine, outputs.data(), expanderManager, args.sampleTime);

    // ── Mode B Gate Slaving (with smoothing) ──
    // In Mode B (Seq + Gate), the gate duration must follow the external Gate 1 input.
    // This nullifies the impact of internal Note Length/Variation parameters.
    // if (modeSelect == 1) {
    //     // Only override if the sequencer decided to play a note (not a rest).
    //     // If it's a rest, the gate should be off regardless of Gate 1.
    //     if (engine.lastStepResult.decision != MonoDecision::Rest) { // Use the smoothed state of Gate 1 from g1Trig.
    //         // This prevents clicks from raw voltage changes.
    //         outputs[GATE_OUTPUT].setVoltage(engine.g1Trig.isHigh() ? 10.f : 0.f);
    //     } else {
    //         // If it's a rest, ensure the gate is off.
    //         outputs[GATE_OUTPUT].setVoltage(0.f);
    //     }
    // }
    
    // // Poly Sands editors (East visual, and the deprecated knob path) only do
    // // anything when the Straits BASE poly output expander is connected AND the
    // // user has requested at least one poly voice (numPolyVoices >= 1, i.e. more
    // // than the lone mono voice). Without the base expander there is nowhere for
    // // poly voices to come out, so the LOR/spread work would be inert anyway —
    // // this makes that explicit and keeps the editors no-op.



    // ── Modulation-viz snapshot (normalised effective big-5 values) ──
    // Published EVERY sample (not throttled) so the knob mod-arcs' modulated value
    // tracks the live param with ≤1-sample lag. When this was throttled, a manual
    // knob turn raced ahead of the snapshot by up to a throttle interval, and the
    // resulting set-vs-mod delta (≈1/7 per note step ≫ the 0.01 draw threshold)
    // drew a transient arc each frame that read as a red "trail" — even with no
    // modulator attached. Cheap (~17 scalar getters/sample).
    if (paramManager) {
        modViz.noteValue = paramManager->getNoteValueNorm();
        modViz.variation = paramManager->getVariationNorm();
        modViz.legato    = paramManager->getLegatoNorm();
        modViz.rest      = paramManager->getRestNorm();
        modViz.accent    = paramManager->getAccentNorm();
        modViz.active    = paramManager->anyBig5Modulated();
        for (int i = 0; i < 5; ++i) modViz.big5Lane[i] = paramManager->big5LaneModulated(i);
        modViz.rhythmSlew = paramManager->getRhythmSlewNorm();
        modViz.melodySlew = paramManager->getMelodySlewNorm();
        modViz.rhythmMix  = paramManager->getRhythmMixNorm();
        modViz.melodyMix  = paramManager->getMelodyMixNorm();
        modViz.activeCv3  = paramManager->anyCv3Modulated();
        for (int i = 0; i < 4; ++i) modViz.cv3Lane[i] = paramManager->cv3LaneModulated(i);
        for (int i = 0; i < 12; ++i) modViz.semitone[i] = paramManager->getSemitoneNorm(i);
        modViz.octaveLo   = paramManager->getOctaveLoNorm();
        modViz.octaveHi   = paramManager->getOctaveHiNorm();
        modViz.activePitch = paramManager->anyPitchModulated();
        for (int i = 0; i < 14; ++i) modViz.pitchLane[i] = paramManager->pitchLaneModulated(i);
    }

    // ── Throttle UI and Light processing ──
    if (lightDivider.process()) {
        if (uiManager) {
            // Move these here from per-sample logic
            uiManager->updateDiceLights(engine.pe.isRhythmSeedPending(), engine.pe.isMelodySeedPending());
            uiManager->updateLockLight(locked);
            uiManager->updateMuteLight(muted);
            
            // Aggregate DNA and Poly counts for the status LEDs
            int totalDnaCount = expanderManager.dnaExpanderCount + expanderManager.straitsSandsExpanderCount + expanderManager.deepStraitsSandsEastExpanderCount + expanderManager.deepStraitsSandsWestExpanderCount;
            int totalPolyCount = expanderManager.polyExpanderCount;
            uiManager->updateExpanderLights(expanderManager.scaleExpanderCount, totalDnaCount, totalPolyCount);

            uiManager->updateRunGateLight(runGateActive);
            uiManager->updateResetLight(resetArmed, args.sampleTime * 512.f);
        }
        
        outputs[RUN_GATE_OUTPUT].setVoltage(runGateActive ? 10.f : 0.f);

        // Seed monitor out — outputs the most recently armed or committed seed.
        // Prefers a pending (armed) value if present; reflects both rhythm and
        // melody seeds so the output is useful regardless of which DICE is active.
        // Rhythm and melody seeds share the same output (last-armed wins).
        float outSeed = rhythmSeedFloat;
        if (engine.pe.isRhythmSeedPending())  outSeed = rhythmSeedPendingFloat;
        if (engine.pe.isMelodySeedPending())  outSeed = melodySeedPendingFloat;   // melody armed overrides rhythm
        outputs[SEED_OUTPUT].setVoltage(clampv<float>(outSeed, 0.f, 10.f));

        // ── Button Processing (via UIManager) ──
        if (uiManager) {
            bool rhythmTriggered, melodyTriggered;
            if (uiManager->processDiceButtons(rhythmTriggered, melodyTriggered)) {
                // A dice press ROLLS (advance RNG, A/B morph) unless SEED is
                // patched (then reproducible reseed). Shared with gate re-dice
                // via diceRhythm()/diceMelody().
                if (rhythmTriggered) diceRhythm();
                if (melodyTriggered) diceMelody();
            }

            // Trial/audition dice: roll a fresh candidate B with A ANCHORED, so
            // the user auditions against a fixed A (raise slew to move toward the
            // trial, lower to fall back to A). Pressing the REGULAR dice then
            // commits the current B→A (main mode) when ready to move on.
            bool trialR, trialM;
            if (uiManager->processTrialButtons(trialR, trialM)) {
                if (trialR) { rhythmMode = 0; engine.pe.setPendingRhythmTrial(); }
                if (trialM) { melodyMode = 0; engine.pe.setPendingMelodyTrial(); }
            }

            // LastDice: roll stepping the index OPPOSITE to plain dice (previous draw).
            // Normal-mode only — the setters no-op on reversible streams.
            bool lastDiceR, lastDiceM;
            if (uiManager->processLastDiceButtons(lastDiceR, lastDiceM)) {
                if (lastDiceR) { rhythmMode = 0; engine.pe.setPendingRhythmLastRoll(); }
                if (lastDiceM) { melodyMode = 0; engine.pe.setPendingMelodyLastRoll(); }
            }
            // LastTrial: audition the previous candidate (index −1, A anchored).
            bool lastTrialR, lastTrialM;
            if (uiManager->processLastTrialButtons(lastTrialR, lastTrialM)) {
                if (lastTrialR) { rhythmMode = 0; engine.pe.setPendingRhythmLastTrial(); }
                if (lastTrialM) { melodyMode = 0; engine.pe.setPendingMelodyLastTrial(); }
            }
            
            if (uiManager->processLockButton()) {
                locked = !locked;
            }
            
            if (uiManager->processMuteButton()) {
                muted = !muted;
                if (!muted && restartOnUnmute) {
                    handleRestart(/*manual=*/true, /*resetImmediate=*/true);
                }
            }
            
            if (uiManager->processModeButton(modeSelect)) {
                // Mode was changed by button
            }
            
            // Mode lamps (updated by UIManager)
            uiManager->updateModeLights(modeSelect, lastModeSelect);
        }

        // --- Ring LEDs (Steps) and Semitone LEDs ---
        {
            // Get step brightness values
            float stepBrightness[16];
            for (int i = 0; i < 16; ++i) {
                stepBrightness[i] = engine.getStepLightBrightness(i);
            }
            
            // Get semitone flash brightness values
            float semiLedBrightness[12];
            for (int i = 0; i < 12; ++i) {
                float b = engine.gs.semiLedBrightness(i);
                // Aggregate brightness from all active poly voices
                for (int v = 0; v < engine.numPolyVoices; ++v) {
                    b = std::max(b, engine.voices[v].gs.semiLedBrightness(i));
                }
                semiLedBrightness[i] = b;
            }
            
            // Update both via UIManager
            if (uiManager) {
                uiManager->updateStepLights(stepBrightness, 16);
                uiManager->updateSemitoneFlashLights(semiLedBrightness, 12);
            }
        }

        // ── Semitone fader cache ──
        {
            bool faderDirty = false;
            for (int i = 0; i < 12; ++i) {
                // NON-DESTRUCTIVE: do NOT snap out-of-scale faders to zero when locked. The lock is
                // enforced at READ time (ScaleManager::getSemitoneWeight returns 0 for out-of-scale
                // semitones while locked) and the out-of-scale faders are DIMMED in the UI. The
                // faders keep the user's stored values, so toggling lock / changing scale reveals
                // them unchanged. (Previously this loop called setValue(0) here, which destroyed the
                // user's values — the exact behaviour the non-destructive scale work removed.)
                float w = getSemitoneParam(i);
                if (std::fabs(w - faderCache[i]) > 1e-5f) {
                    faderDirty = true;
                    faderCache[i] = w; // Update the cache!
                }
            }
            if (faderDirty || activeSemiCount == 0) rebuildSemiCache_();
        }

        engine.pe.refreshVisualCache(modeController->currentPatternInput);
    }

    // ── Control-Rate DNA and Window Updates (Optimized CPU) ──
    if (controlDivider.process()) {
        updateExpanderPointers();

        if (expanderManager.cachedRafflesExpander) {
            rack::Module* cw = expanderManager.cachedRafflesExpander;
            for (int i = 0; i < 14; ++i) {
                int in = MonsoonIds::RAFFLES_GATE_TRIAL_R + i;
                if (cw->inputs[in].isConnected()
                    && rafflesGateTrig[i].process(cw->inputs[in].getVoltage(), 0.1f, 1.f)) {
                    fireDieAction(i);
                }
            }
        }

        // Refresh Sequencer Parameters (Throttled sampling of all knobs/CV)
        modeController->updatePatternInput();
        engine.accentProb = paramManager->getAccent();

        // Check for expander changes and update cached pointers
        // Mirror the global spread-target mode onto the engine so display SpreadManagers
        // pull one value (no per-widget push). Playback still passes it explicitly below.
        engine.pe.setSpreadInterpMono(spreadInterpMono);
        engine.beginStrandWriteBlock();   // debug-only: reset the per-block strand-writer ledger
        dnaManager.processDNA(expanderManager, spreadInterpMono);

        // ── Deep Straits Sands Expanders (Control Rate Orchestration) ──
        expanderManager.sync(engine, spreadInterpMono);
        
        // Refresh Audio-Rate Caches (Throttled)
        cachedBpmParam = params[BPM_PARAM].getValue();
        cachedClkConnected = inputs[CLK_INPUT].isConnected();
        // Apply BPM CV modulation if CV1 is in BPM mode
        cachedBpmParam += paramManager->cv1BpmOffset;
        cachedBpmParam = clampv(cachedBpmParam, 20.f, 300.f); // Clamp to valid BPM range

        cachedCv1Connected = inputs[CV1_INPUT].isConnected();
        cachedCv2Connected = inputs[CV2_INPUT].isConnected();
        cachedCv3Connected = inputs[CV3_MOD_INPUT].isConnected();
        cachedGate3Connected = inputs[GATE3_MOD_INPUT].isConnected();
        cachedGate1Connected = inputs[GATE1_INPUT].isConnected();
        cachedGate2Connected = inputs[GATE2_INPUT].isConnected();
        cachedRunConnected = inputs[RUN_GATE_INPUT].isConnected();
        cachedResetConnected = inputs[RESET_TRIGGER_INPUT].isConnected();

        cachedRunBtn = params[RUN_GATE_PARAM].getValue();
        cachedResetBtn = params[RESET_BUTTON_PARAM].getValue();

        // (Per-voice rest/accent + Causeway CV modulation is applied in
        //  ModeController::updatePolyVoiceRest_(), which pulls from Monsoon::getEffectivePolyRest/
        //  Accent — the single resolver — right before executePolyVoices. The Straits mod arcs pull
        //  from the same resolver, so there are no cached-effective copies to drift or clobber.)

        // Handle Throttled CV1 Logic (Range Modulation)
        if (cachedCv1Connected) {
            if (cv1Mode == 2 || cv1Mode == 3) {
                cvRouter->processCV1Input(cv1Mode, inputs[CV1_INPUT].getVoltage(), *paramManager, 0.f, true);
            }
            paramManager->setCv1LoOffset(cvRouter->getLoOffset());
            paramManager->setCv1HiOffset(cvRouter->getHiOffset());
        } else {
            cvRouter->resetOffsets();
            paramManager->setCv1LoOffset(0.f);
            paramManager->setCv1HiOffset(0.f);
        }

        // (Unused poly voices are zeroed by the OutputGenerator, which writes all 16
        //  poly-cable channels each frame — gate-low/0V beyond numPolyVoices.)

        engine.updateWindow(
            params[PATTERN_LENGTH_PARAM].getValue(), inputs[LENGTH_INPUT].getVoltage(), inputs[LENGTH_INPUT].isConnected(),
            params[PATTERN_OFFSET_PARAM].getValue(), inputs[OFFSET_INPUT].getVoltage(), inputs[OFFSET_INPUT].isConnected()
        );

        // Update CV2 modulation offsets (Throttled)
        paramManager->clearCv2Offsets();
        // Mode C and D use CV2 as the pitch input to be quantized
        if (modeSelect < 2 && inputs[CV2_INPUT].isConnected()) { // CV2 is used for quantization in modes C and D
            float v    = clampv<float>(inputs[CV2_INPUT].getVoltage(), -5.f, 5.f);
            float norm = v / 5.f;   // now bipolar -1..+1 (was 0..1; rectified the negative half)
            if (cv2Mode == 0) paramManager->setCv2Offset(0, norm * 8.f);
            if (cv2Mode == 1) paramManager->setCv2Offset(1, norm);
            if (cv2Mode == 2) paramManager->setCv2Offset(2, norm);
            if (cv2Mode == 3) paramManager->setCv2Offset(3, norm);
            if (cv2Mode == 4) paramManager->setCv2Offset(4, norm); // New: Accent modulation
        }

        // ── Assignable CV3 & Raffles Modulation (Throttled) ──
        float cv3Mods[4] = {0.f, 0.f, 0.f, 0.f};
        
        // 1. Main Panel CV3 (bipolar offset to selected target; was unipolar 0..5
        //    which rectified the negative half — an attenuverter implies bipolar).
        if (cachedCv3Connected) {
            float v = inputs[CV3_MOD_INPUT].getVoltage();
            cv3Mods[cv3Target] = clampv<float>(v, -5.f, 5.f) / 5.f;
        }

        // 2. Raffles Expander (Summing bipolar attenuverted CVs)
        if (expanderManager.cachedRafflesExpander) {
            rack::Module* cw = expanderManager.cachedRafflesExpander;
            for (int i = 0; i < 4; ++i) {
                if (cw->inputs[RAFFLES_SLEW_R_CV + i].isConnected()) {
                    float v = cw->inputs[RAFFLES_SLEW_R_CV + i].getVoltage();
                    float att = cw->params[RAFFLES_SLEW_R_ATT + i].getValue();
                    cv3Mods[i] += (v / 5.f) * att;
                }
            }
        }

        // Apply final summed offsets to ParameterManager
        for (int i = 0; i < 4; ++i) {
            paramManager->setCv3Offset(i, clampv<float>(cv3Mods[i], -1.f, 1.f));
        }
    }
}


Model* modelMonsoon = createModel<Monsoon, MonsoonWidget>("Monsoon");

void init(rack::Plugin* p) {
	pluginInstance = p;
	p->addModel(modelMonsoon);
	p->addModel(modelMonsoonInterchangeExpander);
	p->addModel(modelMonsoonRafflesExpander);
	p->addModel(modelMonsoonJunctionExpander);
	//p->addModel(modelMonsoonSandsExpander);
	p->addModel(modelMonsoonStraitsExpander);
	p->addModel(modelMonsoonCausewayPolyExpander);
	p->addModel(modelMonsoonChangiExpander);
	p->addModel(modelMonsoonShophouseExpander);
	p->addModel(modelLantern);                       // Lantern note-output visualiser
	// West retired (Straits redesign): p->addModel(modelMonsoonStraitWestExpander);
	//p->addModel(modelMonsoonStraitsSands);          // Macro: global DNA
	//p->addModel(modelMonsoonDeepStraitsSandsEast);  // Deep: voices 2-8
	//p->addModel(modelMonsoonDeepStraitsSandsWest);  // Deep: voices 9-16
	// Visual editor expanders
	p->addModel(modelMonsoonSandsVisualExpander);   // Mono visual DNA editor
	p->addModel(modelStraitsEastSandsVisual);       // East visual DNA editor (tabbed)
	// RETIRED: West visual editor merged into East (15-voice). Source kept, not registered.
	// p->addModel(modelStraitsWestSandsVisual);
	p->addModel(modelStraitsSandsMacroVisual);      // Macro visual DNA editor
}