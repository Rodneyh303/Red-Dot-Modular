#include "MonsoonModeController.hpp"
#include "MonsoonLockManager.hpp"

// LOCK_SCOPE_MENU: SequencerEngine::dirLive_ hard-codes the Sands scope-bit values (to stay
// include-free of this header). Pin them to the ScopeBit enum here, where BOTH are visible, so a
// renumber of ScopeBit fails the build instead of silently desyncing the engine's direction gate.
static_assert((uint32_t)dotModular::SB_SANDS_R == (1u << 2), "dirLive_ kSandsR out of sync with ScopeBit");
static_assert((uint32_t)dotModular::SB_SANDS_M == (1u << 3), "dirLive_ kSandsM out of sync with ScopeBit");
// Dice bits: updatePatternInput below hard-codes 1<<8 / 1<<9 for diceLiveR/M (PatternInput has no
// LockManager include). Pin them here too.
static_assert((uint32_t)dotModular::SB_DICE_R == (1u << 8), "diceLiveR bit out of sync with ScopeBit");
static_assert((uint32_t)dotModular::SB_DICE_M == (1u << 9), "diceLiveM bit out of sync with ScopeBit");
// q-mix own axis: updatePatternInput hard-codes 1<<11 (diceLiveQ) and the A/B latch hard-codes 1<<10
// (SB_ABRESEED_Q); Monsoon.cpp handleRestart also hard-codes 1<<10. Pin all three here.
static_assert((uint32_t)dotModular::SB_DICE_Q == (1u << 11), "diceLiveQ bit out of sync with ScopeBit");
static_assert((uint32_t)dotModular::SB_ABRESEED_Q == (1u << 10), "abReseedQ bit out of sync with ScopeBit");
// q-mix Change Alley + Sands DNA own axes: the manager gates hard-code 1<<12 / 1<<13. Pin them.
static_assert((uint32_t)dotModular::SB_CA_Q == (1u << 12), "SB_CA_Q bit out of sync with ScopeBit");
static_assert((uint32_t)dotModular::SB_SANDS_Q == (1u << 13), "SB_SANDS_Q bit out of sync with ScopeBit");
#include "../../Monsoon.hpp"
#include "../../MonsoonCausewayPolyExpander.hpp"
#include "../../MonsoonStraitsExpander.hpp"   // Q2: poly quantiser CV-in (StraitsIds::QUANT_CV_INPUT)
#include "MonsoonExpanderManager.hpp"

using namespace rack;

// ──── Helper: Update poly voice rest probabilities ──────────────────────────

void ModeController::updatePolyVoiceRest_() {
    // Write the engine's per-voice decision cache from the SINGLE resolver on Monsoon
    // (getEffectivePolyRest/Accent = knob + Causeway CV × att, clamped). The engine reads
    // voices[i].restProb per-sample in its hot loop, so it needs the value in the struct — but this
    // is the ONLY writer, sourced from the one resolver, applied right before executePolyVoices.
    // The Straits mod arcs pull from the same resolver directly (no cached-effective copies), so
    // there is nothing to drift or clobber.
    if (engine.numPolyVoices <= 0 || !mainModule) return;
    // LOCK Phase 2 (§9): poly REST/ACCENT are Tier V LATCH, same class as mono Big-5. Being the SOLE
    // writer, skipping this refresh under lock HOLDS the pre-lock per-voice values (identical to the
    // mono write-gate). BigFive category covers poly Big-5 too. The prime forces a first populate even
    // under lock (voices[] default to 0.0 and aren't persisted — a locked-load would otherwise rest all
    // poly voices). Once primed, obey the lock.
    if (polyVoiceCachePrimed_
        && !dotModular::LockManager::liveNow(dotModular::Control::BigFive, engine.locked, engine.scopeLiveMask)) return;
    for (int i = 0; i < engine.numPolyVoices; ++i) {
        engine.voices[i].restProb   = mainModule->getEffectivePolyRest(i);
        engine.voices[i].accentProb = mainModule->getEffectivePolyAccent(i);
        // Task 4 (poly QMIX): per-voice q-mix LEVEL, mirroring rest/accent. The engine reads
        // voices[i].qmixLevel per-step in executePolyVoice's source-select. getEffectivePolyQmix
        // is the single resolver (Straits knob; no Causeway q-mix CV yet).
        engine.voices[i].qmixLevel  = mainModule->getEffectivePolyQmix(i);
    }
    polyVoiceCachePrimed_ = true;
}

