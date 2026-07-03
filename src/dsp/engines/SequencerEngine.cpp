#include "SequencerEngine.hpp"
#include <cmath>
#include <algorithm>

// Note-value data (fraction, allowedPPQN, label) lives in dsp/NoteValues.hpp as
// the single source of truth. The sequencer uses NOTE_VALUES[i].allowedPPQN to
// snap an unsupported selection (e.g. 1/4T at PPQN=4) to the nearest valid one.



static const int DNA_LCM = 720720; // LCM of 1..16 ensures drift continuity

void SequencerEngine::reset() {
    pe.reset();
    gs.reset();
    for (int i = 0; i < 15; ++i) voices[i].gs.reset();
    // restProb values are NOT reset — the caller re-applies them from expander knobs.
    for (int i = 0; i < 15; i++) wasHeldPolyPrev[i] = false;
    lastStepResult = StepResult{};
    stepIndex = -1;
    lastStepIndex = -1;
    startStep = 0;
    endStep = 15;
    cachedLength = 16;
    cachedOffset = 0;
    totalStepsElapsed = 0;
    rhythmLen = variationLen = legatoLen = accentLen = melodyLen = octaveLen = 16;
    rhythmOff = variationOff = legatoOff = accentOff = melodyOff = octaveOff = 0;
    rhythmRot = variationRot = legatoRot = accentRot = melodyRot = octaveRot = 0;
    for (int i = 0; i < 15; i++) {
        for (int l = 0; l < 3; ++l) {
            polyLen[i][l] = 16;
            polyOff[i][l] = 0;
            polyRot[i][l] = 0;
        }
    }
    windowMask = 0xFFFF;
    locked = false;
    muted = false;
    runGateActive = false;
    resetArmed = false;
    prevGate1High = false;
    modeSelect = 0;
    ppqnSetting = 24;
    noteVariationMask = 0b111;
    activeSemiCount = 0;
    for (int i = 0; i < 12; ++i) faderCache[i] = -1.f;
    lastQuantIn = -100.f;
    lastQuantOut = 0.f;
}

bool SequencerEngine::isStepInWindow(int idx) const {
    return (windowMask & (1 << (idx & 0x0F))) != 0;
}

void SequencerEngine::setWindow(int length, int offset) {
    cachedLength = length;
    cachedOffset = offset;
    startStep = offset;
    endStep = (offset + length - 1) & 0x0F;

    uint16_t mask = 0;
    for (int i = 0; i < length; ++i) {
        mask |= (1 << ((offset + i) & 0x0F));
    }
    windowMask = mask;

    if (stepIndex != -1 && !isStepInWindow(stepIndex)) {
        stepIndex = (startStep - 1 + 16) % 16;
    }
}

bool SequencerEngine::advancePlayhead(int dir) {
    int prevStep = stepIndex;
    lastPlayDir = (dir < 0) ? -1 : +1;
    // Step the global DNA tick WITH direction: +1 forward, -1 backward. This is what
    // maps physical positions to drifting DNA content (strand indices) and the ring
    // lights; counting up in reverse desyncs it from stepIndex and both the strand
    // drift and the lights go the wrong way (ring lights up all around).
    if (dir < 0) totalStepsElapsed = (totalStepsElapsed - 1 + DNA_LCM) % DNA_LCM;
    else         totalStepsElapsed = (totalStepsElapsed + 1) % DNA_LCM;

    if (dir < 0) {
        // ── Reverse traversal: mirror of forward, leftward through the window. ──
        // From the "not started" state, seed just AFTER endStep so the first reverse
        // step lands on endStep (the symmetric counterpart of forward seeding before
        // startStep and stepping onto startStep).
        if (stepIndex == -1) stepIndex = (endStep + 1) & 0x0F;
        stepIndex = (stepIndex - 1 + 16) & 0x0F;
        if (!isStepInWindow(stepIndex)) stepIndex = endStep;
        for (int s = 0; s < 16 && !isStepInWindow(stepIndex); ++s)
            stepIndex = (stepIndex - 1 + 16) & 0x0F;
        // Phrase boundary (reverse): wrapped back round to endStep.
        return (prevStep != -1 && stepIndex == endStep);
    }

    // ── Forward traversal (unchanged) ──
    if (stepIndex == -1) stepIndex = (startStep - 1 + 16) % 16;
    stepIndex = (stepIndex + 1) & 0x0F;
    if (!isStepInWindow(stepIndex)) stepIndex = startStep;
    for (int s = 0; s < 16 && !isStepInWindow(stepIndex); ++s)
        stepIndex = (stepIndex + 1) & 0x0F;

    // Return true if wrapped to start (phrase boundary)
    return (prevStep != -1 && stepIndex == startStep);
}

