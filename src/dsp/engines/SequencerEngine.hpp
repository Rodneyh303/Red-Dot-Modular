#pragma once
#include "RhythmDecisionCore.hpp"   // RDM: MonoDecision + ParentView — the R-2 seam (plan §2)
#include "PatternEngine.hpp"
#include "../SpreadInterp.hpp"   // effectiveProbability's spread term (the a2ccb842 one-accessor design it subsumes was deleted by UI_TO_RESOLVER step 2)
#include "../gates/GateState.hpp"
#ifdef MONSOON_PROFILE
#include "../../MonsoonProfile.hpp"   // CC_PROMPT_cpu_section_profile: Suspect-A call counter (empty flag-off)
#endif
#include "ClockEngine.hpp"
#include "../../ui/RateManager.hpp"   // RATE MANAGER (S3b): executeModeA gates on the Step tier (rack-free)
#include "../NoteValues.hpp"
#include "../LaneMapping.hpp"
#include "../SubgateKnobs.hpp"   // SUBGATE TIER: the resolved-threshold dims + the three-way resolver (rack-free)
#include "../StepAdvance.hpp"     // STEP-ADVANCE: the pure decideAdvance(p, u) (CC_PROMPT_GATE_STEP_ADVANCE_SPEC, rack-free)
#include "../VoiceLaneTypes.hpp"   // VoiceSlot — the VoiceBank below (strong per-voice indexing)
#include <cassert>
#include <cstdio>

// TARGETED DEBUG for Rule 2 per-voice legato — set to 0 (and rebuild) to remove.
// Shared by SequencerEngine.cpp (roll/consume trace) and MonsoonExpanderManager.cpp
// (delegation-param + LEG-LOR push trace). Output → Rack log.txt (stderr).
#define RULE2_DEBUG 0

// ── Poly voice architecture ────────────────────────────────────────────────────

// (MonoDecision MOVED to RhythmDecisionCore.hpp — R-2 of RHYTHM_DECISION_MANAGER_PLAN:
//  the decision vocabulary is RDM vocabulary, carried by ParentView. Included above, so
//  every existing user sees the type unchanged.)

// Returned by executeStep and the mode executors.
struct StepResult {
    MonoDecision decision = MonoDecision::MidNote;
    int  nvIdx   = 0;       // note-length index used this step
    bool stepped = false;   // true if a step edge actually fired this sample
    bool wrapped = false;   // true if the phrase boundary wrapped
    bool accented = false;  // true if this step is accented (NEW)
    bool qmixHit  = false;  // Task 4: true if the mono q-mix draw crossed QMIX_LEVEL this step
    int  forStep = -1;      // the stepIndex this result was computed for (Lantern pairs decision→column)
};

// One additional voice beyond mono.  Owns its own GateState and rest
// probability; everything else (nvIdx, legatoProb, scale) is shared from mono.
struct PolyVoice {
    GateState gs;
    GateState gsStep;   // STEP mirror: same calls as gs but continuations re-struck (no fusion)
    float restProb = 0.0f;
    float accentProb = 0.0f;   // per-voice accent probability (accent as a poly lane)
    float qmixLevel = 0.0f;    // per-voice q-mix LEVEL (Task 4 poly): the level this voice's q-mix
                               // draw is thresholded against. 0 = always quantised (legacy), 1 =
                               // always generated. Mirrors restProb/accentProb (per-voice decision
                               // cache written by ModeController from getEffectivePolyQmix).
    // STEP-ADVANCE VALUE PAIR (CC_PROMPT_STEP_ADVANCE_CONSUMPTION Stage 2 — the
    // bipolar sourcing). One knob+modulation sum, TWO clamps, written as a pair by
    // ModeController (the sole writer, mirroring restProb/accentProb):
    //   variationProb     = clamp(sum, -1, +1) — the GATE-MODE STEP-ADVANCE value:
    //                       BIPOLAR (negative = p(step BACKWARD), the probabilistic
    //                       retrograde; positive = p(forward); 0 = always hold). The
    //                       advance decision (decideStepAdvance_) and the follow-main
    //                       ADVANCE tier (subgateMainThreshold) read THIS field.
    //   variationRateProb = clamp(sum,  0,  1) — the rectified VARIATION-STRAND RATE
    //                       (the ghost-CANDIDACY threshold, r_vary >= rate → no
    //                       ghost): a rate is inherently unipolar, and the candidacy
    //                       must NOT inherit the advance's negative half (a negative
    //                       threshold would silence every ghost). == the pre-Stage-2
    //                       write bit-exactly, so the module-path candidacy is
    //                       unchanged.
    // With no negative modulation the two clamps agree bit-exactly (the sum is
    // >= 0) — the default behaviour is unchanged. Rack-free engines (never
    // primed by the controller) keep reading variationProb verbatim at BOTH
    // consumers via the subResLive gates — every existing pin holds.
    float variationProb = 0.5f;     // bipolar gate-mode step-advance value [-1,+1]
    float variationRateProb = 0.5f; // rectified strand rate [0,1] — the ghost-candidacy threshold
    float legatoProb = 0.0f;    // per-voice LEGATO probability — mirrors restProb/accentProb.
    // SUBGATE TIER (CC_PROMPT_STRAITS_SUBGATE_KNOBS_SPEC): the RESOLVED per-voice
    // subgate thresholds [SubgateKnobs::LEVELS][SubgateKnobs::DECS] — levels
    // RATCHET/GHOST, decs REST/LEGATO/ADVANCE/ACCENT. Written per block by
    // ModeController (the sole subgate writer, mirroring the main probs above)
    // from the Straits subgate bank through SK::resolve: own-subpanel claim >
    // {global | follow-main} by the per-decision switch, mod deltas added
    // post-resolution. BYTE-IDENTITY DEFAULT: with nothing authored, follow-main
    // resolves to the main values EXACTLY, so the subgate-edge reads below are
    // bit-identical to the pre-subgate behaviour (the spec's degenerate case).
    // The ADVANCE columns ARE consumed as of the step-advance Stage 1/3 build: the
    // MONO playhead's advance decision (decideStepAdvance_, at the rise sites) reads
    // THIS slot's subRes[level][ADVANCE] — V1's row is the mono subgate row. The
    // playhead is SINGULAR (ONE PLAYHEAD, the spec's law), so the poly voices'
    // ADVANCE columns (rows 1..15) stay resolved + stored with no per-voice consumer
    // (documented scope line at decideStepAdvance_ — they exist for a per-voice
    // playhead if one ever lands; graceful, never uncontrolled).
    float subRes[dotModular::SubgateKnobs::LEVELS][dotModular::SubgateKnobs::DECS];
    // The live gate for the reads above: subRes has no struct-level default that
    // could equal the DYNAMIC main values, so an engine that has never been
    // through ModeController's per-block write (the rack-free tests construct
    // engines directly) must keep reading the main thresholds verbatim. The
    // controller flips this once it owns the writes (first prime); from then on
    // the subgate-edge reads use subRes. Default false ⟹ byte-identity for any
    // engine nobody resolved.
    bool subResLive = false;
    bool  accented = false;    // result of this voice's own accent draw this step
    // Rule 2 (EAST_EXTRA_LANES §4d): latched at the mono chain onset — true if this voice
    // PLAYED (vs rested) when mono started the gate, and held for the chain's life. It, not
    // the gate-held flag, is what tells a landing whether this voice is part of the chain:
    // a voice that opted out and let its short note close still reads participating==true and
    // re-articulates, whereas a rester reads false and stays silent. Always consulted (the
    // per-voice VAR/LEG draw is now unconditional — SANDS CONSOLIDATION Step 2). Reset at each
    // new onset and when mono rests.
    bool  participating = false;
};

