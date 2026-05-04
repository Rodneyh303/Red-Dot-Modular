#include "SequencerEngine.hpp"
#include <cmath>
#include <algorithm>

const NoteVal NOTEVALS[8] = {
    {1.0f, 1|2|4}, {0.5f, 1|2|4}, {0.25f, 1|2|4}, {0.125f, 2|4},
    {0.0625f, 2|4}, {1.0f/6.0f, 4}, {1.0f/12.0f, 4}, {0.03125f, 4},
};

void SequencerEngine::reset() {
    pe.reset();
    gs.reset();
    stepIndex = -1;
    lastStepIndex = -1;
    startStep = 0;
    endStep = 15;
    cachedLength = 16;
    cachedOffset = 0;
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
    // Differentiate notes and rests visually: Notes are brighter (0.35), Rests are dim (0.07)
    bool isNote = pe.rhythmPattern[lightIdx];
    float baseActive = isStepInWindow(lightIdx) ? (isNote ? 0.35f : 0.07f) : 0.0f;

    int currentStep = (modeSelect == 2) ? stepIndex : getOffsetStep();
    float current = (modeSelect < 3 && lightIdx == currentStep) ? 1.0f : 0.0f;

    return std::max(baseActive, current);
}

int SequencerEngine::getOffsetStep() const {
    if (stepIndex == -1) return 0;
    return (stepIndex - startStep + cachedOffset + 16) % 16;
}

bool SequencerEngine::shouldTriggerStep(int ppqn) const {
    if (stepIndex == -1) return false;
    if (ppqn <= 1) {
        // Only trigger on main 4 beats relative to startStep + offset
        int relative = (stepIndex - startStep + cachedOffset + 16) % 16;
        return (relative & 0x03) == 0;
    }
    return true; 
}

void SequencerEngine::executeStep(float restProb, float legatoProb, int nvIdx, float r_rest, float r_legato_tie, const PatternInput& input, bool wasHeld) {
    float dur = gs_noteSteps(nvIdx);
    int offsetStep = getOffsetStep();
    
    int sem = 0;
    float pitchV = pe.genPitchLive(sem, input, pe.melodyRandom[offsetStep], pe.octaveRandom[offsetStep]);

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

bool SequencerEngine::executeModeA(const ClockEngine& clock, float restProb, float legatoProb, float noteVal, const PatternInput& input) {
    if (!clock.sixteenthEdge || muted) return false;

    bool wrapped = advancePlayhead();
    int offsetStep = getOffsetStep();
    float r_vary   = pe.variationRandom[offsetStep];
    float r_rest   = pe.rhythmRandom[offsetStep];
    float r_legato = pe.legatoRandom[offsetStep];
    
    int nvIdx = getNoteLenIdx(noteVal, input, r_vary);

    bool wasHeld = gs.gateHeld;
    gs.tick();
    executeStep(restProb, legatoProb, nvIdx, r_rest, r_legato, input, wasHeld);
    return wrapped;
}

bool SequencerEngine::executeModeB(bool gate1Rise, bool gate1High, float restProb, float legatoProb, float noteVal, const PatternInput& input) {
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

    if (triggered) {
        int offsetStep = getOffsetStep();
        float r_vary   = pe.variationRandom[offsetStep];
        float r_rest   = pe.rhythmRandom[offsetStep];
        float r_legato = pe.legatoRandom[offsetStep];
        
        int nvIdx = getNoteLenIdx(noteVal, input, r_vary);

        bool wasHeld = gs.gateHeld;
        gs.tick();
        executeStep(restProb, legatoProb, nvIdx, r_rest, r_legato, input, wasHeld);
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