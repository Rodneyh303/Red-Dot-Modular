#pragma once
// RhythmDecisionCore — the Rhythm Decision Manager (RDM), rack-free (no Rack headers; the mock
// battery compiles it directly, same discipline as ContainmentModuleCore/ContainmentPending).
// SequencerEngine composes it.
//
// Authority: docs/design/RHYTHM_DECISION_MANAGER_PLAN.md §2. Deliberately NOT:
//   • Not a state owner.  Gate state stays in VoiceBank voices (SequencerEngine.hpp) — one
//     source of truth, the CA-style "engine renders what the state says" contract intact.
//     The RDM computes PERMISSIONS from states; it holds none.
//   • Not a new writer.   ModeController keeps W1/W2 (the sole-writer map, reference doc §2).
//   • Not a behavioural change. Every Stage-R commit is byte-identical on the differential
//     marches (test_rhythm_decision_differential) or it does not land.
//
// Migration ladder (plan §2.3): R-1 the reference oracle (landed 7891599f) → R-2 ParentView
// (this commit) → R-3 the five named predicates → R-4 the RhythmDecision tuple → R-5 the
// star as DATA. Each step is gated on the marches' golden digests.

#include <cstdint>

// ── MonoDecision ────────────────────────────────────────────────────────────────
// MOVED HERE from SequencerEngine.hpp (R-2): the decision vocabulary is RDM vocabulary —
// ParentView (below) carries it, and the R-4 tuple's gateType field is a MonoDecision.
// Same enumerators, same order, same meaning; SequencerEngine.hpp includes this header,
// so every existing user sees the type unchanged.

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

// ── ParentView ──────────────────────────────────────────────────────────────────
// The named parent read (plan §2.2): today's four mono scalars, promoted to a type.
// In Stage R the only producer is SequencerEngine::monoParentView() (the V1 star's single
// parent, D2's legacy graph); Stage F re-points the producer at the containment graph
// (PatternEngine::containmentParents, per-container views) WITHOUT touching the cascade's
// read side — that is the seam.
struct ParentView {
    MonoDecision decision;      // the container's verdict this edge
    int          nvIdx;         // the container's note length this step (the clamp ceiling)
    bool         gateHeld;      // the container's gate is high right now (live read)
    bool         wasHeld;       // the container was held at the edge (captured pre-tick)
    bool         hadTail;       // the container has a fractional tail still sounding
};

// ── The five named predicates (plan §2.3, R-3) ─────────────────────────────────
// The C1–C5 mechanisms of VOICE1_CONTAINMENT_ALGORITHM.md's mechanism table, extracted as
// PER-CONTAINER pure functions: Stage R calls each with the single V1 ParentView; Stage F
// folds them over parentsOf(v) — D3's UNION for onsets (events), D1's INTERSECTION for
// everything state-shaped (rests, ceilings, holds, envelopes). Own state (wasHeldPoly /
// hadPolyTail, polyVoiceActive, the voice's own draws) stays at the call site — it is the
// CONTAINED voice's, not a parent read. Byte-identity gate: test_rhythm_decision_differential.

// C1 — onset permission: this container OFFERS A TRIGGER this edge — a NewNote, or a
// Legato/LegatoMax shift from a dead state (low gate, no tail; the isolated-teal guard).
// EVENTS: union over containers (D3). (SequencerEngine.cpp executePolyVoice monoGateStart.)
inline bool onsetPermission(const ParentView& p) {
    return (p.decision == MonoDecision::NewNote) ||
           ((p.decision == MonoDecision::Legato || p.decision == MonoDecision::LegatoMax) &&
            !p.wasHeld && !p.hadTail);
}

// C2 — rest close: this container rested → every contained voice force-closes and consults
// no draws. STATES: intersection — ANY resting container closes (D1).
// (SequencerEngine.cpp Rest branch; Monsoon.cpp gateOpen's !isRest term.)
inline bool restClose(const ParentView& p) {
    return p.decision == MonoDecision::Rest;
}

// C3 — length ceiling: the voice may never hold past this container's next event.
// NOTE_VALUES is ordered slowest→fastest, so the LARGER index is the SHORTER note — the
// result is the shorter window; folding over containers keeps the shortest ceiling (D1).
// (SequencerEngine.cpp nvIdxForVoice clamp.)
inline int lengthCeiling(int ownNv, const ParentView& p) {
    return (ownNv > p.nvIdx) ? ownNv : p.nvIdx;
}

// C4 — hold follow: mid-hold, the voice follows the articulation shape only while this
// container is still holding. The voice's OWN armed window (gatePulseRemain/holdRemain)
// stays at the call site. STATES: intersection — follow the shortest still-sounding
// container (D1). (SequencerEngine.cpp MidNote branch's parent term.)
inline bool holdFollow(const ParentView& p) {
    return p.gateHeld;
}

// C5 — envelope AND: this container's gate contributes one open cell to the contained
// voice's output envelope; the voice's own latched candidacy (polyVoiceActive) stays at
// the call site. STATES: intersection over containers (D1). In clock mode C5 is implicit
// in C1+C2+C4; gate mode gives it this explicit form. (Monsoon.cpp vOpen.)
inline bool envelopeAnd(const ParentView& p) {
    return p.gateHeld;
}

// ── The verdict tuple + its assembly (plan §2.2, R-4) ──────────────────────────
// The per-voice, per-edge verdicts the engine consumes, assembled ONCE per voice per edge;
// the cascade reads the tuple instead of re-deriving permissions mid-flight. THIS is the
// commit where "all decisions centralised" becomes literally true — one address answers
// every permission question the engine asks.
struct RhythmDecision {
    bool         mayOnset;       // C1: a permission point exists this edge (union in F, D3)
    bool         envelopeOpen;   // C5: this container's envelope cell is open (intersection in F, D1)
    bool         forcedRest;     // C2: a container rested -> close, consult no draws (intersection, D1)
    int          lengthCeiling;  // C3: the NOTE_VALUES index ceiling; the voice applies its own
                                  //     draw under it (nvIdxForVoice -> the lengthCeiling fold;
                                  //     the shortest ceiling across containers in F, D1)
    bool         holdFollow;     // C4: containers mid-gate -> follow the articulation shape (D1)
    MonoDecision gateType;        // D4: whose verdict names trigger vs slide at a shared onset
                                  //     (topologically-first SOUNDING onsetting container in F)
};

struct RhythmDecisionCore {
    // R-4: the five named predicates composed ONCE. Single-parent star today (a 1-parent
    // composition IS today's code path); R-5 adds ParentSource/parentsOf and Stage F turns
    // this into the fold over the parents bitmask — D3's union for mayOnset/gateType,
    // D1's intersection for forcedRest/lengthCeiling/holdFollow/envelopeOpen.
    static RhythmDecision decide(const ParentView& p) {
        RhythmDecision d;
        d.mayOnset      = onsetPermission(p);   // C1
        d.envelopeOpen  = envelopeAnd(p);       // C5
        d.forcedRest    = restClose(p);         // C2
        d.lengthCeiling = p.nvIdx;              // C3 — the ceiling; the own draw is applied
                                                //        under it at nvIdxForVoice (the named
                                                //        lengthCeiling fold)
        d.holdFollow    = holdFollow(p);        // C4
        d.gateType      = p.decision;           // D4
        return d;
    }
};
