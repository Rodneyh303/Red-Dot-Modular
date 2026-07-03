#pragma once
#include "PatternEngine.hpp"
#include "../gates/GateState.hpp"
#include "ClockEngine.hpp"
#include "../NoteValues.hpp"
#include "../LaneMapping.hpp"
#include <cassert>
#include <cstdio>

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
    float accentProb = 0.0f;   // per-voice accent probability (accent as a poly lane)
    bool  accented = false;    // result of this voice's own accent draw this step
};

// ── SequencerEngine ────────────────────────────────────────────────────────────

// ── Debug-only strand-write ledger ────────────────────────────────────────────
// Every producer that writes a mono strand's LOR (length/offset/rotation) declares
// WHO it is via this role. In debug builds the engine records the role per strand
// per process block and asserts that no strand is written by two different roles in
// the same block — the invariant "exactly one writer per strand". This is the guard
// that would have caught the Mono+Macro clobber (Macro-only sync stamping Mono's V1
// strands) on the first frame: "melodyLen written by MONO and MACRO".
enum class StrandWriter : uint8_t {
    NONE = 0,
    MONO,      // MonsoonSandsManager::readStrand (Mono visual owns V1, per-lane)
    EAST,      // StraitsEastSandsVisual V1 editor (East owns / East-delegated-to-Macro)
    MACRO,     // MonsoonExpanderManager Macro-only sync (Macro is the sole Sands visual)
};

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

    // Leading-edge legato instrument (STEP 1: characterization only, no behaviour change).
    // Counts starting notes and how often the leading-edge slur-forward flag would DIVERGE
    // from the current model's connect/not decision at the join. Purely diagnostic — read
    // these to gauge how different the two models are before step 2 flips the decision source.
    long legatoLE_startCount   = 0;
    long legatoLE_divergeCount = 0;

    // ── TEMP ring — REMOVE after ──
    static constexpr int DBG_N = 64, DBG_W = 200;
    char dbgLog[DBG_N][DBG_W] = {};
    int  dbgHead = 0, dbgTail = 0;
    void dbgPush(const char* s) {
        std::snprintf(dbgLog[dbgHead], DBG_W, "%s", s);
        dbgHead = (dbgHead + 1) % DBG_N;
        if (dbgHead == dbgTail) dbgTail = (dbgTail + 1) % DBG_N;
    }
    // TEMP: the note length (nvIdx) and canLead of the step that most recently SET slurForward,
    // so we can see whether the live slurForward came from a legal (integer-length) leader or is
    // stale/illegal.
    int  dbgSlurSetByNvIdx = -1;
    bool dbgSlurSetByCanLead = false;
    int  dbgSlurSetAtStep = -1;

    // STEP 2: when true, the legato connection is governed by the PREVIOUS note's onset
    // commitment (gs.slurForward) instead of a fresh roll at the joining onset — the
    // leading-edge model. Default OFF = exact current behaviour. Toggle (context menu /
    // JSON) so the two models can be A/B'd by ear. Rest still cancels an optional slur
    // (rest branch takes priority); fractional tails still outrank rest (canRest).
    bool legatoLeadingEdge = true;   // BRANCH DEFAULT ON: this branch tests the leading-edge
                                     // model. (Reactive path still selectable by setting false;
                                     // a proper context-menu toggle comes later per
                                     // RHYTHM_BEHAVIOUR_POLICIES.md.)

    // ── Rhythm-behaviour toggles (context menu; see RHYTHM_BEHAVIOUR_POLICIES.md) ──
    // "Rest beats legato" — when a committed slur (prevSlur) is reaching N+1 and N+1 rolls a
    // rest: TRUE (default) = rest WINS, cancelling the slur (N+1 silent). FALSE = slur WINS,
    // the rest roll is IGNORED and N+1 plays as a Legato/Tie (its own drawn pitch, gate-
    // connected from N). Only affects the case where a genuine committed slur lands on a held
    // predecessor; a rest on a non-slur note is unaffected.
    bool restBeatsLegato = true;

    // "Boundary interrupt" — at the phrase boundary (wrap): FALSE (default) = CONTINUE, gate/
    // state carries across the loop (lap 2 can differ from lap 1). TRUE = INTERRUPT, force a
    // fresh start at the boundary (reset the held gate) so every lap is identical.
    bool boundaryInterrupt = false;

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
    enum PolyLane { PL_REST = 0, PL_MELODY = 1, PL_OCTAVE = 2, PL_ACCENT = 3, PL_LANES = 4 };
    int polyLen[15][PL_LANES];
    int polyOff[15][PL_LANES];
    int polyRot[15][PL_LANES];

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

    // ── Strand-write ledger (debug-only assert; behaviour-inert in release) ──────
    // strandWriter[strand] = the role that wrote this strand this block. Reset at the
    // top of each process block via beginStrandWriteBlock(). setStrand() records the
    // writer and, in debug, asserts no second role writes the same strand in one block.
    // Strand index domain is dotModular::STRAND_* (0..5). NONE means "not yet written".
    StrandWriter strandWriter[6] = { StrandWriter::NONE };

    void beginStrandWriteBlock() {
        for (int i = 0; i < 6; ++i) strandWriter[i] = StrandWriter::NONE;
    }

    // The ONLY sanctioned way to write a mono strand's LOR. Records the writer role,
    // asserts single-writer-per-block in debug, then assigns via the ref accessors.
    // (len clamped 1..16; off/rot wrapped 0..15 — same normalisation the call sites did.)
    void setStrand(StrandWriter role, int strand, int len, int off, int rot) {
#ifndef NDEBUG
        if (strand >= 0 && strand < 6) {
            StrandWriter prev = strandWriter[strand];
            if (prev != StrandWriter::NONE && prev != role) {
                // Two different producers wrote the same strand this block — the exact
                // bug class that made Mono+Macro V1 uneditable. Log loudly; the engine
                // value is now ambiguous (last writer wins, races the display).
#ifdef WARN
                WARN("[StrandLedger] CONFLICT strand=%d written by %d then %d (two writers in one block)",
                     strand, (int)prev, (int)role);
#else
                std::fprintf(stderr,
                     "[StrandLedger] CONFLICT strand=%d written by %d then %d (two writers in one block)\n",
                     strand, (int)prev, (int)role);
#endif
                assert(false && "two writers for one engine strand in a single block");
            }
            strandWriter[strand] = role;
        }
#endif
        if (strand >= 0 && strand < 6) {
            strandLenRef(strand) = rack::math::clamp(len, 1, 16);
            strandOffRef(strand) = ((off % 16) + 16) % 16;
            strandRotRef(strand) = ((rot % 16) + 16) % 16;
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

    // ── Probability CV-out accessors (Sands East/Macro poly outs) ──────────────
    // Per-voice final probability (post A/B mix + spread + LOR) for a poly lane
    // (0=REST→rhythm, 1=MEL→melody, 2=OCT→octave), at that voice's own LOR step.
    // 0..1. Used to assemble the poly probability cables (channels 2..1+nVoices).
    inline float polyLaneProbability(int polyLane, int voice) const {
        if (voice < 0 || voice >= 15 || polyLane < 0 || polyLane >= PL_LANES) return 0.f;
        int idx = getStrandIdx(totalStepsElapsed, polyLen[voice][polyLane],
                               polyOff[voice][polyLane], polyRot[voice][polyLane]);
        idx &= 0x0F;
        switch (polyLane) {
            case PL_REST:   return pe.polyRhythmRandom[voice][idx];
            case PL_MELODY: return pe.polyMelodyRandom[voice][idx];
            case PL_OCTAVE: return pe.polyOctaveRandom[voice][idx];
            case PL_ACCENT: return pe.polyAccentRandom[voice][idx];
            default:        return 0.f;
        }
    }
    // Per-voice draw value for a poly lane at an EXPLICIT step (caller supplies the
    // step from its own LOR view). Used by Macro's prob-out, which samples each voice's
    // draw at MACRO's own global LOR step — independent of East/ownership. East instead
    // uses polyLaneProbability (resolved per-voice step). 0..1.
    inline float polyLaneProbabilityAtStep(int polyLane, int voice, int step) const {
        if (voice < 0 || voice >= 15 || polyLane < 0 || polyLane >= PL_LANES) return 0.f;
        step &= 0x0F;
        switch (polyLane) {
            case PL_REST:   return pe.polyRhythmRandom[voice][step];
            case PL_MELODY: return pe.polyMelodyRandom[voice][step];
            case PL_OCTAVE: return pe.polyOctaveRandom[voice][step];
            case PL_ACCENT: return pe.polyAccentRandom[voice][step];
            default:        return 0.f;
        }
    }

    // Step indices (for S&H edge detection) matching the probability accessors above.
    inline int polyLaneStep(int polyLane, int voice) const {
        if (voice < 0 || voice >= 15 || polyLane < 0 || polyLane >= PL_LANES) return -1;
        return getStrandIdx(totalStepsElapsed, polyLen[voice][polyLane],
                            polyOff[voice][polyLane], polyRot[voice][polyLane]) & 0x0F;
    }
    inline int masterLaneStep(int polyLane) const {
        int strand = (polyLane == PL_REST)   ? dotModular::STRAND_RHYTHM
                   : (polyLane == PL_MELODY) ? dotModular::STRAND_MELODY
                   : (polyLane == PL_ACCENT) ? dotModular::STRAND_ACCENT
                                             : dotModular::STRAND_OCTAVE;
        return getStrandIdx(totalStepsElapsed, strandLen(strand),
                            strandOff(strand), strandRot(strand)) & 0x0F;
    }
    inline float masterLaneProbability(int polyLane) const {
        int strand = (polyLane == PL_REST)   ? dotModular::STRAND_RHYTHM
                   : (polyLane == PL_MELODY) ? dotModular::STRAND_MELODY
                   : (polyLane == PL_ACCENT) ? dotModular::STRAND_ACCENT
                                             : dotModular::STRAND_OCTAVE;
        return pe.finalRandomByStrand(strand, masterLaneStep(polyLane));
    }


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
