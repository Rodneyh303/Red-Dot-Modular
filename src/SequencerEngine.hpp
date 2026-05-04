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
    GateState gs;

    int stepIndex = -1;
    int lastStepIndex = -1;
    int startStep = 0;
    int endStep = 15;
    int cachedLength = 16;
    int cachedOffset = 0;
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
    bool shouldTriggerStep(int ppqn) const;
    void executeStep(float restProb, float legatoProb, int nvIdx, float r_rest, float r_legato_tie, const PatternInput& input, bool wasHeld);
    void handlePhraseBoundary(PatternInput input, bool isMelodyRealtime, bool isRhythmRealtime);
    bool executeModeA(const ClockEngine& clock, float restProb, float legatoProb, float noteVal, const PatternInput& input);
    bool executeModeB(bool gate1Rise, bool gate1High, float restProb, float legatoProb, float noteVal, const PatternInput& input);
    void executeModeC(const ClockEngine& clock, float inCV);
    void executeModeD(bool gateHigh, float inCV);
    float quantize(float vIn);
};
