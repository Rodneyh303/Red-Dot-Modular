#pragma once
#include "PatternEngine.hpp"
#include "GateState.hpp"

struct NoteVal {
    float fraction;
    int allowedPPQN;
};

static const NoteVal NOTEVALS[8] = {
    {1.0f, 1|2|4}, {0.5f, 1|2|4}, {0.25f, 1|2|4}, {0.125f, 2|4},
    {0.0625f, 2|4}, {1.0f/6.0f, 4}, {1.0f/12.0f, 4}, {0.03125f, 4},
};

struct SequencerEngine {
    PatternEngine pe;
    GateState gs;

    int stepIndex = -1;
    int lastStepIndex = -1;
    int startStep = 0;
    int endStep = 15;
    int cachedLength = 16;
    int cachedOffset = 0;

    bool locked = false;
    bool muted = false;
    bool runGateActive = false;
    bool resetArmed = false;

    int modeSelect = 0;
    int ppqnSetting = 4;
    int noteVariationMask = 0b111;

    // Quantizer cache
    int activeSemiList[12] = {};
    float activeSemiWeight[12] = {};
    int activeSemiCount = 0;
    float faderCache[12] = {};

    void reset() {
        pe.reset();
        gs.reset();
        stepIndex = -1;
        lastStepIndex = -1;
        startStep = 0;
        endStep = 15;
        cachedLength = 16;
        cachedOffset = 0;
        locked = false;
        muted = false;
        runGateActive = false;
        resetArmed = false;
        modeSelect = 0;
        ppqnSetting = 4;
        noteVariationMask = 0b111;
        activeSemiCount = 0;
        for (int i = 0; i < 12; ++i) faderCache[i] = -1.f;
    }

    bool isStepInWindow(int idx) const {
        if (startStep <= endStep) return (idx >= startStep && idx <= endStep);
        return (idx >= startStep || idx <= endStep);
    }

    void setWindow(int length, int offset) {
        cachedLength = length;
        cachedOffset = offset;
        startStep = offset;
        endStep = (offset + length - 1) & 0x0F;

        if (stepIndex != -1 && !isStepInWindow(stepIndex)) {
            stepIndex = (startStep - 1 + 16) % 16;
        }
    }

    bool advancePlayhead() {
        int prevStep = stepIndex;
        if (stepIndex == -1) stepIndex = (startStep - 1 + 16) % 16;
        stepIndex = (stepIndex + 1) & 0x0F;
        if (!isStepInWindow(stepIndex)) stepIndex = startStep;
        for (int s = 0; s < 16 && !isStepInWindow(stepIndex); ++s)
            stepIndex = (stepIndex + 1) & 0x0F;

        // Return true if wrapped to start (phrase boundary)
        return (prevStep != -1 && stepIndex == startStep);
    }

