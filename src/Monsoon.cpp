// src/Monsoon.cpp
// Monsoon - full implementation with clean Mode A stepping logic
// C++11 compatible
//
// Patched: separate RNGs for melody & rhythm using rack::random::Xoroshiro128Plus,
// SEED CV input/output, RESET input, deferred reseed at phrase boundary, JSON serialization,
// two-part 64-bit seeding, widget ports for RESET/SEED, and preserved behavior.

#include <rack.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <cstring>

#include "MonsoonSandsExpander.hpp"
#include "MonsoonInterchangeExpander.hpp"
#include "MonsoonStraitsEastExpander.hpp"
#include "MonsoonStraitWestExpander.hpp"      // NEW (Phase 4)
#include "MonsoonStraitSandsExpander.hpp"     // NEW (Phase 6)
#include "MonsoonWidget.hpp"
#include "Monsoon.hpp"
#include "dsp/engines/PatternEngine.hpp"
#include "dsp/gates/GateState.hpp"

using namespace rack;
using namespace MonsoonIds;

Plugin* pluginInstance = nullptr;

void Monsoon::reseedXoroshiroFromFloat(rack::random::Xoroshiro128Plus& rng, float seedFloat) {
    engine.pe.seedRngFromFloat(rng, seedFloat);
}

void Monsoon::onSampleRateChange(const SampleRateChangeEvent& e) {
    lightDivider.setDivision(std::max(1, (int)std::round(e.sampleRate / 90.f)));
    controlDivider.setDivision(std::max(1, (int)std::round(e.sampleRate / 1500.f)));
}