// ── VoiceBank — per-voice storage, VoiceSlot-indexed (CC_PROMPT_STRONG_TYPES) ──
// voices[] takes a MINTED VoiceSlot (VoiceResolver::mono() / slot(V) / allSlots());
// the 16-wide storage convention (0 = V1 … 15 = V16) made STRUCTURAL. TIGHTEN: the
// int operator[] MIGRATION SHIM is DELETED — a bare voices[0] does not compile and
// the mono role must be spelled mono(). Layout- and behaviour-identical to the raw
// PolyVoice[16] it wraps (same cells, typed addressing).
struct VoiceBank {
    PolyVoice slots[16];
    PolyVoice&       operator[](dotModular::VoiceSlot s)       { return slots[s.v]; }
    const PolyVoice& operator[](dotModular::VoiceSlot s) const { return slots[s.v]; }
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

// --- WriteLedger: generalised single-writer detector (debug-only; behaviour-inert in release) ---
// Generalises the StrandLedger idea (see setStrand) to arbitrary shared engine fields that have had
// multi-writer "shape-A" drift bugs (restProb, accentProb, wrapped, lastSelectedScale, ...). Each
// field records the ROLE that wrote it this block; a second differing role in the same block is a
// conflict -> loud debug WARN at the moment of introduction, instead of a silent drift that ships.
// ZERO release-path cost: every noteWrite() call and the ledger storage compile out under NDEBUG.
// This is DATAFLOW step 1 -- the prerequisite that makes the rate-discipline pass verifiable
// (re-timing a control can't silently reintroduce a two-writer drift; the ledger fires if it does).
enum class WriteField : uint8_t {
    // AccentProb is RETIRED: mono accent was normalised to a single writer (PatternInput::accentProb),
    // so its A1/A2/A3 noteWrite sites are gone. Kept as an enumerator (no index shift) but no longer
    // written — the drift hazard it policed is now structurally impossible.
    RestProb = 0, AccentProb, Wrapped, LastSelectedScale, StepGate,
    COUNT
};
enum class WriteRole : uint8_t {
    NONE = 0, MONO, EAST, MACRO, POLY, ENGINE, EXPANDER, CVROUTER, SCALEMGR, PARAMMGR,
};
struct WriteLedger {
#ifndef NDEBUG
    WriteRole writer[(int)WriteField::COUNT] = { WriteRole::NONE };
    void beginBlock() {
        for (int i = 0; i < (int)WriteField::COUNT; ++i) writer[i] = WriteRole::NONE;
    }
    void noteWrite(WriteRole role, WriteField field) {
        const int f = (int)field;
        if (f < 0 || f >= (int)WriteField::COUNT) return;
        WriteRole prev = writer[f];
        if (prev != WriteRole::NONE && prev != role) {
#ifdef WARN
            WARN("[WriteLedger] CONFLICT field=%d written by %d then %d (two writers in one block)",
                 f, (int)prev, (int)role);
#else
            std::fprintf(stderr,
                 "[WriteLedger] CONFLICT field=%d written by %d then %d (two writers in one block)\n",
                 f, (int)prev, (int)role);
#endif
        }
        writer[f] = role;
    }
#else
    // release: fully inert, no storage, calls compile to nothing
    inline void beginBlock() {}
    inline void noteWrite(WriteRole, WriteField) {}
#endif
};

struct SequencerEngine {
    PatternEngine pe;
    GateState gs;
    GateState gsStep;   // STEP mirror of gs (mono) — see STEP_GATE_IMPLEMENTATION.md

    // ── Poly voices (VoiceSlot-indexed — see VoiceBank above) ──────────────────
    // voices[VoiceResolver::mono()] = V1 (voice 0 of the unified per-voice system);
    // slots 1..15 = V2..V16 (was a 15-wide V2..V16 bank pre-COLLAPSE; widened to 16
    // with V1 at 0). The stale pre-COLLAPSE comment that claimed slot 0 was V2 —
    // the polyBank-era lie the seam plan flagged — is deleted with the typed bank.
    // numPolyVoices is set from the context-menu user preference (0 = mono only).
    VoiceBank voices;
    int       numPolyVoices = 0;
    // Phase B: track the poly count used last step. When numPolyVoices increases,
    // newly-activated voices have stale gs state (gateHeld/holdRemain from when
    // they were last active). resetActivatedPolyVoices_() clears their state so
    // they fresh-trigger instead of legato-ing from a stale held gate.
    int       prevNumPolyVoices_ = 0;

    // Most recent mono decision — written by executeStep, read by executePolyVoices.
    StepResult lastStepResult;

    // F-1 (RHYTHM_DECISION_MANAGER_PLAN §3.1): the RDM instance — source = GRAPH, reading
    // the staged containment graph (pe.containmentParents, wired AT CONSTRUCTION via the
    // NSDMI — pe is declared before rdm, so the array address is stable; reset() re-wires
    // the same address as the documented seam). D2: graph truth everywhere — no engine
    // legacy mode; the all-follow-V1 star is an AUTHORED graph (the Cantonment preset /
    // the test seam), not a source the production engine switches to. Star graphs are
    // byte-identity-gated on the differential marches; the empty graph is free voices (the
    // self-rooted tuple); poly-container graphs are spec'd in test_forest_rhythm.
    RhythmDecisionCore rdm = RhythmDecisionCore(pe.containmentParents);

    // (lastLegatoProb_ REMOVED — Phase A: V1 legato lives on the mono slot's legatoProb, read
    // directly by executeStep, legatoCheckpointOnFall, and the debug print in executePolyVoice.)

    // Leading-edge legato is now the ONLY legato model: the connection is governed by the
    // PREVIOUS note's onset commitment (gs.slurForward), captured before this note's cascade
    // recomputes it. The old reactive model (fresh r_legato_tie roll at the joining onset)
    // and its legatoLeadingEdge toggle were removed — reactive was never UI-reachable (no
    // menu/switch ever wrote it) and Stage 3 poly opt-in requires a single, onset-committed
    // semantics. Rest still cancels an optional slur (rest branch takes priority); fractional
    // tails still outrank rest (canRest). See RHYTHM_BEHAVIOUR_POLICIES.md.

    // ── Rhythm-behaviour toggles (context menu; see RHYTHM_BEHAVIOUR_POLICIES.md) ──
    // "Generated rest beats legato" — governs GENERATED rests (the rest lane rolls a Rest at a
    // rise). When a committed slur (prevSlur) is reaching N+1 and N+1 rolls a rest: TRUE (default)
    // = rest WINS, cancelling the slur (N+1 silent). FALSE = slur WINS, the rest roll is IGNORED
    // and N+1 plays as a Legato/Tie (its own drawn pitch, gate-connected from N). Only affects the
    // case where a genuine committed slur lands on a held predecessor; a rest on a non-slur note
    // is unaffected. (Renamed from restBeatsLegato to disambiguate from the gap toggle. Patch-compat
    // is broken freely — dev, not released — so the JSON key is "generatedRestBeatsLegato".)
    bool generatedRestBeatsLegato = true;

    // "Tie across rests" — governs STRUCTURAL gaps (source sent no gate between two rises; NOT a
    // MonoDecision::Rest, which the engine never synthesises for a gap). Polarity-flipped rename of
    // the old "incoming rest beats legato" (LEGATO_GATE_GAP_BUG.md CONTEXT-MENU LAYOUT §412):
    //   TRUE (default) = TIE ACROSS GAPS (== the old FALSE mode / incoming-rest-does-NOT-beat-
    //     legato). The module-layer bridge holds the gate high across the gap (gateOpen +=
    //     slurForward) so the next rise sees wasHeld true; the tie forms because the PREDECESSOR
    //     committed (slurForward -> prevSlur, unchanged across the gap). The arriving note's OWN
    //     legato roll governs tying OUT only (it does NOT re-earn the incoming tie). A REST decision
    //     on the arriving note ENDS the chain. NO self-bound / no timer: a committed slur ties into
    //     the next gate regardless of gap length; the only brake is a rest. Broad (any gate
    //     sequencer); not seed-reproducible (depends on live gate timing).
    //   FALSE = ABUTTING-GATES-ONLY (sample-accurate). A falling edge ends the note+slur; the gate
    //     is held for ONE sample after the fall (overlap / <=1-sample gap -> tie) then drops (a
    //     >=2-sample gap -> fresh). The <=1-sample allowance is REQUIRED by signal continuity (a
    //     continuous gate cannot go high->low->high without one low sample between the highs; overlap
    //     alone is one continuous gate = one note, not a tie). No ms timer, no lookahead, no gate-end
    //     smear, seed-reproducible.
    bool tieAcrossRests = true;