void ModeController::updatePatternInput() {
    // LOCK Phase 2 (LOCK_SEMANTICS §9): NoteSliders (semiWeights) + OctaveRange (octaveLo/Hi) LATCH.
    // These fields feed genPitchLive at EXECUTION time (SequencerEngine.cpp:434), which re-maps the
    // frozen draw arrays live — so unlike the redraw material they are NOT frozen by the engine's
    // lock. Latching them = HOLD the pre-lock value by skipping this per-block refresh (the struct
    // field persists across blocks, mirroring the LOR engine-state push-gate). liveNow(LATCH)==!locked.
    //   pitchLive = update NoteSliders/OctaveRange this block?
    //   The one-shot prime forces a populate on block 1 even under lock, because lock STATE is
    //   persisted but these struct fields are not (a patch saved+loaded locked would otherwise blank
    //   pitch — see patternInputPrimed_ in the header).
    const bool pitchLive = !patternInputPrimed_
                         || dotModular::LockManager::liveNow(dotModular::Control::NoteSliders, engine.locked, engine.scopeLiveMask);
    const bool octLive   = !patternInputPrimed_
                         || dotModular::LockManager::liveNow(dotModular::Control::OctaveRange, engine.locked, engine.scopeLiveMask);
    // BigFive rhythm axis (REST / VARIATION / LEGATO / NOTE_VALUE / ACCENT) — LATCH together (§9).
    // Same prime-aware hold as the pitch axis. ACCENT was normalised onto PatternInput in this same
    // pass (was the scattered engine.accentProb), so all five Big-5 members now latch through one gate.
    const bool rhythmLive = !patternInputPrimed_
                         || dotModular::LockManager::liveNow(dotModular::Control::BigFive, engine.locked, engine.scopeLiveMask);
    if (pitchLive) {
        // MODEL A (Micro-12, microtonal Phase 2): when a mask-authoring tuning expander is claimed, the
        // SCALE MASK comes from the shared TuningTable.weight[] (the Micro's 12 faders) INSTEAD of
        // Monsoon's own ScaleManager-gated faders — this is the delegation that greys Monsoon's faders.
        // When NOT authored (no Micro, or a Sikit which writes cents only), the mask stays with Monsoon
        // exactly as before → byte-identical. The pitchLive latch (lock) is unchanged: we only swap the
        // SOURCE of the per-degree weight, not when it's sampled.
        const bool micro = engine.pe.tuning.maskAuthored;
        if (micro) {
            // A mask-authoring Micro owns tuning.N degrees (12 for Colonnades, up to 24 for Colonnades
            // Duo). Copy all N weights; zero the tail so a shrink (e.g. Duo→12-Micro swap) can't leave
            // stale high-degree weights that pickSemitone (summed to tuning.N) would… it won't read the
            // tail, but keep semiWeights clean for the LED/quantizer consumers that scan MAXN.
            // enabled[] is the MASK (ENABLED_MASK_BUILD_BRIEF): an out-of-scale degree reads 0 REGARDLESS
            // of its stored weight (which is now pure loudness). In-scale degrees pass their weight, so a
            // fader turned to 0 stays in-scale and raisable (the round-8/9 freeze bug is gone). Tail 0.
            const int nD = engine.pe.tuning.N;
            for (int i = 0; i < dotModular::TuningTable::MAXN; ++i)
                currentPatternInput.semiWeights[i] =
                    (i < nD && engine.pe.tuning.enabled[i]) ? engine.pe.tuning.weight[i] : 0.f;
        } else {
            // Host path: Monsoon's own 12 faders (byte-identical to the legacy loop). Degrees 12..23
            // stay 0 (N=12), so pickSemitone/quantize behave exactly as before.
            for (int i = 0; i < 12; ++i)
                currentPatternInput.semiWeights[i] =
                    mainModule ? mainModule->getSemitoneParam(i) : paramManager.getSemitone(i);
            for (int i = 12; i < dotModular::TuningTable::MAXN; ++i)
                currentPatternInput.semiWeights[i] = 0.f;
        }
    }
    if (rhythmLive) {   // BigFive LATCH — hold REST/VARIATION/LEGATO/NOTE_VALUE under lock
        // MONO rest: use the Causeway-modulated effective value (mirrors the poly idiom
        // engine.voices[i].restProb = mainModule->getEffectivePolyRest(i) above). Falls back to the
        // raw param when there's no mainModule.
        currentPatternInput.restProb      = mainModule ? mainModule->getEffectiveMonoRest(paramManager.getRestUnclamped())
                                                       : paramManager.getRest();
        engine.writeLedger.noteWrite(WriteRole::MONO, WriteField::RestProb); // STEP1 WriteLedger: mono currentPatternInput.restProb (R2)
        currentPatternInput.variationAmount = paramManager.getVariation();
        // LEGATO + NOTE_VALUE now staged here (were passed live at the executeMode call sites); the
        // call sites read in.legato / in.noteValue so the lock hold applies to them too.
        currentPatternInput.legato        = paramManager.getLegato();
        currentPatternInput.noteValue     = paramManager.getNoteValue();
        // ACCENT: single writer now (was engine.accentProb written at 3 sites — control-rate plus a
        // redundant re-fetch in executeModeE/A). Causeway-modulated effective value, mirroring rest.
        currentPatternInput.accentProb    = mainModule ? mainModule->getEffectiveMonoAccent(paramManager.getAccentUnclamped())
                                                       : paramManager.getAccent();
        // Task 4 (QMIX): mono q-mix threshold level — Causeway-modulated effective value, mirroring
        // rest/accent above. The Causeway QMIX_CV_INPUT (ch0 = mono) attenuated by MONO_QMIX_ATT +
        // the global is added to the QMIX_LEVEL_PARAM knob, then clamped to [0,1]. Falls back to the
        // raw param when there's no mainModule.
        currentPatternInput.qmixLevel     = mainModule ? mainModule->getEffectiveMonoQmix(paramManager.getQmixLevel())
                                                       : paramManager.getQmixLevel();
    }
    if (octLive) {   // OctaveRange LATCH — hold OCT LO/HI under lock (see pitch-axis note above)
        currentPatternInput.octaveLo      = paramManager.getOctaveLo();
        currentPatternInput.octaveHi      = paramManager.getOctaveHi();
    }
    // TRANSPOSE is LIVE (LOCK_SEMANTICS §9: post-generation output mapping) — it MUST keep refreshing
    // under lock so the frozen pattern still transposes audibly. Do NOT move it inside a lock gate.
    currentPatternInput.transpose         = paramManager.getTranspose();
    currentPatternInput.noteVariationMask = engine.noteVariationMask;
    currentPatternInput.locked            = engine.locked;
    // Dice-scope (LOCK_SCOPE_MENU §6): may each dice stream draw under lock? Uses the Dice_R/M bits.
    // Under whole-module lock (mask 0) both are false => dice fully frozen, the pre-menu behaviour.
    currentPatternInput.diceLiveR = engine.locked
        && (engine.scopeLiveMask & (1u << 8)) != 0;   // == dotModular::SB_DICE_R
    currentPatternInput.diceLiveM = engine.locked
        && (engine.scopeLiveMask & (1u << 9)) != 0;   // == dotModular::SB_DICE_M
    currentPatternInput.diceLiveQ = engine.locked
        && (engine.scopeLiveMask & (1u << 11)) != 0;  // == dotModular::SB_DICE_Q (q-mix own axis)
    currentPatternInput.rhythmSlew        = paramManager.getRhythmSlew();
    currentPatternInput.melodySlew        = paramManager.getMelodySlew();
    currentPatternInput.qmixSlew          = paramManager.getQmixSlew();   // Task 4 (QMIX)
    currentPatternInput.rhythmMix         = paramManager.getRhythmMix();
    currentPatternInput.melodyMix         = paramManager.getMelodyMix();
    currentPatternInput.qmixMix           = paramManager.getQmixMix();    // Task 4 (QMIX)
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
    // A/B mix scope (LOCK_SCOPE_MENU): latchMix now gates each stream independently, so the freeze is
    // EXACT per axis — rhythm A/B and melody A/B latch/free on their own bits (no combined coupling).
    {
        const bool abR = dotModular::LockManager::liveNow(dotModular::Control::ABMix, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/false);
        const bool abM = dotModular::LockManager::liveNow(dotModular::Control::ABMix, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/true);
        // q-mix A/B latches on its OWN axis (SB_ABRESEED_Q), not melody's. ABMix is LATCH, so this is
        // !locked OR the q-mix bit opted live — evaluated directly on the bit (scopeBitFor's melodyAxis
        // bool can't express a third axis).
        const bool abQ = !engine.locked
                       || (engine.scopeLiveMask & (1u << 10)) != 0;   // == dotModular::SB_ABRESEED_Q
        if (abR || abM || abQ)
            engine.pe.latchMix(currentPatternInput.rhythmMix,
                               currentPatternInput.melodyMix,
                               currentPatternInput.qmixMix,
                               currentPatternInput.rhythmSlew,
                               currentPatternInput.melodySlew,
                               currentPatternInput.qmixSlew,
                               /*applyRhythm=*/abR, /*applyMelody=*/abM, /*applyQmix=*/abQ);
    }
    if (mainModule) {
        // seedConnected IS read elsewhere (realtime !seedConnected checks). The former
        // per-block seedSampleValue sample was DEAD CODE (written here, never consumed) —
        // the "continuous reseed" path it fed was never implemented. Removed; keep the bool.
        currentPatternInput.seedConnected = mainModule->inputs[MonsoonIds::SEED_INPUT].isConnected();
    }
    // First populate done (unconditionally, even under lock). From here the LATCH gates above obey
    // the lock. See patternInputPrimed_ in the header for why the prime is needed (persisted lock
    // state vs non-persisted currentPatternInput fields).
    patternInputPrimed_ = true;
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
    // ACCENT is now assembled once in updatePatternInput() (in.accentProb) — the redundant re-fetch
    // that used to live here is gone (single-writer; see PatternInput::accentProb note).
    PatternInput in = assemblePatternInput_();

    ClockEngine phaseView;            // edge-only view; executeModeA reads sixteenthEdge
    phaseView.sixteenthEdge = true;

    StepResult result = engine.executeModeA(
        phaseView,
        in.restProb,
        in.legato,          // BigFive LATCH: staged in updatePatternInput (was paramManager.getLegato())
        in.noteValue,       // BigFive LATCH: staged in updatePatternInput (was paramManager.getNoteValue())
        in,
        phaseReverse ? -1 : +1        // within-draw reverse traversal
    );
    postExecute_(result);
    updateLastStepIndex();
    return result.stepped;
}

