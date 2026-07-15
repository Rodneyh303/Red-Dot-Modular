#include "SequencerEngine.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>

// RULE2_DEBUG is defined in SequencerEngine.hpp (shared with the manager). The trace uses
// Rack's INFO() logger (writes to log.txt); fprintf(stderr) is discarded under a GUI launch.
#if RULE2_DEBUG
#include <rack.hpp>
#endif

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
    for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
        lorRef(s, LOR_LEN) = 16;   // Length defaults to full 16-step window
        lorRef(s, LOR_OFF) = 0;
        lorRef(s, LOR_ROT) = 0;
        laneTick_[s] = 0;          // per-lane direction: reset walk + default forward
        laneSign_[s] = 1;
        laneSignPending_[s] = 1;
        lanePendulum_[s] = false;
        laneDir_[s] = LaneDir::Forward;
        laneDirPending_[s] = LaneDir::Forward;
        lanePingPongHold_[s] = false;
        macroLaneTick_[s] = 0;
        macroLaneSign_[s] = 1;
        macroLaneDir_[s] = LaneDir::Forward;
        macroPingPongHold_[s] = false;
        for (int v = 0; v < 15; ++v) {
            laneTickV_[v][s] = 0;   // per-voice tick: reset (inherits mono dir)
            laneSignV_[v][s] = 1;   // CRITICAL: must be +1, not 0 — -0 == 0 in integer
                                    // arithmetic, so a bounce flip on 0 is a no-op (the
                                    // Pendulum/PingPong bounce would never reverse direction).
            laneSignVPending_[v][s] = 1;
            laneDirV_[v][s] = LaneDir::Forward;
            laneDirVPending_[v][s] = LaneDir::Forward;
            lanePingPongHoldV_[v][s] = false;
        }
    }
    for (int i = 0; i < 15; i++) {
        for (int l = 0; l < PL_LANES; ++l) {   // was l < 3 — PL_ACCENT(3) was never reset (pre-existing bug)
            polyLenERef(i, l) = 16;
            polyOffERef(i, l) = 0;
            polyRotERef(i, l) = 0;
        }
        // VARIATION/LEGATO per-voice LOR were NEVER seeded (lorStore_ is zero-init, so len==0 →
        // getStrandIdx's max(1,len) pinned every step to index 0 — NOT identity). Editor-order
        // accessor: these lanes have no PL_ id.
        for (int el : { EDITOR_LANE_VARIATION, EDITOR_LANE_LEGATO }) {
            polyLORRef(i, el, LOR_LEN) = 16;
            polyLORRef(i, el, LOR_OFF) = 0;
            polyLORRef(i, el, LOR_ROT) = 0;
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

    // ── Per-lane direction ──────────────────────────────────────────────────────
    // StepEdge: promote the pending sign/dir now (before advancing) so the lane turns around
    // THIS step. Then advance each lane's own tick by globalDir * its sign — all-forward
    // (sign=+1) keeps laneTick_[l] == totalStepsElapsed exactly (no behaviour change).
    if (laneFlipQuant == LaneFlipQuant::StepEdge) {
        for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
            laneSign_[l] = laneSignPending_[l];
            laneDir_[l]  = laneDirPending_[l];
            for (int v = 0; v < 15; ++v) {
                laneSignV_[v][l] = laneSignVPending_[v][l];
                laneDirV_[v][l]  = laneDirVPending_[v][l];
            }
        }
    }
    // Sync laneSign_ from laneDir_ for Forward/Reverse (Pendulum/PingPong keep their
    // current flipped sign, managed by the auto-flip at the wrap below).
    for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
        if (laneDir_[l] == LaneDir::Forward)      laneSign_[l] = +1;
        else if (laneDir_[l] == LaneDir::Reverse) laneSign_[l] = -1;
        // Pendulum/PingPong: laneSign_ is managed by the auto-flip; don't override — EXCEPT
        // if it's 0 (unset). -0 == 0, so a bounce on 0 is a no-op. Default to +1 (forward).
        else if (laneSign_[l] == 0) laneSign_[l] = +1;
    }
    // Same derivation for per-voice signs: Forward/Reverse get a deterministic sign
    // from laneDirV_; Pendulum/PingPong keep their bounce-managed sign. This lets the
    // widget push ONLY laneDirVPending_ (not laneSignVPending_), so the engine's
    // bounce-induced sign flip in laneSignVPending_ is not overwritten every frame.
    for (int v = 0; v < 15; ++v)
        for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
            if (laneDirV_[v][l] == LaneDir::Forward)      laneSignV_[v][l] = +1;
            else if (laneDirV_[v][l] == LaneDir::Reverse) laneSignV_[v][l] = -1;
            // Pendulum/PingPong: laneSignV_ managed by auto-flip; don't override — EXCEPT
            // if it's 0 (unset/default). -0 == 0 in integer arithmetic, so a bounce flip on
            // 0 is a no-op and the direction never reverses. Default to +1 (forward) so the
            // first bounce can flip it to -1.
            else if (laneSignV_[v][l] == 0) laneSignV_[v][l] = +1;
        }
    // Advance each lane's tick, with Pendulum/PingPong endpoint-bounce support.
    // Both Pendulum and PingPong reverse at the LOR window endpoint (pos len-1 fwd, pos 0 rev).
    // PingPong also repeats the endpoint step (hold for one step before bouncing); Pendulum
    // bounces immediately (no repeat).
    for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
        if (laneDir_[l] == LaneDir::PingPong && lanePingPongHold_[l]) {
            // Hold was set last step → bounce: flip sign, clear hold, advance normally.
            lanePingPongHold_[l] = false;
            laneSign_[l] = -laneSign_[l];
            laneSignPending_[l] = laneSign_[l];   // keep pending in sync
        } else if (laneDir_[l] == LaneDir::PingPong) {
            // Check if we're at the endpoint (about to leave the window). The LOR length for
            // this strand determines the window; laneTick_ walks 0..len-1 cyclically.
            int len = std::max(1, strandLen(l));
            int pos = ((laneTick_[l] % len) + len) % len;
            bool atFwdEnd = (laneSign_[l] > 0 && pos == len - 1);
            bool atRevEnd = (laneSign_[l] < 0 && pos == 0);
            if (atFwdEnd || atRevEnd) {
                // Hold: don't advance this step (the endpoint repeats). Set the hold flag so
                // next step bounces.
                lanePingPongHold_[l] = true;
                continue;   // skip the advance — tick stays at the endpoint
            }
        }
        laneTick_[l] = (int)((((long)laneTick_[l] + (long)dir * laneSign_[l]) % DNA_LCM + DNA_LCM) % DNA_LCM);
        // Pendulum: after advancing, check if we just reached the endpoint → flip sign now
        // (at the LOR window edge, not waiting for the phrase wrap). No hold (no repeat).
        if (laneDir_[l] == LaneDir::Pendulum) {
            int len = std::max(1, strandLen(l));
            int pos = ((laneTick_[l] % len) + len) % len;
            bool atEnd = (laneSign_[l] > 0 && pos == len - 1) || (laneSign_[l] < 0 && pos == 0);
            if (atEnd) {
                laneSign_[l] = -laneSign_[l];
                laneSignPending_[l] = laneSign_[l];
            }
        }
    }
    // Per-voice tick: the EFFECTIVE sign is the voice's own sign (polyLaneSign) — ABSOLUTE,
    // not relative to mono. The DirCell directly controls the direction: Forward = +1, Reverse =
    // -1, Pendulum/PingPong bounce at the voice's own LOR endpoint. All-forward keeps
    // laneTickV_[v][l] == laneTick_[l] exactly when mono is also forward → no-op until something
    // is reversed. (Previously effSign = laneSign_[l] * polyLaneSign(v, l), which made poly
    // direction relative to mono — counter-intuitive when mono is reversed.)
    // Per-voice PingPong: same endpoint-hold logic as mono, on the voice's own tick + LOR.
    // Per-voice Pendulum: same endpoint-bounce as mono (flip at the LOR edge, no hold).
    for (int v = 0; v < 15; ++v)
        for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
            int effSign = polyLaneSign(v, l);
            // Per-voice PingPong hold (analogous to mono's, on the voice's own LOR window)
            if (laneDirV_[v][l] == LaneDir::PingPong && lanePingPongHoldV_[v][l]) {
                lanePingPongHoldV_[v][l] = false;
                laneSignV_[v][l] = -laneSignV_[v][l];
                laneSignVPending_[v][l] = laneSignV_[v][l];
                effSign = polyLaneSign(v, l);   // recompute after flip
            } else if (laneDirV_[v][l] == LaneDir::PingPong) {
                int len = std::max(1, polyLOR(v, l, LOR_LEN));
                int pos = ((laneTickV_[v][l] % len) + len) % len;
                bool atEnd = (effSign > 0 && pos == len - 1) || (effSign < 0 && pos == 0);
                if (atEnd) { lanePingPongHoldV_[v][l] = true; continue; }
            }
            laneTickV_[v][l] = (int)((((long)laneTickV_[v][l] + (long)dir * effSign) % DNA_LCM + DNA_LCM) % DNA_LCM);
            // Per-voice Pendulum: after advancing, flip if at the LOR endpoint (no hold, no repeat).
            if (laneDirV_[v][l] == LaneDir::Pendulum) {
                int len = std::max(1, polyLOR(v, l, LOR_LEN));
                int pos = ((laneTickV_[v][l] % len) + len) % len;
                bool atEnd = (effSign > 0 && pos == len - 1) || (effSign < 0 && pos == 0);
                if (atEnd) {
                    laneSignV_[v][l] = -laneSignV_[v][l];
                    laneSignVPending_[v][l] = laneSignV_[v][l];
                }
            }
        }

    // Macro's own per-lane tick — same bounce logic as laneTick_ but with macroLaneDir_.
    // Uses Macro's LOR (strandLen, same as mono's since Macro's base is pushed to the
    // engine strands by the manager). This lets Macro's playhead always follow Macro's
    // DirCell, independent of who owns the lane.
    for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
        if (macroLaneDir_[l] == LaneDir::Forward)      macroLaneSign_[l] = +1;
        else if (macroLaneDir_[l] == LaneDir::Reverse) macroLaneSign_[l] = -1;
        else if (macroLaneSign_[l] == 0)               macroLaneSign_[l] = +1;
        if (macroLaneDir_[l] == LaneDir::PingPong && macroPingPongHold_[l]) {
            macroPingPongHold_[l] = false;
            macroLaneSign_[l] = -macroLaneSign_[l];
        } else if (macroLaneDir_[l] == LaneDir::PingPong) {
            int len = std::max(1, (l < 4) ? macroLOR_[l] : strandLen(l));
            int pos = ((macroLaneTick_[l] % len) + len) % len;
            bool atEnd = (macroLaneSign_[l] > 0 && pos == len - 1) || (macroLaneSign_[l] < 0 && pos == 0);
            if (atEnd) { macroPingPongHold_[l] = true; continue; }
        }
        macroLaneTick_[l] = (int)((((long)macroLaneTick_[l] + (long)dir * macroLaneSign_[l]) % DNA_LCM + DNA_LCM) % DNA_LCM);
        if (macroLaneDir_[l] == LaneDir::Pendulum) {
            int len = std::max(1, (l < 4) ? macroLOR_[l] : strandLen(l));
            int pos = ((macroLaneTick_[l] % len) + len) % len;
            bool atEnd = (macroLaneSign_[l] > 0 && pos == len - 1) || (macroLaneSign_[l] < 0 && pos == 0);
            if (atEnd) macroLaneSign_[l] = -macroLaneSign_[l];
        }
    }

    // Step the global DNA tick WITH direction: +1 forward, -1 backward. This is what
    // maps physical positions to drifting DNA content (strand indices) and the ring
    // lights; counting up in reverse desyncs it from stepIndex and both the strand
    // drift and the lights go the wrong way (ring lights up all around).
    if (dir < 0) totalStepsElapsed = (totalStepsElapsed - 1 + DNA_LCM) % DNA_LCM;
    else         totalStepsElapsed = (totalStepsElapsed + 1) % DNA_LCM;

    bool wrapped;
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
        wrapped = (prevStep != -1 && stepIndex == endStep);
    } else {
        // ── Forward traversal (unchanged) ──
        if (stepIndex == -1) stepIndex = (startStep - 1 + 16) % 16;
        stepIndex = (stepIndex + 1) & 0x0F;
        if (!isStepInWindow(stepIndex)) stepIndex = startStep;
        for (int s = 0; s < 16 && !isStepInWindow(stepIndex); ++s)
            stepIndex = (stepIndex + 1) & 0x0F;
        // Return true if wrapped to start (phrase boundary)
        wrapped = (prevStep != -1 && stepIndex == startStep);
    }

    // Pendulum/PingPong now bounce at the LOR window endpoint (handled above in the tick
    // advance loop), NOT at the phrase boundary. The old phrase-boundary auto-flip is removed
    // — it was the legacy lanePendulum_ mechanism, superseded by the per-lane endpoint check.
    // (If a lane's LOR length == the phrase length, the endpoint coincides with the wrap, so
    // the behaviour is the same. If the LOR is shorter, the bounce happens sooner — correct.)

    // Phrase: defer the manual flip to the wrap, so the next phrase plays in the new direction.
    if (laneFlipQuant == LaneFlipQuant::Phrase && wrapped)
        for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
            laneSign_[l] = laneSignPending_[l];
            laneDir_[l]  = laneDirPending_[l];
            for (int v = 0; v < 15; ++v) {
                laneSignV_[v][l] = laneSignVPending_[v][l];
                laneDirV_[v][l]  = laneDirVPending_[v][l];
            }
        }

    return wrapped;
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

        int dnaIdx = getStrandIdx(tickForLight, lor(dotModular::STRAND_RHYTHM,LOR_LEN), lor(dotModular::STRAND_RHYTHM,LOR_OFF), lor(dotModular::STRAND_RHYTHM,LOR_ROT));
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
    lastLegatoProb_ = legatoProb;   // for the Rule 2 per-voice slur roll (read in executePolyVoice)
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
        result.forStep = stepIndex; lastStepResult = result;
        return result;
    }

    int   sem    = 0;
    float pitchV = pe.genPitchLive(sem, input,
                                    pe.melodyRandom[getMelodyStep()],
                                    pe.octaveRandom[getOctaveStep()]);

    // ── Leading-edge legato (STEP 2, the only legato model) ──
    // The PREVIOUS starting note recorded, at its own onset, whether it intends to hold
    // its gate forward into THIS note (gs.slurForward). It survives untouched through the
    // previous note's held steps (the MidNote guard above returns early). Capture it now,
    // BEFORE this note's cascade recomputes gs.slurForward at the bottom. This onset
    // commitment — not a fresh roll at the join — governs whether THIS note connects back.
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

    // Whether THIS note connects (tie/legato) to the previous one: the previous note's onset
    // commitment — prevSlur — decides it (leading-edge). The joining onset does NOT re-roll;
    // r_legato_tie is consumed only at the LEAD commitment (gs.slurForward, at the bottom of
    // this cascade), i.e. by the note that produces the next note's prevSlur.
    // legatoProb>=0.999 (LegatoMax) still forces connection.
    const bool legatoConnects = (legatoProb >= 0.999f) || prevSlur;

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
        // note still sounding". legatoConnects (== prevSlur, the predecessor's onset
        // commitment) expresses the INTENT to connect, but intent alone isn't enough:
        // prevSlur can be true while the predecessor left NO held gate (it ended / was cut),
        // which would otherwise slide into nothing — producing an isolated Legato cell (teal
        // note connected to no predecessor). Requiring a held predecessor here makes the
        // connection real; otherwise fall through to NewNote (the slur had nothing to land
        // on → a fresh note). (Supersedes master's standalone r_legato_tie<legatoProb form:
        // the joining onset no longer re-rolls — prevSlur carries the decision.)
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

    // ── LEAD COMMITMENT (leading-edge legato) — sets the next note's prevSlur ─────
    // A note that is STARTING commits here (at its own onset) to hold its gate forward into
    // the next note, iff BOTH: (a) its own legato draw fires (r_legato_tie < legatoProb, or
    // LegatoMax), AND (b) its length is grid-aligned so it can cleanly reach the next onset
    // (noteCanLeadLegato). This is the sole consumer of r_legato_tie: the roll happens once,
    // at the LEAD, and is recorded on gs.slurForward — which the NEXT step reads as prevSlur
    // to decide its connect (see legatoConnects above). Only meaningful on a starting note;
    // cleared otherwise.
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
    bool leadsSlur = leStarting
                  && noteCanLeadLegato(nvIdx)
                  && (legatoProb >= 0.999f || r_legato_tie < legatoProb);
    gs.slurForward = leadsSlur;

    result.forStep = stepIndex; lastStepResult = result;
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
    lastNoteVal_ = noteVal;   // poly voices derive their own nvIdx from this (stage 2)
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
            voices[i].participating = false;
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
    // executeStep already assigned lastStepResult (BEFORE wrapped/stepped were set on the local
    // result), so lastStepResult.wrapped was stale-false. Re-sync so consumers that read
    // engine.lastStepResult.wrapped (e.g. the Shophouse boundary sampler) see the real value.
    lastStepResult = result;
    return result;
}