    // "Advance playhead on tie-into-rest" — DEFAULT FALSE. The FALSE-refinement checkpoint
    // (LEGATO_GATE_GAP_BUG.md §345/§425): on a Gate-1 FALL while a slur is pending (tieAcrossRests &&
    // this flag), advance the playhead ONE step into the incoming-rest position and re-evaluate the
    // slur candidacy there — the legato draw at that step decides whether the slur REMAINS a
    // candidate (survives across the rest) or ENDS. The next gate RISE then plays that step WITHOUT
    // a second advance (the fall already advanced; "one advance, one arrival" — no double-advance
    // jump). This is the SOLE violation of the edge-driven "advance on onset only" invariant, opt-in.
    // Not seed-reproducible (depends on live gate-fall timing). See legatoCheckpointOnFall().
    bool advanceOnTieIntoRest = false;
    // Latch set by legatoCheckpointOnFall (the fall advanced one step); the next executeModeB rise
    // consumes it and SKIPS its own advancePlayhead (plays the already-stepped-to step). One-shot.
    bool pendingCheckpointArrival = false;

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
    // NUM_STRANDS-sized (7 with QMIX). The SIGN arrays MUST default +1 for every strand — a 6-element
    // brace on a 7-element array (the accent anti-pattern, QMIX_LANE_PARITY_CHECKLIST.md) value-inited
    // strand 6 (LEGATO) to 0 until the first reset(). Braces now list all 7 so adding an 8th strand can't
    // silently drop it (and mismatches the count visibly if not updated).
    int laneTick_[dotModular::NUM_STRANDS]        = {0,0,0,0,0,0,0};
    int laneSign_[dotModular::NUM_STRANDS]        = {1,1,1,1,1,1,1};   // derived from laneDir_ (+1/-1)
    int laneSignPending_[dotModular::NUM_STRANDS] = {1,1,1,1,1,1,1};
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

    // Clear per-lane walk state on hard restart. Under the fully-stateless model this is
    // REDUNDANT — every lane is a pure function of totalStepsElapsed, so zeroing that at
    // handleRestart() already syncs all of them to Beat 1 (the next advancePlayhead recomputes
    // these fields from t regardless). Kept because it is free, self-documenting, and correct
    // again the moment any walker is reintroduced.
    //
    // It is NOT redundant historically: with per-lane ACCUMULATORS, handleRestart() zeroed the
    // master clock but nothing cleared these, so RESET silently stopped syncing polymeters —
    // lanes had forked off the shared clock and the reset path was never updated to follow.
    // Direction scope (LOCK_SCOPE_MENU): may this STRAND's pending direction promote right now?
    // Unlocked => always. Locked => only if the strand's Sands scope bit is set in scopeLiveMask
    // (direction opted out of the freeze for that axis). MELODY/OCTAVE strands are the melody axis,
    // the rest (RHYTHM/VAR/LEG/ACCENT) the rhythm axis. Kept inline on the engine (NOT routed through
    // LockManager) so the audio-thread promotion stays include/dependency-free. The bit VALUES MUST
    // match dotModular::ScopeBit::SB_SANDS_R/M in MonsoonLockManager.hpp (single source of truth);
    // duplicated here only to avoid pulling the manager header into the engine. If ScopeBit is
    // renumbered, update these two lines (they are the only place the engine hard-codes the layout).
    inline bool dirLive_(int strand) const {
        if (!locked) return true;
        constexpr uint32_t kSandsR = 1u << 2;   // == dotModular::SB_SANDS_R
        constexpr uint32_t kSandsM = 1u << 3;   // == dotModular::SB_SANDS_M
        const bool melodyAxis = (strand == dotModular::STRAND_MELODY || strand == dotModular::STRAND_OCTAVE);
        return (scopeLiveMask & (melodyAxis ? kSandsM : kSandsR)) != 0;
    }