void SequencerEngine::updateWindow(float lenParam, float lenCv, bool lenPatched, float offParam, float offCv, bool offPatched) {
    int length = pe_clamp<int>((int)std::round(lenParam), 1, 16);
    int offset = (int)std::round(offParam) & 0x0F;

    if (lenPatched) {
        float cv = pe_clamp(lenCv, 0.f, 10.f);
        length = pe_clamp<int>((int)std::round(cv / 10.f * 15.f) + 1, 1, 16);
    }
    if (offPatched) {
        float cv = pe_clamp(offCv, 0.f, 10.f);
        offset = ((int)std::round(cv / 10.f * 15.f)) & 0x0F;
    }
    setWindow(length, offset);
}

int SequencerEngine::computeNoteLengthIdx(int requestedIdx, int ppqnMask) const {
    int idx = pe_clamp(requestedIdx, 0, 7);
    while (idx > 0 && !(NOTE_VALUES[idx].allowedPPQN & ppqnMask)) {
        idx--;
    }
    return idx;
}

int SequencerEngine::getNoteLenIdx(float baseNoteParam, const PatternInput& input, float r) {
    int baseIdx = pe_clamp<int>((int)std::round(baseNoteParam), 0, 7);
    // PPQN is now always 24/48/96 — all of which resolve every note value to an
    // integer pulse count (24 already covers 1/32 and all triplets). So every
    // value is legal; mask = the full-resolution bit (4). (The old 1/4 PPQN
    // settings, which gated out sub-step values, are gone.)
    int ppqnMask = 4;
    baseIdx = computeNoteLengthIdx(baseIdx, ppqnMask);
    return pe.varyNoteIndex(baseIdx, input, r);
}

void SequencerEngine::rebuildScaleCache(const float weights[12]) {
    activeSemiCount = 0;
    for (int s = 0; s < 12; ++s) {
        float w = pe_clamp<float>(weights[s], 0.f, 1.f);
        faderCache[s] = w;
        if (w > 0.f) {
            activeSemiList[activeSemiCount] = s;
            activeSemiWeight[activeSemiCount] = w;
            ++activeSemiCount;
        }
    }
}

float SequencerEngine::getStepLightBrightness(int lightIdx) const {
    bool inWindow = isStepInWindow(lightIdx);
    float baseActive = 0.0f;
    
    if (inWindow) {
        // Calculate distance from current playhead to map physical LED to drifting DNA content
        int relCurrent = (stepIndex - startStep + 16) % 16;
        int relLight = (lightIdx - startStep + 16) % 16;
        int tickForLight = totalStepsElapsed - relCurrent + relLight;

        int dnaIdx = getStrandIdx(tickForLight, rhythmLen, rhythmOff, rhythmRot);
        bool isNote = pe.rhythmPattern[dnaIdx];
        baseActive = isNote ? 0.35f : 0.07f;
    }

    // The moving playhead should always follow the global timeline index. Shown for
    // stepped modes A/B/C (0/1/2) and the phase Mode E (4); Mode D (3) is continuous
    // (no discrete playhead step).
    bool steppedMode = (modeSelect == 0 || modeSelect == 1 || modeSelect == 2 || modeSelect == 4);
    float current = (steppedMode && lightIdx == stepIndex) ? 1.0f : 0.0f;

    // Direction cue (Mode E especially): a one-LED comet trail BEHIND the playhead in
    // the travel direction, so forward vs reverse is visibly distinct. The trailing
    // LED is the step the playhead just left: stepIndex - lastPlayDir.
    if (steppedMode && current < 1.0f) {
        int trailIdx = ((stepIndex - lastPlayDir) % 16 + 16) % 16;
        if (lightIdx == trailIdx && isStepInWindow(lightIdx))
            baseActive = std::max(baseActive, 0.5f);
    }

    return std::max(baseActive, current);
}

int SequencerEngine::getOffsetStep() const {
    // Legacy support for global pattern engine visual lookups.
    // It now uses the Melody strand as the anchor for visualization.
    return getMelodyStep();
}

bool SequencerEngine::shouldTriggerStep(int ppqn) const {
    if (stepIndex == -1) return false;
    if (ppqn <= 1) {
        // Only trigger on main 4 beats relative to startStep + offset
        int relative = (stepIndex - startStep + 16) % 16;
        return (relative & 0x03) == 0;
    }
    return true; 
}