Monsoon::Monsoon() {
        using namespace MonsoonIds;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        // Main controls
        configSwitch(NOTE_VALUE_PARAM, 0.f, 7.f, 2.f, "Note value",
            {"1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32"});
        configParam(VARIATION_PARAM,   0.f, 1.f, 0.5f, "Variation (longer–shorter)");
        configParam(LEGATO_PARAM,      0.f, 1.f, 0.10f, "Legato probability");
        configParam(REST_PARAM,        0.f, 1.f, 0.10f, "Rest probability");
        configParam(ACCENT_KNOB,       0.f, 1.f, 0.25f, "Accent gate probability");  // New
        configParam(TRANSPOSE_PARAM,  -12.f, 12.f, 0.f, "Transpose (semitones)");
        //Pattern ring controls
        // Main window controls (Always present)
        configParam(PATTERN_LENGTH_PARAM, 1.f, 16.f, 16.f, "Pattern length");
        configParam(PATTERN_OFFSET_PARAM, 0.f, 15.f, 0.f, "Pattern offset");

        // DNA Action Buttons (Main module only configures these if no expander)
        // If an expander is present, these are configured on the expander.
        // We configure them here as a fallback for standalone operation.
        configButton(DNA_SCRAMBLE_ALL_PARAM, "Scramble ALL DNA");
        configButton(DNA_SCRAMBLE_R_PARAM,   "Scramble Rhythm");
        configButton(DNA_SCRAMBLE_V_PARAM,   "Scramble Variation");
        configButton(DNA_SCRAMBLE_L_PARAM,   "Scramble Legato");
        configButton(DNA_SCRAMBLE_M_PARAM,   "Scramble Melody");
        configButton(DNA_SCRAMBLE_O_PARAM,   "Scramble Octave");

        configButton(DNA_RESET_ALL_PARAM,    "Reset ALL DNA");
        configButton(DNA_RESET_R_PARAM,      "Reset Rhythm");
        configButton(DNA_RESET_V_PARAM,      "Reset Variation");
        configButton(DNA_RESET_L_PARAM,      "Reset Legato");
        configButton(DNA_RESET_M_PARAM,      "Reset Melody");
        configButton(DNA_RESET_O_PARAM,      "Reset Octave");

        // 12 semitone sliders – default to major scale C (C,D,E,F,G,A,B)
        for (int i = 0; i < 12; ++i) {
            float def = 0.f;
            if (i == 0 || i == 2 || i == 4 || i == 5 || i == 7 || i == 9 || i == 11) def = 1.f;
            configParam(SEMI0_PARAM + i, 0.f, 1.f, def, "Semitone weight");
        }

        configParam(OCT_LO_PARAM, 0.f, 8.f, 2.f, "Lowest octave");
        configParam(OCT_HI_PARAM, 0.f, 8.f, 5.f, "Highest octave");

        configParam(BPM_PARAM, 20.f, 300.f, 120.f, "BPM (internal clock)");

        // Buttons (momentary)
        configButton(DICE_R_PARAM, "Dice rhythm");
        configButton(DICE_M_PARAM, "Dice melody");
        configButton(LOCK_PARAM,   "Lock");
        configButton(MUTE_PARAM,   "Mute");
        configButton(MODE_PARAM,   "Mode (Cycle A-B-C-D)");
        configButton(RESET_BUTTON_PARAM,  "Reset");
        configButton(RUN_GATE_PARAM,      "Run/Stop");

        // I/O
        configInput(CLK_INPUT,   "Clock");
        configInput(GATE1_INPUT, "Gate In 1");
        configInput(GATE2_INPUT, "Gate In 2");
        configInput(CV1_INPUT,   "CV In 1");
        configInput(CV2_INPUT,   "CV In 2");
        configInput(ACCENT_CV_INPUT, "Accent Probability CV");  // New

        // --- RNG/SEED ADDITION: new inputs
        configInput(RESET_TRIGGER_INPUT, "Reset (phrase restart)");
        configInput(SEED_INPUT,   "Seed CV (0..10V)");
        configInput(LENGTH_INPUT, "Pattern Length CV (0..10V)");
        configInput(OFFSET_INPUT, "Pattern Offset CV (0..10V)");
        configInput(RUN_GATE_INPUT, "Run/Stop Gate");

        // DNA Gate Inputs (Main module only configures these if no expander)
        configInput(DNA_SCRAMBLE_ALL_INPUT, "Scramble ALL DNA Gate");
        configInput(DNA_SCRAMBLE_R_INPUT,   "Scramble Rhythm Gate");
        configInput(DNA_SCRAMBLE_V_INPUT,   "Scramble Variation Gate");
        configInput(DNA_SCRAMBLE_L_INPUT,   "Scramble Legato Gate");
        configInput(DNA_SCRAMBLE_A_INPUT,   "Scramble Accent Gate");  // New
        configInput(DNA_SCRAMBLE_M_INPUT,   "Scramble Melody Gate");
        configInput(DNA_SCRAMBLE_O_INPUT,   "Scramble Octave Gate");

        configInput(DNA_RESET_ALL_INPUT,    "Reset ALL DNA Gate");
        configInput(DNA_RESET_R_INPUT,      "Reset Rhythm Gate");
        configInput(DNA_RESET_V_INPUT,      "Reset Variation Gate");
        configInput(DNA_RESET_L_INPUT,      "Reset Legato Gate");
        configInput(DNA_RESET_A_INPUT,      "Reset Accent Gate");  // New
        configInput(DNA_RESET_M_INPUT,      "Reset Melody Gate");
        configInput(DNA_RESET_O_INPUT,      "Reset Octave Gate");

        configOutput(GATE_OUTPUT,           "Gate");
        configOutput(CV_OUTPUT,             "1V/Oct");
        configOutput(SEED_OUTPUT,           "Seed Voltage Out (0..10V)");
        configOutput(RESET_TRIGGER_OUTPUT,  "Reset Trigger Out");
        configOutput(RUN_GATE_OUTPUT,       "Run Gate Out");
        configOutput(TIE_OUTPUT,            "Tie Gate (high on Tie)");          // New
        configOutput(LEGATO_OUTPUT,         "Legato Gate (high on Legato/Max)"); // New
        configOutput(ACCENT_OUTPUT,         "Accent Gate (high when accented)");  // New

        // Seed RNGs with a random value — safe to call here (uses rack::random, not inputs[])
        rhythmSeedFloat = rack::random::uniform() * 10.f;
        melodySeedFloat = rack::random::uniform() * 10.f;
        reseedXoroshiroFromFloat(rhythmRng, rhythmSeedFloat);
        //stochasticSeedFloat = rack::random::uniform() * 10.f;
        //reseedXoroshiroFromFloat(stochasticRng, stochasticSeedFloat);
        reseedXoroshiroFromFloat(melodyRng, melodySeedFloat);
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

        // Default patterns: all gates on, CV at 0V (C0), semitone 0
        // genPitchV() reads params[] which aren't valid yet, so use safe literals
        for (int i = 0; i < 16; ++i) {
            rhythmPattern[i]  = true;
            engine.pe.rhythmRandom[i] = 0.; //rack::random::uniform();
            engine.pe.variationRandom[i] =0.; //rack::random::uniform();
            engine.pe.legatoRandom[i] = 0.; //rack::random::uniform();
            engine.pe.melodyRandom[i] = 0.; //rack::random::uniform();
            engine.pe.octaveRandom[i] = 0.; //rack::random::uniform();
            engine.pe.accentRandom[i] = 0.; //rack::random::uniform();  // New
            for (int v = 0; v < 7; v++) {
                engine.pe.polyRhythmRandom[v][i] = 0.; //(float)rack::random::uniform(); // Seed with random for immediate DNA feedback
                engine.pe.polyMelodyRandom[v][i] = 0.5f;
                engine.pe.polyOctaveRandom[v][i] = 0.5f;
            }
            melodyPitchV[i]   = 0.f;   // C0 = 0V
            melodySemitone[i] = i % 12; // Spread initial semitones so all lights work
        }

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
    // Finalize state
    reseedXoroshiroFromFloat(engine.pe.rhythmRng, rhythmSeedFloat);
    reseedXoroshiroFromFloat(engine.pe.melodyRng, melodySeedFloat);
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

    // regenerate rhythm pattern (uses rhythmRng) — skipped when locked
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



// ---------------- Helper: phrase boundary hook -------------------------------
// Called at phrase boundary (stepIndex wraps from endStep back to startStep).
// Seeds are applied FIRST so the subsequent redraw uses the new RNG state.
void Monsoon::onPhraseBoundary_() {
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
    // --- Audio-rate Input Fetching (Cached & Guarded) ---
    float clkV      = cachedClkConnected ? inputs[CLK_INPUT].getVoltage() : 0.f;
    float gate1V    = cachedGate1Connected ? inputs[GATE1_INPUT].getVoltage() : 0.f;
    float gate2V    = cachedGate2Connected ? inputs[GATE2_INPUT].getVoltage() : 0.f;
    float runGateV  = cachedRunConnected ? inputs[RUN_GATE_INPUT].getVoltage() : 0.f;
    float resetGateV= cachedResetConnected ? inputs[RESET_TRIGGER_INPUT].getVoltage() : 0.f;
    float cv1V      = cachedCv1Connected ? inputs[CV1_INPUT].getVoltage() : 0.f;
    float cv2V      = (modeSelect >= 2 && cachedCv2Connected) ? inputs[CV2_INPUT].getVoltage() : 0.f;

    // Logic References (Eliminate pointer indirection in hot path)
    TimingController& tc = *timingController;
    ModeController& mc = *modeController;
    CVRouter& cvr = *cvRouter;

    // ── Centralised clock tick (processes CLK IN once, before all mode handlers) ──
    // Derives bpm from external clock period or BPM knob, emits sixteenthEdge + quarterEdge.
    clock.process(
        clkV,
        cachedClkConnected,
        cachedBpmParam,
        ppqnSetting,
        args.sampleTime
    );
    bpm = clock.bpm;  // keep module-level bpm in sync for note duration calculations

    // ── Run/Reset Gate Processing ──
    runGateActive = tc.processRunGate(
        runGateActive,
        runGateV,
        cachedRunBtn
    );
    
    tc.processResetGate(
        resetGateV,
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
    auto gateEdges = tc.processGateEdges(gate1V, gate2V);
    
    bool gate1Rise = gateEdges.gate1Rise;

    // ── Gate Assignment Handling ──
    tc.handleGate1Assignment(gate1Assign, gate1Rise);
    tc.handleGate2Assignment(gate2Assign, gateEdges.gate2Rise, tc.getGate2High(), invertMuteLogic);

    // --- Mode dispatch (only if running) ---
    if (runGateActive) {
        // Optimization: Only execute mode logic if a relevant trigger/state is active.
        // This avoids calling executeMode and its internal switch every sample for Modes A, B, C.
        bool gate1High = gate1V >= 1.0f;
        bool shouldExecute = (modeSelect == 3); // Mode D is continuous
        if (!shouldExecute) {
            if (modeSelect == 0) shouldExecute = clock.sixteenthEdge;
            else if (modeSelect == 1) shouldExecute = gate1Rise || (gate1High && engine.stepIndex == -1);
            else if (modeSelect == 2) shouldExecute = clock.quarterEdge;
        }

        if (shouldExecute) {
            float cv2ToUse = (modeSelect >= 2) ? cv2V : 0.f;
            if (mc.executeMode(modeSelect, gate1Rise, gate1High, tc.getGate2High(), cv2ToUse)) {
                // Mode took a step; update poly voices if expander is present
                if (engine.numPolyVoices > 0 && expanderManager.cachedPolyVoiceExpander) {
                for (int i = 0; i < engine.numPolyVoices; ++i) {
                    engine.voices[i].restProb = cachedPolyRest[i];
                }
                    engine.executePolyVoices(mc.currentPatternInput);
                }
            }
        }
    }

    // --- CV Routing (via CVRouter) ---
    float cvOutVoltage = currentPitchV;
    if (cachedCv1Connected && cv1V != 0.f && (cv1Mode == 0 || cv1Mode == 1)) {
        cvOutVoltage = cvr.processCV1Input(
                cv1Mode,
                cv1V,
                currentPitchV,
                true);
    }
    if (outputs[CV_OUTPUT].isConnected()) outputs[CV_OUTPUT].setVoltage(cvOutVoltage);

    // --- Output Generation (Inlined logic to minimize cross-translation unit calls) ---
    float gateV = (runGateActive) ? engine.gs.process(args.sampleTime) : 0.f;
    outputs[GATE_OUTPUT].setVoltage(muted ? 0.f : gateV);
    
    bool isTie = (engine.lastStepResult.decision == MonoDecision::Tie);
    outputs[TIE_OUTPUT].setVoltage((isTie && !muted) ? 10.f : 0.f);
    
    bool isLegato = (engine.lastStepResult.decision == MonoDecision::Legato || 
                        engine.lastStepResult.decision == MonoDecision::LegatoMax);
    outputs[LEGATO_OUTPUT].setVoltage((isLegato && !muted) ? 10.f : 0.f);
    
    bool engineAccented = engine.lastStepResult.accented;
    outputs[ACCENT_OUTPUT].setVoltage((engineAccented && gateV > 5.f && !muted) ? 10.f : 0.f);
    
    // Poly voice outputs (Guard: only if expander present)
    auto* polyExp = expanderManager.cachedPolyVoiceExpander;
    if (polyExp && engine.numPolyVoices > 0) {
        using namespace PolyVoiceExpanderIds;
        
        // ── Rest Probability Modulation (voices 2-8 on East) ──
        for (int i = 0; i < 7; i++) {
            if (inputs[POLY_REST_MOD_CV_INPUT_1 + i].isConnected()) {
                float modCV = inputs[POLY_REST_MOD_CV_INPUT_1 + i].getVoltage();
                float attenuverter = params[POLY_REST_MOD_ATT_1 + i].getValue(); // [-1, 1]
                float modulation = modCV * attenuverter * 0.1f; // Scale CV by attenuverter
                engine.voices[i].restProb = clamp(cachedPolyRest[i] + modulation, 0.f, 1.f);
            } else {
                engine.voices[i].restProb = cachedPolyRest[i];
            }
        }
        
        // Output individual gates/CVs/accents for voices 2-8
        for (int i = 0; i < 7; ++i) {
            if (muted) {
                polyExp->outputs[POLY_GATE_OUT_1 + i].setVoltage(0.f);
                polyExp->outputs[POLY_ACCENT_OUT_1 + i].setVoltage(0.f);
                continue;
            }
            float vg = engine.voices[i].gs.process(args.sampleTime);
            polyExp->outputs[POLY_GATE_OUT_1 + i].setVoltage(vg);
            polyExp->outputs[POLY_CV_OUT_1 + i].setVoltage(engine.voices[i].gs.currentPitchV);
            float polyAccent = (engineAccented && vg > 5.f) ? 10.f : 0.f;
            polyExp->outputs[POLY_ACCENT_OUT_1 + i].setVoltage(polyAccent);
        }
        
        // Generate poly outputs for voices 1-8
        float polyGate1_8 = 0.f;
        float polyCv1_8 = 0.f;
        if (!muted) {
            polyGate1_8 = (engine.gs.process(args.sampleTime) > 5.f) ? 10.f : 0.f;
            polyCv1_8 = currentPitchV;
        }
        polyExp->outputs[POLY_GATE_1_8_OUT].setVoltage(polyGate1_8);
        polyExp->outputs[POLY_CV_1_8_OUT].setVoltage(polyCv1_8);
    }
    
    // ── Straits West Expander (voices 9-16) ──
    auto* westExp = expanderManager.cachedStraitWestExpander;
    if (westExp && engine.numPolyVoices > 7) {
        using namespace StraitWestExpanderIds;
        
        // ── Rest Probability Modulation (voices 9-16 on West) ──
        for (int i = 0; i < 8; i++) {
            if (inputs[POLY_REST_MOD_CV_INPUT_8 + i].isConnected()) {
                float modCV = inputs[POLY_REST_MOD_CV_INPUT_8 + i].getVoltage();
                float attenuverter = params[POLY_REST_MOD_ATT_8 + i].getValue(); // [-1, 1]
                float modulation = modCV * attenuverter * 0.1f;
                engine.voices[7 + i].restProb = clamp(cachedPolyRest[7 + i] + modulation, 0.f, 1.f);
            } else {
                engine.voices[7 + i].restProb = cachedPolyRest[7 + i];
            }
        }
        
        // Output individual gates/CVs/accents for voices 9-16
        for (int i = 0; i < 8; ++i) {
            if (muted) {
                westExp->outputs[POLY_GATE_OUT_1 + i].setVoltage(0.f);
                westExp->outputs[POLY_ACCENT_OUT_1 + i].setVoltage(0.f);
                continue;
            }
            float vg = engine.voices[7 + i].gs.process(args.sampleTime);
            westExp->outputs[POLY_GATE_OUT_1 + i].setVoltage(vg);
            westExp->outputs[POLY_CV_OUT_1 + i].setVoltage(engine.voices[7 + i].gs.currentPitchV);
            float polyAccent = (engineAccented && vg > 5.f) ? 10.f : 0.f;
            westExp->outputs[POLY_ACCENT_OUT_1 + i].setVoltage(polyAccent);
        }
        
        // Generate poly outputs for voices 1-16 (complete mix)
        float polyGate1_16 = 0.f;
        float polyCv1_16 = 0.f;
        if (!muted) {
            // Use primary voice poly gate for 1-16 output
            polyGate1_16 = (engine.gs.process(args.sampleTime) > 5.f) ? 10.f : 0.f;
            polyCv1_16 = currentPitchV;
        }
        westExp->outputs[POLY_GATE_1_16_OUT].setVoltage(polyGate1_16);
        westExp->outputs[POLY_CV_1_16_OUT].setVoltage(polyCv1_16);
    }
    
    // ── Straits Sands Expander (DNA control + scramble/reset) ──
    auto* sandsExp = expanderManager.cachedStraitSandsExpander;
    if (sandsExp) {
        using namespace StraitSandsExpanderIds;
        
        // Read DNA controls from Sands for all voices 1-15
        for (int v = 0; v < 15; v++) {
            int paramBase = POLY_DNA_VOICE_1_LEN + v * 3;
            float len = params[paramBase].getValue();
            float off = params[paramBase + 1].getValue();
            float rot = params[paramBase + 2].getValue();
            
            // Apply DNA to pattern engine (voice index 0-14)
            engine.pe.polyLen[v] = (int)len;
            engine.pe.polyOff[v] = (int)off;
            engine.pe.polyRot[v] = (int)rot;
        }
        
        // Handle Scramble triggers (randomize length & offset)
        bool scrambleAll = params[SCRAMBLE_ALL_PARAM].getValue() > 0.5f ||
                          inputs[SCRAMBLE_ALL_INPUT].getVoltage() > 1.f;
        if (scrambleAll) {
            for (int v = 0; v < 15; v++) {
                int paramBase = POLY_DNA_VOICE_1_LEN + v * 3;
                params[paramBase].setValue(random::uniform() * 15.f + 1.f);     // Length: 1-16
                params[paramBase + 1].setValue(random::uniform() * 15.f);        // Offset: 0-15
            }
        } else {
            // Check individual scramble buttons
            for (int v = 0; v < 15; v++) {
                bool scramble = params[SCRAMBLE_VOICE_1 + v].getValue() > 0.5f ||
                               inputs[SCRAMBLE_VOICE_1_INPUT + v].getVoltage() > 1.f;
                if (scramble) {
                    int paramBase = POLY_DNA_VOICE_1_LEN + v * 3;
                    params[paramBase].setValue(random::uniform() * 15.f + 1.f);
                    params[paramBase + 1].setValue(random::uniform() * 15.f);
                }
            }
        }
        
        // Handle Reset triggers (restore defaults)
        bool resetAll = params[RESET_ALL_PARAM].getValue() > 0.5f ||
                       inputs[RESET_ALL_INPUT].getVoltage() > 1.f;
        if (resetAll) {
            for (int v = 0; v < 15; v++) {
                int paramBase = POLY_DNA_VOICE_1_LEN + v * 3;
                params[paramBase].setValue(16.f);        // Length: 16 (default)
                params[paramBase + 1].setValue(0.f);     // Offset: 0 (default)
                params[paramBase + 2].setValue(0.f);     // Rotation: 0 (default)
            }
        } else {
            // Check individual reset buttons
            for (int v = 0; v < 15; v++) {
                bool reset = params[RESET_VOICE_1 + v].getValue() > 0.5f ||
                            inputs[RESET_VOICE_1_INPUT + v].getVoltage() > 1.f;
                if (reset) {
                    int paramBase = POLY_DNA_VOICE_1_LEN + v * 3;
                    params[paramBase].setValue(16.f);
                    params[paramBase + 1].setValue(0.f);
                    params[paramBase + 2].setValue(0.f);
                }
            }
        }
    }

    // ── Throttle UI and Light processing ──
    if (lightDivider.process()) {
        // Throttled Visuals/Outputs
        // ── UI Light Updates ──
        if (uiManager) {
            // Move these here from per-sample logic
            uiManager->updateDiceLights(engine.pe.isRhythmSeedPending(), engine.pe.isMelodySeedPending());
            uiManager->updateLockLight(locked);
            uiManager->updateMuteLight(muted);
            uiManager->updateExpanderLights(expanderManager.scaleExpanderCount, expanderManager.dnaExpanderCount, expanderManager.polyExpanderCount);

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
                if (rhythmTriggered) {
                    rhythmMode = 0;
                    engine.pe.setPendingRhythmSeed(sampleSeedFromSource());
                }
                if (melodyTriggered) {
                    melodyMode = 0;
                    engine.pe.setPendingMelodySeed(sampleSeedFromSource());
                }
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
                if (scaleManager && scaleManager->lockScaleNotes && !(scaleManager->activeScaleMask & (1 << i))) {
                    if (params[SEMI0_PARAM + i].getValue() != 0.f)
                        params[SEMI0_PARAM + i].setValue(0.f);
                }
                float w = getSemitoneParam(i);
                if (std::fabs(w - faderCache[i]) > 1e-5f) {
                    faderDirty = true;
                    faderCache[i] = w; // Update the cache!
                }
            }
            if (faderDirty || activeSemiCount == 0) rebuildSemiCache_();
        }

        // Refresh visual cache so LEDs react to knob changes (at ~90Hz instead of 44kHz)
        modeController->updatePatternInput();
        engine.pe.refreshVisualCache(modeController->currentPatternInput);
    }

    // ── Control-Rate DNA and Window Updates (Optimized CPU) ──
    if (controlDivider.process()) {
        dnaManager.processDNA(expanderManager);

        // Refresh Audio-Rate Caches (Throttled)
        cachedBpmParam = params[BPM_PARAM].getValue();
        cachedClkConnected = inputs[CLK_INPUT].isConnected();
        cachedCv1Connected = inputs[CV1_INPUT].isConnected();
        cachedCv2Connected = inputs[CV2_INPUT].isConnected();
        cachedGate1Connected = inputs[GATE1_INPUT].isConnected();
        cachedGate2Connected = inputs[GATE2_INPUT].isConnected();
        cachedRunConnected = inputs[RUN_GATE_INPUT].isConnected();
        cachedResetConnected = inputs[RESET_TRIGGER_INPUT].isConnected();

        cachedRunBtn = params[RUN_GATE_PARAM].getValue();
        cachedResetBtn = params[RESET_BUTTON_PARAM].getValue();

        // Cache Poly Rest probabilities
        for (int i = 0; i < 7; ++i) {
            cachedPolyRest[i] = paramManager->getPolyRest(i);
        }

        // Handle Throttled CV1 Logic (Range Modulation)
        if (cachedCv1Connected) {
            if (cv1Mode == 2 || cv1Mode == 3) {
                cvRouter->processCV1Input(cv1Mode, inputs[CV1_INPUT].getVoltage(), 0.f, true);
            }
            paramManager->setCv1LoOffset(cvRouter->getLoOffset());
            paramManager->setCv1HiOffset(cvRouter->getHiOffset());
        } else {
            cvRouter->resetOffsets();
            paramManager->setCv1LoOffset(0.f);
            paramManager->setCv1HiOffset(0.f);
        }

        // Zero unused voice outputs so they don't emit stale voltages (Transferred from audio rate)
        if (expanderManager.cachedPolyVoiceExpander) {
            using namespace PolyVoiceExpanderIds;
            for (int i = engine.numPolyVoices; i < 7; ++i) {
                expanderManager.cachedPolyVoiceExpander->outputs[POLY_GATE_OUT_1 + i].setVoltage(0.f);
                expanderManager.cachedPolyVoiceExpander->outputs[POLY_CV_OUT_1 + i].setVoltage(0.f);
                expanderManager.cachedPolyVoiceExpander->outputs[POLY_ACCENT_OUT_1 + i].setVoltage(0.f);
            }
        }

        engine.updateWindow(
            params[PATTERN_LENGTH_PARAM].getValue(), inputs[LENGTH_INPUT].getVoltage(), inputs[LENGTH_INPUT].isConnected(),
            params[PATTERN_OFFSET_PARAM].getValue(), inputs[OFFSET_INPUT].getVoltage(), inputs[OFFSET_INPUT].isConnected()
        );

        // Update CV2 modulation offsets (Throttled)
        paramManager->clearCv2Offsets();
        if (inputs[CV2_INPUT].isConnected()) {
            float v    = clampv<float>(inputs[CV2_INPUT].getVoltage(), 0.f, 5.f);
            float norm = v / 5.f; 
            if (cv2Mode == 0) paramManager->setCv2Offset(0, norm * 8.f);
            if (cv2Mode == 1) paramManager->setCv2Offset(1, norm);
            if (cv2Mode == 2) paramManager->setCv2Offset(2, norm);
            if (cv2Mode == 3) paramManager->setCv2Offset(3, norm);
        }
    }
}


Model* modelMonsoon = createModel<Monsoon, MonsoonWidget>("Monsoon");

void init(rack::Plugin* p) {
	pluginInstance = p;
	p->addModel(modelMonsoon);
	p->addModel(modelMonsoonInterchangeExpander);
	p->addModel(modelMonsoonSandsExpander);
	p->addModel(modelMonsoonStraitsEastExpander);
	p->addModel(modelMonsoonStraitWestExpander);    // NEW (Phase 4)
	p->addModel(modelMonsoonStraitSandsExpander);   // NEW (Phase 6)
}