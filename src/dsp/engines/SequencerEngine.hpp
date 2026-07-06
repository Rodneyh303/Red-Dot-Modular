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
    int  forStep = -1;      // the stepIndex this result was computed for (Lantern pairs decision→column)
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

    // ── Mono (V1) strand windowing: Length (1..16), Offset (0..15), Rotation ──
    // Step 1 of the probability-modifier unification (docs/design/PROBABILITY_MODIFIER_MODEL.md):
    // the 18 named fields (rhythmLen…octaveRot) + their 6 hand-written switch(strand) accessors are
    // now ONE array indexed by strand (== editor lane, already the identity) × item {LEN,OFF,ROT}.
    // This is mono/V1 only for now; a later step folds it in as slot 0 of a unified len/off/rot[16][6]
    // alongside the poly arrays. Item order is fixed by MonoItem below. Access via strandLen/Off/Rot
    // + …Ref (unchanged signatures); direct callers use lorRef(strand, item).
    enum MonoItem { LOR_LEN = 0, LOR_OFF = 1, LOR_ROT = 2, LOR_ITEMS = 3 };
    int monoLOR[dotModular::NUM_STRANDS][LOR_ITEMS] = {};   // [strand][item]; seeded in ctor/reset

    int&       lorRef(int strand, int item)       { return monoLOR[strandClamp(strand)][item]; }
    int        lor   (int strand, int item) const { return monoLOR[strandClamp(strand)][item]; }
    static int strandClamp(int s) { return (s >= 0 && s < dotModular::NUM_STRANDS) ? s : dotModular::STRAND_RHYTHM; }

    // Per-voice-per-lane poly LOR — the INDEX into the 16-step probability vectors, kept SEPARATE
    // from the probabilities. Lane: 0=REST,1=MELODY,2=OCTAVE,3=ACCENT. East sets all [voice][lane]
    // independently; Macro sets the same lane LOR for every voice. (Folds into the unified array as
    // slots 1..15 in a later step.)
    enum PolyLane { PL_REST = 0, PL_MELODY = 1, PL_OCTAVE = 2, PL_ACCENT = 3, PL_LANES = 4 };
    int polyLen[15][PL_LANES];
    int polyOff[15][PL_LANES];
    int polyRot[15][PL_LANES];

    // Editor-order poly LOR accessors (Step 2a of the probability-modifier unification): present the
    // poly storage in EDITOR lane order (MEL=0,OCT=1,REST=2,ACC=3) — the canonical order the unified
    // len/off/rot[16][6] will use — converting to the underlying PL_* engine lane internally via
    // EDITOR_TO_ENGINE_LANE. Storage is UNCHANGED here; this just gives poly the same editor-order
    // interface mono already has, so Step 2b can physically merge the arrays with no permutation.
    // editorLane is 0..3 (poly has no VAR/LEG); item is MonoItem {LOR_LEN,LOR_OFF,LOR_ROT}.
    int& polyLORRef(int bank, int editorLane, int item) {
        int eng = dotModular::EDITOR_TO_ENGINE_LANE[editorLane & 3];   // editor → PL_ engine lane
        switch (item) {
            case LOR_OFF: return polyOff[bank][eng];
            case LOR_ROT: return polyRot[bank][eng];
            case LOR_LEN:
            default:      return polyLen[bank][eng];
        }
    }
    int polyLOR(int bank, int editorLane, int item) const {
        int eng = dotModular::EDITOR_TO_ENGINE_LANE[editorLane & 3];
        switch (item) {
            case LOR_OFF: return polyOff[bank][eng];
            case LOR_ROT: return polyRot[bank][eng];
            case LOR_LEN:
            default:      return polyLen[bank][eng];
        }
    }

    // Indexable strand accessors keyed by dotModular::EngineStrand order
    // (0 rhythm, 1 variation, 2 legato, 3 accent, 4 melody, 5 octave). These let
    // callers go editor-lane → strand (via MONO_LANE_TO_STRAND) → value without
    // hardcoding the permutation. Single source of truth for strand indexing.
    // Strand LOR readers/writers, now backed by monoLOR[strand][item] (one array, no switches).
    // Out-of-range semantics preserved EXACTLY from the previous 6 switches:
    //   readers  → literal fallback (LEN 16, OFF 0, ROT 0) for an invalid strand;
    //   writers  → the rhythm cell (strandClamp maps invalid → STRAND_RHYTHM), so an out-of-range
    //              write lands on rhythm just as the old `default: return rhythmLen/Off/Rot` did.
    int strandLen(int strand) const {
        return (strand >= 0 && strand < dotModular::NUM_STRANDS) ? lor(strand, LOR_LEN) : 16;
    }
    int strandOff(int strand) const {
        return (strand >= 0 && strand < dotModular::NUM_STRANDS) ? lor(strand, LOR_OFF) : 0;
    }
    int strandRot(int strand) const {
        return (strand >= 0 && strand < dotModular::NUM_STRANDS) ? lor(strand, LOR_ROT) : 0;
    }
    int& strandLenRef(int strand) { return lorRef(strand, LOR_LEN); }   // strandClamp → rhythm fallback
    int& strandOffRef(int strand) { return lorRef(strand, LOR_OFF); }
    int& strandRotRef(int strand) { return lorRef(strand, LOR_ROT); }

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
                //
                // NOTE: this is a diagnostic, NOT a fatal condition — the documented
                // behaviour is "last writer wins", which is a defined (if imperfect)
                // fallback. A hard abort here (the old assert(false)) crashed the plugin on
                // a LOAD-TIME TRANSIENT: during patch load there's a window where East's
                // widget step() already writes its strand as EAST while Monsoon::process
                // still sees cachedEastSandsVisual==nullptr (not yet populated by the
                // expander scan) and so classifies MACRO_SOLE, writing the same strand as
                // MACRO. Both fire in one block → conflict → (previously) crash. The cache
                // populates a frame later and the conflict clears. So: WARN and continue
                // (last-writer-wins), which is exactly the intent above. If a conflict ever
                // persists in steady state, the log line will show it every block.
#ifdef WARN
                WARN("[StrandLedger] CONFLICT strand=%d written by %d then %d (two writers in one block)",
                     strand, (int)prev, (int)role);
#else
                std::fprintf(stderr,
                     "[StrandLedger] CONFLICT strand=%d written by %d then %d (two writers in one block)\n",
                     strand, (int)prev, (int)role);
#endif
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