bool ModeController::executeModeA() {
    if (clock.sixteenthEdge) {
        // ACCENT assembled once in updatePatternInput() (in.accentProb) — redundant re-fetch removed.
        // Ensure pattern input is fresh
        PatternInput in = assemblePatternInput_();

        // Execute the mode
        StepResult result = engine.executeModeA(
            clock,
            in.restProb,
            in.legato,          // BigFive LATCH: staged in updatePatternInput (was paramManager.getLegato())
            in.noteValue,       // BigFive LATCH: staged in updatePatternInput (was paramManager.getNoteValue())
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

bool ModeController::executeModeB(const InputState& input,
                                   bool useSubGate) {
    const bool gate1Rise = input.gate1Rise;
    const bool gate1High = input.gate1 >= 1.0f;
    // subGate subdivision (GATE mode): three edge streams — main (gate1) rise, ratchet (Gate 2)
    // in-gate, ghost (Gate 3, normalled to ratchet) in-gap.  The dispatch (Monsoon.cpp) gated
    // shouldExecute on whichever edge fired; pass all three + the ghost level to the engine.
    if (useSubGate) {
        PatternInput in = assemblePatternInput_();
        StepResult result = engine.executeModeBSubdivided(gate1Rise, gate1High, input.subGateRise,
                                                          in.restProb, in.legato, in.noteValue, in,
                                                          input.ghostRise, input.ghostHigh);
        postExecute_(result);
        updateLastStepIndex();
        return result.stepped;
    }
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
            modeBPatternInput.legato,   // BigFive LATCH: staged in updatePatternInput (was paramManager.getLegato())
            // Note value (which influences note length) should have no impact.
            // Pass a neutral value (e.g., 2.f for 1/4 note, a common default).
            0.f,
            modeBPatternInput // Pass the modified PatternInput
        );
        
        // Handle post-execution
        postExecute_(result);
        updateLastStepIndex();
        
        return result.stepped;
    }
    return false;
}

// ──── Quantiser source setup (Q1b + Q2) ─────────────────────────────────────
// Load the engine's quantiser pitch-source for a step: turn ON quantiserPitchSource and fill the
// per-voice CV (index 0 = mono/voice 1, 1..15 = poly voices 2..16 — the quantiserCV[] layout matches
// Straits' poly-cable channel convention exactly).
//
// SOURCE PRECEDENCE (Q2 — poly CV in on Straits):
//   1. If a Straits expander is attached AND its QUANT_CV_INPUT is patched → read it PER-CHANNEL
//      (getPolyVoltage: a 1-channel cable normals to every voice, so a mono source still drives all
//      voices in unison; a 16ch cable gives each voice its OWN pitch → true poly quantise).
//   2. Otherwise → fall back to Monsoon's own mono CV2 jack (cv2Voltage), filled to all voices.
// Either way the poly voices STILL differentiate their phrasing (rest/legato/accent) via the Sands
// rules — Q2 additionally lets them differentiate their PITCH. The flag stays on across executeMode*
// AND postExecute_ (poly voices draw pitch there), cleared by the caller after.
void ModeController::beginQuantiserSource_(float cv2Voltage) {
    engine.quantiserPitchSource = true;

    MonsoonStraitsExpander* straits =
        mainModule ? mainModule->expanderManager.cachedPolyVoiceExpander : nullptr;
    if (straits && straits->inputs[StraitsIds::QUANT_CV_INPUT].isConnected()) {
        // Per-voice: ch v feeds engine voice v (0 = mono). getPolyVoltage normals a mono cable to all.
        rack::engine::Input& in = straits->inputs[StraitsIds::QUANT_CV_INPUT];
        for (int v = 0; v < 16; ++v)
            engine.quantiserCV[v] = clampv<float>(in.getPolyVoltage(v), 0.f, 5.f);
        return;
    }

    // Fallback: Monsoon's own mono CV2 — unison across all voices (the Q1b behaviour).
    const float inCV = clampv<float>(cv2Voltage, 0.f, 5.f);
    for (int v = 0; v < 16; ++v) engine.quantiserCV[v] = inCV;
}

// ──── High-Level Dispatcher ─────────────────────────────────────────────────

bool ModeController::executeMode(int modeId,
                                  const InputState& input,
                                  bool gate2High) {
    (void)gate2High;   // unused since Mode D unified with Mode B (Gate 1 = main); kept for call-site stability
    // MODE_COLLAPSE_6_TO_3: three TIMING ORIGINS — clock(0) / gate(1) / phase(2). Pitch origin
    // (generate vs quantise) is the q-mix AXIS, not a mode. It is engaged PER STEP here via
    // beginQuantiserSource_ (quantiserPitchSource set true, cleared after — NEVER block-latched, so
    // q-mix sweeps stay sample-accurate and the engine's per-step determinism is preserved). The
    // engine's qmixUseGenerated + the patched-detection resolver decide per-step whether each voice
    // quantises; the section-421 invariant (flag-true + forceGenerated == flag-false) keeps the pure-
    // generate path (q-mix=0) byte-identical to the pre-collapse generator modes. Old C/D/F
    // ("quantiser modes") are now just "that timing origin with q-mix up" — no separate mode.
    const bool useSubGate = (modeId == 1) && (input.subGateConnected || input.ghostConnected);
    beginQuantiserSource_(input.cv2);   // per-step engage; inert at q-mix=0 (forceGenerated)
    bool stepped = false;
    switch (modeId) {
        case 0:  stepped = executeModeA();                  break;  // clock  (was A; C = clock + q-mix)
        case 1:  stepped = executeModeB(input, useSubGate); break;  // gate   (was B; D = gate  + q-mix)
        case 2:  stepped = executeModeE();                  break;  // phase  (was E; F = phase + q-mix)
        default: break;
    }
    engine.quantiserPitchSource = false;   // per-step clear (never latched for the block)
    return stepped;
}
