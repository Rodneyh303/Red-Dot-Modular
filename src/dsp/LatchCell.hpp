#pragma once
// LatchCell — the "latched-at-boundary control" lifecycle, as ONE definition.
//
// CONCEPT_ODR_AUDIT CA1 (HIGH). The commit TRIGGER is already unified
// (`MonsoonLockManager::queueFires() = (boundaryNow && !locked) || unlockNow`). But the
// OTHER half the audit names — the {pending store, committed store, commit-copy,
// per-scope live-gate} — is re-rolled per control (voice/lane LOR, lane direction,
// lane sign, dice/CA/cantonment; the "copy CA exactly" comment is the smoking gun).
// It drifted TWICE: the double-commit bug (the commit edge checked against a phase that
// advanced between two sites) and BUG 6 (the edge itself wrong — entry phase vs landed
// phase). Both were a drift INSIDE one copy of this lifecycle, fixed by hand across the
// copies. This is the single pinned definition those fixes should have had.
//
// WHAT THIS OWNS — and why the minimal version was a cop-out. The two bugs that actually
// bit were in the EDGE (the double-commit checked the edge against a phase that advanced
// between two sites; BUG 6 computed the edge from the entry phase instead of the landed
// phase). A primitive that only unifies the precedence + copy leaves the edge at each call
// site, so neither bug would have been caught, and nothing forces a control through it.
// So this header owns the three things that make both bug classes UNREPRESENTABLE:
//   1. the EDGE cadence is ONE definition per control — `Policy::edge(ctx)` (a wrong-phase
//      edge can't hide in a copy because there is only one);
//   2. there is ONE commit entry per control — `LatchedControl::tick` (kills the
//      double-commit's "two sites" by construction);
//   3. the committed store is reachable only through the cell — `read()`/`commit()`, no raw
//      array — so a second or ad-hoc commit cannot be hand-written.
// The one input that is intrinsically per-tick live state — the transport phase — is
// computed ONCE by the engine into `LatchCtx` (at the landed phase) and read here, so there
// is one phase truth, not N.
//
// Reference precedent (the audit's target shape): `SandsModifierResolver<Policy>` + the
// `SandsModifierPolicy` concept unified Spread/Warp/LOR/Dir resolution. A
// `LatchedControl<Policy>` wrapper (below) plays the same role for the commit lifecycle;
// `LatchCell<Value>` is the primitive it is built from.
//
// Rack-free, header-only, audio-thread safe (no allocation, no RTTI, no engine includes).

#include <cstddef>

namespace redDot {

// ── The primitive: one pending/committed pair + the ONE commit mechanic ──────────────
//
// THE COMMIT PRECEDENCE (the single definition the drift bugs lacked):
//   commit pending -> committed  IFF  boundaryFires AND scopeLive AND NOT held.
// Returns whether it committed (some callers log the commit edge).
//
// `boundaryFires` — the transport edge for THIS control (queueFires()-style: a phrase
//     boundary while unlocked, or the unlock flush; or a step edge for a StepEdge-quantised
//     control). Computed by the caller from the transport — NOT re-derived here.
// `scopeLive`     — this control's scope bit under lock (dirLive_/lorLive_ = !locked ||
//     bit). Computed by the caller — NOT re-decoded here.
// `held`          — this element holds its committed sample across the edge (LOR
//     suspend/sticky drag). Defaults false (direction/sign never hold).
template <class Value>
struct LatchCell {
    Value pending{};
    Value committed{};

    // THE ONE COMMIT. All three gates must pass; precedence is fixed here, once.
    bool commit(bool boundaryFires, bool scopeLive, bool held = false) {
        if (boundaryFires && scopeLive && !held) {
            committed = pending;
            return true;
        }
        return false;
    }

    // Stage a new value (the user's edit lands in pending immediately; the effective read
    // of the control reads `committed` until the next commit).
    void stage(const Value& v) { pending = v; }

    // Read the committed value (what the engine consumes).
    const Value& read() const { return committed; }

    // Peek the pending (staged) value — what the control is currently holding to commit at the
    // next boundary. The engine never reads this to decide output; it is here for the few sites
    // that legitimately need the LIVE value (persistence save, a mid-life "did it change?" guard).
    const Value& peek() const { return pending; }

