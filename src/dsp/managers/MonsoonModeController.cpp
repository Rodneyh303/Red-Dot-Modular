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
    // CV3 assignable mod: add the (normalised 0..1 from 0..10V) CV to the selected
    // continuous target, summing with the knob. Clamped to [0,1].
    if (mainModule && mainModule->inputs[MonsoonIds::CV3_MOD_INPUT].isConnected()) {
        float cv = mainModule->inputs[MonsoonIds::CV3_MOD_INPUT].getVoltage() / 10.f;
        auto add01 = [](float base, float d){ return rack::math::clamp(base + d, 0.f, 1.f); };
        switch (mainModule->cv3Target) {
            case Monsoon::CV3_RHYTHM_SLEW: currentPatternInput.rhythmSlew = add01(currentPatternInput.rhythmSlew, cv); break;
            case Monsoon::CV3_MELODY_SLEW: currentPatternInput.melodySlew = add01(currentPatternInput.melodySlew, cv); break;
            case Monsoon::CV3_RHYTHM_MIX:  currentPatternInput.rhythmMix  = add01(currentPatternInput.rhythmMix,  cv); break;
            case Monsoon::CV3_MELODY_MIX:  currentPatternInput.melodyMix  = add01(currentPatternInput.melodyMix,  cv); break;
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
        // Fetch current parameters
        engine.accentProb = paramManager.getAccent();
        
        PatternInput in = assemblePatternInput_();

        // Execute the mode
        StepResult result = engine.executeModeB(
            gate1Rise,
            gate1High,
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
                                  bool gate1Rise,
                                  bool gate1High,
                                  bool gate2High,
                                  float cv2Voltage) {
    switch (modeId) {
        case 0: return executeModeA();
        case 1: return executeModeB(gate1Rise, gate1High);
        case 2: return executeModeC(cv2Voltage);
        case 3: return executeModeD(gate2High, cv2Voltage);
        default: return false;
    }
}
