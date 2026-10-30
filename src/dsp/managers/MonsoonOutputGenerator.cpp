#include "MonsoonOutputGenerator.hpp"
#include "../engines/SequencerEngine.hpp"
#include "../../Monsoon.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonStraitsExpander.hpp"
#include "../../MonsoonChangiExpander.hpp"
#include "../../MonsoonChangiT2Expander.hpp"
#include "../VoiceResolver.hpp"   // CC_PROMPT_STRONG_TYPES: VoiceSlot minting (slot)

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

    // ── UNIFIED ADDRESSING Stage 4: the OUTPUT MASK (output-gate-only) ──────────
    // The ONE output layer (the doc's containment hierarchy): a per-step play/mute
    // mask per voice — cycle = the all-on degenerate mask (today's behaviour, the
    // default), dynamic = the range-as-mask (O/L-derived = absolute-1), painted =
    // the fixed-rotatable pattern (static = absolute-2, follows-R = absolute-3).
    // Where the mask mutes the CURRENT PHRASE STEP, zero the voice's GATE + CV OUT
    // (and the articulation terminals derived from them) — NOT the modulation CV
    // inputs, NOT generation, NOT correlation: the same safe output-stage mute as
    // the run/mute gates; marginals/correlation untouched. The phrase-wrap cut rule
    // falls out of the per-step evaluation: a wrap INSIDE the mask carries the gate
    // across normally; only the mask's real end cuts; step 1 outside = cut at 16.
    // At defaults (cycle everywhere) outputMaskPasses is TRUE every step — every
    // voltage below lands bit-identically (the goldens' output path untouched).
    const int maskStep = engine.stepIndex;   // the GLOBAL phrase step (global-position space)
    const bool monoMaskPass = engine.outputMaskPasses(dotModular::VoiceResolver::mono(), maskStep);
    if (!monoMaskPass) {
        outputs[GATE_OUTPUT].setVoltage(0.f);
        outputs[CV_OUTPUT].setVoltage(0.f);
        outputs[ACCENT_OUTPUT].setVoltage(0.f);
        outputs[STEP_GATE_OUTPUT].setVoltage(0.f);
        outputs[STEP_LEGATO_GATE_OUTPUT].setVoltage(0.f);
        currentPitchV = 0.f;   // the cached local feeds the Straits/Changi ch0 writes below
    }

    // Cache gate state for summary outputs
    float gateV = outputs[GATE_OUTPUT].getVoltage();
    float stepGateV   = outputs[STEP_GATE_OUTPUT].getVoltage();         // mono STEP  (for poly ch0)
    float stepLegatoV = outputs[STEP_LEGATO_GATE_OUTPUT].getVoltage();  // mono SLEG  (for poly ch0)
    bool accentActive = engine.lastStepResult.accented && !effectiveMuted;
    (void)accentActive;

    // ── POLY VOICE PRECOMPUTE (ch1..15 = poly voices 2..16) ─────────────────────
    // gs.process() / gsStep.process() advance a PulseGenerator (1ms retrigger) and are NOT
    // idempotent within a sample: each call consumes sampleTime. Previously Straits AND Changi
    // each called process() per voice, so the poly retrigger pulse advanced TWICE per sample when
    // both were present (a latent bug; a naive Changi T2 would make it 3x). Compute each voice
    // ONCE here — exactly as the mono path precomputes in generateOutputs() — and let every
    // consumer (Straits / Changi T1 / T2) READ these cached voltages. This fixes the double-advance
    // and keeps all breakout terminals in lockstep off one source of truth.
    //
    // Semantics preserved verbatim: muted / inactive voices (i >= numPolyVoices) DO NOT call
    // process() (gate stays frozen, as before) and read out as 0V. The guard runs the precompute
    // only when at least one poly consumer is attached, so the pulse isn't advanced when nothing
    // reads it (matching the old behaviour where the blocks only ran under their own if).
    auto* straits  = expanderManager.cachedPolyVoiceExpander;
    auto* changi   = expanderManager.cachedChangiExpander;
    auto* changiT2 = expanderManager.cachedChangiT2Expander;
    const bool anyPolyConsumer = (straits != nullptr) || (changi != nullptr) || (changiT2 != nullptr);

    float polyGateV[16]   = {};
    float polyStepV[16]   = {};
    float polyCV[16]      = {};
    float polyAccentV[16] = {};
    float polySlegV[16]   = {};
    if (anyPolyConsumer) {
        // NOTE (CC_PROMPT_STRONG_TYPES audit — behaviour PRESERVED, flagged not fixed):
        // this loop indexes voices[i] for i = 0..14 against cable channels documented
        // "ch1..15 = poly voices 2..16" (V2..V16 = slots 1..15). voices[0] is V1's
        // DORMANT per-voice slot post-COLLAPSE (mono runs on eng.gs), so ch1 reads a
        // dead gate and every later channel shows the PREVIOUS voice — the suspected
        // pre-existing off-by-one this migration exists to expose. Fix belongs in its
        // own red-first commit + Rack feel-test, NOT in the behaviour-identical sweep.
        for (int i = 0; i < 15; ++i) {
            if (effectiveMuted || i >= (int)engine.numPolyVoices) {
                // Skipped voice: leave arrays at 0 and DON'T advance process() (frozen gate).
                continue;
            }
            // OFF-BY-ONE FIX (audit item 1, confirmed in Rack — Lantern row 2 missing,
            // Straits lane N drove row N+1): this loop is 0-BASED (writes polyGateV[i] →
            // ch = i+1), so ch(i+1) must show poly voice V(i+2) = VoiceSlot{i+1} = slot(i+2).
            // Was slot(i+1) = VoiceSlot{i}: ch1 read V1's DORMANT slot and V16 was never
            // output. (The other poly loops here are 1-BASED (i=1..numPolyVoices) where
            // slot(i+1) is already correct — do NOT change those.)
            const auto vs = dotModular::VoiceResolver::slot(i + 2);
            float vg     = engine.voices[vs].gs.process(sampleTime);
            float vgStep = engine.voices[vs].gsStep.process(sampleTime);   // STEP mirror (per voice)
            polyGateV[i]   = vg;
            polyStepV[i]   = vgStep;
            polyCV[i]      = engine.voices[vs].gs.currentPitchV;
            // Accent is a poly lane: each voice fires its OWN accent (drawn per-voice in
            // executePolyVoice), gated by the voice actually sounding.
            polyAccentV[i] = (engine.voices[vs].accented && vg > 5.f) ? 10.f : 0.f;
            polySlegV[i]   = engine.voices[vs].gs.slurMember ? vgStep : 0.f;  // SLEG: slur-masked
            // Stage 4: the voice's OUTPUT MASK — zero the whole terminal set where the
            // mask mutes the current phrase step (output-gate-only; the state machines
            // above ran untouched — the mute is post-process, read-side only).
            if (!engine.outputMaskPasses(vs, maskStep)) {
                polyGateV[i]   = 0.f;
                polyStepV[i]   = 0.f;
                polyCV[i]      = 0.f;
                polyAccentV[i] = 0.f;
                polySlegV[i]   = 0.f;
            }
        }
    }

    // 2. Straits base poly expander — three 16-channel poly cables (gate / CV / accent), plus the
    //    STEP and STEP-LEGATO cables. ch0 = MONO voice (voice 1) ALWAYS; ch1..15 = poly voices
    //    2..16. Cables are always 16ch wide; voices beyond numPolyVoices output gate-low / 0V.
    //    Replaces the old East(2-8)+West(9-16) 21-individual-jack split.
    if (straits) {
        using namespace StraitsIds;
        auto& gateOut   = straits->outputs[POLY_GATE_OUT];
        auto& cvOut     = straits->outputs[POLY_CV_OUT];
        auto& accentOut = straits->outputs[POLY_ACCENT_OUT];
        auto& stepOut   = straits->outputs[POLY_STEP_GATE_OUT];
        auto& slegOut   = straits->outputs[POLY_STEP_LEGATO_GATE_OUT];
        gateOut.setChannels(16);
        cvOut.setChannels(16);
        accentOut.setChannels(16);
        stepOut.setChannels(16);
        slegOut.setChannels(16);

        // ch0 = mono voice (voice 1), duplicated from the parent's mono path.
        gateOut.setVoltage((gateV > 5.f && !effectiveMuted) ? 10.f : 0.f, 0);
        cvOut.setVoltage(effectiveMuted ? 0.f : currentPitchV, 0);
        accentOut.setVoltage((!effectiveMuted && engine.lastStepResult.accented && gateV > 5.f) ? 10.f : 0.f, 0);
        stepOut.setVoltage((stepGateV   > 5.f && !effectiveMuted) ? 10.f : 0.f, 0);
        slegOut.setVoltage((stepLegatoV > 5.f && !effectiveMuted) ? 10.f : 0.f, 0);

        // ch1..15 = poly voices 2..16 (read from the precomputed arrays).
        for (int i = 0; i < 15; ++i) {
            const int ch = i + 1;
            gateOut.setVoltage(polyGateV[i], ch);
            cvOut.setVoltage(polyCV[i], ch);
            accentOut.setVoltage(polyAccentV[i], ch);
            stepOut.setVoltage(polyStepV[i], ch);
            slegOut.setVoltage(polySlegV[i], ch);
        }
    }

    // 3. Changi T1 — per-voice individual mono jacks (gate/CV/accent). Index 0 = MONO voice (voice
    //    1, from the parent mono path, matching Straits' poly-cable ch0); 1..15 = poly voices 2..16
    //    (read from the precomputed arrays — no process() calls here anymore).
    if (changi) {
        using namespace ChangiIds;
        // index 0 = mono voice
        changi->outputs[GATE_OUT_0   + 0].setVoltage((gateV > 5.f && !effectiveMuted) ? 10.f : 0.f);
        changi->outputs[CV_OUT_0     + 0].setVoltage(effectiveMuted ? 0.f : currentPitchV);
        changi->outputs[ACCENT_OUT_0 + 0].setVoltage((!effectiveMuted && engine.lastStepResult.accented && gateV > 5.f) ? 10.f : 0.f);
        // indices 1..15 = poly voices 2..16 (engine.voices[0..14])
        for (int i = 0; i < 15; ++i) {
            const int out = i + 1;
            changi->outputs[GATE_OUT_0   + out].setVoltage(polyGateV[i]);
            changi->outputs[CV_OUT_0     + out].setVoltage(polyCV[i]);
            changi->outputs[ACCENT_OUT_0 + out].setVoltage(polyAccentV[i]);
        }
    }

    // 4. Changi T2 — per-voice ARTICULATION jacks (step gate / step legato). Index 0 = MONO voice
    //    (voice 1, from the parent mono path, matching Straits' poly-cable ch0 STEP/SLEG); 1..15 =
    //    poly voices 2..16 (read from the precomputed arrays). Mirrors Straits' STEP/SLEG cables
    //    split per-voice, exactly as T1 mirrors GATE/CV/ACCENT.
    if (changiT2) {
        using namespace ChangiT2Ids;
        // index 0 = mono voice (same masking as Straits' ch0 STEP / SLEG).
        changiT2->outputs[STEP_GATE_OUT_0   + 0].setVoltage((stepGateV   > 5.f && !effectiveMuted) ? 10.f : 0.f);
        changiT2->outputs[STEP_LEGATO_OUT_0 + 0].setVoltage((stepLegatoV > 5.f && !effectiveMuted) ? 10.f : 0.f);
        // indices 1..15 = poly voices 2..16 (engine.voices[0..14])
        for (int i = 0; i < 15; ++i) {
            const int out = i + 1;
            changiT2->outputs[STEP_GATE_OUT_0   + out].setVoltage(polyStepV[i]);
            changiT2->outputs[STEP_LEGATO_OUT_0 + out].setVoltage(polySlegV[i]);
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
    // gsStep is the mono STEP mirror-GateState: it receives the SAME ticks/pulses as gs but
    // every continuation is re-struck (triggerNote) instead of fused (slide/extend), so its
    // gate falls+rises at every sub-note boundary. Gated by runGateActive exactly like gs so
    // the two countdowns stay in lockstep (process() advances the retrigger pulse). See
    // STEP_GATE_IMPLEMENTATION.md.
    float stepGateV = engine.runGateActive ? engine.gsStep.process(sampleTime) : 0.f;

    // ── STEP LEGATO GATE (SLEG): STEP GATE masked to slurred notes only (silent on isolated). ──
    // gsStep gated by slurMember (= this note leads OR continues a slur; set at the articulation
    // site). Isolated notes have slurMember=false → silent; every note of a slur (lead + all
    // continuations) masks in. See STEP_GATE_IMPLEMENTATION.md (SLUR GATE section).
    float stepLegatoV = engine.gs.slurMember ? stepGateV : 0.f;

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
    for (int i = 1; i <= numPolyVoices && i <= 7; ++i) {
        float vg = engine.voices[dotModular::VoiceResolver::slot(i + 1)].gs.process(sampleTime);
        
        // Main voice gate
        setGateWithMute_(outputs[i], vg, muted);
        
        // Poly voice CV (pitch)
        float pv = engine.voices[dotModular::VoiceResolver::slot(i + 1)].gs.currentPitchV;
        if (i + 7 < 14) {  // Assume CV outputs follow gate outputs
            outputs[i + 7].setVoltage(muted ? 0.f : pv);
        }
        
        // Poly accent: each voice fires its OWN accent (poly lane), gated by sounding.
        float polyAccent = (engine.voices[dotModular::VoiceResolver::slot(i + 1)].accented && vg > 5.f) ? 10.f : 0.f;
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
