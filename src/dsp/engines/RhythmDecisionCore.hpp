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
#include <algorithm>

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
    // ── R-5: the parents source — the star made explicit ───────────────────────────
    // THE SEAM Stage F re-points (plan §2.2). Stage R runs LEGACY_STAR: every poly voice's
    // sole container is V1 (the D2 legacy graph, declared as DATA below). Stage F re-points
    // at ParentSource::GRAPH — the staged containmentParents, topologically ordered — and
    // D2's legacy mapping becomes a TOGGLE, not a rewrite.
    enum class ParentSource {
        LEGACY_STAR,   // word[v] = bit 0 (V1) for v >= 1, 0 for v = 0 — today's graph, as data
        GRAPH          // Stage F: the staged containment graph (containmentParents[16])
    };

    // LEGACY_STAR AS DATA (D2): one parents-bitmask word per contained voice (storage slots
    // 0..15 = V1..V16; bit u set = voice u contains v). V1 is the root — no container. The
    // all-follow-V1 graph ships as a Cantonment PRESET under D2; test suites inject graphs
    // through this same seam.
    static constexpr uint16_t LEGACY_STAR_PARENTS[16] = {
        0x0000,                                   // v0 = V1: the root
        0x0001, 0x0001, 0x0001, 0x0001, 0x0001,   // v1..v5  (V2..V6)   -> bit 0 = V1
        0x0001, 0x0001, 0x0001, 0x0001, 0x0001,   // v6..v10 (V7..V11)  -> bit 0 = V1
        0x0001, 0x0001, 0x0001, 0x0001, 0x0001    // v11..v15 (V12..V16) -> bit 0 = V1
    };

    ParentSource source = ParentSource::LEGACY_STAR;

    void     setParentSource(ParentSource src) { source = src; }

    // The parents bitmask AS DATA. GRAPH reads neutral (no containers) until Stage F-1
    // re-points it at the staged graph — a no-container voice decides as uncontained
    // (D2: absent/zero graph = free voices, INTENDED).
    uint16_t parentsOf(int v) const {
        if (source == ParentSource::LEGACY_STAR)
            return (v >= 0 && v < 16) ? LEGACY_STAR_PARENTS[v] : 0x0000;
        return 0x0000;   // GRAPH: wired in Stage F-1
    }

    // ── R-4: the per-container composition ─────────────────────────────────────
    // The five named predicates composed ONCE for ONE container.
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

    // ── R-5: the forest fold — the C-compositions loop over the bitmask ─────────
    // A 1-parent loop IS today's code path (the star: exactly one bit set, so the body
    // runs once and the fold is the identity over the single container). ViewOf is the
    // caller's per-container view projection — on the star it returns the V1 view for the
    // one bit; Stage F provides per-container views over the containment graph.
    // Fold semantics (the banked rulings):
    //   mayOnset      — D3 UNION (events: any container's attack offers a trigger)
    //   forcedRest    — D1 (any resting container closes the envelope)
    //   lengthCeiling — D1 (max-fold: the SHORTEST container window wins)
    //   holdFollow    — D1 (follow the shortest still-sounding container)
    //   envelopeOpen  — D1 (the AND of the containers' envelope cells)
    //   gateType      — D4 (the FIRST onsetting container in bit order names the verdict;
    //                    with no onsetting container, the first container's verdict stands)
    template <typename ViewOf>
    RhythmDecision decide(int v, ViewOf viewOf) const {
        return decideMask(parentsOf(v), viewOf);
    }

    // The fold core over an explicit parents bitmask — the source-driven decide() delegates
    // here; tests (and Stage F) can fold over hand-built masks to spec the multi-parent
    // rules directly.
    template <typename ViewOf>
    static RhythmDecision decideMask(uint16_t parents, ViewOf viewOf) {
        RhythmDecision d{};                     // no containers -> uncontained (all neutral)
        bool first = true, gateTypeNamed = false;
        for (int u = 0; u < 16; ++u) {
            if (!(parents & (1u << u))) continue;
            const RhythmDecision c = decide(viewOf(u));
            d.mayOnset      = d.mayOnset || c.mayOnset;                          // D3 union
            d.forcedRest    = d.forcedRest || c.forcedRest;                      // D1
            d.lengthCeiling = first ? c.lengthCeiling
                                    : std::max(d.lengthCeiling, c.lengthCeiling); // D1
            d.holdFollow    = first ? c.holdFollow    : (d.holdFollow && c.holdFollow);    // D1
            d.envelopeOpen  = first ? c.envelopeOpen  : (d.envelopeOpen && c.envelopeOpen); // D1
            if (first) d.gateType = c.gateType;                                  // D4 default
            if (c.mayOnset && !gateTypeNamed) { d.gateType = c.gateType; gateTypeNamed = true; }
            first = false;
        }
        return d;
    }
};