    // Seed both stores (reset / identity push — pending and committed coincide).
    void seed(const Value& v) { pending = v; committed = v; }
};

// ── The transport/lock context — computed ONCE per tick, at the landed phase ─────────
//
// The drift bugs were in PRODUCING these booleans (double-commit: in two places; BUG 6: at
// the wrong — entry vs landed — phase). The engine fills this once per tick and every
// LatchedControl reads it, so there is one phase truth and one commit site per control, not
// N hand-rolled edge computations that can disagree.
struct LatchCtx {
    bool     stepEdge      = false;  // a step advance happened this tick
    bool     wrapped       = false;  // the phrase wrapped this tick (landed phase == start)
    bool     unlockNow     = false;  // lock -> unlock flush this tick
    bool     locked        = false;  // the engine's live lock
    uint32_t scopeLiveMask = 0;      // scope bits live under lock
};

// ── The named, self-forcing control ──────────────────────────────────────────────────
//
// A Policy owns the two things that drifted — the EDGE cadence and the SCOPE-live gate — as
// ONE definition each. The control owns the cells (committed storage unreachable except via
// read()/commit()); `tick` is the ONE commit entry. So a control cannot be committed on an
// ad-hoc edge, cannot be committed from two sites, and its committed value cannot be
// hand-written. This is the shape `SandsModifierResolver<Policy>` set the precedent for.
//
// Policy requirements (a C++20 concept would state these; kept as a comment so this header
// pulls in no <concepts> and no engine types):
//   using Value = ...;                                      // the latched value type
//   static constexpr std::size_t N;                         // element count
//   bool edge(const LatchCtx&) const;                       // THE edge cadence (one def)
//   bool scopeLive(std::size_t i, const LatchCtx&) const;   // THE scope gate (one def)
// `held` (LOR suspend/sticky) is live drag state the engine tracks per element — supplied
// to tick() as a callable, not baked into the policy.
template <class Policy>
struct LatchedControl {
    using Value = typename Policy::Value;
    static constexpr std::size_t N = Policy::N;
    Policy            policy{};
    LatchCell<Value>  cell[N];

    // THE ONE COMMIT ENTRY. The edge is evaluated ONCE for the whole control (before the
    // sweep), so there is a single commit site; scope + held are per element.
    template <class HeldFn>
    void tick(const LatchCtx& ctx, HeldFn&& held) {
        if (!policy.edge(ctx)) return;
        for (std::size_t i = 0; i < N; ++i)
            cell[i].commit(/*boundaryFires=*/true, policy.scopeLive(i, ctx), held(i));
    }
    // Overload: no per-element hold (direction / sign).
    void tick(const LatchCtx& ctx) { tick(ctx, [](std::size_t){ return false; }); }

    void stage(std::size_t i, const Value& v) { cell[i].stage(v); }
    const Value& read(std::size_t i) const    { return cell[i].read(); }
    void seed(std::size_t i, const Value& v)   { cell[i].seed(v); }
    static constexpr std::size_t size()        { return N; }
};

// ── The named wrapper: a Policy supplies the value type and the per-element HOLD rule ──
//
// Policy requirements (a C++20 concept would state these; kept as a comment so this header
// pulls in no <concepts> and no engine types):
//   using Value = ...;                                   // the latched value type
//   static constexpr std::size_t N;                      // number of latched elements
//   static bool held(std::size_t i, const HoldCtx&);     // per-element suspend/sticky
// The wrapper owns an array of N cells and commits them in one sweep, so a control with M
// elements (lanes, voice*lane*item) is ONE `commitAll` call, not an open-coded loop that
// can forget an element or check the wrong edge.
template <class Value, std::size_t N>
struct LatchBank {
    LatchCell<Value> cell[N];

    // Commit every element through the one mechanic. `held` is a callable
    // (std::size_t)->bool so the caller plugs in its suspend/sticky decision per element
    // without this header depending on the control's state layout.
    template <class HeldFn>
    void commitAll(bool boundaryFires, bool scopeLive, HeldFn&& held) {
        for (std::size_t i = 0; i < N; ++i)
            cell[i].commit(boundaryFires, scopeLive, held(i));
    }
    // Overload: no per-element hold (direction/sign).
    void commitAll(bool boundaryFires, bool scopeLive) {
        for (std::size_t i = 0; i < N; ++i)
            cell[i].commit(boundaryFires, scopeLive, /*held=*/false);
    }

    void stage(std::size_t i, const Value& v) { cell[i].stage(v); }
    const Value& read(std::size_t i) const { return cell[i].read(); }
    void seed(std::size_t i, const Value& v) { cell[i].seed(v); }
    static constexpr std::size_t size() { return N; }
};

} // namespace redDot