StepResult SequencerEngine::executeStep(float restProb, float legatoProb, int nvIdx, float r_rest, float r_legato_tie, float r_accent, float accentProb, const PatternInput& input, bool wasHeld, bool hadTail) {
    // ── Fractional notes (1/4T=2.667, 1/8T=1.333, 1/32=0.5 steps) & legato/tie ──
    // These notes end MID-STEP (closed by the gateSecRemain seconds-timer), not on
    // a 1/16 grid edge. Legato/tie decisions only happen AT an edge and require the
    // previous note to still be sounding (wasHeld). That gives three rules, all
    // verified to hold with the current engine — they are by-design, not bugs:
    //
    //  1. A fractional note CANNOT be the FIRST/source note of a legato or tie.
    //     It has already closed before the next edge, so wasHeld is false there and
    //     the legato/tie branch is unreachable — the next step starts fresh. This
    //     is musically correct: a sub-step note leaves a real gap, which is not
    //     legato. (Design choice "A": accepted, not patched.)
    //
    //  2. A fractional note CAN be the LAST/destination note of a legato or tie.
    //     The gate is already open from the held source note; extendHold()/
    //     slideNote() add the fractional length and re-arm gateSecRemain on the
    //     summed hold, so the gate rides open to the exact sub-step end.
    //     e.g. 1/8→1/4T = 4.667 steps, 1/4→1/8T = 5.333, 1/8→1/32 = 2.5 (exact).
    //
    //  3. A fractional note CANNOT be a MIDDLE note of a 3-note tie. To be a middle
    //     note it would have to be both a destination (rule 2, OK) AND a source
    //     (rule 1, impossible). After it closes mid-step, the third note finds
    //     wasHeld=false at its edge and retriggers as a NewNote — the tie chain
    //     breaks cleanly at the fractional note, with no false reopened gate.
    StepResult result;
    result.nvIdx = nvIdx;

    // Mid-note: previous note's hold is still counting down.
    // Poly voices seeing MidNote will tick their own holds and return.
    //
    // The guard must also stay in MidNote while a fractional sub-step tail is
    // still sounding: a triplet (1/4T = 2.667 steps, 1/8T = 1.333) leaves
    // holdRemain < 1 on its final partial step while the precise gate timer
    // (gateSecRemain) is still open. Without the gateSecRemain check, that last
    // step fired a NEW note and cut the triplet to a whole-step length — 1/4T
    // played as 1/8, 1/8T as 1/16. (1/32 = 0.5 steps closes within its own step
    // before any edge, so it was unaffected — matching the observed scope.)
    if (gs.holdRemain >= 1.f || gs.gatePulseRemain > 0) {
        result.decision = MonoDecision::MidNote;
        result.accented = lastStepResult.accented;
        lastStepResult = result;
        return result;
    }

    int   sem    = 0;
    float pitchV = pe.genPitchLive(sem, input,
                                    pe.melodyRandom[getMelodyStep()],
                                    pe.octaveRandom[getOctaveStep()]);

    // ── Leading-edge legato (STEP 2, toggled by legatoLeadingEdge; default OFF) ──
    // The PREVIOUS starting note recorded, at its own onset, whether it intends to hold
    // its gate forward into THIS note (gs.slurForward). It survives untouched through the
    // previous note's held steps (the MidNote guard above returns early). Capture it now,
    // BEFORE this note's cascade recomputes gs.slurForward at the bottom. In leading-edge
    // mode this — not a fresh r_legato_tie roll — governs whether THIS note connects back.
    const bool prevSlur = gs.slurForward;

    // The previous PLAYED step's decision (lastStepResult is set at the end of each executeStep,
    // so here it is the step played immediately before this one — in PLAY ORDER, correct in both
    // directions because it is temporal, not step-numbered). A legato/tie means "connected to the
    // note played just before"; if that step was a REST (or the initial Inactive), there is no
    // note to connect to → this must NOT be a connection. This is the direction-correct
    // predecessor test. wasHeld alone is NOT sufficient: in reverse, wasHeld (a gate read) can be
    // true from a note that is not the play-order-adjacent predecessor, so a teal could otherwise
    // be emitted with a rest immediately before it in play order (the reverse isolated-teal bug).
    const MonoDecision prevPlayedDec = lastStepResult.decision;
    const bool prevPlayedSounded = (prevPlayedDec == MonoDecision::NewNote)
                                || (prevPlayedDec == MonoDecision::Legato)
                                || (prevPlayedDec == MonoDecision::LegatoMax)
                                || (prevPlayedDec == MonoDecision::Tie)
                                || (prevPlayedDec == MonoDecision::MidNote);

    // Structural Rest Roll:
    // canRest is true only if the previous note finished exactly on a step edge 
    // or was already silent. Fractional tails (e.g. triplets) block the rest roll
    // to ensure rhythmic alignment.
    bool canRest = (gs.holdRemain <= 0.0001f && !hadTail);

    // Whether THIS note connects (tie/legato) to the previous one.
    //  - Current model: fresh roll at this (joining) onset — r_legato_tie < legatoProb.
    //  - Leading-edge:  the previous note's onset commitment — prevSlur — decides it.
    // legatoProb>=0.999 (LegatoMax) forces connection in BOTH models.
    const bool legatoConnects = (legatoProb >= 0.999f)
        || (legatoLeadingEdge ? prevSlur : (r_legato_tie < legatoProb));

    // A genuine committed slur reaching THIS note: it connects AND has a held predecessor to
    // connect to (exactly the condition the legato branch below uses). "Rest beats legato"
    // toggle: when OFF, such a slur IGNORES the rest roll here (slur wins → plays as Legato/
    // Tie); when ON (default), the rest branch takes priority (rest cancels the slur). A
    // fractional TAIL still always outranks rest (canRest, below), regardless of the toggle.
    const bool slurReachesHere    = legatoConnects && (wasHeld || hadTail) && prevPlayedSounded;
    const bool slurSuppressesRest = !restBeatsLegato && slurReachesHere;

    if (legatoProb >= 0.999f) {
        gs.slideMax(pitchV, sem, nvIdx);
        result.decision = MonoDecision::LegatoMax;
    }
    else if ((r_rest < restProb) && !slurSuppressesRest) {
        // Rest precedence:
        //  - A fractional NOTE TAIL always outranks a rest (physical — !canRest). Unchanged.
        //  - "Rest beats legato" ON (default): a rest CANCELS an optional slur reach ("can't
        //    slur into silence") — even a committed prevSlur yields to the rest (N+1 silent).
        //    OFF: slurSuppressesRest skips this branch, so the slur wins and the note falls
        //    through to the legato branch below (rest ignored, N+1 plays as Legato/Tie with
        //    its own drawn pitch). See RHYTHM_BEHAVIOUR_POLICIES.md.
        if (canRest) {
            gs.gateHeld = false;
            result.decision = MonoDecision::Rest;
        } else {
            // Precedence: note tail takes priority over pattern structural rest.
            gs.gateHeld = true; // Re-activate gate to allow the fractional tail to finish
            result.decision = MonoDecision::MidNote;
        }
    }
    else if (legatoConnects && (wasHeld || hadTail) && prevPlayedSounded) {
        // A slur can only CONNECT if the previous note actually held its gate into this step
        // (wasHeld || hadTail) — the documented invariant "legato/tie requires the previous
        // note still sounding". legatoConnects expresses the INTENT to connect (a fresh roll
        // in reactive mode, or the previous note's prevSlur commitment in leading-edge mode),
        // but intent alone isn't enough: in leading-edge, prevSlur can be true while the
        // predecessor left NO held gate (it ended / was cut), which would otherwise slide into
        // nothing — producing an isolated Legato cell (teal note connected to no predecessor).
        // Requiring a held predecessor here makes the connection real; otherwise fall through
        // to NewNote (the slur had nothing to land on → a fresh note).
        if (sem == gs.lastSemitone) {
            gs.extendHold(sem, nvIdx);
            result.decision = MonoDecision::Tie;
        } else {
            gs.slideNote(pitchV, sem, nvIdx, /*wasHeld=*/true);
            result.decision = MonoDecision::Legato;
        }
    }
    else {
        gs.triggerNote(pitchV, sem, nvIdx);
        result.decision = MonoDecision::NewNote;
    }

    // Determine if this note is accented. 
    // Decision happens on NewNote, or Legato shifts from a dead state.
    bool monoStarting = (result.decision == MonoDecision::NewNote) || 
                        ((result.decision == MonoDecision::Legato || result.decision == MonoDecision::LegatoMax) && !wasHeld && !hadTail);

    if (monoStarting) {
        result.accented = (r_accent < accentProb);
    } else if (result.decision == MonoDecision::Rest) {
        result.accented = false;
    } else {
        // Sustain/Inherit: Tie, MidNote, or Legato shift during an existing held note
        result.accented = lastStepResult.accented;
    }

    // ── STEP 1 INSTRUMENT (leading-edge legato) — computed, NOT consulted ─────────
    // In the leading-edge model, a note that is STARTING commits here (at its own onset)
    // to hold its gate forward into the next note, iff BOTH: (a) its own legato draw
    // fires (r_legato_tie < legatoProb, or LegatoMax), AND (b) its length is grid-aligned
    // so it can cleanly reach the next onset (noteCanLeadLegato). This records that intended
    // commitment on gs.slurForward WITHOUT changing any decision — the join above still
    // decides exactly as today. Step 2 will make the next step consume this flag instead of
    // rolling fresh. Only meaningful on a starting note; cleared otherwise.
    // A note "starts" (and may thus lead the chain FORWARD) if it is a real sounding note
    // at this onset: NewNote, Legato, LegatoMax — AND Tie. A Tie (same-pitch continuation)
    // is a sounding note that can carry a 3+ note legato chain onward to the next note, IF
    // its own length is grid-aligned and it rolls forward. Excluding Tie would break any
    // chain at a tie (chain of 3+ can only continue past note 2 if note 2 is itself an
    // eligible lead — see LEGATO_TIE_REDESIGN.md). MidNote (mid-hold, not an onset) and Rest
    // (silent) correctly do NOT start. slurForward set at a note's onset survives its held
    // steps (the MidNote guard returns early before this block).
    bool leStarting = (result.decision == MonoDecision::NewNote)
                   || (result.decision == MonoDecision::Legato)
                   || (result.decision == MonoDecision::LegatoMax)
                   || (result.decision == MonoDecision::Tie);
    bool leWouldSlur = leStarting
                    && noteCanLeadLegato(nvIdx)
                    && (legatoProb >= 0.999f || r_legato_tie < legatoProb);
    gs.slurForward = leWouldSlur;
    if (leWouldSlur) { dbgSlurSetByNvIdx = nvIdx; dbgSlurSetByCanLead = noteCanLeadLegato(nvIdx); dbgSlurSetAtStep = stepIndex; }
    // Divergence probe: what the leading-edge flag dictates vs what the current model DID
    // at this join (Tie/Legato = connected; NewNote = not). Counts only; no behaviour change.
    if (leStarting) {
        bool currentConnected = (result.decision == MonoDecision::Tie)
                             || (result.decision == MonoDecision::Legato)
                             || (result.decision == MonoDecision::LegatoMax);
        if (leWouldSlur != currentConnected) ++legatoLE_divergeCount;
        ++legatoLE_startCount;
    }

    // ── TEMP probe: what did THIS step decide, and what was the previous-PLAYED decision +
    //    prevPlayedSounded that the guard saw? Shows directly whether a teal's predecessor was
    //    recorded as Rest. REMOVE after. ──
    {
        auto nm = [](MonoDecision d)->const char*{
            switch(d){case MonoDecision::NewNote:return "NEW";case MonoDecision::Legato:return "LEG";
            case MonoDecision::LegatoMax:return "LMX";case MonoDecision::Tie:return "TIE";
            case MonoDecision::Rest:return "REST";case MonoDecision::MidNote:return "MID";default:return "OTH";}};
        char line[200];
        std::snprintf(line, sizeof(line),
            "[G] dir=%+d step=%d dec=%s prevPlayedDec=%s prevSounded=%d prevSlur=%d wasHeld=%d hadTail=%d slurSetAt=%d slurNv=%d slurCanLead=%d",
            lastPlayDir, stepIndex, nm(result.decision), nm(prevPlayedDec),
            (int)prevPlayedSounded, (int)prevSlur, (int)wasHeld, (int)hadTail,
            dbgSlurSetAtStep, dbgSlurSetByNvIdx, (int)dbgSlurSetByCanLead);
        dbgPush(line);
    }

    lastStepResult = result;
    return result;
}

