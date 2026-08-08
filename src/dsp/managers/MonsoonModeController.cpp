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
#include "../../Monsoon.hpp"
#include "../../MonsoonCausewayPolyExpander.hpp"

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
        for (int i = 0; i < 12; ++i) {
            // Use the SCALE-GATED weight (mainModule->getSemitoneParam → ScaleManager::getSemitoneWeight),
            // not the raw fader value. When Conservation/lock is enforced, out-of-scale semitones read 0
            // here, so the DICE/PATTERN engine (which picks from semiWeights) won't generate out-of-scale
            // notes — matching the realtime path. (Previously this used paramManager.getSemitone(i), the
            // raw value, so locked patterns still fired out-of-scale notes.)
            currentPatternInput.semiWeights[i] =
                mainModule ? mainModule->getSemitoneParam(i) : paramManager.getSemitone(i);
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
    // A/B mix scope (LOCK_SCOPE_MENU): latchMix now gates each stream independently, so the freeze is
    // EXACT per axis — rhythm A/B and melody A/B latch/free on their own bits (no combined coupling).
    {
        const bool abR = dotModular::LockManager::liveNow(dotModular::Control::ABMix, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/false);
        const bool abM = dotModular::LockManager::liveNow(dotModular::Control::ABMix, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/true);
        if (abR || abM)
            engine.pe.latchMix(currentPatternInput.rhythmMix,
                               currentPatternInput.melodyMix,
                               currentPatternInput.rhythmSlew,
                               currentPatternInput.melodySlew,
                               /*applyRhythm=*/abR, /*applyMelody=*/abM);
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
