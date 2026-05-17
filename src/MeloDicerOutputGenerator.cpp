#include "MeloDicerOutputGenerator.hpp"
#include "SequencerEngine.hpp"
#include "MeloDicer.hpp"

using namespace rack;

// ──── Helper: Gate with mute masking ────────────────────────────────────────

void OutputGenerator::setGateWithMute_(engine::Output& out, float gateV, bool muted) {
    out.setVoltage(muted ? 0.f : gateV);
}

// ──── Main Output Generation ────────────────────────────────────────────────

void OutputGenerator::generateOutputs(SequencerEngine& engine,
                                       engine::Output* outputs,
                                       float currentPitchV,
                                       bool muted) {
    using namespace MeloDicerIds;
    
    // Process main gate and apply mute
    float gateV = engine.gs.process(0.f);  // sampleTime not needed for state read
    setGateWithMute_(outputs[GATE_OUTPUT], gateV, muted);
    
    // Derived outputs based on mono decision
    float tieGateV = (engine.lastStepResult.decision == MonoDecision::Tie) ? 10.f : 0.f;
    float legatoGateV = (engine.lastStepResult.decision == MonoDecision::Legato || 
                         engine.lastStepResult.decision == MonoDecision::LegatoMax) ? 10.f : 0.f;
    float accentGateV = (engine.lastStepResult.accented && gateV > 5.f) ? 10.f : 0.f;
    
    setGateWithMute_(outputs[TIE_OUTPUT], tieGateV, muted);
    setGateWithMute_(outputs[LEGATO_OUTPUT], legatoGateV, muted);
    setGateWithMute_(outputs[ACCENT_OUTPUT], accentGateV, muted);
}

void OutputGenerator::setGateOutput(engine::Output& gateOut, float gateV, bool muted) {
    setGateWithMute_(gateOut, gateV, muted);
}

void OutputGenerator::setTieGateOutput(engine::Output& tieGateOut, bool isTie) {
    tieGateOut.setVoltage(isTie ? 10.f : 0.f);
}

void OutputGenerator::setAccentGateOutput(engine::Output& accentGateOut, bool isAccented) {
    accentGateOut.setVoltage(isAccented ? 10.f : 0.f);
}

void OutputGenerator::setPolyVoiceOutputs(engine::Output* outputs,
                                          SequencerEngine& engine,
                                          int numPolyVoices) {
    if (!outputs) return;
    
    // Mono decision for accent gating
    bool monoAccented = engine.lastStepResult.accented;
    
    // Output active voices
    for (int i = 0; i < numPolyVoices && i < 7; ++i) {
        float vg = engine.voices[i].gs.process(0.f);  // sampleTime not needed
        
        // Main voice gate
        outputs[i].setVoltage(vg);
        
        // Poly voice CV (pitch)
        float pv = engine.voices[i].gs.currentPitchV;
        if (i + 7 < 14) {  // Assume CV outputs follow gate outputs
            outputs[i + 7].setVoltage(pv);
        }
        
        // Poly accent: fires when mono is accented AND this voice is sounding
        float polyAccent = (monoAccented && vg > 5.f) ? 10.f : 0.f;
        if (i + 14 < 21) {  // Assume accent outputs follow CV outputs
            outputs[i + 14].setVoltage(polyAccent);
        }
    }
    
    // Zero unused voice outputs (beyond numPolyVoices)
    for (int i = numPolyVoices; i < 7; ++i) {
        outputs[i].setVoltage(0.f);
        if (i + 7 < 14) outputs[i + 7].setVoltage(0.f);
        if (i + 14 < 21) outputs[i + 14].setVoltage(0.f);
    }
}