void SequencerEngine::handlePhraseBoundary(PatternInput input, bool isMelodyRealtime, bool isRhythmRealtime) {
    if (isMelodyRealtime) {
        pe.melodySeedPendingFloat = pe.melodySeedFloat; // Or sample from module
        pe.melodySeedPending = true;
    }
    if (isRhythmRealtime) {
        pe.rhythmSeedPendingFloat = pe.rhythmSeedFloat;
        pe.rhythmSeedPending = true;
    }
    pe.applyPendingSeedsAndRedraw(input);
}

StepResult SequencerEngine::executeModeA(const ClockEngine& clock, float restProb, float legatoProb, float noteVal, const PatternInput& input, int dir) {
    StepResult result;
    if (!clock.sixteenthEdge || muted) return result;

    bool wrapped = advancePlayhead(dir);

    // "Boundary interrupt" toggle: at the phrase boundary (wrap), force a fresh start by
    // clearing any held gate BEFORE wasHeld is captured — so the note at step 0 sees no held
    // predecessor and can't slur/tie across the loop. Result: every lap is identical (no
    // cross-lap memory). Default OFF = CONTINUE (gate carries across the loop, laps can differ
    // — see the lap-1-vs-lap-2 illustration in RHYTHM_BEHAVIOUR_POLICIES.md). Applied to mono
    // and all poly voices.
    if (wrapped && boundaryInterrupt) {
        gs.gateHeld = false;
        gs.holdRemain = 0.f;
        gs.slurForward = false;
        for (int i = 0; i < numPolyVoices; ++i) {
            voices[i].gs.gateHeld   = false;
            voices[i].gs.holdRemain = 0.f;
            voices[i].gs.slurForward = false;
        }
    }

    float r_vary   = pe.variationRandom[getVariationStep()];
    float r_rest   = pe.rhythmRandom[getRhythmStep()];
    float r_legato = pe.legatoRandom[getLegatoStep()];
    float r_accent = pe.accentRandom[getAccentStep()];  // New: accent strand
    
    int nvIdx = getNoteLenIdx(noteVal, input, r_vary);

    float prevHold = gs.holdRemain;
    wasHeldMono = gs.gateHeld || (prevHold > 0.0001f);
    gs.tick(ClockEngine::pulsesPer16th(ppqnSetting));
    hadMonoTail = (prevHold > 0.0001f && prevHold < 0.999f);

    for (int i = 0; i < numPolyVoices; ++i) {
        wasHeldPolyPrev[i] = voices[i].gs.gateHeld || (voices[i].gs.holdRemain > 0.0001f);
        float ph = voices[i].gs.holdRemain;
        voices[i].gs.tick(ClockEngine::pulsesPer16th(ppqnSetting));
        hadPolyTail[i] = (ph > 0.0001f && ph < 0.999f);
    }
    
    result = executeStep(restProb, legatoProb, nvIdx, r_rest, r_legato, r_accent, accentProb, input, wasHeldMono, hadMonoTail);
    result.stepped = true;
    result.wrapped = wrapped;
    return result;
}

