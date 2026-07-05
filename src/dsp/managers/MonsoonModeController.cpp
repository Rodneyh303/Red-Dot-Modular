#include "MonsoonModeController.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonCausewayPolyExpander.hpp"

using namespace rack;

// ──── Helper: Update poly voice rest probabilities ──────────────────────────

void ModeController::updatePolyVoiceRest_() {
    // Single source of truth for per-voice rest/accent probability, applied BEFORE the step
    // decision runs. Base = the Straits knob (paramManager); modulation = Causeway per-voice CV ×
    // (per-voice attenuator + global attenuator), summed. Writing the MODULATED value here (not the
    // raw knob) is what makes Causeway CV actually affect the engine's rest/accent decisions — an
    // earlier design set restProb = raw knob here and applied CV in a LATER block that the decision
    // had already passed, so CV modulation never reached a decision (only knob moves did).
    if (engine.numPolyVoices <= 0) return;
    auto* cway = mainModule ? mainModule->expanderManager.cachedCausewayPolyExpander : nullptr;
    for (int i = 0; i < engine.numPolyVoices; ++i) {
        float restBase = paramManager.getPolyRest(i);
        float accBase  = paramManager.getPolyAccent(i);
        float restMod = 0.f, accMod = 0.f;
        if (cway) {
            const int ch = i + 1;   // poly voice i (voice 2..16) → poly-cable channel 1..15
            auto& restIn = cway->inputs[MonsoonIds::POLY_REST_CV_INPUT];
            auto& accIn  = cway->inputs[MonsoonIds::POLY_ACCENT_CV_INPUT];
            float restAtt = cway->params[MonsoonIds::POLY_REST_MOD_ATT_1 + i].getValue()
                          + cway->params[MonsoonIds::POLY_REST_MOD_ATT_GLOBAL].getValue();
            float accAtt  = cway->params[MonsoonIds::POLY_ACCENT_MOD_ATT_1 + i].getValue()
                          + cway->params[MonsoonIds::POLY_ACCENT_MOD_ATT_GLOBAL].getValue();
            if (restIn.isConnected() && ch < restIn.getChannels())
                restMod = restIn.getVoltage(ch) * restAtt * 0.1f;
            if (accIn.isConnected() && ch < accIn.getChannels())
                accMod = accIn.getVoltage(ch) * accAtt * 0.1f;
        }
        float restEff = rack::math::clamp(restBase + restMod, 0.f, 1.f);
        float accEff  = rack::math::clamp(accBase + accMod, 0.f, 1.f);
        engine.voices[i].restProb   = restEff;
        engine.voices[i].accentProb = accEff;
        // Mirror into the mod-viz caches (read by the Straits mod arcs).
        if (mainModule && i < 15) {
            mainModule->cachedPolyRest[i]            = restBase;
            mainModule->cachedPolyRestEffective[i]   = restEff;
            mainModule->cachedPolyAccent[i]          = accBase;
            mainModule->cachedPolyAccentEffective[i] = accEff;
        }
    }
}

void ModeController::updatePatternInput() {
    for (int i = 0; i < 12; ++i) {
        // Use the SCALE-GATED weight (mainModule->getSemitoneParam → ScaleManager::getSemitoneWeight),
        // not the raw fader value. When Conservation/lock is enforced, out-of-scale semitones read 0
        // here, so the DICE/PATTERN engine (which picks from semiWeights) won't generate out-of-scale
        // notes — matching the realtime path. (Previously this used paramManager.getSemitone(i), the
        // raw value, so locked patterns still fired out-of-scale notes.)
        currentPatternInput.semiWeights[i] =
            mainModule ? mainModule->getSemitoneParam(i) : paramManager.getSemitone(i);
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
    // Junction expander: 5 big-5 CV (x attenuverter) -> offsets the param getters add.
    // CV normalised 0..10V -> 0..1, scaled bipolar by the attenuverter.
    paramManager.clearJunctionOffsets();
    if (mainModule && mainModule->expanderManager.cachedJunctionExpander) {
        rack::Module* sg = mainModule->expanderManager.cachedJunctionExpander;
        for (int i = 0; i < 5; ++i) {
            float cv  = sg->inputs[MonsoonIds::JUNCTION_NOTEVAL_CV + i].getVoltage() / 10.f;
            float att = sg->params[MonsoonIds::JUNCTION_NOTEVAL_ATT + i].getValue();
            paramManager.setJunctionOffset(i, cv * att);
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
