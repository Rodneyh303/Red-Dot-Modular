#include "SequencerEngine.hpp"
#include <cmath>
#include <algorithm>

const NoteVal NOTEVALS[8] = {
    {1.0f, 1|2|4}, {0.5f, 1|2|4}, {0.25f, 1|2|4}, {0.125f, 2|4},
    {0.0625f, 2|4}, {1.0f/6.0f, 4}, {1.0f/12.0f, 4}, {0.03125f, 4},
};

static const int DNA_LCM = 720720; // LCM of 1..16 ensures drift continuity

void SequencerEngine::reset() {
    pe.reset();
    gs.reset();
    for (int i = 0; i < 7; ++i) voices[i].gs.reset();
    // restProb values are NOT reset — the caller re-applies them from expander knobs.
    lastStepResult = StepResult{};
    stepIndex = -1;
    lastStepIndex = -1;
    startStep = 0;
    endStep = 15;
    cachedLength = 16;
    cachedOffset = 0;
    totalStepsElapsed = 0;
    rhythmLen = variationLen = legatoLen = melodyLen = octaveLen = 16;
    rhythmOff = variationOff = legatoOff = melodyOff = octaveOff = 0;
    rhythmRot = variationRot = legatoRot = melodyRot = octaveRot = 0;
    for (int i = 0; i < 7; i++) {
        polyLen[i] = 16;
        polyOff[i] = 0;
        polyRot[i] = 0;
    }
    windowMask = 0xFFFF;
    locked = false;
    muted = false;
    runGateActive = false;
    resetArmed = false;
    prevGate1High = false;
    modeSelect = 0;
    ppqnSetting = 4;
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

bool SequencerEngine::advancePlayhead() {
    int prevStep = stepIndex;
    totalStepsElapsed = (totalStepsElapsed + 1) % DNA_LCM;
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
    while (idx > 0 && !(NOTEVALS[idx].allowedPPQN & ppqnMask)) {
        idx--;
    }
    return idx;
}

int SequencerEngine::getNoteLenIdx(float baseNoteParam, const PatternInput& input, float r) {
    int baseIdx = pe_clamp<int>((int)std::round(baseNoteParam), 0, 7);
    int ppqnMask = (ppqnSetting == 1) ? 1 : (ppqnSetting == 4) ? 2 : 4;
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

    // The moving playhead should always follow the global timeline index
    float current = (modeSelect < 3 && lightIdx == stepIndex) ? 1.0f : 0.0f;

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

StepResult SequencerEngine::executeStep(float restProb, float legatoProb, int nvIdx, float r_rest, float r_legato_tie, const PatternInput& input, bool wasHeld) {
    StepResult result;
    result.nvIdx = nvIdx;

    // Mid-note: previous note's hold is still counting down.
    // Poly voices seeing MidNote will tick their own holds and return.
    if (gs.holdRemain >= 1.f) {
        result.decision = MonoDecision::MidNote;
        lastStepResult = result;
        return result;
    }

    int   sem    = 0;
    float pitchV = pe.genPitchLive(sem, input,
                                    pe.melodyRandom[getMelodyStep()],
                                    pe.octaveRandom[getOctaveStep()]);

    if (legatoProb >= 0.999f) {
        gs.slideMax(pitchV, sem, nvIdx);
        result.decision = MonoDecision::LegatoMax;
    }
    else if (r_rest < restProb) {
        gs.gateHeld = false;
        // holdRemain reset is unnecessary — gateHeld=false already closes gate in process().
        // The holdRemain value is ignored when gateHeld=false.
        result.decision = MonoDecision::Rest;
    }
    else if (r_legato_tie < legatoProb) {
        if (sem == gs.lastSemitone && wasHeld) {
            gs.extendHold(sem, nvIdx);
            result.decision = MonoDecision::Tie;
        } else {
            gs.slideNote(pitchV, sem, nvIdx, wasHeld);
            result.decision = MonoDecision::Legato;
        }
    }
    else {
        gs.triggerNote(pitchV, sem, nvIdx);
        result.decision = MonoDecision::NewNote;
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

StepResult SequencerEngine::executeModeA(const ClockEngine& clock, float restProb, float legatoProb, float noteVal, const PatternInput& input) {
    StepResult result;
    if (!clock.sixteenthEdge || muted) return result;

    bool wrapped = advancePlayhead();
    float r_vary   = pe.variationRandom[getVariationStep()];
    float r_rest   = pe.rhythmRandom[getRhythmStep()];
    float r_legato = pe.legatoRandom[getLegatoStep()];
    
    int nvIdx = getNoteLenIdx(noteVal, input, r_vary);

    bool wasHeld = gs.gateHeld;
    gs.tick();
    
    // Tick poly voices at the same time as mono so gates expire in lockstep.
    // This ensures poly trailing edges align with mono and don't trail.
    for (int i = 0; i < numPolyVoices; ++i)
        voices[i].gs.tick();
    
    result = executeStep(restProb, legatoProb, nvIdx, r_rest, r_legato, input, wasHeld);
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
        float r_vary   = pe.variationRandom[getVariationStep()];
        float r_rest   = pe.rhythmRandom[getRhythmStep()];
        float r_legato = pe.legatoRandom[getLegatoStep()];
        
        int nvIdx = getNoteLenIdx(noteVal, input, r_vary);

        bool wasHeld = gs.gateHeld;
        gs.tick();
        
        // Tick poly voices at the same time as mono so gates expire in lockstep.
        for (int i = 0; i < numPolyVoices; ++i)
            voices[i].gs.tick();
        
        result = executeStep(restProb, legatoProb, nvIdx, r_rest, r_legato, input, wasHeld);
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
//              own pitch using stochasticRng.  Gate type (retrigger vs slide)
//              follows mono's decision.  Within the legato zone a poly-internal
//              tie still emerges naturally when the voice's random pitch
//              matches its own previous semitone.

void SequencerEngine::executePolyVoice(int voiceIdx, const PatternInput& input) {
    PolyVoice& v = voices[voiceIdx];
    bool wasHeld = v.gs.gateHeld;
    // v.gs.tick() is NO LONGER called here — poly voices tick in executeModeA/B
    // at the same time as mono, ensuring gates expire in lockstep.

    switch (lastStepResult.decision) {

        case MonoDecision::MidNote:
            return;  // mono is still mid-note; poly continues naturally

        case MonoDecision::Rest:
            // Mono rested — poly cannot initiate a new note.
            // Already-held notes continue to decay naturally from their shared tick().
            return;

        case MonoDecision::Tie:
            // Mono extended its hold on the same pitch.
            // Poly voices that are currently sounding extend their own holds.
            // Voices already silent stay silent — don't resurrect a resting voice.
            if (wasHeld)
                v.gs.extendHold(v.gs.lastSemitone, lastStepResult.nvIdx);
            return;

        case MonoDecision::Legato:
        case MonoDecision::LegatoMax:
        case MonoDecision::NewNote:
            break;  // fall through to independent note draw
    }

    // Access pre-generated DNA index for specific poly voice
    int polyIdx = getStrandIdx(totalStepsElapsed, polyLen[voiceIdx], polyOff[voiceIdx], polyRot[voiceIdx]);

    float r_rest = pe.polyRhythmRandom[voiceIdx][polyIdx];
    if (r_rest < v.restProb) return;

    int   sem    = 0;
    float pitchV = pe.genPitchLive(sem, input,
                                    pe.polyMelodyRandom[voiceIdx][polyIdx],
                                    pe.polyOctaveRandom[voiceIdx][polyIdx]);

    if (lastStepResult.decision == MonoDecision::NewNote) {
        v.gs.triggerNote(pitchV, sem, lastStepResult.nvIdx);
    } else {
        // Legato zone — no retrigger.
        // Poly-internal tie: if this voice's random pitch matches its previous
        // semitone while its gate is held, extend rather than slide.
        if (sem == v.gs.lastSemitone && wasHeld)
            v.gs.extendHold(sem, lastStepResult.nvIdx);
        else
            v.gs.slideNote(pitchV, sem, lastStepResult.nvIdx, wasHeld);
    }
}

void SequencerEngine::executePolyVoices(const PatternInput& input) {
    for (int i = 0; i < numPolyVoices; ++i)
        executePolyVoice(i, input);
}

void SequencerEngine::executeModeC(const ClockEngine& clock, float inCV) {
    gs.gateHeld = false;
    if (clock.quarterEdge) {
        gs.tick();
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
int SequencerEngine::getMelodyStep() const    { return getStrandIdx(totalStepsElapsed, melodyLen, melodyOff, melodyRot); }
int SequencerEngine::getOctaveStep() const    { return getStrandIdx(totalStepsElapsed, octaveLen, octaveOff, octaveRot); }