StepResult SequencerEngine::executeModeB(bool gate1Rise, bool gate1High, float restProb, float legatoProb, float noteVal, const PatternInput& input) {
    StepResult result;
    if (muted) {
        prevGate1High = gate1High;
        return result;
    }
    bool wrapped = false;
    bool triggered = false;

    if (gate1Rise) {
        wrapped = advancePlayhead();
        triggered = true;
    } else if (gate1High && !prevGate1High && stepIndex == -1) {
        advancePlayhead();
        triggered = true;
    }

    if (triggered) {
        // Boundary interrupt (see executeModeA): clear held gate at wrap so every lap starts
        // fresh. Applied before wasHeld capture, mono + poly.
        if (wrapped && boundaryInterrupt) {
            gs.gateHeld = false; gs.holdRemain = 0.f; gs.slurForward = false;
            for (int i = 0; i < numPolyVoices; ++i) {
                voices[i].gs.gateHeld = false; voices[i].gs.holdRemain = 0.f;
                voices[i].gs.slurForward = false;
            }
        }
        float r_vary   = pe.variationRandom[getVariationStep()];
        float r_rest   = pe.rhythmRandom[getRhythmStep()];
        float r_legato = pe.legatoRandom[getLegatoStep()];
        float r_accent = pe.accentRandom[getAccentStep()];  // New: accent strand
        
        int nvIdx = getNoteLenIdx(noteVal, input, r_vary);

        float prevHold = gs.holdRemain;
        wasHeldMono = gs.gateHeld || (prevHold > 0.0001f);
        gs.tick();
        hadMonoTail = (prevHold > 0.0001f && prevHold < 0.999f);

        for (int i = 0; i < numPolyVoices; ++i) {
            wasHeldPolyPrev[i] = voices[i].gs.gateHeld || (voices[i].gs.holdRemain > 0.0001f);
            float ph = voices[i].gs.holdRemain;
            voices[i].gs.tick();
            hadPolyTail[i] = (ph > 0.0001f && ph < 0.999f);
        }
        
        result = executeStep(restProb, legatoProb, nvIdx, r_rest, r_legato, r_accent, accentProb, input, wasHeldMono, hadMonoTail);
        result.stepped = true;
        result.wrapped = wrapped;
    }

    prevGate1High = gate1High;
    return result;
}