    void updateWindow(float lenParam, float lenCv, bool lenPatched, float offParam, float offCv, bool offPatched) {
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

    int computeNoteLengthIdx(int requestedIdx, int ppqnMask) const {
        int idx = pe_clamp(requestedIdx, 0, 7);
        while (idx > 0 && !(NOTEVALS[idx].allowedPPQN & ppqnMask)) {
            idx--;
        }
        return idx;
    }

    int getNoteLenIdx(float baseNoteParam, const PatternInput& input) {
        int baseIdx = pe_clamp<int>((int)std::round(baseNoteParam), 0, 7);
        int ppqnMask = (ppqnSetting == 1) ? 1 : (ppqnSetting == 4) ? 2 : 4;
        baseIdx = computeNoteLengthIdx(baseIdx, ppqnMask);
        return pe.varyNoteIndex(baseIdx, input);
    }

    void rebuildScaleCache(const float weights[12]) {
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

    float getStepLightBrightness(int lightIdx) const {
        float baseActive = isStepInWindow(lightIdx) ? 0.25f : 0.0f;
        int currentStep = (modeSelect == 2) ? stepIndex : getOffsetStep();
        float current = (modeSelect < 3 && lightIdx == currentStep) ? 1.f : 0.f;
        return std::max(baseActive, current);
    }

    int getOffsetStep() const {
        if (stepIndex == -1) return 0;
        return (stepIndex - startStep + cachedOffset + 16) % 16;
    }

    /**
     * Returns true if the current stepIndex should trigger a note event,
     * considering the PPQN setting and the pattern offset.
     */
    bool shouldTriggerStep(int ppqn) const {
        if (stepIndex == -1) return false;
        if (ppqn <= 1) {
            // Only trigger on main 4 beats relative to startStep + offset
            int relative = (stepIndex - startStep + cachedOffset + 16) % 16;
            return (relative & 0x03) == 0;
        }
        return true; 
    }

    /**
     * Performs the stochastic decision for a single sequencer step.
     * This is the "Secret Sauce" of MeloDicer logic.
     */
    void executeStep(float restProb, float legatoProb, int nvIdx) {
        float dur = gs_noteSteps(nvIdx);
        int offsetStep = getOffsetStep();
        int sem = pe.melodySemitone[offsetStep];
        float pitchV = pe.melodyPitchV[offsetStep];

        // --- Max-legato shortcut: gate always on ---
        if (legatoProb >= 0.999f) {
            gs.slideMax(pitchV, sem, nvIdx);
            return;
        }

        // --- Mid-note: inside a multi-step note or rest ---
        if (gs.holdRemain >= 1.f) {
            if (gs.gateHeld) {
                // Mid-note: maybe extend the tie
                if (pe.unitRhythm() < legatoProb) {
                    gs.extendHold(sem, nvIdx);
                }
            }
            return; // Stay inside existing note/rest body
        }

        // --- Note boundary: holdRemain < 1.0 — Fire full new decision ---

        // Draw rest first
        if (pe.unitRhythm() < restProb) {
            gs.gateHeld = false;
            gs.holdRemain = dur;
            return;
        }

        // Legato: pitch slides, gate extends, no retrigger
        bool doLegato = pe.unitRhythm() < legatoProb;
        if (doLegato) {
            gs.slideNote(pitchV, sem, nvIdx);
            return;
        }

        // Tie: same semitone only, gate extends, no pitch change
        bool doTie = (sem == gs.lastSemitone) && gs.gateHeld && (pe.unitRhythm() < legatoProb);
        if (doTie) {
            gs.extendHold(sem, nvIdx);
            return;
        }

        // Normal note: retrigger gate, set new pitch and duration
        gs.triggerNote(pitchV, sem, nvIdx);
    }

    /**
     * Comprehensive phrase boundary management.
     */
    void handlePhraseBoundary(PatternInput input, bool isMelodyRealtime, bool isRhythmRealtime) {
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

    /**
     * Mode C: Quarter-note latched quantizer.
     * Advances stepIndex on quarterEdge and quantizes inCV.
     */
    void executeModeC(bool quarterEdge, float inCV) {
        if (quarterEdge) {
            // Step index advances on each quarter-note edge
            stepIndex = (stepIndex + 1) & 0x0F;
            if (!isStepInWindow(stepIndex)) {
                stepIndex = startStep;
            }
            for (int s = 0; s < 16 && !isStepInWindow(stepIndex); ++s) {
                stepIndex = (stepIndex + 1) & 0x0F;
            }

            // Latch quantized CV
            gs.currentPitchV = quantize(inCV);
            
            // Update highlight for semitone LEDs
            int sem = int(std::round((gs.currentPitchV - std::floor(gs.currentPitchV)) * 12.f)) % 12;
            gs.lastSemitone = sem;
            gs.markSemi(sem, 4.0f); // Highlight for roughly 4 steps

            // Trigger gate pulse for output (1ms)
            gs.gatePulse.trigger(1e-3f);
        }
    }

    /**
     * Mode D: Gate-level continuous quantizer.
     * Quantizes inCV while gateHigh is true; otherwise clears gate and sets pitch to 0.
     */
    void executeModeD(bool gateHigh, float inCV) {
        gs.gateHeld = gateHigh;
        if (gateHigh) {
            // Continuous quantization while gate is held
            gs.currentPitchV = quantize(inCV);
            
            // Update highlight for semitone LEDs
            int sem = int(std::round((gs.currentPitchV - std::floor(gs.currentPitchV)) * 12.f)) % 12;
            gs.markSemi(sem, 1.0f); // Fast decay for continuous tracking
        } else {
            // Clear state when gate is low
            gs.currentPitchV = 0.f;
            gs.gatePulse.reset();
        }
    }

    float quantize(float vIn) {
        if (activeSemiCount == 0) return pe_clamp(vIn, 0.f, 5.f);
        int octave = (int)std::floor(vIn);
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
        return pe_clamp<float>(bestV, 0.f, 5.f);
    }
};
