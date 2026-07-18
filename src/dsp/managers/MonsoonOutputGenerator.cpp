#include "MonsoonOutputGenerator.hpp"
#include "../engines/SequencerEngine.hpp"
#include "../../Monsoon.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonStraitsExpander.hpp"
#include "../../MonsoonChangiExpander.hpp"

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

    // 3. Changi — per-voice individual mono jacks (gate/CV/accent). Index 0 = MONO voice (voice 1,
    //    from the parent mono path, matching Straits' poly-cable ch0); 1..15 = poly voices 2..16.
    auto* changi = expanderManager.cachedChangiExpander;
    if (changi) {
        using namespace ChangiIds;
        // index 0 = mono voice
        changi->outputs[GATE_OUT_0   + 0].setVoltage((gateV > 5.f && !effectiveMuted) ? 10.f : 0.f);
        changi->outputs[CV_OUT_0     + 0].setVoltage(effectiveMuted ? 0.f : currentPitchV);
        changi->outputs[ACCENT_OUT_0 + 0].setVoltage((!effectiveMuted && engine.lastStepResult.accented && gateV > 5.f) ? 10.f : 0.f);
        // indices 1..15 = poly voices 2..16 (engine.voices[0..14])
        for (int i = 0; i < 15; ++i) {
            const int out = i + 1;
            if (effectiveMuted || i >= (int)engine.numPolyVoices) {
                changi->outputs[GATE_OUT_0   + out].setVoltage(0.f);
                changi->outputs[CV_OUT_0     + out].setVoltage(0.f);
                changi->outputs[ACCENT_OUT_0 + out].setVoltage(0.f);
                continue;
            }
            float vg = engine.voices[i].gs.process(sampleTime);
            changi->outputs[GATE_OUT_0   + out].setVoltage(vg);
            changi->outputs[CV_OUT_0     + out].setVoltage(engine.voices[i].gs.currentPitchV);
            changi->outputs[ACCENT_OUT_0 + out].setVoltage((engine.voices[i].accented && vg > 5.f) ? 10.f : 0.f);
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

    float accentGateV = (engine.lastStepResult.accented && gateV > 5.f) ? 10.f : 0.f;

    // ── STEP GATE: the un-fused gate (legato/tie removed; every sub-note articulated). ──
    // Replaces the three retired TIE/LEGATO/TIE_OR_LEGATO outputs, which emitted a join
    // CLASSIFICATION from lastStepResult.decision. STEP GATE is NOT a classification: it is
    // the gate BEFORE legato drops its edges -- see LEGATO_TIE_MODEL_NOTE.md.
    //
    // TODO(engine, MSYS2): emit the pre-legato-drop gate. The current gateV already has the
    // legato drop APPLIED (edges fused across held notes). STEP GATE needs the gate as it
    // would be if legato/tie did nothing: a fresh gate edge at every sub-step boundary,
    // each carrying its own note length. This requires the engine to TRACK that pre-drop
    // gate (a second gate-state, or a per-step "would-articulate" flag recorded at the point
    // legato decides to suppress an edge) rather than reconstructing it here. Until that
    // exists, STEP GATE mirrors the fused gate (safe: identical to GATE on non-legato
    // patterns; only wrong DURING legato, where it under-articulates rather than misfires).
    float stepGateV = gateV;   // PLACEHOLDER — see TODO above; replace with pre-drop gate.

    // ── STEP LEGATO GATE: STEP GATE masked to slurred notes only (silent on isolated). ──
    // = gsStep gated by (slurForward OR prevSlur) -- the articulations INSIDE a slur, lead
    // and continuations both. ENGINE EMISSION PENDING (see STEP_GATE_IMPLEMENTATION.md, SLUR
    // GATE section). Placeholder emits 0 (never falsely fires); real emission masks stepGateV
    // by the slur commitment. 0 is safe: STEP LEGATO is a strict subset of STEP, so an
    // all-zero placeholder is silent, not wrong-signal.
    float stepLegatoV = 0.f;   // PLACEHOLDER — mask stepGateV by slurForward||prevSlur.

    setGateWithMute_(outputs[STEP_GATE_OUTPUT], stepGateV, muted);
    setGateWithMute_(outputs[STEP_LEGATO_GATE_OUTPUT], stepLegatoV, muted);
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
