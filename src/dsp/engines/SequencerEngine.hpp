#pragma once
#include "PatternEngine.hpp"
#include "../gates/GateState.hpp"
#include "ClockEngine.hpp"
#include "../NoteValues.hpp"
#include "../LaneMapping.hpp"

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
    bool accented = false;  // true if this step is accented (NEW)
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
    PolyVoice voices[15];
    int       numPolyVoices = 0;

    // Most recent mono decision — written by executeStep, read by executePolyVoices.
    StepResult lastStepResult;

    int stepIndex = -1;
    int lastPlayDir = +1;   // +1 forward, -1 reverse (Mode E); for UI direction cues
    int lastStepIndex = -1;
    int startStep = 0;
    int endStep = 15;
    int cachedLength = 16;
    int cachedOffset = 0;
    int totalStepsElapsed = 0; // Running counter for drifting polymetric DNA alignment

    bool hadMonoTail = false;
    bool wasHeldMono = false; // Capture mono state before tick() for start-detection
    bool hadPolyTail[15] = {};
    bool wasHeldPolyPrev[15] = {}; // Capture poly state before tick()

    // Strand-specific Windowing (Length 1..16, Offset 0..15)
    int rhythmLen = 16;
    int rhythmOff = 0;
    int variationLen = 16;
    int variationOff = 0;
    int legatoLen = 16;
    int legatoOff = 0;
    int accentLen = 16;     // NEW: accent strand length (DNA)
    int accentOff = 0;      // NEW: accent strand offset (DNA)
    int melodyLen = 16;
    int melodyOff = 0;
    int octaveLen = 16;
    int octaveOff = 0;
    // Per-voice-per-lane LOR (length/offset/rotation) — the INDEX into the
    // 16-step probability vectors, kept SEPARATE from the probabilities.
    // Lane: 0=REST, 1=MELODY, 2=OCTAVE. East sets all [voice][lane]
    // independently; Macro sets the same lane LOR for every voice.
    enum PolyLane { PL_REST = 0, PL_MELODY = 1, PL_OCTAVE = 2, PL_LANES = 3 };
    int polyLen[15][3];
    int polyOff[15][3];
    int polyRot[15][3];

    // Discrete mutation offsets (mutation from scramble/context menu)
    int rhythmRot = 0, variationRot = 0, legatoRot = 0, accentRot = 0, melodyRot = 0, octaveRot = 0;

    // Indexable strand accessors keyed by dotModular::EngineStrand order
    // (0 rhythm, 1 variation, 2 legato, 3 accent, 4 melody, 5 octave). These let
    // callers go editor-lane → strand (via MONO_LANE_TO_STRAND) → value without
    // hardcoding the permutation. Single source of truth for strand indexing.
    int strandLen(int strand) const {
        switch (strand) {
            case dotModular::STRAND_RHYTHM:    return rhythmLen;
            case dotModular::STRAND_VARIATION: return variationLen;
            case dotModular::STRAND_LEGATO:    return legatoLen;
            case dotModular::STRAND_ACCENT:    return accentLen;
            case dotModular::STRAND_MELODY:    return melodyLen;
            case dotModular::STRAND_OCTAVE:    return octaveLen;
            default: return 16;
        }
    }
    int strandOff(int strand) const {
        switch (strand) {
            case dotModular::STRAND_RHYTHM:    return rhythmOff;
            case dotModular::STRAND_VARIATION: return variationOff;
            case dotModular::STRAND_LEGATO:    return legatoOff;
            case dotModular::STRAND_ACCENT:    return accentOff;
            case dotModular::STRAND_MELODY:    return melodyOff;
            case dotModular::STRAND_OCTAVE:    return octaveOff;
            default: return 0;
        }
    }
    int strandRot(int strand) const {
        switch (strand) {
            case dotModular::STRAND_RHYTHM:    return rhythmRot;
            case dotModular::STRAND_VARIATION: return variationRot;
            case dotModular::STRAND_LEGATO:    return legatoRot;
            case dotModular::STRAND_ACCENT:    return accentRot;
            case dotModular::STRAND_MELODY:    return melodyRot;
            case dotModular::STRAND_OCTAVE:    return octaveRot;
            default: return 0;
        }
    }

    // Writable strand references (same EngineStrand keying) so producers can
    // assign by index too — used by readStrand to write the per-lane result to
    // the correct strand via MONO_LANE_TO_STRAND, instead of a hand-ordered call
    // list. rhythmLen is the safe fallback for an out-of-range index.
    int& strandLenRef(int strand) {
        switch (strand) {
            case dotModular::STRAND_VARIATION: return variationLen;
            case dotModular::STRAND_LEGATO:    return legatoLen;
            case dotModular::STRAND_ACCENT:    return accentLen;
            case dotModular::STRAND_MELODY:    return melodyLen;
            case dotModular::STRAND_OCTAVE:    return octaveLen;
            case dotModular::STRAND_RHYTHM:
            default:                           return rhythmLen;
        }
    }
    int& strandOffRef(int strand) {
        switch (strand) {
            case dotModular::STRAND_VARIATION: return variationOff;
            case dotModular::STRAND_LEGATO:    return legatoOff;
            case dotModular::STRAND_ACCENT:    return accentOff;
            case dotModular::STRAND_MELODY:    return melodyOff;
            case dotModular::STRAND_OCTAVE:    return octaveOff;
            case dotModular::STRAND_RHYTHM:
            default:                           return rhythmOff;
        }
    }
    int& strandRotRef(int strand) {
        switch (strand) {
            case dotModular::STRAND_VARIATION: return variationRot;
            case dotModular::STRAND_LEGATO:    return legatoRot;
            case dotModular::STRAND_ACCENT:    return accentRot;
            case dotModular::STRAND_MELODY:    return melodyRot;
            case dotModular::STRAND_OCTAVE:    return octaveRot;
            case dotModular::STRAND_RHYTHM:
            default:                           return rhythmRot;
        }
    }

    int dnaLength = 16; // Legacy/Global fallback
    int dnaOffset = 0;

    uint16_t windowMask = 0xFFFF;

    bool locked = false;
    bool muted = false;
    bool runGateActive = false;
    bool resetArmed = false;
    bool prevGate1High = false;

    int modeSelect = 0;
    int ppqnSetting = 24;  // master PPQN pulse grid (24/48/96)
    int noteVariationMask = 0b111;
    float accentProb = 0.25f;  // Probability of accent on each note (0..1) (NEW)

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
    bool advancePlayhead(int dir = +1);   // dir<0 = reverse traversal (within-draw)
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
    int getAccentStep() const;  // NEW: accent strand DNA index
    int getMelodyStep() const;
    int getOctaveStep() const;

    bool shouldTriggerStep(int ppqn) const;
    StepResult executeStep(float restProb, float legatoProb, int nvIdx, float r_rest, float r_legato_tie, float r_accent, float accentProb, const PatternInput& input, bool wasHeld, bool hadTail);
    void handlePhraseBoundary(PatternInput input, bool isMelodyRealtime, bool isRhythmRealtime);
    StepResult executeModeA(const ClockEngine& clock, float restProb, float legatoProb, float noteVal, const PatternInput& input, int dir = +1);
    StepResult executeModeB(bool gate1Rise, bool gate1High, float restProb, float legatoProb, float noteVal, const PatternInput& input);
    void executeModeC(const ClockEngine& clock, float inCV);
    void executeModeD(bool gateHigh, float inCV);
    float quantize(float vIn);

    // High-level DNA Actions
    void scrambleRhythmStrands();
    void scrambleMelodyStrands();
    void scrambleAllStrands();
    void resetRhythmStrands();
    void resetMelodyStrands();
    void resetAllStrands();
    void syncVisuals(const PatternInput& in);

    // ── Poly voice execution ──────────────────────────────────────────────────
    // Call executePolyVoices() after any stepped executeModeA/B call.
    // voices[i].restProb must be set by the caller before invoking.
    void executePolyVoice(int voiceIdx, const PatternInput& input, bool wasHeld, bool hadTail);
    void executePolyVoices(const PatternInput& input);
};
