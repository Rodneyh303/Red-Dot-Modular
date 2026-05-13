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
    for (int i = 0; i < 8; i++) gs[i].reset(); // Reset all 8 GateState instances

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

void SequencerEngine::executeStep(float restProb, float legatoProb, int nvIdx, float r_rest, float r_legato_tie, const PatternInput& input, bool wasHeld) {
    float dur = gs_noteSteps(nvIdx);
    
    int sem = 0;
    float pitchV = pe.genPitchLive(sem, input, pe.melodyRandom[getMelodyStep()], pe.octaveRandom[getOctaveStep()]);

    if (gs.holdRemain < 1.f) {
        if (legatoProb >= 0.999f) {
            gs.slideMax(pitchV, sem, nvIdx);
        }
        else {
            if (r_rest < restProb) {
                gs.gateHeld = false;
                gs.holdRemain = dur;
            }
            else if (r_legato_tie < legatoProb) {
                // Connected: Either Tie or Legato
                if (sem == gs.lastSemitone && wasHeld) {
                    gs.extendHold(sem, nvIdx);
                } else {
                    gs.slideNote(pitchV, sem, nvIdx, wasHeld);
                }
            }
            else {
                // Disconnected: New Note
                gs.triggerNote(pitchV, sem, nvIdx);
            }
        }
    }
    // Mid-note: randomness already accounted for by r_rest/r_legato_tie.
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

bool SequencerEngine::executeModeA(const ClockEngine& clock, const float restProbs[8], float legatoProb, float noteVal, const PatternInput& input, int numVoices) {
    if (!clock.sixteenthEdge || muted) return false;

    int poolSize = calculatePitchPoolSize(input);
    int activeVoices = std::min(numVoices, poolSize);
    if (activeVoices < 1) {
        for (int i = 0; i < 8; i++) gs[i].reset();
        advancePlayhead();
        return false;
    }

    bool wrapped = advancePlayhead();
    float r_vary   = pe.variationRandom[getVariationStep()];
    float r_rest   = pe.rhythmRandom[getRhythmStep()];
    float r_legato = pe.legatoRandom[getLegatoStep()];
    int nvIdx = getNoteLenIdx(noteVal, input, r_vary);

    for (int v = 0; v < 8; v++) {
        bool wasHeld = gs[v].gateHeld; // Capture before tick
        gs[v].tick(); // Tick all voices regardless of activeVoices
        if (v < activeVoices) { // Only execute step for active voices
            executeStep(v, restProbs[v], legatoProb, nvIdx, r_rest, r_legato, input, wasHeld);
        }
    }
    return wrapped;
}

bool SequencerEngine::executeModeB(bool gate1Rise, bool gate1High, const float restProbs[8], float legatoProb, float noteVal, const PatternInput& input, int numVoices) {
    if (muted) {
        prevGate1High = gate1High;
        return false;
    }
    bool wrapped = false;
    bool triggered = false;

    if (gate1Rise) {
        wrapped = advancePlayhead();
        triggered = true;
    } else if (gate1High && !prevGate1High && stepIndex == -1) {
        advancePlayhead(); // Advance to startStep
        triggered = true;
    }

    int poolSize = calculatePitchPoolSize(input);
    int activeVoices = std::min(numVoices, poolSize);

    if (triggered) {
        float r_vary   = pe.variationRandom[getVariationStep()];
        float r_rest   = pe.rhythmRandom[getRhythmStep()];
        float r_legato = pe.legatoRandom[getLegatoStep()];
        
        int nvIdx = getNoteLenIdx(noteVal, input, r_vary);
        for (int v = 0; v < 8; v++) {
            bool wasHeld = gs[v].gateHeld;
            gs[v].tick();
            if (v < activeVoices) {
                executeStep(v, restProbs[v], legatoProb, nvIdx, r_rest, r_legato, input, wasHeld);
            }
        }
    }

    prevGate1High = gate1High;
    return wrapped;
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
