#include "MonsoonModeController.hpp"
#include "../../Monsoon.hpp"

using namespace rack;

// ──── Helper: Update poly voice rest probabilities ──────────────────────────

void ModeController::updatePolyVoiceRest_() {
    if (engine.numPolyVoices > 0) {
        for (int i = 0; i < engine.numPolyVoices; ++i) {
            engine.voices[i].restProb = paramManager.getPolyRest(i);
        }
    }
}

void ModeController::updatePatternInput() {
    for (int i = 0; i < 12; ++i) {
        currentPatternInput.semiWeights[i] = paramManager.getSemitone(i);
    }
    currentPatternInput.restProb          = paramManager.getRest();
    currentPatternInput.variationAmount   = paramManager.getVariation();
    currentPatternInput.octaveLo          = paramManager.getOctaveLo();
    currentPatternInput.octaveHi          = paramManager.getOctaveHi();
    currentPatternInput.transpose         = paramManager.getTranspose();
    currentPatternInput.noteVariationMask = engine.noteVariationMask;
    currentPatternInput.locked            = engine.locked;
    currentPatternInput.rhythmSlew        = paramManager.getRhythmSlew();
    currentPatternInput.melodySlew        = paramManager.getMelodySlew();
    currentPatternInput.rhythmMix         = paramManager.getRhythmMix();
    currentPatternInput.melodyMix         = paramManager.getMelodyMix();
    // Surge expander: 5 big-5 CV (x attenuverter) -> offsets the param getters add.
    // CV normalised 0..10V -> 0..1, scaled bipolar by the attenuverter.
    paramManager.clearSurgeOffsets();
    if (mainModule && mainModule->expanderManager.cachedSurgeExpander) {
        rack::Module* sg = mainModule->expanderManager.cachedSurgeExpander;
        for (int i = 0; i < 5; ++i) {
            float cv  = sg->inputs[MonsoonIds::SURGE_NOTEVAL_CV + i].getVoltage() / 10.f;
            float att = sg->params[MonsoonIds::SURGE_NOTEVAL_ATT + i].getValue();
            paramManager.setSurgeOffset(i, cv * att);
        }
    }
    // PLAYABLE LIVE MORPH: apply the live MIX every process (control rate), like
    // spread — this is the continuous A<->B blend. recomputeEffective only does
    // work when MIX actually changes, so it is cheap. SLEW is NOT applied here;
    // it is consumed at roll time (shapes B). Lock freezes the morph.
    if (!engine.locked) {
        engine.pe.latchMix(currentPatternInput.rhythmMix,
                           currentPatternInput.melodyMix);
    }
    if (mainModule) {
        currentPatternInput.reseedOnRoll    = mainModule->reseedOnRoll;
        currentPatternInput.rhythmLiveTrial = mainModule->rhythmLiveTrial;
        currentPatternInput.melodyLiveTrial = mainModule->melodyLiveTrial;
        const bool sc = mainModule->inputs[MonsoonIds::SEED_INPUT].isConnected();
        currentPatternInput.seedConnected   = sc;
        currentPatternInput.seedSampleValue = sc ? mainModule->sampleSeedFromSource() : 0.f;
    }
}

PatternInput ModeController::assemblePatternInput_() {
    updatePatternInput();
    return currentPatternInput;
}

// ──── Helper: Post-execution logic ──────────────────────────────────────────

void ModeController::postExecute_(const StepResult& result) {
    // Handle phrase boundary
    if (result.wrapped && mainModule) {
        mainModule->onPhraseBoundary_();
    }

    // (Playable slew is now latched every process in updatePatternInput(), so the
    // A→B blend follows the knob continuously like spread — no wrap-gated latch
    // here. Lock freezes it at the updatePatternInput site.)
    
    // Execute poly voices if step was taken
    if (result.stepped && engine.numPolyVoices > 0) {
        updatePolyVoiceRest_();

        // Execute poly voice decision logic for the new step
        engine.executePolyVoices(currentPatternInput);
    }
}

// ──── Mode A: Clock-Driven Sequencing ───────────────────────────────────────

