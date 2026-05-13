#pragma once
#include "PatternEngine.hpp"
#include "GateState.hpp"
#include "ClockEngine.hpp"

struct NoteVal {
    float fraction;
    int allowedPPQN;
};

extern const NoteVal NOTEVALS[8];

struct SequencerEngine {
    PatternEngine pe;
    GateState gs[8]; // Array of GateState for polyphony

    int stepIndex = -1;
    int lastStepIndex = -1;
    int startStep = 0;
    int endStep = 15;
    int cachedLength = 16;
    int cachedOffset = 0;
    int totalStepsElapsed = 0; // Running counter for drifting polymetric DNA alignment

    // Strand-specific Windowing (Length 1..16, Offset 0..15)
    int rhythmLen = 16;
    int rhythmOff = 0;
    int variationLen = 16;
    int variationOff = 0;
    int legatoLen = 16;
    int legatoOff = 0;
    int melodyLen = 16;
    int melodyOff = 0;
    int octaveLen = 16;
    int octaveOff = 0;

    // Discrete mutation offsets (mutation from scramble/context menu)
    int rhythmRot = 0, variationRot = 0, legatoRot = 0, melodyRot = 0, octaveRot = 0;

    int dnaLength = 16; // Legacy/Global fallback
    int dnaOffset = 0;

    uint16_t windowMask = 0xFFFF;

    bool locked = false;
    bool muted = false;
    bool runGateActive = false;
    bool resetArmed = false;
    bool prevGate1High = false;

    int modeSelect = 0;
    int ppqnSetting = 4;
    int noteVariationMask = 0b111;

    // Quantizer cache
    int activeSemiList[12] = {};
    float activeSemiWeight[12] = {};
    int activeSemiCount = 0;
    float faderCache[12] = {};

    // Quantizer memoization
    float lastQuantIn = -100.f;
    float lastQuantOut = 0.f;

    void reset();
    bool isStepInWindow(int idx) const;
    void setWindow(int length, int offset);
    bool advancePlayhead();
    void updateWindow(float lenParam, float lenCv, bool lenPatched, float offParam, float offCv, bool offPatched);
    int computeNoteLengthIdx(int requestedIdx, int ppqnMask) const;
    int getNoteLenIdx(float baseNoteParam, const PatternInput& input, float r);
    void rebuildScaleCache(const float weights[12]);
    float getStepLightBrightness(int lightIdx) const;
    int getOffsetStep() const;
    int getStrandIdx(int tickCount, int len, int off, int mutation) const;

    int calculatePitchPoolSize(const PatternInput& input) const;

    // Independent lookup indices for each "DNA strand"
    int getRhythmStep() const;
    int getVariationStep() const;
    int getLegatoStep() const;
    int getMelodyStep() const;
    int getOctaveStep() const;

    bool shouldTriggerStep(int ppqn) const;
    void executeStep(int voice, float restProb, float legatoProb, int nvIdx, float r_rest, float r_legato_tie, const PatternInput& input, bool wasHeld); // Added voice parameter
    void handlePhraseBoundary(PatternInput input, bool isMelodyRealtime, bool isRhythmRealtime);
    bool executeModeA(const ClockEngine& clock, const float restProbs[8], float legatoProb, float noteVal, const PatternInput& input, int numVoices); // Added restProbs and numVoices
    bool executeModeB(bool gate1Rise, bool gate1High, const float restProbs[8], float legatoProb, float noteVal, const PatternInput& input, int numVoices); // Added restProbs and numVoices
    void executeModeC(const ClockEngine& clock, float inCV);
    void executeModeD(bool gateHigh, float inCV);
    float quantize(float vIn);
};
