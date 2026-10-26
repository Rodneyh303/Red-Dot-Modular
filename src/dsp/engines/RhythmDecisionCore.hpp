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
