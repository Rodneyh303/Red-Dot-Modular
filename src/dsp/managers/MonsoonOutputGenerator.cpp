#include "MonsoonOutputGenerator.hpp"
#include "../engines/SequencerEngine.hpp"
#include "../../Monsoon.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonStraitsExpander.hpp"

using namespace rack;

// ──── Helper: Gate with mute masking ────────────────────────────────────────

void OutputGenerator::setGateWithMute_(engine::Output& out, float gateV, bool muted) {
    out.setVoltage(muted ? 0.f : gateV);
}

// ──── Main Output Generation ────────────────────────────────────────────────

void OutputGenerator::drive(SequencerEngine& engine,
                             rack::engine::Output* outputs,
                             const MonsoonExpanderManager& expanderManager,
                             float sampleTime) {
    using namespace MonsoonIds;
    
    // 1. Primary Mono Outputs
    float currentPitchV = engine.gs.currentPitchV;
    bool effectiveMuted = engine.muted || !engine.runGateActive;
    
    generateOutputs(engine, outputs, currentPitchV, effectiveMuted, sampleTime);

    // Cache gate state for summary outputs
    float gateV = outputs[GATE_OUTPUT].getVoltage();
    bool accentActive = engine.lastStepResult.accented && !effectiveMuted;
    (void)accentActive;

    // 2. Straits base poly expander — three 16-channel poly cables (gate / CV / accent).
    //    ch0 = MONO voice (voice 1) ALWAYS; ch1..15 = poly voices 2..16. Cables are always
    //    16ch wide; voices beyond numPolyVoices output gate-low / 0V. Replaces the old
    //    East(2-8)+West(9-16) 21-individual-jack split.
    auto* straits = expanderManager.cachedPolyVoiceExpander;
    if (straits) {
        using namespace StraitsIds;
        auto& gateOut   = straits->outputs[POLY_GATE_OUT];
        auto& cvOut     = straits->outputs[POLY_CV_OUT];
        auto& accentOut = straits->outputs[POLY_ACCENT_OUT];
        gateOut.setChannels(16);
        cvOut.setChannels(16);
        accentOut.setChannels(16);

        // ch0 = mono voice (voice 1), duplicated from the parent's mono path.
        gateOut.setVoltage((gateV > 5.f && !effectiveMuted) ? 10.f : 0.f, 0);
        cvOut.setVoltage(effectiveMuted ? 0.f : currentPitchV, 0);
        accentOut.setVoltage((!effectiveMuted && engine.lastStepResult.accented && gateV > 5.f) ? 10.f : 0.f, 0);

        // ch1..15 = poly voices 2..16.
        for (int i = 0; i < 15; ++i) {
            const int ch = i + 1;
            if (effectiveMuted || i >= (int)engine.numPolyVoices) {
                gateOut.setVoltage(0.f, ch);
                cvOut.setVoltage(0.f, ch);
                accentOut.setVoltage(0.f, ch);
                continue;
            }
            float vg = engine.voices[i].gs.process(sampleTime);
            gateOut.setVoltage(vg, ch);
            cvOut.setVoltage(engine.voices[i].gs.currentPitchV, ch);
            // Accent is a poly lane: each voice fires its OWN accent (drawn per-voice in
            // executePolyVoice), gated by the voice actually sounding.
            accentOut.setVoltage((engine.voices[i].accented && vg > 5.f) ? 10.f : 0.f, ch);
        }
    }
}

void OutputGenerator::generateOutputs(SequencerEngine& engine,
                                       engine::Output* outputs,
                                       float currentPitchV,
                                       bool muted,
                                       float sampleTime) {
    using namespace MonsoonIds;
    
    // Process main gate and apply mute
    float gateV = engine.runGateActive ? engine.gs.process(sampleTime) : 0.f;
    setGateWithMute_(outputs[GATE_OUTPUT], gateV, muted);

    // Set main pitch CV
    outputs[CV_OUTPUT].setVoltage(currentPitchV);
    
    // Derived outputs based on mono decision
    float tieGateV = (engine.lastStepResult.decision == MonoDecision::Tie) ? 10.f : 0.f;
    float legatoGateV = (engine.lastStepResult.decision == MonoDecision::Legato || 
                         engine.lastStepResult.decision == MonoDecision::LegatoMax) ? 10.f : 0.f;
    float accentGateV = (engine.lastStepResult.accented && gateV > 5.f) ? 10.f : 0.f;
    float tieOrLegatoV = (tieGateV > 5.f || legatoGateV > 5.f) ? 10.f : 0.f;
    
    setGateWithMute_(outputs[TIE_OUTPUT], tieGateV, muted);
    setGateWithMute_(outputs[LEGATO_OUTPUT], legatoGateV, muted);
    setGateWithMute_(outputs[TIE_OR_LEGATO_OUTPUT], tieOrLegatoV, muted);
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
                                          int numPolyVoices,
                                          bool muted,
                                          float sampleTime) {
    if (!outputs) return;
    
    // (mono accent stays on the mono ACCENT_OUTPUT; poly voices now use their OWN accent)
    
    // Output active voices
    for (int i = 0; i < numPolyVoices && i < 7; ++i) {
        float vg = engine.voices[i].gs.process(sampleTime);
        
        // Main voice gate
        setGateWithMute_(outputs[i], vg, muted);
        
        // Poly voice CV (pitch)
        float pv = engine.voices[i].gs.currentPitchV;
        if (i + 7 < 14) {  // Assume CV outputs follow gate outputs
            outputs[i + 7].setVoltage(muted ? 0.f : pv);
        }
        
        // Poly accent: each voice fires its OWN accent (poly lane), gated by sounding.
        float polyAccent = (engine.voices[i].accented && vg > 5.f) ? 10.f : 0.f;
        if (i + 14 < 21) {  // Assume accent outputs follow CV outputs
            setGateWithMute_(outputs[i + 14], polyAccent, muted);
        }
    }
    
    // Zero unused voice outputs (beyond numPolyVoices)
    for (int i = numPolyVoices; i < 7; ++i) {
        outputs[i].setVoltage(0.f);
        if (i + 7 < 14) outputs[i + 7].setVoltage(0.f);
        if (i + 14 < 21) outputs[i + 14].setVoltage(0.f);
    }
}