// ── Poly voice execution ──────────────────────────────────────────────────────
//
// Rules (see design doc):
//   MidNote  — mono note still in progress; poly ticks its hold and returns.
//   Rest     — mono rested; poly cannot initiate a new note this step.
//   Tie      — mono held the same pitch; poly voices that are sounding extend
//              their own hold.  Voices already at rest remain silent.
//   Legato / LegatoMax / NewNote — mono played something new; each poly voice
//              independently rolls its own rest probability then draws its
//              own pitch (legacy note). Gate type (retrigger vs slide)
//              follows mono's decision.  Within the legato zone a poly-internal
//              tie still emerges naturally when the voice's random pitch
//              matches its own previous semitone.

void SequencerEngine::executePolyVoice(int voiceIdx, const PatternInput& input, bool wasHeldPoly, bool hadPolyTail) {
    PolyVoice& v = voices[voiceIdx];

    // Start Detection: A new note/retrigger happens on NewNote, or on 
    // Legato shifts from a dead state (low gate and no tail).
    bool monoGateStart = (lastStepResult.decision == MonoDecision::NewNote) || 
                         ((lastStepResult.decision == MonoDecision::Legato || lastStepResult.decision == MonoDecision::LegatoMax) && !wasHeldMono && !hadMonoTail);

    if (monoGateStart) {
        // This is the decision point for this entire mono gate.
        // Each strand indexes with ITS OWN lane LOR (rest/melody/octave).
        int restIdx = getStrandIdx(totalStepsElapsed, polyLen[voiceIdx][PL_REST],   polyOff[voiceIdx][PL_REST],   polyRot[voiceIdx][PL_REST]);
        int melIdx  = getStrandIdx(totalStepsElapsed, polyLen[voiceIdx][PL_MELODY], polyOff[voiceIdx][PL_MELODY], polyRot[voiceIdx][PL_MELODY]);
        int octIdx  = getStrandIdx(totalStepsElapsed, polyLen[voiceIdx][PL_OCTAVE], polyOff[voiceIdx][PL_OCTAVE], polyRot[voiceIdx][PL_OCTAVE]);
        float r_rest = pe.polyRhythmRandom[voiceIdx][restIdx];
        
        if (r_rest < v.restProb) {
            // Decide to Rest: Stick with it until mono gate drops. No accent while resting.
            v.accented = false;
            if (v.gs.holdRemain > 0.0001f) v.gs.gateHeld = true; // allow previous note tail to finish
            else v.gs.gateHeld = false;
            return;
        }
        
        // Decide to Play: Draw pitch and follow mono's triggering behavior.
        int sem = 0;
        float pitchV = pe.genPitchLive(sem, input, pe.polyMelodyRandom[voiceIdx][melIdx], pe.polyOctaveRandom[voiceIdx][octIdx]);
        // Accent as a poly lane (modelled after rest): this voice draws its OWN accent at
        // its own accent LOR and compares to its own accentProb — not shared from mono.
        int accIdx = getStrandIdx(totalStepsElapsed, polyLen[voiceIdx][PL_ACCENT], polyOff[voiceIdx][PL_ACCENT], polyRot[voiceIdx][PL_ACCENT]);
        v.accented = (pe.polyAccentRandom[voiceIdx][accIdx] < v.accentProb);
        if (lastStepResult.decision == MonoDecision::NewNote)
            v.gs.triggerNote(pitchV, sem, lastStepResult.nvIdx);
        else if (wasHeldPoly || hadPolyTail)
            // Mono is slurring/tying and THIS poly voice had a held predecessor → real slide.
            v.gs.slideNote(pitchV, sem, lastStepResult.nvIdx, /*wasHeld=*/true);
        else
            // Mono slurs but this poly voice had NO held gate (it was resting/silent on its
            // previous played step) — sliding would produce an isolated Legato cell (teal note
            // with no predecessor on this lane), the poly analogue of the mono isolated-teal
            // bug. Trigger a fresh note instead. (Direction-independent; surfaced in reverse
            // mode but present forward too.)
            v.gs.triggerNote(pitchV, sem, lastStepResult.nvIdx);
        return;
    }

    // If mono is Sustaining or Resting, poly must stick to its current role.
    if (lastStepResult.decision == MonoDecision::Rest) {
        v.gs.gateHeld = false;
    } else {
        // Mono is Sustaining (MidNote, Tie, or shifted Legato shift).
        // Poly follows mono gate presence strictly IF it was already active ("in").
        if (gs.gateHeld && wasHeldPoly) {
            if (lastStepResult.decision == MonoDecision::Tie) {
                v.gs.extendHold(v.gs.lastSemitone, lastStepResult.nvIdx);
            } else {
                // Re-draw pitch for glides (Legato) or sustains (MidNote).
                // This allows the poly melody to move independently even if 
                // the mono rhythm is static. Melody/octave use their own lane LOR.
                int melIdx = getStrandIdx(totalStepsElapsed, polyLen[voiceIdx][PL_MELODY], polyOff[voiceIdx][PL_MELODY], polyRot[voiceIdx][PL_MELODY]);
                int octIdx = getStrandIdx(totalStepsElapsed, polyLen[voiceIdx][PL_OCTAVE], polyOff[voiceIdx][PL_OCTAVE], polyRot[voiceIdx][PL_OCTAVE]);
                int sem = 0;
                float pitchV = pe.genPitchLive(sem, input, pe.polyMelodyRandom[voiceIdx][melIdx], pe.polyOctaveRandom[voiceIdx][octIdx]);
                
                if (lastStepResult.decision == MonoDecision::Legato || lastStepResult.decision == MonoDecision::LegatoMax) {
                    v.gs.slideNote(pitchV, sem, lastStepResult.nvIdx, wasHeldPoly);
                } else {
                    // MidNote sustain: update pitch and keep gate high
                    v.gs.currentPitchV = pitchV;
                    v.gs.lastSemitone = sem;
                    v.gs.gateHeld = true;
                }
            }
        } else {
            // Mono gate is low OR poly chose to rest for this current high cycle.
            v.gs.gateHeld = false;
        }
    }
}