// Mode E: phase-driven. The dispatch (Monsoon.cpp) only calls this when
// phase.sixteenthEdge fired, so a 1/16 step is due now. Reuses the Mode A decision
// path exactly (clock-style stepping) — the only difference is the edge SOURCE is
// the phase ramp, not the clock. A synthesized clock-view carries sixteenthEdge=true
// into engine.executeModeA, which reads nothing else from the clock. (Forward only;
// reverse traversal is the next branch.)
bool ModeController::executeModeE() {
    engine.accentProb = paramManager.getAccent();
    PatternInput in = assemblePatternInput_();

    ClockEngine phaseView;            // edge-only view; executeModeA reads sixteenthEdge
    phaseView.sixteenthEdge = true;

    StepResult result = engine.executeModeA(
        phaseView,
        in.restProb,
        paramManager.getLegato(),
        paramManager.getNoteValue(),
        in,
        phaseReverse ? -1 : +1        // within-draw reverse traversal
    );
    postExecute_(result);
    updateLastStepIndex();
    return result.stepped;
}

bool ModeController::executeModeA() {
    if (clock.sixteenthEdge) {
        // Fetch current parameters
        engine.accentProb = paramManager.getAccent();
        
        // Ensure pattern input is fresh
        PatternInput in = assemblePatternInput_();

        // Execute the mode
        StepResult result = engine.executeModeA(
            clock,
            in.restProb,
            paramManager.getLegato(),
            paramManager.getNoteValue(),
            in
        );
        
        // Handle post-execution
        postExecute_(result);
        updateLastStepIndex();
        
        return result.stepped;
    }
    return false;
}

// ──── Mode B: Gate-Driven Sequencing ────────────────────────────────────────

bool ModeController::executeModeB(bool gate1Rise,
                                   bool gate1High) {
    if (gate1Rise || (gate1High && engine.stepIndex == -1)) {
        // In Mode B, variation and note length should have no impact on the gate.
        // Only legato, rest, and accent apply.
        // Create a local copy of PatternInput and override relevant values for Mode B.
        PatternInput modeBPatternInput = currentPatternInput; // Start with current settings
        modeBPatternInput.noteVariationMask = 0b111; // Allow all note lengths (e.g., 1/1 to 1/32T)
        modeBPatternInput.variationAmount = 0.5f;    // No bias for longer/shorter notes

        // Execute the mode
        StepResult result = engine.executeModeB(
            gate1Rise,
            gate1High,
            modeBPatternInput.restProb, // Rest still applies
            paramManager.getLegato(),
            // Note value (which influences note length) should have no impact.
            // Pass a neutral value (e.g., 2.f for 1/4 note, a common default).
            2.f,
            modeBPatternInput // Pass the modified PatternInput
        );
        
        // Handle post-execution
        postExecute_(result);
        updateLastStepIndex();
        
        return result.stepped;
    }
    return false;
}

// ──── Mode C: Quantizer Mode 1 ──────────────────────────────────────────────

bool ModeController::executeModeC(float cv2Voltage) {
    if (clock.quarterEdge) {
        // Clamp and validate CV2 input
        float inCV = clampv<float>(cv2Voltage, 0.f, 5.f);
        
        // Execute the mode
        engine.executeModeC(clock, inCV);
        const StepResult& result = engine.lastStepResult;
        
        // Handle post-execution (usually minimal for quantizer modes)
        if (result.wrapped && mainModule) {
            mainModule->onPhraseBoundary_();
        }
        
        updateLastStepIndex();
        return result.stepped;
    }
    return false;
}

// ──── Mode D: Quantizer Mode 2 ──────────────────────────────────────────────

bool ModeController::executeModeD(bool gate2High,
                                   float cv2Voltage) {
    // Mode D executes continuously based on gate2 state and CV2 voltage
    // (no edge detection needed)
    
    // Clamp and validate CV2 input
    float inCV = clampv<float>(cv2Voltage, 0.f, 5.f);
    
    // Execute the mode
    engine.executeModeD(gate2High, inCV);
    const StepResult& result = engine.lastStepResult;
    
    // Handle post-execution
    if (result.wrapped && mainModule) {
        mainModule->onPhraseBoundary_();
    }
    
    if (result.stepped) {
        updateLastStepIndex();
    }
    
    return result.stepped;
}

// ──── High-Level Dispatcher ─────────────────────────────────────────────────

bool ModeController::executeMode(int modeId,
                                  const InputState& input,
                                  bool gate2High) {
    bool gate1High = input.gate1 >= 1.0f;
    switch (modeId) {
        case 0: return executeModeA();
        case 1: return executeModeB(input.gate1Rise, gate1High);
        case 2: return executeModeC(input.cv2);
        case 3: return executeModeD(gate2High, input.cv2);
        case 4: return executeModeE();
        default: return false;
    }
}
