#include "MonsoonOutputGenerator.hpp"
#include "../engines/SequencerEngine.hpp"
#include "../../Monsoon.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonStraitsEastExpander.hpp"
#include "../../MonsoonStraitWestExpander.hpp"

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

    // 2. Straits East Expander (Voices 2-8)
    auto* polyExp = expanderManager.cachedPolyVoiceExpander;
    if (polyExp && engine.numPolyVoices > 0) {
        using namespace PolyVoiceExpanderIds;
        drivePolyExpanderSection_(engine,
                                  polyExp->outputs,
                                  0,
                                  7,
                                  effectiveMuted,
                                  gateV,
                                  currentPitchV,
                                  POLY_GATE_OUT_1,
                                  POLY_CV_OUT_1,
                                  POLY_ACCENT_OUT_1,
                                  POLY_GATE_1_8_OUT,
                                  POLY_CV_1_8_OUT,
                                  sampleTime);
    }

    // 3. Straits West Expander (Voices 9-16)
    auto* westExp = expanderManager.cachedStraitWestExpander;
    if (westExp && engine.numPolyVoices > 7) {
        using namespace StraitWestExpanderIds;
        drivePolyExpanderSection_(engine,
                                  westExp->outputs,
                                  7,
                                  8,
                                  effectiveMuted,
                                  gateV,
                                  currentPitchV,
                                  POLY_GATE_OUT_1,
                                  POLY_CV_OUT_1,
                                  POLY_ACCENT_OUT_1,
                                  POLY_GATE_1_16_OUT,
                                  POLY_CV_1_16_OUT,
                                  sampleTime);
    }
}

void OutputGenerator::drivePolyExpanderSection_(SequencerEngine& engine,
                                                std::vector<rack::engine::Output>& expanderOutputs,
                                                int voiceOffset,
                                                int slotCount,
                                                bool effectiveMuted,
                                                float gateV,
                                                float currentPitchV,
                                                int gateOutBase,
                                                int cvOutBase,
                                                int accentOutBase,
                                                int summaryGateOut,
                                                int summaryCvOut,
                                                float sampleTime) {
    for (int i = 0; i < slotCount; ++i) {
        int vIdx = voiceOffset + i;
        if (effectiveMuted || vIdx >= (int)engine.numPolyVoices) {
            expanderOutputs[gateOutBase + i].setVoltage(0.f);
            expanderOutputs[accentOutBase + i].setVoltage(0.f);
            continue;
        }

        float vg = engine.voices[vIdx].gs.process(sampleTime);
        expanderOutputs[gateOutBase + i].setVoltage(vg);
        expanderOutputs[cvOutBase + i].setVoltage(engine.voices[vIdx].gs.currentPitchV);
        expanderOutputs[accentOutBase + i].setVoltage((!effectiveMuted && engine.voices[vIdx].accented && vg > 5.f) ? 10.f : 0.f);
    }

    expanderOutputs[summaryGateOut].setVoltage((gateV > 5.f && !effectiveMuted) ? 10.f : 0.f);
    expanderOutputs[summaryCvOut].setVoltage(effectiveMuted ? 0.f : currentPitchV);
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