void SequencerEngine::executePolyVoices(const PatternInput& input) {
    for (int i = 0; i < numPolyVoices; ++i) {
        // Use the state from BEFORE the tick to decide if we were active.
        // This ensures that if a note ended exactly at the boundary, 
        // we still consider it "active" for slaving to a continuing mono gate.
        // hadPolyTail is kept for safety with fractional start/ends.
        bool wasHeldPoly = wasHeldPolyPrev[i] || hadPolyTail[i];
        executePolyVoice(i, input, wasHeldPoly, hadPolyTail[i]);
    }
}

void SequencerEngine::executeModeC(const ClockEngine& clock, float inCV) {
    gs.gateHeld = false;
    if (clock.quarterEdge) {
        gs.tick(ClockEngine::pulsesPer16th(ppqnSetting));
        advancePlayhead();
        gs.currentPitchV = quantize(inCV);
        int sem = int(std::round((gs.currentPitchV - std::floor(gs.currentPitchV)) * 12.f)) % 12;
        gs.lastSemitone = sem;
        gs.markSemi(sem, 4.0f);
        gs.gatePulse.trigger(1e-3f);
    }
}

void SequencerEngine::executeModeD(bool gateHigh, float inCV) {
    gs.gateHeld = gateHigh;
    if (gateHigh) {
        gs.currentPitchV = quantize(inCV);
        int sem = int(std::round((gs.currentPitchV - std::floor(gs.currentPitchV)) * 12.f)) % 12;
        gs.markSemi(sem, 1.0f);
    } else {
        gs.currentPitchV = 0.f;
        gs.gatePulse.reset();
    }
}

