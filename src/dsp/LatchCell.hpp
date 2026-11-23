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
// SCOPE OF THIS HEADER. This is deliberately the minimal, provable core: the cell and the
// ONE commit mechanic. It does NOT own the transport-edge detection (`queueFires`, the
// step/phrase quantisation) or the scope-bit decode (`dirLive_`/`lorLive_`) — those read
// live engine/transport state and already have one home each. The duplication the audit
// targets is the *cadence + copy*: "commit pending->committed exactly when my edge fires
// and my scope is live, unless this element is held." Every control derives its three
// inputs its own way (that part is intrinsic) and then commits through the SAME call, so a
// future cadence/precedence change is one edit here, not N hand-copies.
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

    // Seed both stores (reset / identity push — pending and committed coincide).
    void seed(const Value& v) { pending = v; committed = v; }
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
