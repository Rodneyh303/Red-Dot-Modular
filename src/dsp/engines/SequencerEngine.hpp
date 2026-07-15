#pragma once
#include "PatternEngine.hpp"
#include "../gates/GateState.hpp"
#include "ClockEngine.hpp"
#include "../NoteValues.hpp"
#include "../LaneMapping.hpp"
#include <cassert>
#include <cstdio>

// TARGETED DEBUG for Rule 2 per-voice legato — set to 0 (and rebuild) to remove.
// Shared by SequencerEngine.cpp (roll/consume trace) and MonsoonExpanderManager.cpp
// (delegation-param + LEG-LOR push trace). Output → Rack log.txt (stderr).
#define RULE2_DEBUG 0

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
    // Rule 2 (EAST_EXTRA_LANES §4d): latched at the mono chain onset — true if this voice
    // PLAYED (vs rested) when mono started the gate, and held for the chain's life. It, not
    // the gate-held flag, is what tells a landing whether this voice is part of the chain:
    // a voice that opted out and let its short note close still reads participating==true and
    // re-articulates, whereas a rester reads false and stays silent. Only consulted when
    // perVoiceArticulation is on. Reset at each new onset and when mono rests.
    bool  participating = false;
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

    // Most recent mono legato probability — written by executeStep, read by executePolyVoice
    // for the Rule 2 per-voice slur roll (the legato THRESHOLD stays global/mono; only each
    // voice's reading CELL differs, via getLegatoStepForVoice). Same write-once/read-by-poly
    // pattern as lastStepResult above.
    float lastLegatoProb_ = 0.f;

    // Leading-edge legato is now the ONLY legato model: the connection is governed by the
    // PREVIOUS note's onset commitment (gs.slurForward), captured before this note's cascade
    // recomputes it. The old reactive model (fresh r_legato_tie roll at the joining onset)
    // and its legatoLeadingEdge toggle were removed — reactive was never UI-reachable (no
    // menu/switch ever wrote it) and Stage 3 poly opt-in requires a single, onset-committed
    // semantics. Rest still cancels an optional slur (rest branch takes priority); fractional
    // tails still outrank rest (canRest). See RHYTHM_BEHAVIOUR_POLICIES.md.

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

    // ── Per-lane direction (reverse a lane's read independently) ──────────────────
    // 4-state direction per strand: Forward, Reverse, Pendulum (bounce, no endpoint repeat),
    // PingPong (bounce, WITH endpoint repeat — the endpoint step plays twice per turn).
    // Each strand walks its OWN accumulated tick; the sign is derived from the state (+1 fwd,
    // -1 rev; Pendulum/PingPong flip at the window wrap). PingPong also holds the tick at the
    // endpoint for one extra step (repeat) before flipping. See plans/lane_direction_ui.md.
    enum class LaneDir : uint8_t { Forward = 0, Reverse = 1, Pendulum = 2, PingPong = 3 };
    enum class LaneFlipQuant { StepEdge, Phrase };
    int laneTick_[dotModular::NUM_STRANDS]        = {0,0,0,0,0,0};
    int laneSign_[dotModular::NUM_STRANDS]        = {1,1,1,1,1,1};   // derived from laneDir_ (+1/-1)
    int laneSignPending_[dotModular::NUM_STRANDS] = {1,1,1,1,1,1};
    LaneDir laneDir_[dotModular::NUM_STRANDS]        = {};   // default Forward
    LaneDir laneDirPending_[dotModular::NUM_STRANDS] = {};
    bool lanePendulum_[dotModular::NUM_STRANDS]   = {false,false,false,false,false,false}; // derived: Pendulum/PingPong
    // PingPong endpoint-repeat: when true, the tick stays at the endpoint for one extra step
    // (the endpoint step repeats) before the direction flips. Per-strand hold flag.
    bool lanePingPongHold_[dotModular::NUM_STRANDS] = {};
    // Closed-form window position for a lane at global tick t (STATELESS model). Position is
    // f(totalStepsElapsed, len, dir) — no accumulator — so lanes cannot drift out of phase with
    // the DNA clock or each other, whenever their direction was flipped. laneTick_* below are
    // now a CACHE of this, recomputed each step in advancePlayhead, kept only so existing
    // readers (displays, prob-outs) need no change. Implemented in the .cpp.
    static long laneTickFor(LaneDir d, long t, int len);

    // Clear the WALK state of the bouncing lanes (Pendulum/PingPong) so they restart from the
    // window origin. Forward/Reverse need nothing — they are a closed form of totalStepsElapsed
    // and follow it automatically. Called on hard restart, where zeroing the master clock alone
    // would leave a walker carrying on and silently ignoring RESET.
    void resetLaneWalk() {
        for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
            laneTick_[s] = 0; laneSign_[s] = 1; lanePingPongHold_[s] = false;
            macroLaneTick_[s] = 0; macroLaneSign_[s] = 1; macroPingPongHold_[s] = false;
            for (int v = 0; v < 15; ++v) {
                laneTickV_[v][s] = 0; laneSignV_[v][s] = 1; lanePingPongHoldV_[v][s] = false;
            }
        }
    }
    // Macro's own per-lane tick — always follows Macro's direction, independent of
    // who owns the lane. Same bounce logic as laneTick_. Advanced in advancePlayhead.
    // Macro's widget reads this for its playhead, so Macro's display always reflects
    // Macro's DirCell, even when Mono owns the lane (engine's laneTick_ follows Mono).
    int macroLaneTick_[dotModular::NUM_STRANDS] = {};
    int macroLaneSign_[dotModular::NUM_STRANDS] = {1,1,1,1,1,1};
    LaneDir macroLaneDir_[dotModular::NUM_STRANDS] = {};
    bool macroPingPongHold_[dotModular::NUM_STRANDS] = {};
    int macroLOR_[4] = {16,16,16,16};  // Macro's own LOR lengths (lanes 0..3) for bounce
    // Per-voice per-strand accumulated tick (poly analogue of laneTick_). Advanced in advancePlayhead
    // by dir * polyLaneSign(v, s) — the effective sign is the voice's OWN, i.e. ABSOLUTE, not
    // relative to mono. laneSignV_ = +1 (default) = Forward; -1 = Reverse. A voice therefore does
    // NOT inherit mono's reversal: the DirCell directly controls that voice's direction. (It was
    // once laneSign_[s] * polyLaneSign(v, s) — mono-relative — which read as counter-intuitive
    // when mono was reversed.) laneTickV_[v][s] == laneTick_[s] exactly only when the voice AND
    // mono's lane are both Forward → no-op until something is reversed.
    // laneSignVPending_ is promoted to laneSignV_ at the same boundary as mono's (laneFlipQuant),
    // so a per-voice flip also takes effect musically (no jump).
    int laneTickV_[15][dotModular::NUM_STRANDS]        = {};
    int laneSignV_[15][dotModular::NUM_STRANDS]        = {};   // default 0 → treated as +1 (see polyLaneSign)
    int laneSignVPending_[15][dotModular::NUM_STRANDS] = {};
    LaneDir laneDirV_[15][dotModular::NUM_STRANDS]        = {};   // per-voice direction (default Forward)
    LaneDir laneDirVPending_[15][dotModular::NUM_STRANDS] = {};
    bool lanePingPongHoldV_[15][dotModular::NUM_STRANDS] = {};   // per-voice ping-pong hold flag
    // Effective per-voice sign for a strand: +1 if laneSignV_ is 0 (default/unset → follow mono) or
    // positive, -1 if negative. Multiplied with mono's laneSign_ in advancePlayhead.
    int polyLaneSign(int bank, int strand) const {
        if (bank < 0 || bank >= 15) return 1;
        return (laneSignV_[bank][strand] < 0) ? -1 : +1;
    }
    int polyLaneSignPending(int bank, int strand) const {
        if (bank < 0 || bank >= 15) return 1;
        return (laneSignVPending_[bank][strand] < 0) ? -1 : +1;
    }
    // Derive sign (+1/-1) from a LaneDir state.
    static int laneDirSign(LaneDir d) { return (d == LaneDir::Reverse) ? -1 : +1; }
    // Is this state an auto-flip (pendulum/ping-pong)?
    static bool laneDirAutoFlip(LaneDir d) { return d == LaneDir::Pendulum || d == LaneDir::PingPong; }
    LaneFlipQuant laneFlipQuant = LaneFlipQuant::Phrase;   // musical default; StepEdge = live turns
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
    // ── Unified probability-modifier LOR storage (Step 2b-ii) ────────────────────
    // ONE array for all 16 voices × 6 editor lanes × 3 items, replacing the separate monoLOR[6][3]
    // and polyLen/Off/Rot[15][4]. Indexed [voiceSlot][editorLane][item] where
    //   voiceSlot = VoiceResolver::voiceSlot(v):  slot 0 = V1 (mono), slots 1..15 = V2..V16 (poly);
    //   editorLane 0..5 = MEL,OCT,REST,ACC,VAR,LEG (poly uses only 0..3; VAR/LEG are mono-only);
    //   item = MonoItem {LOR_LEN,LOR_OFF,LOR_ROT}.
    // This is the storage layer finally matching the VoiceResolver addressing layer. All access goes
    // through the accessors below — mono via lor()/lorRef() (editor lane == strand), poly via the
    // editor-order polyLOR() and the engine-order polyLenE() family (which now converts engine→editor
    // here, at the single storage boundary). Seeded in ctor/reset.
    enum MonoItem { LOR_LEN = 0, LOR_OFF = 1, LOR_ROT = 2, LOR_ITEMS = 3 };
    static constexpr int kVoiceSlots = 16;
    int lorStore_[kVoiceSlots][dotModular::NUM_STRANDS][LOR_ITEMS] = {};   // [slot][editorLane][item]

    // Mono (V1 = slot 0): editor lane == strand index (the identity), so strand IS the lane column.
    int&       lorRef(int strand, int item)       { return lorStore_[0][strandClamp(strand)][item]; }
    int        lor   (int strand, int item) const { return lorStore_[0][strandClamp(strand)][item]; }
    static int strandClamp(int s) { return (s >= 0 && s < dotModular::NUM_STRANDS) ? s : dotModular::STRAND_RHYTHM; }

    enum PolyLane { PL_REST = 0, PL_MELODY = 1, PL_OCTAVE = 2, PL_ACCENT = 3, PL_LANES = 4 };

    // ── EAST_EXTRA_LANES stage 2: per-voice ARTICULATION (clamped) ─────────────────────────────
    // VARIATION/LEGATO are mono STRANDS, not poly lanes, so they have no PL_ id and CANNOT be
    // addressed via polyLenE()/editorLane() (that masks & 3 and would alias VAR->REST). Use the
    // editor-order accessors polyLOR/polyLORRef, which mask & 7.
    static constexpr int EDITOR_LANE_VARIATION = 4;
    static constexpr int EDITOR_LANE_LEGATO    = 5;

    // OFF by default: every poly voice uses mono's nvIdx exactly, as before. Even when ON, the
    // per-voice LOR defaults to identity (len 16, off 0, rot 0), so voices read mono's own
    // variation index and the result is bit-identical. Doubly inert.
    bool perVoiceArticulation = false;

    // VAR/LEG per-voice delegation (EAST_EXTRA_LANES §4d). false (default) = delegate to
    // mono → the voice reads MONO's VAR/LEG position, so it mirrors mono (silent). true =
    // Local East → the voice reads its OWN LOR slot and may diverge. Indexed [bank][lane],
    // bank 0..14 = V2..V16, lane 0 = VAR, 1 = LEG. Zero-init = all-delegate = follow mono,
    // which is also the correct fallback when East is absent (nothing sets Local East).
    // Pushed each cycle by MonsoonExpanderManager from the East delegation params.
    bool varlegLocalEast_[15][2] = {};
    void setVarlegLocalEast(int bank, int lane, bool localEast) {
        if (bank >= 0 && bank < 15 && lane >= 0 && lane < 2) varlegLocalEast_[bank][lane] = localEast;
    }
    // VAR = lane 0, LEG = lane 1.
    bool varlegDelegated(int bank, int lane) const {
        return !(bank >= 0 && bank < 15 && lane >= 0 && lane < 2 && varlegLocalEast_[bank][lane]);
    }

    // Per-voice VARIATION step — mono's getStrandIdx transform, driven by THIS voice's LOR window.
    // Delegated voices (default) return mono's variation step, so they mirror mono.
    int getVariationStepForVoice(int bank) const;

    // Per-voice LEGATO step — same, for Rule 2's per-voice slur roll. Delegated voices (default)
    // return mono's legato step. Present now (Step 1) so Rule 2 consumes it without re-plumbing.
    int getLegatoStepForVoice(int bank) const;

    // This voice's note-length index, CLAMPED so its hold can never exceed mono's.
    // NOTE_VALUES is documented "ordered slowest -> fastest", so a HIGHER index is a SHORTER note;
    // std::max(nv, mono) therefore guarantees duration <= mono's. Articulation is thus SUBTRACTIVE,
    // exactly like rest and accent: voices may release early, never hold past mono's next event.
    int nvIdxForVoice(int bank, const PatternInput& input) const;

    // The single engine→editor lane conversion. The modifier stores (lorStore_, spread) are editor-
    // ordered; every accessor that takes an engine PL_ lane converts through HERE — one definition
    // instead of the ENGINE_LANE_TO_EDITOR[engLane&3] formula previously inlined in each accessor.
    // engLane is a POLY lane (PL_REST/PL_MELODY/PL_OCTAVE/PL_ACCENT) — 0..3 ONLY. VARIATION and
    // LEGATO are mono strands, NOT poly lanes (PL_LANES == 4), so they have no engine-order id.
    // The `& 3` therefore cannot be reached with 4/5 by correct code — but if it were, it would
    // silently alias VAR->REST and LEG->MELODY. For per-voice VAR/LEG LOR use the EDITOR-order
    // accessors polyLOR/polyLORRef (they mask & 7 and index lorStore_ directly). See
    // docs/design/EAST_EXTRA_LANES.md.
    static int editorLane(int engLane) { return dotModular::ENGINE_LANE_TO_EDITOR[engLane & 3]; }

    // Editor-order poly accessors: bank b → slot b+1, editorLane indexes the unified array directly
    // (storage IS editor order now — no permutation).
    int& polyLORRef(int bank, int editorLane, int item) { return lorStore_[bank + 1][editorLane & 7][item]; }
    int  polyLOR   (int bank, int editorLane, int item) const { return lorStore_[bank + 1][editorLane & 7][item]; }

    // Engine-order poly accessors: bank b → slot b+1; the engine PL_ lane converts to editor lane
    // via ENGINE_LANE_TO_EDITOR HERE — the single storage boundary that absorbs the permutation.
    int& polyLenERef(int bank, int engLane) { return lorStore_[bank + 1][editorLane(engLane)][LOR_LEN]; }
    int& polyOffERef(int bank, int engLane) { return lorStore_[bank + 1][editorLane(engLane)][LOR_OFF]; }
    int& polyRotERef(int bank, int engLane) { return lorStore_[bank + 1][editorLane(engLane)][LOR_ROT]; }
    int  polyLenE(int bank, int engLane) const { return lorStore_[bank + 1][editorLane(engLane)][LOR_LEN]; }
    int  polyOffE(int bank, int engLane) const { return lorStore_[bank + 1][editorLane(engLane)][LOR_OFF]; }
    int  polyRotE(int bank, int engLane) const { return lorStore_[bank + 1][editorLane(engLane)][LOR_ROT]; }

    // ── Spread: the 4th probability modifier, now ENGINE STATE (MVC: was on the Mono visual, which the
    // engine reached into for the draws — model reading from view. Now the model owns it). Stored in the
    // SAME shape/order/index as lorStore_: spread[voiceSlot][editorLane], editor-ordered, slot 0 = V1
    // (mono). Only the 4 spread lanes (editor MEL/OCT/REST/ACC = 0..3) are used; VAR/LEG (4,5) have no
    // spread (documented-unused, keeps spread column-compatible with lorStore_ so the two can later
    // share one modifier accessor). Written by the manager via SpreadResolver; read by the engine draws
    // and (for its own display) the Mono visual — both through spreadE(slot, engineLane), which absorbs
    // the engine→editor permutation at this single boundary (same trick as polyLenE).
    float spread[kVoiceSlots][dotModular::NUM_STRANDS] = {};   // [slot][editorLane]; spread lanes 0..3 used

    float& spreadERef(int slot, int engLane) { return spread[slot][editorLane(engLane)]; }
    float  spreadE   (int slot, int engLane) const { return spread[slot][editorLane(engLane)]; }


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
    float lastNoteVal_ = 4.f;   // mono NOTE_VALUE param of the current step (poly reads it)

    // ── Probability CV-out accessors (Sands East/Macro poly outs) ──────────────
    // Per-voice final probability (post A/B mix + spread + LOR) for a poly lane
    // (0=REST→rhythm, 1=MEL→melody, 2=OCT→octave), at that voice's own LOR step.
    // 0..1. Used to assemble the poly probability cables (channels 2..1+nVoices).
    inline float polyLaneProbability(int polyLane, int voice) const {
        if (voice < 0 || voice >= 15 || polyLane < 0 || polyLane >= PL_LANES) return 0.f;
        // Follow this voice's own lane position, not the raw global tick: a reversed lane's
        // notes come from polyLaneTick, so its probability CV must too or the CV reports a
        // different cell than the one being played (they silently disagreed before).
        int idx = getStrandIdx(polyLaneTick(voice, polyLane), polyLenE(voice, polyLane),
                               polyOffE(voice, polyLane), polyRotE(voice, polyLane));
        idx &= 0x0F;
        // Switch collapsed: polyRandom takes the lane directly (unified storage), so no per-case map.
        if (polyLane < 0 || polyLane >= PL_LANES) return 0.f;
        return pe.polyRandom(voice, polyLane)[idx];
    }
    // Per-voice draw value for a poly lane at an EXPLICIT step (caller supplies the
    // step from its own LOR view). Used by Macro's prob-out, which samples each voice's
    // draw at MACRO's own global LOR step — independent of East/ownership. East instead
    // uses polyLaneProbability (resolved per-voice step). 0..1.
    inline float polyLaneProbabilityAtStep(int polyLane, int voice, int step) const {
        if (voice < 0 || voice >= 15 || polyLane < 0 || polyLane >= PL_LANES) return 0.f;
        step &= 0x0F;
        return pe.polyRandom(voice, polyLane)[step];   // switch collapsed (unified storage)
    }

    // Step indices (for S&H edge detection) matching the probability accessors above.
    inline int polyLaneStep(int polyLane, int voice) const {
        if (voice < 0 || voice >= 15 || polyLane < 0 || polyLane >= PL_LANES) return -1;
        return getStrandIdx(polyLaneTick(voice, polyLane), polyLenE(voice, polyLane),
                            polyOffE(voice, polyLane), polyRotE(voice, polyLane)) & 0x0F;
    }
    // Engine poly lane (PL_REST..PL_ACCENT) → strand index. Single mapping (was inlined in
    // masterLaneStep and polyLaneTick). PL_LANES.. have no strand (VAR/LEG are mono-only); this
    // is only valid for the 4 poly lanes.
    static int polyLaneStrand(int polyLane) {
        return (polyLane == PL_REST)   ? dotModular::STRAND_RHYTHM
             : (polyLane == PL_MELODY) ? dotModular::STRAND_MELODY
             : (polyLane == PL_ACCENT) ? dotModular::STRAND_ACCENT
                                       : dotModular::STRAND_OCTAVE;
    }
    // Per-voice tick for an engine poly lane. The voice's direction is ABSOLUTE (its own DirCell),
    // so this tracks mono's laneTick_[strand] only while both the voice and mono's lane are
    // Forward — a voice does NOT inherit mono's reversal. Out-of-range bank falls back to mono's tick.
    int polyLaneTick(int bank, int polyLane) const {
        int strand = polyLaneStrand(polyLane);
        return (bank >= 0 && bank < 15) ? laneTickV_[bank][strand] : laneTick_[strand];
    }
    // Per-voice effective sign for an engine poly lane — ABSOLUTE (the voice's own sign,
    // not relative to mono). +1 = forward (default); -1 = reversed. Used by the visual
    // cue (each poly voice's leading-edge marker points its own way).
    int polyLaneDir(int bank, int polyLane) const {
        int strand = polyLaneStrand(polyLane);
        return polyLaneSign(bank, strand);
    }
    inline int masterLaneStep(int polyLane) const {
        int strand = polyLaneStrand(polyLane);
        // Mono lane position, direction-aware (was the raw global tick, so the master
        // prob-out ignored per-lane direction while the notes followed it).
        return getStrandIdx(laneTick_[strand], strandLen(strand),
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