float SequencerEngine::quantize(float vIn) {
    if (std::abs(vIn - lastQuantIn) < 1e-6f) return lastQuantOut;
    lastQuantIn = vIn;

    if (activeSemiCount == 0) return pe_clamp(vIn, 0.f, 5.f);
    int octave = (int)vIn; 
    float bestScore = -1.f;
    float bestV = vIn;

    for (int ai = 0; ai < activeSemiCount; ++ai) {
        int s = activeSemiList[ai];
        float w = activeSemiWeight[ai];
        float radius = w * (1.f / 12.f);
        for (int o = octave - 1; o <= octave + 1; ++o) {
            float cand = o + s / 12.f;
            float dist = std::fabs(vIn - cand);
            if (dist <= radius + 1e-4f) {
                float score = w / (dist + 1e-4f);
                if (score > bestScore) { bestScore = score; bestV = cand; }
            }
        }
    }
    if (bestScore < 0.f) {
        float minDist = 1e9f;
        for (int ai = 0; ai < activeSemiCount; ++ai) {
            int s = activeSemiList[ai];
            for (int o = octave - 1; o <= octave + 1; ++o) {
                float cand = o + s / 12.f;
                float dist = std::fabs(vIn - cand);
                if (dist < minDist) { minDist = dist; bestV = cand; }
            }
        }
    }
    lastQuantOut = pe_clamp<float>(bestV, 0.f, 5.f);
    return lastQuantOut;
}
// Shared base index calculation with strand-specific phase and windowing
int SequencerEngine::getStrandIdx(int tickCount, int len, int off, int mutation) const
{
    int safeLen = std::max(1, len);
    // 1. (tickCount + mutation) % len  => Drift inside the DNA loop
    // Wrap properly for negative ticks (used by LED distance logic)
    int timelineIdx = ((tickCount + mutation) % safeLen + safeLen) % safeLen;
    // 2. ... + off                     => Slide the entire DNA window across the source buffer
    return (timelineIdx + off) % 16;
}

int SequencerEngine::getRhythmStep() const    { return getStrandIdx(totalStepsElapsed, rhythmLen, rhythmOff, rhythmRot); }
int SequencerEngine::getVariationStep() const { return getStrandIdx(totalStepsElapsed, variationLen, variationOff, variationRot); }
int SequencerEngine::getLegatoStep() const    { return getStrandIdx(totalStepsElapsed, legatoLen, legatoOff, legatoRot); }
int SequencerEngine::getAccentStep() const    { return getStrandIdx(totalStepsElapsed, accentLen, accentOff, accentRot); }
int SequencerEngine::getMelodyStep() const    { return getStrandIdx(totalStepsElapsed, melodyLen, melodyOff, melodyRot); }
int SequencerEngine::getOctaveStep() const    { return getStrandIdx(totalStepsElapsed, octaveLen, octaveOff, octaveRot); }

void SequencerEngine::syncVisuals(const PatternInput& in) {
    pe.refreshVisualCache(in);
}

void SequencerEngine::scrambleRhythmStrands() {
    pe.rotateRhythm(rack::random::u32() % 16);
    pe.rotateVariation(rack::random::u32() % 16);
    pe.rotateLegato(rack::random::u32() % 16);
}

void SequencerEngine::scrambleMelodyStrands() {
    pe.rotateMelody(rack::random::u32() % 16);
    pe.rotateOctave(rack::random::u32() % 16);
}

void SequencerEngine::scrambleAllStrands() {
    scrambleRhythmStrands();
    scrambleMelodyStrands();
    pe.rotateAccent(rack::random::u32() % 16);
}

void SequencerEngine::resetRhythmStrands() {
    for (int i = 0; i < 16; ++i) {
        pe.rhythmRandom[i] = pe.rhythmSource[i];
        pe.variationRandom[i] = pe.variationSource[i];
        pe.legatoRandom[i] = pe.legatoSource[i];
    }
}

void SequencerEngine::resetMelodyStrands() {
    for (int i = 0; i < 16; ++i) {
        pe.melodyRandom[i] = pe.melodySource[i];
        pe.octaveRandom[i] = pe.octaveSource[i];
    }
}

void SequencerEngine::resetAllStrands() {
    pe.resetDnaRotation();
}
