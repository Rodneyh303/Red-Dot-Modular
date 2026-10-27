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
// Migration ladder (plan §2.3): Stage R LANDED (7891599f → f2c416e3 → cf68f620 → fb094dbb →
// c5e2c904) — the engine is a forest machine. Stage F (plan §3): F-1 LANDED (2148dec3 — the
// source re-point; parentsOf(GRAPH) reads the staged containmentParents; the empty mask is a
// FREE voice, the self-rooted tuple E(v) = Own(v)) → F-2 LANDED (f56a2ab6 — the MATERIALIZED
// forest, D8: resolved at the graph-change boundary — topological visit order + per-voice
// container lists; the per-sample drive reads plain state, no per-sample work) → F-3 LANDED
// (a304a4cd — the MONO DRIVE INSIDE THE ALGEBRA, D7: the materialized order and the drive's
// C5 loop include slot 0; a parentless V1 is bit-identical to the pre-F-3 drive) → F-4 (this
// commit) the GHOST HOSTING UNDER A FOREST (D5 RULED: "agree intersection" — a voice's ghost
// exists only in gaps of the intersection envelope; candidacy = its own VAR draw) + the D6
// VAR/LEG delegation sweep (engine-internal, no code change: the delegated default keeps
// reading V1's cell — the draw production is not the decision structure) → F-5 the property
// suite + compat pin. Star graphs stay byte-identity-gated on the marches at every step;
// the free-voice and poly-container semantics are spec'd in test_forest_rhythm.

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
    // F-4 (D5): the container's THIS-EDGE GHOST EVENT — its onset (iff it offered one) is a
    // ghost-cell note. u = 0: the mono's own stream latch (engine.ghostActive, §8 — the mono
    // ghost machinery is the mono's own articulation; D7's drive bounds its audibility).
    // u > 0: polyGhostOnset_[u] — the capture the voice's own cascade wrote (it PLAYED at a
    // permission point that was a ghost cell of ITS containers — the recursion: ghost events
    // propagate DOWN the forest, one level per edge). This is stream truth captured by the
    // producer that KNOWS, never inferred from envelope reads (a bridge-held ghost rise or a
    // first-ever main onset reads identical to a main cell in the gate words — the latch does
    // not, which is exactly why the star reduction is exact where an inference would not be).
    bool         ghostEvent = false;
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
    bool         ghostCell;      // F-4 (D5): this permission point is a GHOST CELL of the
                                  //     containers — every ONSETTING container is itself ghosting
                                  //     this edge (its offer rides its own gap). The engine's
                                  //     placement block keys on this (star: exactly the old
                                  //     global ghostActive read — see decideMask). Candidacy at
                                  //     a ghost cell is the voice's OWN VAR draw (D5); the
                                  //     note's EXTENT is the landed C5 intersection (the drive).
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
    // through this same seam (the differential harness + the V1-semantics suites author the
    // star INTO pe.containmentParents — the preset path — so the battery pins the GRAPH read,
    // not a legacy code path).
    static constexpr uint16_t LEGACY_STAR_PARENTS[16] = {
        0x0000,                                   // v0 = V1: the root
        0x0001, 0x0001, 0x0001, 0x0001, 0x0001,   // v1..v5  (V2..V6)   -> bit 0 = V1
        0x0001, 0x0001, 0x0001, 0x0001, 0x0001,   // v6..v10 (V7..V11)  -> bit 0 = V1
        0x0001, 0x0001, 0x0001, 0x0001, 0x0001    // v11..v15 (V12..V16) -> bit 0 = V1
    };

    ParentSource source = ParentSource::LEGACY_STAR;

    // F-1: the GRAPH source's backing store — a pointer to PatternEngine::containmentParents
    // (the engine-side authoritative array; the publish contract stages it every block, so a
    // re-staged graph is visible at the next edge with no re-wiring). Rack-free discipline
    // holds: the core owns nothing, it reads through the engine's array. Null (never wired)
    // reads as the empty graph — every voice free (D2: absent graph = free voices, INTENDED).
    const uint16_t* graphWords = nullptr;

    // The ENGINE's composition (the NSDMI in SequencerEngine wires pe.containmentParents at
    // construction — pe is declared first, so the address is stable before rdm initializes):
    // GRAPH from the first edge, even for engines whose test sites skip reset(). D2: the
    // production engine never runs LEGACY_STAR — the star is an authored graph, not a mode.
    explicit RhythmDecisionCore(const uint16_t* stagedGraph)
        : source(ParentSource::GRAPH), graphWords(stagedGraph) {}
    // The core-level default (unit tests, the pure layer): LEGACY_STAR, no store. The ENGINE
    // never uses this — its NSDMI composes the GRAPH ctor.
    RhythmDecisionCore() = default;

    void     setParentSource(ParentSource src) { source = src; }

    // The parents bitmask AS DATA. GRAPH (F-1) reads the staged containment graph; the
    // engine composes it at GRAPH permanently (D2: no engine legacy mode — the star is an
    // authored graph, not a source the production engine ever switches to). LEGACY_STAR
    // remains as core-level test data + the preset's word definition.
    uint16_t parentsOf(int v) const {
        if (source == ParentSource::LEGACY_STAR)
            return (v >= 0 && v < 16) ? LEGACY_STAR_PARENTS[v] : 0x0000;
        return (graphWords && v >= 0 && v < 16) ? graphWords[v] : 0x0000;
    }

    // ── F-2: the MATERIALIZED forest (D8 RULED: "forest mod on phrase boundary, no per-sample
    // work") ─────────────────────────────────────────────────────────────────────────────
    // The graph is resolved ONCE per CHANGE into plain per-voice state; every reader — the
    // edge-level visit in executePolyVoices, the module-layer per-sample drive (Monsoon.cpp) —
    // reads this state and derives NOTHING at read time. materializeIfStale() is called ONLY
    // at graph-change boundaries: the publish point (MonsoonSandsManager::processDNA, per
    // block) and the top of executePolyVoices (per edge — this also covers test sites that
    // author pe.containmentParents directly, without the manager). The per-sample path NEVER
    // calls it — that is the ruling.
    //
    //   topoOrder[]/topoCount      — ALL 16 voices (storage 0..15) in topological order,
    //                                smallest-index-first among ready nodes (deterministic;
    //                                the STAR resolves to 0,1,…,15 — the poly visit skips
    //                                slot 0, so the poly order stays 1..15 = slot order =
    //                                today's visit order, the byte-identity anchor). F-3/D7:
    //                                V1 (storage 0) IS in the order — the per-sample DRIVE
    //                                needs its position (a poly container's gate must be
    //                                complete before V1's AND reads it, and V1's gate
    //                                complete before its poly children read the mono view);
    //                                the EDGE visit (executePolyVoices) skips slot 0 — V1's
    //                                cascade is executeStep, which runs before the visit.
    //   parentCount[]/parentSlot[] — the materialized container list per voice (storage slots
    //                                0..15; slot 0 = V1 the mono voice — F-3: its list is
    //                                REAL, the drive ANDs it into the mono gate). ZERO
    //                                parents = a FREE voice (its envelope is its own).
    //   materializeGeneration      — bumped per rebuild (test observability of the cache).
    //
    // A cycle cannot occur (the pin plane rejects cycles; invariants CI-marched on 600+ seeds)
    // — but defensively, any unplaceable voice is appended in slot order so the visit is TOTAL
    // and the engine can never silently skip a voice.
    int      topoOrder[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    int      topoCount     = 16;
    int      parentCount[16] = {};
    uint8_t  parentSlot[16][16] = {};
    uint32_t materializeGeneration = 0;
    uint16_t materializedWords_[16] = {};
    bool     materializedValid_ = false;

    void materializeForest() {
        // Kahn's algorithm, smallest-index-first, over ALL 16 voices — V1 INCLUDED (F-3, D7:
        // the mono voice is inside the algebra; the DRIVE needs its position — containers
        // complete before V1's AND, V1 complete before its poly children read the mono view).
        // Bit 0 of a POLY voice's word is a REAL dependency now (V1 orders). Bit v of voice v's
        // OWN word is dropped defensively (a self-loop is rejected by the pin plane; the drop
        // keeps the visit total even against a hostile hand-authored word). The smallest-index
        // scan keeps the byte-identity anchor: a PARENTLESS V1 (the star, the empty graph) is
        // always picked FIRST (index 0, indeg 0) and the poly voices' relative order is
        // untouched — the star's order is 0,1,…,15 and the poly visit (skipping 0) is exactly
        // the pre-F-3 slot order.
        int indeg[16] = {};
        for (int v = 0; v < 16; ++v) {
            const uint16_t pars = parentsOf(v) & uint16_t(~(1u << v));   // drop self defensively
            int n = 0;
            for (int u = 0; u < 16; ++u) if (pars & (1u << u)) ++n;
            indeg[v] = n;
        }
        bool placed[16] = {};
        topoCount = 0;
        for (;;) {
            int pick = -1;
            for (int v = 0; v < 16; ++v)
                if (!placed[v] && indeg[v] == 0) { pick = v; break; }   // smallest index first
            if (pick < 0) break;
            placed[pick] = true;
            topoOrder[topoCount++] = pick;
            for (int c = 0; c < 16; ++c)
                if (!placed[c] && (parentsOf(c) & (1u << pick))) --indeg[c];
        }
        for (int v = 0; v < 16; ++v)                 // defensive cycle fallback: total order
            if (!placed[v]) topoOrder[topoCount++] = v;
        for (int v = 0; v < 16; ++v) {               // the materialized container lists
            int n = 0;
            for (int u = 0; u < 16; ++u)
                if (parentsOf(v) & (1u << u)) parentSlot[v][n++] = (uint8_t)u;
            parentCount[v] = n;
        }
        for (int v = 0; v < 16; ++v) materializedWords_[v] = parentsOf(v);
        materializedValid_ = true;
        ++materializeGeneration;
    }

    // The ONLY sanctioned entry points are the graph-change boundaries (see above). Cost:
    // 16 parentsOf reads + compare — one 32-byte word compare in spirit; nothing per sample.
    void materializeIfStale() {
        if (materializedValid_) {
            bool same = true;
            for (int v = 0; v < 16; ++v)
                if (materializedWords_[v] != parentsOf(v)) { same = false; break; }
            if (same) return;
        }
        materializeForest();
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
        d.ghostCell     = d.mayOnset && p.ghostEvent;   // F-4 (D5), the single-container form
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

    // ── F-1: the self-rooted tuple — parents(v) = ∅ ⇒ E(v) = Own(v) ────────────────────
    // D2/R2 RULED: a voice with NO containers is a FREE voice — it detaches from V1 and runs
    // its own note machinery: E(v) = Own(v) (plan §4.2 — the algebra's parentless instance).
    // The cascade consumes this tuple through the SAME branches a contained voice's takes;
    // the difference is the state the engine feeds in — the voice's OWN window, own slur
    // commitment, own still-sounding read — so the voice self-clocks at its own window
    // expiry (mono's MidNote-guard predicate), thins itself with its own rest draws at each
    // expiry edge, and slides on its own slur commitment (the Rule-2 shape, self-applied).
    // Ties do not exist for free voices (a poly voice has no rhythm row to encode one):
    // slides and fresh re-strikes only. Rhythm = own length draws (the expiry cadence) thinned
    // by own rest draws.
    //   windowOpen    — the voice's own note window is still open (mono's exact MidNote
    //                   guard: holdRemain >= 1 whole step, or the gate pulse still counting —
    //                   the fractional-tail term)
    //   ownSlur       — the voice's own gs.slurForward, committed at its previous onset
    //   stillSounding — the voice's own still-sounding read (wasHeld || hadTail)
    static RhythmDecision freeDecision(bool windowOpen, bool ownSlur, bool stillSounding) {
        RhythmDecision d{};
        d.envelopeOpen  = true;    // no containers: the envelope is the voice's own — open
        d.forcedRest    = false;   // nothing forces a rest
        d.lengthCeiling = 0;       // no clamp: the own draw IS the window (the nvIdxForVoice
                                   // fold is max(own, containers); the empty fold adds nothing)
        // ghostCell stays false (F-4/D5): the ghost is a CONTAINMENT texture — a gap event of
        // the containers' envelopes. A FREE voice has no containers, hence no container gaps to
        // ride: its notes are all its own (candidacy rides the host containers' ghost stream,
        // and there is no stream). Same for its children: a free voice never ghost-places, so
        // it never emits a ghost event for them either.
        if (windowOpen) {
            d.mayOnset   = false;
            d.holdFollow = true;   // hold own gate per the own-window predicate (MidNote path)
            d.gateType   = MonoDecision::MidNote;
        } else {
            d.mayOnset   = true;   // every expiry edge is a permission point; the own rest
                                   // draw shapes the rhythm from here
            d.holdFollow = false;
            d.gateType   = (ownSlur && stillSounding) ? MonoDecision::Legato    // slide to own
                                                      : MonoDecision::NewNote;  // fresh strike
        }
        return d;
    }

    // The fold core over an explicit parents bitmask — the source-driven decide() delegates
    // here; tests (and Stage F) can fold over hand-built masks to spec the multi-parent
    // rules directly.
    template <typename ViewOf>
    static RhythmDecision decideMask(uint16_t parents, ViewOf viewOf) {
        RhythmDecision d{};                     // no containers -> uncontained (all neutral)
        bool first = true, gateTypeNamed = false, unionMayOnset = false, offerGapHosted = false;
        bool allOnsettingGhost = true;          // F-4 (D5): every ONSETTING container is ghosting
        for (int u = 0; u < 16; ++u) {
            if (!(parents & (1u << u))) continue;
            const RhythmDecision c = decide(viewOf(u));
            unionMayOnset   = unionMayOnset || c.mayOnset;                        // D3 union (the OFFERS)
            d.forcedRest    = d.forcedRest || c.forcedRest;                      // D1
            d.lengthCeiling = first ? c.lengthCeiling
                                    : std::max(d.lengthCeiling, c.lengthCeiling); // D1
            d.holdFollow    = first ? c.holdFollow    : (d.holdFollow && c.holdFollow);    // D1
            d.envelopeOpen  = first ? c.envelopeOpen  : (d.envelopeOpen && c.envelopeOpen); // D1
            if (first) d.gateType = c.gateType;                                  // D4 default
            if (c.mayOnset) {
                if (!gateTypeNamed) { d.gateType = c.gateType; gateTypeNamed = true; }
                if (!offerGapHosted) offerGapHosted = !c.envelopeOpen;           // D5: an offer FROM a gap
                allOnsettingGhost = allOnsettingGhost && c.ghostCell;            // D5: the ghost cell ∀
            }
            first = false;
        }
        // C1's FOREST form (plan §3.3 + §3.9), with the D5 carve-out the goldens forced: the
        // union of onset OFFERS is gated by the INTERSECTION envelope — UNLESS the offer is
        // GAP-HOSTED (the offering container's own envelope cell is closed at its onset: the
        // ghost path — ghosts LIVE in envelope gaps, and the legacy poly-ghost roll rides this
        // branch on the star). On the star both shapes reduce to today's behavior (a main
        // onset implies the mono gate high; a ghost onset rides the mono gap), so the marches
        // stay byte-identical; with poly containers the gate closes the hole the F-2 forest
        // marches caught (a child firing while a sibling container's gate had fallen and the
        // offer was a MAIN onset).
        d.mayOnset = unionMayOnset && (d.envelopeOpen || offerGapHosted);
        // F-4 (D5 RULED: "agree intersection" — plan §3.3's ghost row): a permission point is
        // a GHOST CELL of the containers iff EVERY ONSETTING container is itself ghosting this
        // edge (all the offers ride container gaps — "hosted only in a gap of the INTERSECTION
        // envelope", a gap for every onsetting container simultaneously). One main-cell offer
        // among them makes it an ordinary onset: the voice plays a real note and does not roll
        // ghost candidacy (a main cell is not a gap). STAR REDUCTION (the byte-identity
        // argument, per verdict shape): Rest → no offer, mayOnset false; MidNote / a
        // bridge-held ghost rise → no onset offer (the MidNote guard / the isolated-teal
        // guard), mayOnset false — the old placement never ran either (it sat inside the
        // mayOnset branch); NewNote / Legato-from-dead → the mono offers AND ghostActive is
        // its stream latch ⇒ allOnsettingGhost == ghostActive. So on the star ghostCell is
        // EXACTLY [mayOnset ∧ ghostActive] — the global read it replaces, bit for bit — and
        // under a forest it is the containers' ghost events, not V1's (the F-4 de-privileging).
        // The engine consumes this in the placement block (executePolyVoice); candidacy is the
        // voice's OWN VAR draw (D5); the note's extent stays the landed C5 intersection (the
        // per-sample drive — the audibility bound, no F-4 change).
        d.ghostCell = d.mayOnset && allOnsettingGhost;
        return d;
    }
};