StepResult SequencerEngine::executeModeB(bool gate1Rise, bool gate1High, float restProb, float legatoProb, float noteVal, const PatternInput& input) {
    lastNoteVal_ = noteVal;   // poly voices derive their own nvIdx from this (stage 2)
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
                voices[i].gs.slurForward = false; voices[i].participating = false;
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
        lastStepResult = result;   // re-sync wrapped/stepped (executeStep set lastStepResult before they were known)
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
    // Stage 2: this voice's note length. Identical to lastStepResult.nvIdx when
    // perVoiceArticulation is off (default) or the voice's VAR LOR is identity. Clamped so the
    // voice can never hold past mono's next event — articulation is subtractive.
    const int nvV = nvIdxForVoice(voiceIdx, input);

    // Start Detection: A new note/retrigger happens on NewNote, or on 
    // Legato shifts from a dead state (low gate and no tail).
    bool monoGateStart = (lastStepResult.decision == MonoDecision::NewNote) || 
                         ((lastStepResult.decision == MonoDecision::Legato || lastStepResult.decision == MonoDecision::LegatoMax) && !wasHeldMono && !hadMonoTail);

    if (monoGateStart) {
        // This is the decision point for this entire mono gate.
        // Each strand indexes with ITS OWN lane LOR (rest/melody/octave).
        // Per-voice tick (inherits mono's per-lane direction) so a reversed mono lane reverses
        // this voice's read on the same lane.
        int restIdx = getStrandIdx(polyLaneTick(voiceIdx, PL_REST),   polyLenE(voiceIdx, PL_REST),   polyOffE(voiceIdx, PL_REST),   polyRotE(voiceIdx, PL_REST));
        int melIdx  = getStrandIdx(polyLaneTick(voiceIdx, PL_MELODY), polyLenE(voiceIdx, PL_MELODY), polyOffE(voiceIdx, PL_MELODY), polyRotE(voiceIdx, PL_MELODY));
        int octIdx  = getStrandIdx(polyLaneTick(voiceIdx, PL_OCTAVE), polyLenE(voiceIdx, PL_OCTAVE), polyOffE(voiceIdx, PL_OCTAVE), polyRotE(voiceIdx, PL_OCTAVE));
        float r_rest = pe.polyRandom(voiceIdx, PL_REST)[restIdx];
        
        if (r_rest < v.restProb) {
            // Decide to Rest: Stick with it until mono gate drops. No accent while resting.
            v.accented = false;
            if (v.gs.holdRemain > 0.0001f) v.gs.gateHeld = true; // allow previous note tail to finish
            else v.gs.gateHeld = false;
            // Rule 2: a resting voice starts no note, so it commits no forward slur, and it
            // is NOT part of this chain — landings must skip it (distinct from an opted-out
            // voice whose gate merely closed).
            if (perVoiceArticulation) { v.gs.slurForward = false; v.participating = false; }
            return;
        }
        
        // Decide to Play: Draw pitch and follow mono's triggering behavior.
        int sem = 0;
        float pitchV = pe.genPitchLive(sem, input, pe.polyRandom(voiceIdx, PL_MELODY)[melIdx], pe.polyRandom(voiceIdx, PL_OCTAVE)[octIdx]);
        // Accent as a poly lane (modelled after rest): this voice draws its OWN accent at
        // its own accent LOR and compares to its own accentProb — not shared from mono.
        int accIdx = getStrandIdx(polyLaneTick(voiceIdx, PL_ACCENT), polyLenE(voiceIdx, PL_ACCENT), polyOffE(voiceIdx, PL_ACCENT), polyRotE(voiceIdx, PL_ACCENT));
        v.accented = (pe.polyRandom(voiceIdx, PL_ACCENT)[accIdx] < v.accentProb);
        if (lastStepResult.decision == MonoDecision::NewNote)
            v.gs.triggerNote(pitchV, sem, nvV);
        else if (wasHeldPoly || hadPolyTail)
            // Mono is slurring/tying and THIS poly voice had a held predecessor → real slide.
            v.gs.slideNote(pitchV, sem, nvV, /*wasHeld=*/true);
        else
            // Mono slurs but this poly voice had NO held gate (it was resting/silent on its
            // previous played step) — sliding would produce an isolated Legato cell (teal note
            // with no predecessor on this lane), the poly analogue of the mono isolated-teal
            // bug. Trigger a fresh note instead. (Direction-independent; surfaced in reverse
            // mode but present forward too.)
            v.gs.triggerNote(pitchV, sem, nvV);

        // ── Rule 2 LEAD (per-voice leading-edge slur roll) ────────────────────────────────
        // Mirror mono's LEAD commitment (executeStep), per voice: this note commits to slur its
        // gate forward into the NEXT note iff its OWN legato draw fires. The draw reads the
        // SHARED mono legatoRandom array at THIS voice's cell (getLegatoStepForVoice → mono's
        // cell when delegated (default) so it matches mono, own cell when Local East). The
        // THRESHOLD (lastLegatoProb_) and the array stay global/mono — only the reading cell is
        // per-voice, exactly as VARIATION. Rolled here at the leading edge and consumed at the
        // NEXT landing (v.gs.slurForward → prevSlur). Gated by perVoiceArticulation; when off,
        // slurForward is left to the existing follow-mono path (Rule 2 is additive).
        // NOTE: this only ROLLS the commitment. The poly landing does not consume it yet (that
        // is Rule 2 step 3 — the opt-out re-articulate branch); until then this is inert.
        if (perVoiceArticulation) {
            // This voice played at the chain onset → it is part of the chain for its life.
            v.participating = true;
            bool leStartingV = (lastStepResult.decision == MonoDecision::NewNote)
                            || (lastStepResult.decision == MonoDecision::Legato)
                            || (lastStepResult.decision == MonoDecision::LegatoMax)
                            || (lastStepResult.decision == MonoDecision::Tie);
            float r_polyLegato = pe.legatoRandom[getLegatoStepForVoice(voiceIdx)];
            v.gs.slurForward = leStartingV
                             && noteCanLeadLegato(nvV)
                             && (lastLegatoProb_ >= 0.999f || r_polyLegato < lastLegatoProb_);
#if RULE2_DEBUG
            INFO("[R2 roll ] v=%2d step=%3d LE=%d legCell=%2d monoCell=%2d legLOR=(%2d,%2d,%2d) "
                 "r=%.3f prob=%.3f nvV=%d canLead=%d -> slur=%d",
                voiceIdx, (int)totalStepsElapsed, (int)!varlegDelegated(voiceIdx, 1),
                getLegatoStepForVoice(voiceIdx), getLegatoStep(),
                polyLOR(voiceIdx, EDITOR_LANE_LEGATO, LOR_LEN),
                polyLOR(voiceIdx, EDITOR_LANE_LEGATO, LOR_OFF),
                polyLOR(voiceIdx, EDITOR_LANE_LEGATO, LOR_ROT),
                r_polyLegato, lastLegatoProb_, nvV, (int)noteCanLeadLegato(nvV),
                (int)v.gs.slurForward);
#endif
        }
        return;
    }

    // If mono is Sustaining or Resting, poly must stick to its current role.
    if (lastStepResult.decision == MonoDecision::Rest) {
        v.gs.gateHeld = false;
        v.participating = false;   // Rule 2: mono rested → the chain ends for this voice.
    } else {
        // A LANDING = mono connects at this edge (Legato / LegatoMax / Tie). MidNote is a
        // mid-hold, not an onset edge, so it never consumes or rolls a slur.
        const bool monoConnectLanding = (lastStepResult.decision == MonoDecision::Legato)
                                      || (lastStepResult.decision == MonoDecision::LegatoMax)
                                      || (lastStepResult.decision == MonoDecision::Tie);

        if (perVoiceArticulation && monoConnectLanding) {
            // ── Rule 2 CONSUME (per-voice landing) ────────────────────────────────────────
            // A participating voice decides its OWN connect at this edge from its OWN prevSlur
            // (committed at its previous onset): prevSlur → connect (slide/extend, tracking mono's
            // articulation shape); else RE-ARTICULATE a fresh note (clamped ≤ mono via nvV). A
            // rester (participating==false) stays silent — this is why the latch is explicit, not
            // read off the gate (an opted-out voice whose short note closed is still in the chain).
            // Then re-roll this voice's forward commitment for the NEXT landing, exactly as mono
            // re-rolls at every sounding onset, so 3+ note chains carry forward per voice.
            if (!v.participating) {
                v.gs.gateHeld = false;   // rested at the onset → not part of this chain
            } else {
                const bool prevSlur = v.gs.slurForward;
                const bool isTie    = (lastStepResult.decision == MonoDecision::Tie);
                // Mono invariant: "a slur can only connect to a still-sounding predecessor."
                // A participating voice whose short note already closed via the Stage-2 clamp
                // (gatePulseRemain hit 0 mid-step) is NOT still sounding at this edge, so it must
                // RE-ARTICULATE rather than connect — even though it remains in the chain
                // (participating). Without this guard, a short legato lead followed by a clamp gap
                // (the visible/audible "rest") would draw a false teal/violet continuation at the
                // next landing, because prevSlur (committed at the lead) is only cleared by a real
                // rest DECISION (line ~568), not by an early gate close. wasHeldPoly/hadPolyTail
                // here are the still-sounding read, made accurate by the MidNote re-assert fix
                // (line ~712) which keeps a clamped voice's gateHeld from sticking high.
                const bool stillSounding = wasHeldPoly || hadPolyTail;
                const bool connect = prevSlur && stillSounding;
#if RULE2_DEBUG
                INFO("[R2 land ] v=%2d step=%3d LE=%d part=%d prevSlur=%d sound=%d isTie=%d -> %s",
                    voiceIdx, (int)totalStepsElapsed, (int)!varlegDelegated(voiceIdx, 1),
                    (int)v.participating, (int)prevSlur, (int)stillSounding, (int)isTie,
                    connect ? "CONNECT" : "REARTICULATE");
#endif
                if (connect && isTie) {
                    // committed tie → hold own pitch and extend (no redraw), like the follow-mono tie
                    v.gs.extendHold(v.gs.lastSemitone, nvV);
                } else {
                    // committed legato → slide to own new pitch; opted out / not still sounding →
                    // re-articulate fresh. Either way this voice draws its OWN pitch (melody/octave
                    // LOR), never mono's.
                    int melIdx = getStrandIdx(polyLaneTick(voiceIdx, PL_MELODY), polyLenE(voiceIdx, PL_MELODY), polyOffE(voiceIdx, PL_MELODY), polyRotE(voiceIdx, PL_MELODY));
                    int octIdx = getStrandIdx(polyLaneTick(voiceIdx, PL_OCTAVE), polyLenE(voiceIdx, PL_OCTAVE), polyOffE(voiceIdx, PL_OCTAVE), polyRotE(voiceIdx, PL_OCTAVE));
                    int sem = 0;
                    float pitchV = pe.genPitchLive(sem, input, pe.polyRandom(voiceIdx, PL_MELODY)[melIdx], pe.polyRandom(voiceIdx, PL_OCTAVE)[octIdx]);
                    if (connect) {
                        v.gs.slideNote(pitchV, sem, nvV, /*wasHeld=*/true);   // connect (keep chain accent)
                    } else {
                        // re-articulate: a fresh onset, so draw this voice's OWN accent (its accent
                        // LOR vs its own accentProb), exactly as the chain onset does — a re-struck
                        // note isn't stuck with the accent the chain opened on.
                        int accIdx = getStrandIdx(polyLaneTick(voiceIdx, PL_ACCENT), polyLenE(voiceIdx, PL_ACCENT), polyOffE(voiceIdx, PL_ACCENT), polyRotE(voiceIdx, PL_ACCENT));
                        v.accented = (pe.polyRandom(voiceIdx, PL_ACCENT)[accIdx] < v.accentProb);
                        v.gs.triggerNote(pitchV, sem, nvV);                   // re-articulate
                    }
                }
                // Re-roll the forward commitment for the NEXT landing (mirror mono's LEAD, per voice).
                float r_polyLegato = pe.legatoRandom[getLegatoStepForVoice(voiceIdx)];
                v.gs.slurForward = noteCanLeadLegato(nvV)
                                 && (lastLegatoProb_ >= 0.999f || r_polyLegato < lastLegatoProb_);
            }
        } else {
            // ── follow-mono path (perVoiceArticulation OFF, or a MidNote hold) ─────────────
            // Poly follows mono gate presence strictly IF it was already active ("in").
            if (gs.gateHeld && wasHeldPoly) {
                if (lastStepResult.decision == MonoDecision::Legato || lastStepResult.decision == MonoDecision::LegatoMax) {
                    // Mono slid to a NEW pitch (a real pitch move) → poly draws its OWN new pitch
                    // (its own melody/octave LOR) and slides to it. Poly tracks mono's articulation
                    // shape, never mono's actual notes.
                    int melIdx = getStrandIdx(polyLaneTick(voiceIdx, PL_MELODY), polyLenE(voiceIdx, PL_MELODY), polyOffE(voiceIdx, PL_MELODY), polyRotE(voiceIdx, PL_MELODY));
                    int octIdx = getStrandIdx(polyLaneTick(voiceIdx, PL_OCTAVE), polyLenE(voiceIdx, PL_OCTAVE), polyOffE(voiceIdx, PL_OCTAVE), polyRotE(voiceIdx, PL_OCTAVE));
                    int sem = 0;
                    float pitchV = pe.genPitchLive(sem, input, pe.polyRandom(voiceIdx, PL_MELODY)[melIdx], pe.polyRandom(voiceIdx, PL_OCTAVE)[octIdx]);
                    v.gs.slideNote(pitchV, sem, nvV, wasHeldPoly);
                } else if (lastStepResult.decision == MonoDecision::Tie) {
                    // Mono held the same pitch (tie) → poly holds its own current pitch, extends hold.
                    v.gs.extendHold(v.gs.lastSemitone, nvV);
                } else {
                    // MidNote: mono is HOLDING a note still sounding — nothing was chosen this step
                    // (the executeStep guard returned early). Poly follows mono's hold ONLY while THIS
                    // voice's own armed gate window is still open. Setting gateHeld=true unconditionally
                    // here would RE-OPEN a gate that tickPulse already closed (gatePulseRemain == -1) for
                    // a short/Stage-2-clamped voice — and since tickPulse only closes gates it armed
                    // (gatePulseRemain >= 0), that re-opened gate would stick high and pollute
                    // wasHeldPolyPrev at the next landing, producing a false teal/violet continuation
                    // after a rest. So keep the gate high only if the voice is genuinely still sounding;
                    // otherwise honour the length clamp and leave it released.
                    v.gs.gateHeld = (v.gs.gatePulseRemain >= 0) || (v.gs.holdRemain > 0.0001f);
                }
            } else {
                // Mono gate is low OR poly chose to rest for this current high cycle.
                v.gs.gateHeld = false;
            }
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

// ── EAST_EXTRA_LANES stage 2 ──────────────────────────────────────────────────────────────────
int SequencerEngine::getVariationStepForVoice(int bank) const {
    // Delegated (default) → read mono's VARIATION position, so the voice mirrors mono.
    if (varlegDelegated(bank, 0)) return getVariationStep();
    // Local East: read THIS voice's own window, advanced on its per-voice tick (which inherits
    // mono's per-lane direction — a reversed mono VARIATION lane reverses this voice too).
    return getStrandIdx(laneTickV_[bank][dotModular::STRAND_VARIATION],
                        polyLOR(bank, EDITOR_LANE_VARIATION, LOR_LEN),
                        polyLOR(bank, EDITOR_LANE_VARIATION, LOR_OFF),
                        polyLOR(bank, EDITOR_LANE_VARIATION, LOR_ROT));
}

int SequencerEngine::getLegatoStepForVoice(int bank) const {
    // Delegated (default) → read mono's LEGATO position, so the voice's slur roll matches mono.
    if (varlegDelegated(bank, 1)) return getLegatoStep();
    // Local East: read THIS voice's own window, advanced on its per-voice tick (inherits mono's
    // per-lane direction — a reversed mono LEGATO lane reverses this voice's slur roll too).
    return getStrandIdx(laneTickV_[bank][dotModular::STRAND_LEGATO],
                        polyLOR(bank, EDITOR_LANE_LEGATO, LOR_LEN),
                        polyLOR(bank, EDITOR_LANE_LEGATO, LOR_OFF),
                        polyLOR(bank, EDITOR_LANE_LEGATO, LOR_ROT));
}

int SequencerEngine::nvIdxForVoice(int bank, const PatternInput& input) const {
    if (!perVoiceArticulation || bank < 0 || bank >= 15) return lastStepResult.nvIdx;
    // The probability array stays MONO (one shared shape); only the reading position is per-voice.
    const int idx = getVariationStepForVoice(bank) & 0x0F;
    const float r = pe.variationRandom[idx];
    const int nv  = const_cast<SequencerEngine*>(this)->getNoteLenIdx(lastNoteVal_, input, r);
    // CLAMP to the mono event grid: a voice may release early, never hold past mono's next note.
    // NOTE_VALUES is ordered slowest→fastest, so max() picks the SHORTER of the two.
    return (nv > lastStepResult.nvIdx) ? nv : lastStepResult.nvIdx;
}

int SequencerEngine::getRhythmStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_RHYTHM],    lor(dotModular::STRAND_RHYTHM,LOR_LEN), lor(dotModular::STRAND_RHYTHM,LOR_OFF), lor(dotModular::STRAND_RHYTHM,LOR_ROT)); }
int SequencerEngine::getVariationStep() const { return getStrandIdx(laneTick_[dotModular::STRAND_VARIATION], lor(dotModular::STRAND_VARIATION,LOR_LEN), lor(dotModular::STRAND_VARIATION,LOR_OFF), lor(dotModular::STRAND_VARIATION,LOR_ROT)); }
int SequencerEngine::getLegatoStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_LEGATO],    lor(dotModular::STRAND_LEGATO,LOR_LEN), lor(dotModular::STRAND_LEGATO,LOR_OFF), lor(dotModular::STRAND_LEGATO,LOR_ROT)); }
int SequencerEngine::getAccentStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_ACCENT],    lor(dotModular::STRAND_ACCENT,LOR_LEN), lor(dotModular::STRAND_ACCENT,LOR_OFF), lor(dotModular::STRAND_ACCENT,LOR_ROT)); }
int SequencerEngine::getMelodyStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_MELODY],    lor(dotModular::STRAND_MELODY,LOR_LEN), lor(dotModular::STRAND_MELODY,LOR_OFF), lor(dotModular::STRAND_MELODY,LOR_ROT)); }
int SequencerEngine::getOctaveStep() const    { return getStrandIdx(laneTick_[dotModular::STRAND_OCTAVE],    lor(dotModular::STRAND_OCTAVE,LOR_LEN), lor(dotModular::STRAND_OCTAVE,LOR_OFF), lor(dotModular::STRAND_OCTAVE,LOR_ROT)); }

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
