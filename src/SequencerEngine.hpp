#pragma once
#include "PatternEngine.hpp"
#include "GateState.hpp"
#include "ClockEngine.hpp"

struct NoteVal {
    float fraction;
    int allowedPPQN;
};

extern const NoteVal NOTEVALS[8];

// ── Poly voice architecture ────────────────────────────────────────────────────

// What mono decided on the most recent step edge.
// Poly voices read this to determine their own behaviour.
enum class MonoDecision {
    MidNote,    // note still held from a previous step — poly just ticks, no new draw
    Rest,       // mono rested — poly cannot initiate
    Tie,        // mono extended hold (same pitch) — poly voices hold their own notes
    Legato,     // mono slid to a new pitch (no retrigger) — poly may draw new notes
    LegatoMax,  // legatoProb >= 0.999 — poly may draw, no retrigger
    NewNote     // mono retriggered — poly may retrigger independently
};

// Returned by executeStep and the mode executors.
struct StepResult {
    MonoDecision decision = MonoDecision::MidNote;
    int  nvIdx   = 0;       // note-length index used this step
    bool stepped = false;   // true if a step edge actually fired this sample
    bool wrapped = false;   // true if the phrase boundary wrapped
    bool accented = false;  // true if this step is accented
};

// One additional voice beyond mono.  Owns its own GateState and rest
// probability; everything else (nvIdx, legatoProb, scale) is shared from mono.
struct PolyVoice {
    GateState gs;
    float restProb = 0.0f;
};

// ── SequencerEngine ────────────────────────────────────────────────────────────

struct SequencerEngine {
    PatternEngine pe;
    GateState gs;

    // ── Poly voices ───────────────────────────────────────────────────────────
    // voices[0] = voice 2, voices[6] = voice 8.
    // numPolyVoices is set from the context-menu user preference (0 = mono only).
    // It is intentionally NOT cleared by reset() so it survives patch reload.
    PolyVoice voices[7];
    int       numPolyVoices = 0;

    // Most recent mono decision — written by executeStep, read by executePolyVoices.
    StepResult lastStepResult;

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
    int accentLen = 16;     // New: accent strand length (DNA)
    int accentOff = 0;      // New: accent strand offset (DNA)
    int melodyLen = 16;
    int melodyOff = 0;
    int octaveLen = 16;
    int octaveOff = 0;
    int polyLen[7];
    int polyOff[7];
    int polyRot[7];

    // Discrete mutation offsets (mutation from scramble/context menu)
    int rhythmRot = 0, variationRot = 0, legatoRot = 0, accentRot = 0, melodyRot = 0, octaveRot = 0;

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
    float accentProb = 0.25f;  // Probability of accent on each note (0..1)

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


    // Independent lookup indices for each "DNA strand"
    int getRhythmStep() const;
    int getVariationStep() const;
    int getLegatoStep() const;
    int getAccentStep() const;  // New: accent strand DNA index
    int getMelodyStep() const;
    int getOctaveStep() const;

    bool shouldTriggerStep(int ppqn) const;
    StepResult executeStep(float restProb, float legatoProb, int nvIdx, float r_rest, float r_legato_tie, const PatternInput& input, bool wasHeld);
    void handlePhraseBoundary(PatternInput input, bool isMelodyRealtime, bool isRhythmRealtime);
    StepResult executeModeA(const ClockEngine& clock, float restProb, float legatoProb, float noteVal, const PatternInput& input);
    StepResult executeModeB(bool gate1Rise, bool gate1High, float restProb, float legatoProb, float noteVal, const PatternInput& input);
    void executeModeC(const ClockEngine& clock, float inCV);
    void executeModeD(bool gateHigh, float inCV);
    float quantize(float vIn);

    // ── Poly voice execution ──────────────────────────────────────────────────
    // Call executePolyVoices() after any stepped executeModeA/B call.
    // voices[i].restProb must be set by the caller before invoking.
    void executePolyVoice(int voiceIdx, const PatternInput& input);
    void executePolyVoices(const PatternInput& input);
};