    void resetLaneWalk() {
        for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
            laneTick_[s] = 0; laneSign_[s] = 1; lanePingPongHold_[s] = false;
            macroLaneTick_[s] = 0; macroLaneSign_[s] = 1; macroPingPongHold_[s] = false;
            for (int v = 0; v < 16; ++v) {
                laneTickV_[v][s] = 0; laneSignV_[v][s] = 1; lanePingPongHoldV_[v][s] = false;
            }
        }
    }
    // Macro's own per-lane tick — always follows Macro's direction, independent of
    // who owns the lane. Same bounce logic as laneTick_. Advanced in advancePlayhead.
    // Macro's widget reads this for its playhead, so Macro's display always reflects
    // Macro's DirCell, even when Mono owns the lane (engine's laneTick_ follows Mono).
    int macroLaneTick_[dotModular::NUM_STRANDS] = {};
    int macroLaneSign_[dotModular::NUM_STRANDS] = {1,1,1,1,1,1,1};   // NUM_STRANDS-sized (was 6-elem → LEGATO=0 pre-reset)
    LaneDir macroLaneDir_[dotModular::NUM_STRANDS] = {};
    bool macroPingPongHold_[dotModular::NUM_STRANDS] = {};
    int macroLOR_[7] = {16,16,16,16,16,16,16};  // Macro's own LOR lengths (7 poly lanes: REST/MEL/OCT/ACC/QMIX/VAR/LEG)
    // Per-voice per-strand accumulated tick (poly analogue of laneTick_). Advanced in advancePlayhead
    // by dir * polyLaneSign(v, s) — the effective sign is the voice's OWN, i.e. ABSOLUTE, not
    // relative to mono. laneSignV_ = +1 (default) = Forward; -1 = Reverse. A voice therefore does
    // NOT inherit mono's reversal: the DirCell directly controls that voice's direction. (It was
    // once laneSign_[s] * polyLaneSign(v, s) — mono-relative — which read as counter-intuitive
    // when mono was reversed.) laneTickV_[v][s] == laneTick_[s] exactly only when the voice AND
    // mono's lane are both Forward → no-op until something is reversed.
    // laneSignVPending_ is promoted to laneSignV_ at the same boundary as mono's (laneFlipQuant),
    // so a per-voice flip also takes effect musically (no jump).
    int laneTickV_[16][dotModular::NUM_STRANDS]        = {};
    int laneSignV_[16][dotModular::NUM_STRANDS]        = {};   // default 0 → treated as +1 (see polyLaneSign)
    int laneSignVPending_[16][dotModular::NUM_STRANDS] = {};
    LaneDir laneDirV_[16][dotModular::NUM_STRANDS]        = {};   // per-voice direction (default Forward)
    LaneDir laneDirVPending_[16][dotModular::NUM_STRANDS] = {};
    bool lanePingPongHoldV_[16][dotModular::NUM_STRANDS] = {};   // per-voice ping-pong hold flag
    // Effective per-voice sign for a strand: +1 if laneSignV_ is 0 (default/unset → follow mono) or
    // positive, -1 if negative. Multiplied with mono's laneSign_ in advancePlayhead.
    int polyLaneSign(int bank, int strand) const {
        if (bank < 0 || bank >= 16) return 1;
        return (laneSignV_[bank][strand] < 0) ? -1 : +1;
    }
    int polyLaneSignPending(int bank, int strand) const {
        if (bank < 0 || bank >= 16) return 1;
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
    bool hadPolyTail[16] = {};
    // F-1 per-container reads (RHYTHM_DECISION_MANAGER_PLAN §3.1): each poly voice's rhythm
    // VERDICT + drawn note length, captured by executePolyVoice at its decision points and
    // read back through containerView(u) when a CHILD folds its containers — under the
    // forest a poly voice is a real parent, and its child needs the same four scalars V1
    // exposes through monoParentView(). Indexed by STORAGE slot (0 = V1, unused — the mono
    // verdict lives in lastStepResult). Default Rest = a voice that has not executed offers
    // no onset and closes its children (fail-closed); reset() restores it.
    MonoDecision polyVerdict_[16] = {};   // zero-init = MidNote; reset() fills Rest
    int polyNv_[16] = {};                 // the voice's drawn window this edge (the C3 ceiling
                                          // a child clamps against); 0 = no clamp contribution
    // SUBGATE TIER: the edge level for the poly cascade this block (−1 = main, else
    // SubgateKnobs::RATCHET/GHOST). Set by executeModeBSubdivided (which knows the
    // edge stream), consumed + cleared by executePolyVoices at entry (the controller
    // calls it exactly once per block, right after the mode exec — deterministic).
    // Main paths never touch it; it stays −1.
    int polySubLevel_ = -1;
    // STEP-ADVANCE (CC_PROMPT_GATE_STEP_ADVANCE_SPEC Stages 1-3): the advance-decision
    // rise counter — the address of the step-rate uniform (PatternEngine::stepRateUniform,
    // the rhythm Philox stream at its reserved chunk). Incremented ONCE per advance
    // DECISION (each gate-mode rise that runs the decision: the plain-B main rise, each
    // subdivided main/ratchet/ghost edge). The seed advance (stepIndex == −1) and the
    // checkpoint arrival consume NO draw (no decision is made there). Session-lived,
    // NOT persisted: the advance draw is an ephemeral per-rise decision input, not step
    // content (unlike the lane drawCtr, which addresses regenerated cell CONTENT).
    uint64_t stepAdvanceRiseCtr_ = 0;
    // F-4 (D5 — RHYTHM_DECISION_MANAGER_PLAN §3.3): each poly voice's THIS-EDGE GHOST EVENT —
    // the voice PLAYED an onset at a permission point that was a ghost cell of ITS containers
    // (rd.ghostCell). Captured by executePolyVoice at the play verdict (the polyVerdict_
    // discipline: written on EVERY executed edge, true only on the ghost-placed play path), and
    // read back through containerView(u) as ParentView::ghostEvent when a CHILD folds its
    // containers — the recursion that lets ghost events propagate DOWN the forest (a child
    // ghosts inside its containers' ghost notes, one level per edge). Indexed by STORAGE slot
    // (0 = V1, unused — the mono's ghost event is its own stream latch, engine.ghostActive,
    // carried by monoParentView()). Default false = no ghost event (fail-closed, same as the
    // Rest verdict); reset() restores it.
    bool polyGhostOnset_[16] = {};
    // Ghost (subGate_ghost): true while a ghost note is sustaining — set when a ghost candidate
    // (variation-gated) plays, cleared when it rests/fails or a main/ratchet onset takes over.
    // The module-layer IMPL 2b reads this (&& ghostHigh) so the gate only opens for a ghost that
    // actually fired, not merely because Gate 3 is high (variation=0 -> no ghost).
    bool ghostActive = false;
    bool wasHeldPolyPrev[16] = {}; // Capture poly state before tick()

    // ── Mono (V1) strand windowing: Length (1..16), Offset (0..15), Rotation ──
    // Step 1 of the probability-modifier unification (docs/design/PROBABILITY_MODIFIER_MODEL.md):
    // the 18 named fields (rhythmLen…octaveRot) + their 6 hand-written switch(strand) accessors are
    // now ONE array indexed by strand (== editor lane, already the identity) × item {LEN,OFF,ROT}.
    // This is mono/V1 only for now; a later step folds it in as slot 0 of a unified len/off/rot[16][6]
    // alongside the poly arrays. Item order is fixed by MonoItem below. Access via strandLen/Off/Rot
    // + …Ref (unchanged signatures); direct callers use lorRef(strand, item).
    // ── Unified probability-modifier LOR storage (Step 2b-ii) ────────────────────
    // ONE array for all 16 voices × 7 editor lanes × 3 items, replacing the separate monoLOR[7][3]
    // and polyLen/Off/Rot[15][5]. Indexed [voiceSlot][editorLane][item] where
    //   voiceSlot = VoiceResolver::voiceSlot(v):  slot 0 = V1 (mono), slots 1..15 = V2..V16 (poly);
    //   editorLane 0..6 = MEL,OCT,QMIX,REST,ACC,VAR,LEG (poly uses 0..4; VAR/LEG are mono-only);
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

    // Lane-index unification: PL_* now matches the VISUAL/EDITOR/STRAND order
    // (MEL=0,OCT=1,QMIX=2,REST=3,ACC=4,VAR=5,LEG=6). Editor==engine==strand —
    // the old EDITOR_TO_ENGINE_LANE_QMIX table is deleted (Phase 3b, identity).
    // Streams: MELODY=0..1, QMIX=2, RHYTHM=3..6 (stream-contiguous).
    enum PolyLane { PL_MELODY = 0, PL_OCTAVE = 1, PL_QMIX = 2, PL_REST = 3, PL_ACCENT = 4, PL_VARIATION = 5, PL_LEGATO = 6, PL_LANES = 7 };

    // STRONG typed indices (CC_PROMPT_STRONG_TYPES): the typed per-voice/lane accessors
    // take MINTED VoiceSlot (VoiceResolver::mono()/slot(V)/allSlots()) and explicit
    // LaneId (e.g. LaneId{PL_MELODY}). The bare-int overloads are MIGRATION shims.
    using VoiceSlot = dotModular::VoiceSlot;
    using LaneId    = dotModular::LaneId;
    static_assert((int)PL_LANES == dotModular::kLaneIdCount, "LaneId covers exactly the PL_* lanes");

    // ── EAST_EXTRA_LANES stage 2: per-voice ARTICULATION (clamped) ─────────────────────────────
    // VARIATION/LEGATO are mono STRANDS, not poly lanes, so they have no PL_ id and CANNOT be
    // addressed via polyLenE()/editorLane() (that masks & 7 and would alias VAR->QMIX). Use the
    // editor-order accessors polyLOR/polyLORRef, which mask & 7.
    static constexpr int EDITOR_LANE_VARIATION = 5;
    static constexpr int EDITOR_LANE_LEGATO    = 6;

    // SANDS CONSOLIDATION Step 5: perVoiceArticulation flag REMOVED entirely.
    // The per-voice VAR/LEG draw is always-on (Step 2 promoted it). The delegation
    // system (varlegLocalEast_) is the sole control. Lantern now reads pv.gs.slurForward
    // directly (no flag check). Menu item and persistence removed.

    // VAR/LEG per-voice delegation (EAST_EXTRA_LANES §4d). false (default) = delegate to
    // mono → the voice reads MONO's VAR/LEG position, so it mirrors mono (silent). true =
    // Local East → the voice reads its OWN LOR slot and may diverge. Indexed [bank][lane],
    // bank 0..14 = V2..V16, lane 0 = VAR, 1 = LEG. Zero-init = all-delegate = follow mono,
    // which is also the correct fallback when East is absent (nothing sets Local East).
    // Pushed each cycle by MonsoonExpanderManager from the East delegation params.
    bool varlegLocalEast_[16][2] = {};
    // TIGHTEN (CC_PROMPT_STRONG_TYPES): VoiceSlot-only. (lane stays int: 0=VAR/1=LEG is
    // the 2-wide varleg axis, NOT a PL_ LaneId.)
    void setVarlegLocalEast(VoiceSlot bank, int lane, bool localEast) {
        if (bank.v >= 0 && bank.v < 16 && lane >= 0 && lane < 2) varlegLocalEast_[bank.v][lane] = localEast;
    }
    // VAR = lane 0, LEG = lane 1.
    bool varlegDelegated(VoiceSlot bank, int lane) const {
        return !(bank.v >= 0 && bank.v < 16 && lane >= 0 && lane < 2 && varlegLocalEast_[bank.v][lane]);
    }

    // Per-voice VARIATION step — mono's getStrandIdx transform, driven by THIS voice's LOR window.
    // Delegated voices (default) return mono's variation step, so they mirror mono.
    int getVariationStepForVoice(VoiceSlot bank) const;

    // Per-voice LEGATO step — same, for Rule 2's per-voice slur roll. Delegated voices (default)
    // return mono's legato step. Present now (Step 1) so Rule 2 consumes it without re-plumbing.
    int getLegatoStepForVoice(VoiceSlot bank) const;

    // This voice's note-length index, CLAMPED so its hold can never exceed mono's.
    // NOTE_VALUES is documented "ordered slowest -> fastest", so a HIGHER index is a SHORTER note;
    // std::max(nv, mono) therefore guarantees duration <= mono's. Articulation is thus SUBTRACTIVE,
    // exactly like rest and accent: voices may release early, never hold past mono's next event.
    int nvIdxForVoice(VoiceSlot bank, const PatternInput& input) const;

    // The single engine→editor lane conversion. The modifier stores (lorStore_, spread) are editor-
    // ordered; every accessor that takes an engine PL_ lane converts through HERE — one definition.
    // engLane is a POLY lane 0..6 (PL_LANES == 7).
    // Lane-index unification: editor == engine (PL_* reordered to visual/strand order).
    // editorLane() is now IDENTITY — kept for call-site readability, no longer permutes.
    // (Historically the two orders differed and the old & 3 formula aliased PL_QMIX 4→0; the
    // conversion tables were deleted in Phase 3b. For per-voice LOR the EDITOR-order accessors
    // polyLOR/polyLORRef mask & 7 and index lorStore_ directly.) See EAST_EXTRA_LANES.md.
    static int editorLane(int engLane) {
        return (engLane >= 0 && engLane < 7) ? engLane : 0;
    }

    // Editor-order poly accessors: minted VoiceSlot + explicit LaneId; the lane indexes the
    // unified array directly (storage IS editor order now — no permutation).
    // TIGHTEN (CC_PROMPT_STRONG_TYPES): the bare-int twins are DELETED.
    int& polyLORRef(VoiceSlot bank, LaneId lane, int item) { return lorStore_[bank.v][lane.l & 7][item]; }
    int  polyLOR   (VoiceSlot bank, LaneId lane, int item) const { return lorStore_[bank.v][lane.l & 7][item]; }

    // Engine-order poly accessors: minted VoiceSlot + LaneId; the engine PL_ lane converts to
    // editor lane via editorLane() HERE — the single storage boundary (identity since the
    // lane-index unification). TIGHTEN: the bare-int twins are DELETED.
    int& polyLenERef(VoiceSlot bank, LaneId lane) { return lorStore_[bank.v][editorLane(lane.l)][LOR_LEN]; }
    int& polyOffERef(VoiceSlot bank, LaneId lane) { return lorStore_[bank.v][editorLane(lane.l)][LOR_OFF]; }
    int& polyRotERef(VoiceSlot bank, LaneId lane) { return lorStore_[bank.v][editorLane(lane.l)][LOR_ROT]; }
    int  polyLenE(VoiceSlot bank, LaneId lane) const { return lorStore_[bank.v][editorLane(lane.l)][LOR_LEN]; }
    int  polyOffE(VoiceSlot bank, LaneId lane) const { return lorStore_[bank.v][editorLane(lane.l)][LOR_OFF]; }
    int  polyRotE(VoiceSlot bank, LaneId lane) const { return lorStore_[bank.v][editorLane(lane.l)][LOR_ROT]; }

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
    // (0 MEL, 1 OCT, 2 QMIX, 3 RHYTHM/REST, 4 ACC, 5 VAR, 6 LEG — editor-aligned).
    // Editor lane == strand index (identity), so callers pass the lane directly.
    // Single source of truth for strand indexing.
    // Strand LOR readers/writers, now backed by monoLOR[strand][item] (one array, no switches).
    // Out-of-range semantics preserved EXACTLY from the previous 6 switches:
    //   readers  → literal fallback (LEN 16, OFF 0, ROT 0) for an invalid strand;
    //   writers  → the rhythm cell (strandClamp maps invalid → STRAND_RHYTHM), so an out-of-range
    //              write lands on rhythm just as the old default: return rhythmLen/Off/Rot did.
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
    StrandWriter strandWriter[dotModular::NUM_STRANDS] = { StrandWriter::NONE };   // 7 strands incl QMIX

    // Generalised single-writer detector for non-strand shared fields (debug-only). Reset with the
    // strand ledger at the top of each process block.
    WriteLedger writeLedger;

    void beginStrandWriteBlock() {
        for (int i = 0; i < dotModular::NUM_STRANDS; ++i) strandWriter[i] = StrandWriter::NONE;
        writeLedger.beginBlock();
    }

    // The ONLY sanctioned way to write a mono strand's LOR. Records the writer role,
    // asserts single-writer-per-block in debug, then assigns via the ref accessors.
    // (len clamped 1..16; off/rot wrapped 0..15 — same normalisation the call sites did.)
    void setStrand(StrandWriter role, int strand, int len, int off, int rot) {
#ifndef NDEBUG
        if (strand >= 0 && strand < dotModular::NUM_STRANDS) {
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
        if (strand >= 0 && strand < dotModular::NUM_STRANDS) {
            strandLenRef(strand) = rack::math::clamp(len, 1, 16);
            strandOffRef(strand) = ((off % 16) + 16) % 16;
            strandRotRef(strand) = ((rot % 16) + 16) % 16;
        }
    }

    int dnaLength = 16; // Legacy/Global fallback
    int dnaOffset = 0;

    uint16_t windowMask = 0xFFFF;

    bool locked = false;
    // Lock SCOPE bitmask (LOCK_SCOPE_MENU.md). Set bit = that generative group is kept LIVE under
    // lock (opted out of the freeze). 0 = whole-module lock (every LATCH frozen) = pre-menu default.
    // Lives beside locked because the static LockManager::liveNow call sites already read
    // engine.locked; they pass engine.scopeLiveMask alongside. Persisted by PersistenceManager.
    uint32_t scopeLiveMask = 0;

    // ── Change Alley pin-matrix (CHANGE_ALLEY_DESIGN.md §3-REVISED) ──────────
    // The pin remap now lives UPSTREAM in the slewed buffers (pe.remapSlewedByPins,
    // pre-spread), so every read here is a PLAIN own-bank read — a pinned voice's
    // borrowed draw already carries the consumer's own spread. No read-time indirection.
    // Warp mirror (CC_PROMPT_05 — UNIFORM): EVERY lane's uniform — what genPitchLive
    // consumes for pitch (mono + poly landings), what the threshold consumers roll
    // against, and what the poly prob CV outs read — comes from the ONE accessor
    // pe.warpRow (the mirror when a bank is wired, the raw row otherwise, so
    // bank-less hosts stay byte-identical). The old per-lane gated seam (MEL/OCT →
    // mirror, boolean/qmix → raw) is DELETED: there is no second path to drift, so
    // "lane is special / voice is special" divergence is impossible by construction.
    // pe.polyRandom survives ONLY as the WRITE target (the spread passes and
    // refreshPolyRandomCell write random_; the mirror rebuilds from it).
    // Stage 2a: refresh ONE cell of polyRandom at step advance, using the cached spread
    // value from the last control-rate sync(). Checks the lane's lock axis; if not live,
    // returns without writing (frozen value persists). Called from executePolyVoice before
    // each warpRow read — replaces the old full-field (15 voices x 7 lanes x 16 steps)
    // control-rate rebuild.
    void refreshPolyRandomCell(VoiceSlot voice, LaneId engLane, int step);
    static inline int polyLaneToStrand(int polyLane) {
        return (polyLane == PL_REST)   ? dotModular::STRAND_RHYTHM
             : (polyLane == PL_MELODY) ? dotModular::STRAND_MELODY
             : (polyLane == PL_OCTAVE) ? dotModular::STRAND_OCTAVE
             : (polyLane == PL_ACCENT) ? dotModular::STRAND_ACCENT
             : (polyLane == PL_QMIX)   ? dotModular::STRAND_QMIX
                                       : dotModular::STRAND_RHYTHM;  // fallback
    }
    // (monoStrand deleted — V1 reads via pe.warpRow(mono, PL_*) now, same path as V2+)
    bool muted = false;
    bool runGateActive = false;
    bool resetArmed = false;
    bool prevGate1High = false;

    int modeSelect = 0;
    int ppqnSetting = 24;  // master PPQN pulse grid (24/48/96)
    int noteVariationMask = 0b111;
    // (mono accentProb removed — accent now lives on PatternInput::accentProb, a single writer
    //  assembled in updatePatternInput and read at the mono executeStep sites. See the code-smell
    //  fix note on PatternInput::accentProb. Poly voices keep their own PolyVoice::accentProb lane.)

    // Quantizer cache — sized MAXN (24) for Micro-24 (Phase 3). rebuildScaleCache fills the first N
    // entries; at N=12 only [0..11] are used → byte-identical to the legacy 12-sized cache.
    int activeSemiList[dotModular::TuningTable::MAXN] = {};
    float activeSemiWeight[dotModular::TuningTable::MAXN] = {};
    int activeSemiCount = 0;
    float faderCache[dotModular::TuningTable::MAXN] = {};

    // Quantizer memoization
    float lastQuantIn = -100.f;
    float lastQuantOut = 0.f;

    // ── QUANTISER UNIFICATION (Q1): pitch-source seam ──────────────────────────────────────────────
    // When quantiserPitchSource is TRUE, the step cascade (executeStep + executePolyVoice) takes each
    // voice's pitch from the EXTERNAL CV (quantised to the active scale/tuning) INSTEAD of the internal
    // melody+octave draw — realising "keep the timing engine, swap the pitch source." Rest/legato/accent
    // and poly differentiation are unchanged (they run the same path). The melody/octave draw is
    // BYPASSED (gated out), not computed-and-discarded, so no RNG/lane state is perturbed.
    // FALSE (default) → the exact legacy internal-draw path (A/B/E byte-identical). Set per-block by the
    // module layer from the mode. quantiserCV[v] = the incoming poly CV per voice (index 0 = mono/voice0).
    bool  quantiserPitchSource = false;
    float quantiserCV[16] = {};

    // The sounding pitch for a voice at its onset: quantised external CV in quantiser mode, else the
    // internal draw via genPitchLive. voiceIdx 0..15 (0 = mono/voice-0). outSem = the sounding degree
    // (for flash LEDs). r_semi/r_oct = the voice's melody/octave draws (used only in the internal path).
    //
    // QMIX per-step source-select (mono/voice-0 only): when quantiserPitchSource is set AND
    // forceGenerated is true, this step takes the INTERNALLY GENERATED pitch (genPitchLive, the pitch
    // mode A would produce) INSTEAD of the quantised external CV — the q-mix "use generated" branch.
    // forceGenerated is the mono q-mix decision (r_qmix < qmixLevel); it is only ever true in
    // quantiser modes (quantiserPitchSource gates it). When forceGenerated is false (the default,
    // and always for poly voices) behaviour is byte-identical to the legacy path. The generated
    // branch flows through genPitchLive, which writes outSem the same way mode A does, so
    // lastSemitone/LED/degree naming stay correct for generated notes.
    float voicePitch(VoiceSlot voiceIdx, int& outSem, const PatternInput& input, float r_semi, float r_oct,
                     bool forceGenerated = false) {
        if (quantiserPitchSource && !forceGenerated) {
            const int vi = (voiceIdx.v < 0) ? 0 : (voiceIdx.v > 15 ? 15 : voiceIdx.v);
            // CA-routed input CV (QMIX_LANE_PARITY §"The blend" step 2): the external input CV rides
            // CA's MELODY source plane, so a voice can quantise ANOTHER voice's input line. voiceIdx is
            // the ENGINE voice (0=mono/V1, k=poly V(k+1)) — the SAME indexing as the CA pin rows — so
            // caInputCvSrcRow(vi) maps directly. Identity default = each voice reads its own CV (the
            // exact legacy quantiserCV[vi]); byte-identical when caMelodySrc is identity.
            const int srcVi = pe.caInputCvSrcRow(vi);
            const int rvi = (srcVi < 0) ? 0 : (srcVi > 15 ? 15 : srcVi);
            float v = quantize(quantiserCV[rvi]);
            float frac = v - std::floor(v);
            // markSemi correctness (QMIX_LANE_PARITY §"one correctness rule"): the QUANTISED branch
            // names its degree in outSem, and the generated branch does so via genPitchLive — so the
            // caller's triggerNote/slideNote/extendHold marks lastSemitone from WHICHEVER value the mux
            // picked. "What we picked" and "what CA/Lantern see" never drift.
            outSem = pe.tuning.isDefault12TET ? (int(std::round(frac * 12.f)) % 12)
                                              : pe.tuning.nearestDegree(frac);
            return v;
        }
        return pe.genPitchLive(outSem, input, r_semi, r_oct);
    }

    void reset();
    bool isStepInWindow(int idx) const;
    void setWindow(int length, int offset);
    bool advancePlayhead(int dir = +1);   // dir<0 = reverse traversal (within-draw)
    // STEP-ADVANCE (CC_PROMPT_GATE_STEP_ADVANCE_SPEC Stages 1-3): the gate-mode advance
    // DECISION for one rise — consumes the step-rate uniform (per-rise fresh, the rhythm
    // Philox stream's reserved chunk), picks the level's resolved threshold (subLevel −1 =
    // the MAIN gate: voices[mono].variationProb, the resolved Variation path; otherwise a
    // SubgateKnobs level: voices[mono].subRes[subLevel][ADVANCE], the three-way-resolved
    // Straits consumer-group column — the SOLE consumer of the ADVANCE store), and returns
    // the pure StepAdvance::decide verdict. The caller moves the playhead on FORWARD and
    // does NOTHING on REPEAT (the corrected held semantics — not moving the playhead IS
    // the repeat; the step-addressed data re-outputs itself by construction). SCOPE: the
    // playhead is singular — MONO-ONLY (V1's row); poly voices' ADVANCE columns stay
    // resolved + stored (no per-voice playhead this build).
    dotModular::StepAdvance::Decision decideStepAdvance_(int subLevel);
    // Recompute all laneTick_/laneTickV_/macroLaneTick_ caches from totalStepsElapsed.
    // Called after snapping totalStepsElapsed externally (e.g. Mode E phase jump snap)
    // so the lane position caches match the new transport position without a full
    // advancePlayhead (which would also increment stepIndex/totalStepsElapsed).
    void recomputeLaneTicks();
    // Snap the transport (stepIndex + totalStepsElapsed) to the phase-derived pulse
    // position, then recompute lane ticks. Used after a Mode E phase jump to eliminate
    // the rounding drift from jumpSixteenths. p16 = pulsesPer16th (ppqn/4).
    void snapToPhaseStep(long pulsePos, int p16);
    // advanceOnTieIntoRest checkpoint (LEGATO_GATE_GAP_BUG.md §345). Called from the module layer on
    // a Gate-1 FALL while a slur is pending. Advances the playhead ONE step into the incoming-rest
    // position and re-evaluates the slur candidacy (legato draw at that step). Sets
    // pendingCheckpointArrival so the next rise skips its own advance (one advance, one arrival).
    void legatoCheckpointOnFall();
    void updateWindow(float lenParam, float lenCv, bool lenPatched, float offParam, float offCv, bool offPatched);
    int computeNoteLengthIdx(int requestedIdx, int ppqnMask) const;
    int getNoteLenIdx(float baseNoteParam, const PatternInput& input, float r);
    void rebuildScaleCache(const float weights[], int n);
    // Degree name of a quantized pitch (Mode C/D flash LEDs). 12-TET default → legacy round(*12)%12;
    // custom tuning → nearest table degree. See the .cpp.
    int degreeOf(float pitchV) const;
    float getStepLightBrightness(int lightIdx) const;
    int getOffsetStep() const;
    int getStrandIdx(int tickCount, int len, int off, int mutation) const;
    float lastNoteVal_ = 4.f;   // mono NOTE_VALUE param of the current step (poly reads it)

    // ── Probability CV-out accessors (Sands East/Macro poly outs) ──────────────
    // Per-voice final probability (post A/B mix + spread + LOR) for a poly lane
    // (0=REST→rhythm, 1=MEL→melody, 2=OCT→octave), at that voice's own LOR step.
    // 0..1. Used to assemble the poly probability cables (channels 2..1+nVoices).
    inline float polyLaneProbability(LaneId polyLane, VoiceSlot voice) const {
        if (voice.v < 0 || voice.v >= 16 || polyLane.l < 0 || polyLane.l >= PL_LANES) return 0.f;
        // Follow this voice's own lane position, not the raw global tick: a reversed lane's
        // notes come from polyLaneTick, so its probability CV must too or the CV reports a
        // different cell than the one being played (they silently disagreed before).
        int idx = getStrandIdx(polyLaneTick(voice, polyLane), polyLenE(voice, polyLane),
                               polyOffE(voice, polyLane), polyRotE(voice, polyLane));
        idx &= 0x0F;
        // Through the Change Alley remap + the ONE uniform source (CC_PROMPT_05):
        // the CV out reports what the voice will CONSUME — warpRow, every lane.
        return pe.warpRow(voice, polyLane)[idx];
    }
    // Per-voice draw value for a poly lane at an EXPLICIT step (caller supplies the
    // step from its own LOR view). Used by Macro's prob-out, which samples each voice's
    // draw at MACRO's own global LOR step — independent of East/ownership. East instead
    // uses polyLaneProbability (resolved per-voice step). 0..1.
    inline float polyLaneProbabilityAtStep(LaneId polyLane, VoiceSlot voice, int step) const {
        if (voice.v < 0 || voice.v >= 16 || polyLane.l < 0 || polyLane.l >= PL_LANES) return 0.f;
        step &= 0x0F;
        return pe.warpRow(voice, polyLane)[step];   // the ONE uniform source (warp mirror)
    }

    // Step indices (for S&H edge detection) matching the probability accessors above.
    inline int polyLaneStep(LaneId polyLane, VoiceSlot voice) const {
        if (voice.v < 0 || voice.v >= 16 || polyLane.l < 0 || polyLane.l >= PL_LANES) return -1;
        return getStrandIdx(polyLaneTick(voice, polyLane), polyLenE(voice, polyLane),
                            polyOffE(voice, polyLane), polyRotE(voice, polyLane)) & 0x0F;
    }
    // Engine poly lane (PL_REST..PL_LEGATO) → strand index. Single mapping (was inlined in
    // masterLaneStep and polyLaneTick). SANDS CONSOLIDATION: VAR/LEG are now full poly lanes
    // with their own strands (STRAND_VARIATION/STRAND_LEGATO), so all 7 lanes are handled.
    static int polyLaneStrand(int polyLane) {
        return (polyLane == PL_REST)      ? dotModular::STRAND_RHYTHM
             : (polyLane == PL_MELODY)    ? dotModular::STRAND_MELODY
             : (polyLane == PL_OCTAVE)    ? dotModular::STRAND_OCTAVE
             : (polyLane == PL_ACCENT)    ? dotModular::STRAND_ACCENT
             : (polyLane == PL_QMIX)      ? dotModular::STRAND_QMIX
             : (polyLane == PL_VARIATION) ? dotModular::STRAND_VARIATION
             : (polyLane == PL_LEGATO)    ? dotModular::STRAND_LEGATO
                                          : dotModular::STRAND_RHYTHM;  // fallback
    }
    // Per-voice tick for an engine poly lane. The voice's direction is ABSOLUTE (its own DirCell),
    // so this tracks mono's laneTick_[strand] only while both the voice and mono's lane are
    // Forward — a voice does NOT inherit mono's reversal. Out-of-range bank falls back to mono's tick.
    int polyLaneTick(VoiceSlot bank, LaneId polyLane) const {
        int strand = polyLaneStrand(polyLane.l);
        return (bank.v >= 0 && bank.v < 16) ? laneTickV_[bank.v][strand] : laneTick_[strand];
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
        // UI-to-resolver FINISH (CC_PROMPT_UI_TO_RESOLVER_FINISH_START): the MASTER
        // prob-out entry (VoiceResolver::laneProbability's V1 branch — East's ch0 out
        // channel) reads the ONE accessor: effectiveProbability at the EXPLICIT V1
        // voiceSlot (0), MacroOwn lens, at the master's own LOR step. The OLD path
        // (finalRandomByStrand with a bare 0 — the hardcoded voice-0 residue) read the
        // SHARED mono row, which the owner's spread pass WRITES, so East's spread
        // modulated this out (the leak Rodney scoped). The MacroOwn lens reads the
        // published base + macroOwnSpread only — one-way borrowing by construction —
        // and the value is bit-identical to VoiceResolver::laneProbabilityAtStep(1, ·)
        // at the same step: the ONE V1 out value (bars and out read the same voice by
        // construction). masterLaneStep keeps the direction-aware mono lane position
        // (SANDS CONSOLIDATION: polyLaneStrand handles all 7 lanes incl. VAR/LEG —
        // previously this switch lacked the VAR/LEG cases, so voice 1's VAR/LEG
        // display fell back to STRAND_RHYTHM — a bug).
        // STRONG types: the mono ROLE, typed (kMonoVoiceSlot == VoiceResolver::mono()).
        return effectiveProbability(dotModular::kMonoVoiceSlot, LaneId{polyLane},
                                    masterLaneStep(polyLane), ProbLens::MacroOwn);
    }

    // ── THE ONE effective-probability accessor (CC_PROMPT_UI_TO_RESOLVER_SPEC step 1) ──
    // ONE call produces the effective (voice, lane, step) probability = the published
    // base draw + the RESOLVER-resolved spread + the warp mix. It SUBSUMES the
    // a2ccb842 one-accessor design (CC_PROMPT_macro_output_east_leak — deleted by
    // UI_TO_RESOLVER step 2) — the same value, produced through the one authority
    // instead of a third hand-rolled resolution.
    // The engine draw, the bars, and the prob-outs ALL read THIS function; there is
    // no second computation of the quantity anywhere (the spec's one-producer law).
    //
    //   voice = voiceSlot (0=V1/mono ... 15=V16) — EXPLICIT, never hardcoded: the bars
    //           pass the DISPLAYED voice, the prob-outs pass the CHANNEL's voice
    //           (ch0=V1, ch v = voice v+1), so bars and out read the same voice by
    //           construction (CC_PROMPT_warp_display_scope's voice-0 residue dies).
    //   lane  = PL_* engine lane (0..6).  step = 0..15.
    //   lens  = the delegation LENS (the one-way borrowing rule, pinned by
    //           test_macro_own_output):
    //             MacroOwn — Macro's OWN resolved amount (macroOwnSpread[lane], the
    //                        delegated branch of the ONE algebra — the manager's
    //                        publishGlobal resolver output). Macro's bars + prob-outs
    //                        read this lens. Macro NEVER borrows East: the amount
    //                        never reads East's spread, so on an East-owned lane East's
    //                        edit cannot reach Macro's bars/out (the leak dies by
    //                        construction). Owner-INDEPENDENT: Macro's own value exists
    //                        whether or not Macro owns the lane.
    //             Owner    — the OWNER's resolved amount (spread[0][lane] for V1,
    //                        pe.cachedPolySpread[voice][lane] for V2+) — the same
    //                        SpreadPolicy::effective outputs the manager writes for
    //                        the ENGINE's audio spread pass, so the engine draw and
    //                        East's bars read the value the audio consumes (East MAY
    //                        borrow Macro: a Macro-owned lane reads Macro's amount).
    //
    // THE SPREAD TERM comes through SandsModifierResolver<SpreadPolicy>::effective —
    // the ONE delegation algebra (ownerEast/macroBase/tap decided ONCE, manager-side,
    // store-mediated; every store this accessor reads is that resolver's output).
    // SpreadInterp is the underlying interpolation math the term wraps — the DISPLAY
    // entry points only (published snapshots + the anchor-V1 target): torn-read safe,
    // never the live slewed* the audio path reads (the V16-flicker law).
    // THE WARP TERM is the ONE composition per LENS: Owner → warpDisplayMix (the
    // engine draw's twin — warpDisplayMix → WarpBank::warpedValue → resolveWarpCell,
    // the SAME path the warp mirror consumes, tilt+atom feel-test stages included);
    // MacroOwn → warpDisplayMixMacroOwn (Macro's GLOBAL cell — see below). No second
    // warp path exists per lens.
    // V1/mono is the spread anchor: interpolate(base, base, sp) — self-target no-op,
    // so V1's value is spread-invariant at positive spread (matches East's V1 display).
    //
    // ── THE MACRO-OWN TRACE (CC_PROMPT_UI_TO_RESOLVER_FINISH2 BUG 1): which arrays
    // the MacroOwn lens reads, and why NO East path can write them ──────────────────
    //   base   = pe.pubSlewedPoly*[voice][step] (via polySlewedPub) — the PUBLISHED
    //            pre-spread slew. Writers: publishSlewedRhythm/Melody/Qmix ONLY,
    //            copying the authored A/B blend in slewed* (recomputeEffective*, the
    //            Change-Alley pin remap, reset). East's spread passes write random_
    //            (the post-spread finals: applyMono → rhythmRandom/melodyRandom/…,
    //            cachedPolySpread + refreshPolyRandomCell → polyRandom) — NEVER
    //            slewed* or pubSlewedPoly*. (Verified mechanically: the pubs' only
    //            writers are the publish* twins; pinned by test_ui_to_resolver T8,
    //            which clobbers every East-written array and asserts the lens is
    //            bit-unchanged.)
    //   amount = macroOwnSpread[lane] — published ONLY by MonsoonSandsManager's
    //            publishGlobal (Macro's knob + Macro's own CV; no East term).
    //   target = monoSlewedPub → pubSlewedPoly*[0][step] (the V2+ anchor) — the same
    //            East-unwritable store as the base.
    //   warp   = Macro's GLOBAL bank cell via warpDisplayMixMacroOwn (delegated=TRUE):
    //            globalWarp{W,F,Tilt,AtomLevel,AtomMass}[lane], authored ONLY by
    //            Macro's panel. The per-(voice,lane) OWN-SLOT pair
    //            (warpW/warpF/warpTilt/warpAtom*[v*7+lane]) is EAST-authorable
    //            (East's V1/poly tabs author those slots) and is what warpDisplayMix
    //            selects through warpDelegated_ on East-owned lanes — the lens must
    //            NOT read it. That was the one-layer-down leak ("East STILL modulates
    //            Macro warp"): pre-FINISH2 the lens composed with the own-slot pair,
    //            so East's curve/w edits moved Macro's bars + prob-outs on East-owned
    //            lanes while every spread-layer test stayed green (pinned by T7).
    // Macro-own ALL THE WAY DOWN: base + amount + target + warp.
    enum class ProbLens { Owner, MacroOwn };
    float effectiveProbability(VoiceSlot voice, LaneId lane, int step, ProbLens lens) const {
#ifdef MONSOON_PROFILE
        // FAST-PATH Suspect A (CC_PROMPT_cpu_section_profile): count EVERY call — the
        // consumers landing here: the Macro prob-CV sweep (engine thread — STEP-cached
        // by the module's step-3 cache), the bars' refresh (UI thread), and the master
        // prob-out entry (East's ch0 out channel, engine thread — control-rate-throttled
        // by East's own out sweep; hence the relaxed atomic). Macro's process() logs
        // calls at ~50k cadence: the fix for a per-sample count is THIS spec (the step
        // cache), not a point-patch.
        monsoonProfile::effProbCalls.fetch_add(1, std::memory_order_relaxed);
#endif
        const int l = (lane.l >= 0 && lane.l < PL_LANES) ? lane.l : PL_REST;
        const int v = (voice.v >= 0 && voice.v < 16) ? voice.v : 0;   // voiceSlot (V2+=1..15)
        const float sp = (lens == ProbLens::MacroOwn)
            ? macroOwnSpread[l]
            : ((v == 0) ? spreadE(0, l) : pe.cachedPolySpread[v][l]);
        // FOLLOW-CA (CC_PROMPT_east_spread_followCA_display_bug_START, Rodney's final
        // architecture): the DISPLAY view of the ONE decision — the SAME resolver
        // entry the audio path reads (redDot::resolveFollowCaSpread), with the
        // endpoints from the PUB snapshots (the V16-flicker rule — the UI never reads
        // the live slewed*) + the pin-gated live preRemap (the approved µs,
        // self-healing window; no new pub arrays). The old unconditional
        // applyAnchorV1Only was the bug: follow-CA bars anchored to V1 (not the CA
        // source) and showed the CA-remapped base as the spread-0 "own".
        const float monoPub = redDot::SpreadInterp::monoSlewedPub(pe, l, step & 0x0F);
        const float base = redDot::SpreadInterp::polySlewedPub(pe, l, v, step & 0x0F);
        const float disp = redDot::resolveFollowCaSpread({
            .mode = pe.spreadTargetMode[l],
            .ownAnchor = (v == 0) ? monoPub : base,
            .ownPreRemap = (v == 0) ? redDot::SpreadInterp::monoPreRemap(pe, l, step & 0x0F)
                                    : redDot::SpreadInterp::polyPreRemap(pe, l, v, step & 0x0F),
            .targetV1 = monoPub,
            .targetPostRemap = (v == 0) ? monoPub : base,
            .amount = sp });
        // Warp display composition (CC_PROMPT_05 uniform): the same per-(voice,lane)
        // mixture the engine consumes, keyed to the sampled voice. MacroOwn composes
        // with Macro's GLOBAL cell (the Macro-own trace above) — never the
        // East-authorable own-slot pair warpDisplayMix selects on East-owned lanes.
        return (lens == ProbLens::MacroOwn)
            ? pe.warpDisplayMixMacroOwn(v, l, (double)disp)
            : pe.warpDisplayMix(v, l, (double)disp);
    }

    // Macro's own effective spread per PL lane — the MacroOwn lens's amount. The ONE
    // delegated-branch output of the spread resolver (publishGlobal's
    // SpreadResolver::effective call: clamp(macroBase[l][3] + macroPostDelta[l][3])),
    // published by the manager at control rate. Read ONLY by effectiveProbability
    // (MacroOwn); written ONLY by MonsoonSandsManager::publishGlobal.
    float macroOwnSpread[PL_LANES] = {};

    // (The a2ccb842 third-computation accessor is DELETED — CC_PROMPT_UI_TO_RESOLVER_SPEC
    //  step 2. Both former callers — the Macro bars and VoiceResolver::
    //  laneProbabilityAtStep, the prob-out seam — now call effectiveProbability(voice,
    //  lane, step, ProbLens::MacroOwn) directly; the shim was a one-commit bridge while
    //  the accessor landed.)


    // Independent lookup indices for each "DNA strand"
    int getRhythmStep() const;
    int getVariationStep() const;
    int getLegatoStep() const;
    int getAccentStep() const;  // NEW: accent strand DNA index
    int getMelodyStep() const;
    int getOctaveStep() const;
    int getQmixStep() const;    // Task 4: q-mix strand DNA index (mono)

    bool shouldTriggerStep(int ppqn) const;
    // Phase B: called at the top of each mode executor. When numPolyVoices increases,
    // newly-activated voices have stale gs state (gateHeld/holdRemain/slurForward from
    // when they were last active). Reset them so they fresh-trigger instead of legato-ing
    // from a stale held gate. Also latches the count for this step (prevNumPolyVoices_).
    // (Definition in the .cpp — the loop MINTS VoiceSlots, which needs VoiceResolver.)
    void resetActivatedPolyVoices_();
    StepResult executeStep(int nvIdx, float r_rest, float r_legato_tie, float r_accent, float r_qmix, const PatternInput& input, bool wasHeld, bool hadTail, int subLevel = -1);
    void handlePhraseBoundary(PatternInput input, bool isMelodyRealtime, bool isRhythmRealtime);
    // RATE MANAGER (S3b): the Step tier replaced the ClockEngine& — the dispatch feeds
    // the tier from the same sixteenth edge (clock or phase), so the engine no longer
    // re-reads a clock object (and Mode E's synthesized phaseView hack is gone).
    StepResult executeModeA(const redDot::RateManager& rm, float noteVal, const PatternInput& input, int dir = +1);
    StepResult executeModeB(bool gate1Rise, bool gate1High, float noteVal, const PatternInput& input);
    // subGate subdivision (GATE_SUBDIVISION_STEP_GATE.md).  Three edge streams advance the playhead
    // and each runs executeStep (rest/legato/accent/pitch — all Sands lanes draw, Tie emergent from
    // pitch equality):
    //   - mainGateRise: a main-gate onset (note event begin).  Main always wins (quantised to the edge).
    //   - subGateRise (ratchet): a fine-grid edge INSIDE a main gate -> sub-cell (ratchet/tie/rest).
    //   - ghostRise: a fine-grid edge OUTSIDE main gates (in a gap) -> a GHOST note.  Ghost + main are
    //     symmetric: both are external gates whose level drives note width (the module-layer IMPL 2b
    //     reads mainGateHigh || ghostHigh).  Legato flows both ways across the ghost<->main boundary
    //     via the leading-edge slurForward model (no special case).  A ghost onset is a CANDIDATE note
    //     — executeStep rolls the rest lane first, so restProb may still silence it.
    // mainGateHigh/ghostHigh are passed for the IMPL 2b mirror in tests; the engine itself is
    // region-agnostic (which edge fired selects the region).  Unpatched = executeModeB.
    StepResult executeModeBSubdivided(bool mainGateRise, bool mainGateHigh, bool subGateRise, float noteVal, const PatternInput& input, bool ghostRise = false, bool ghostHigh = false);
    // (executeModeC/D removed — MODE_COLLAPSE_6_TO_3: dead code; the controller routes C→A, D→B,
    //  and the quantiser is now the q-mix axis engaged per-step in the dispatch, not an engine mode.)
    float quantize(float vIn);

    // High-level DNA Actions
    void syncVisuals(const PatternInput& in);

    // ── Poly voice execution ──────────────────────────────────────────────────
    // Call executePolyVoices() after any stepped executeModeA/B call.
    // voices[voiceSlot].restProb must be set by the caller before invoking.
    // STRONG types (CC_PROMPT_STRONG_TYPES): voiceIdx is a MINTED VoiceSlot.
    void executePolyVoice(VoiceSlot voiceIdx, const PatternInput& input, bool wasHeld, bool hadTail, int subLevel = -1);
    void executePolyVoices(const PatternInput& input);

    // R-2 (RHYTHM_DECISION_MANAGER_PLAN §2.3): the named parent read — the four mono scalars
    // the poly cascade consumed as loose state (lastStepResult.decision/nvIdx, gs.gateHeld,
    // wasHeldMono/hadMonoTail) assembled as ONE ParentView. executePolyVoice + nvIdxForVoice
    // read mono ONLY through this.
    // F-1 (plan §3.1): the producer for container slot 0 — V1 stays the mono voice's view.
    // Containers on slots 1..15 read containerView(u) below (the captured poly verdict +
    // live gate state), so the same cascade folds ANY forest the graph carries.
    ParentView monoParentView() const;

    // F-1: the per-container view — u is a STORAGE slot (0 = V1 … 15 = V16). u == 0
    // delegates to monoParentView(); u >= 1 assembles the poly container's ParentView
    // from its captured rhythm verdict (polyVerdict_), its captured drawn length
    // (polyNv_), and its live VoiceBank gate state. Out-of-range u is defensive-neutral
    // (fail-closed: no onset offer, envelope cell closed). F-2: executePolyVoices visits
    // in the MATERIALIZED topological order, so every container — any slot, any direction —
    // is captured fresh on the edge its child reads it.
    ParentView containerView(int u) const;
};
