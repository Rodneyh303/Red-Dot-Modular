#include "MeloDicerModeController.hpp"

using namespace rack;

// ──── Helper: Update poly voice rest probabilities ──────────────────────────

void ModeController::updatePolyVoiceRest_() {
    if (engine.numPolyVoices > 0) {
        for (int i = 0; i < engine.numPolyVoices; ++i) {
            engine.voices[i].restProb = paramManager.getPolyRest(i);
        }
    }
}

// ──── Helper: Post-execution logic ──────────────────────────────────────────

void ModeController::postExecute_(const StepResult& result, PhraseCallback onPhraseBoundary) {
    // Handle phrase boundary
    if (result.wrapped && onPhraseBoundary) {
        onPhraseBoundary();
    }
    
    // Execute poly voices if step was taken
    if (result.stepped && engine.numPolyVoices > 0) {
        updatePolyVoiceRest_();
        // Note: makePatternInput() is called by the caller
        // This just updates rest probabilities; executePolyVoices happens in process()
    }
}

// ──── Mode A: Clock-Driven Sequencing ───────────────────────────────────────

bool ModeController::executeModeA(const engine::Module::ProcessArgs& args,
                                   PhraseCallback onPhraseBoundary) {
    if (clock.sixteenthEdge) {
        // Fetch current parameters
        engine.accentProb = paramManager.getAccent();
        
        // Execute the mode
        StepResult result = engine.executeModeA(
            clock,
            paramManager.getRest(),
            paramManager.getLegato(),
            paramManager.getNoteValue(),
            PatternInput{/* populated by engine */}
        );
        
        // Handle post-execution
        postExecute_(result, onPhraseBoundary);
        updateLastStepIndex();
        
        return result.stepped;
    }
    return false;
}

// ──── Mode B: Gate-Driven Sequencing ────────────────────────────────────────

bool ModeController::executeModeB(const engine::Module::ProcessArgs& args,
                                   bool gate1Rise,
                                   bool gate1High,
                                   PhraseCallback onPhraseBoundary) {
    if (gate1Rise || (gate1High && engine.stepIndex == -1)) {
        // Fetch current parameters
        engine.accentProb = paramManager.getAccent();
        
        // Execute the mode
        StepResult result = engine.executeModeB(
            gate1Rise,
            gate1High,
            paramManager.getRest(),
            paramManager.getLegato(),
            paramManager.getNoteValue(),
            PatternInput{/* populated by engine */}
        );
        
        // Handle post-execution
        postExecute_(result, onPhraseBoundary);
        updateLastStepIndex();
        
        return result.stepped;
    }
    return false;
}

// ──── Mode C: Quantizer Mode 1 ──────────────────────────────────────────────

bool ModeController::executeModeC(const engine::Module::ProcessArgs& args,
                                   float cv2Voltage,
                                   PhraseCallback onPhraseBoundary) {
    if (clock.quarterEdge) {
        // Clamp and validate CV2 input
        float inCV = clampv<float>(cv2Voltage, 0.f, 5.f);
        
        // Execute the mode
        StepResult result = engine.executeModeC(clock, inCV);
        
        // Handle post-execution (usually minimal for quantizer modes)
        if (result.wrapped && onPhraseBoundary) {
            onPhraseBoundary();
        }
        
        updateLastStepIndex();
        return result.stepped;
    }
    return false;
}

// ──── Mode D: Quantizer Mode 2 ──────────────────────────────────────────────

bool ModeController::executeModeD(const engine::Module::ProcessArgs& args,
                                   bool gate2High,
                                   float cv2Voltage,
                                   PhraseCallback onPhraseBoundary) {
    // Mode D executes continuously based on gate2 state and CV2 voltage
    // (no edge detection needed)
    
    // Clamp and validate CV2 input
    float inCV = clampv<float>(cv2Voltage, 0.f, 5.f);
    
    // Execute the mode
    StepResult result = engine.executeModeD(gate2High, inCV);
    
    // Handle post-execution
    if (result.wrapped && onPhraseBoundary) {
        onPhraseBoundary();
    }
    
    if (result.stepped) {
        updateLastStepIndex();
    }
    
    return result.stepped;
}

// ──── High-Level Dispatcher ─────────────────────────────────────────────────

bool ModeController::executeMode(int modeId,
                                  const engine::Module::ProcessArgs& args,
                                  bool gate1Rise,
                                  bool gate1High,
                                  bool gate2High,
                                  float cv2Voltage,
                                  PhraseCallback onPhraseBoundary) {
    switch (modeId) {
        case 0: return executeModeA(args, onPhraseBoundary);
        case 1: return executeModeB(args, gate1Rise, gate1High, onPhraseBoundary);
        case 2: return executeModeC(args, cv2Voltage, onPhraseBoundary);
        case 3: return executeModeD(args, gate2High, cv2Voltage, onPhraseBoundary);
        default: return false;
    }
}
