// SandsSnap — the host-side resnap cluster for the SANDS right-side subpanels.
// Spec: docs/design/CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md §HARD REQUIREMENT:
// "Reuse the Straits snapping code/pattern directly — do NOT reinvent."
//
// This is the StraitsBase cluster (resnapLanes + chainIntact + the 0.1 s periodic
// adjacency poll + the event-driven fast path + the drag guard + the one-shot
// consumer), family-parameterised so BOTH hosts — the Macro visual (cross-voice,
// may carry TWO subpanels: the per-voice-sends subpanel AND the Distribution
// expander) and the East visual (per-voice) — run the SAME code. The StraitsLane
// differences, deliberate:
//   - the subpanels are BROWSER-ADDED (not base-spawned), so there is no
//     ordered-ID ownership list and no spawn machinery: the RIGHT CHAIN ITSELF
//     is the canonical order (the spec's "canonical ordering resolves ties, but
//     either order is valid" — the user's physical order persists; the resnap
//     preserves it, it never re-sorts);
//   - attachment is RIGHT of the host (the Straits lanes chain right too — same
//     direction, expectedX steps rightward from the host's top-right edge).
// The hardening (CC_PROMPT_STRAITS_RESNAP_HARDENING) is carried over whole:
//   - the SUBPANEL flags its own drag-end (SandsSnapGuest::needsResnap),
//   - the SUBPANEL flags a broken left link (its onExpanderChange),
//   - the HOST polls adjacency every 0.1 s (catches modules inserted between)
//     with the event fast path (~1 frame) on topologyDirty,
//   - all consumption defers while a drag owns the pointer, then one
//     setModulePosForce pass shoves the intruder out and re-seats the run.
#pragma once
#include <rack.hpp>
#include <atomic>

namespace redDot {

// ── the guest mixin: what a Sands subpanel module carries so the host's harvest
// can read+clear its flag without knowing the concrete type (C1: the Distribution
// expander; the Macro send subpanel joins with the same base).
struct SandsSnapGuest {
    std::atomic<bool> needsResnap{false};
};

// ── the host state: each host module embeds one; the widget's step() drives it.
struct SandsSnapHostState {
    std::atomic<bool> topologyDirty{false};  // the event fast path (onExpanderChange)
    bool   needsResnap = false;              // the host's own one-shot request
    double lastAdjacencyCheck = -1.0;        // the 0.1 s poll cadence (system::getTime)
};

// ── chainIntact: the exact StraitsBase predicate — every right-chain family
// subpanel's left link must be the previous node BY POINTER IDENTITY. A foreign
// module inserted between host and subpanel rewires that link (or nulls it) and
// the predicate fails. Dead/missing widgets are the chain's business, not the
// predicate's (Rack nulls the link itself).
template <class FamilyFn>
inline bool sandsChainIntact(rack::Module* hostMod, FamilyFn isOwnFamily, int maxDepth = 12) {
    if (!hostMod) return true;
    rack::Module* prev = hostMod;
    rack::Module* curr = hostMod->rightExpander.module;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (!isOwnFamily(curr)) break;             // end of the family run — intact
        if (curr->leftExpander.module != prev) return false;
        prev = curr;
        curr = curr->rightExpander.module;
    }
    return true;
}

// ── resnap: seat each right-chain family subpanel flush on the host's right
// edge, in chain order, stepping rightward — Rack's setModulePosForce shoves
// bystanders aside without overlap (the StraitsBase placement, verbatim shape).
template <class FamilyFn>
inline void sandsResnapSubpanels(rack::ModuleWidget* hostW, FamilyFn isOwnFamily, int maxDepth = 12) {
    if (!hostW || !hostW->module) return;
    float expectedX = hostW->box.getTopRight().x;
    rack::Module* curr = hostW->module->rightExpander.module;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (!isOwnFamily(curr)) break;
        rack::ModuleWidget* sw = APP->scene->rack->getModule(curr->id);
        if (sw) {
            APP->scene->rack->setModulePosForce(sw, rack::Vec(expectedX, hostW->box.pos.y));
            expectedX = sw->box.getTopRight().x;
        }
        curr = curr->rightExpander.module;
    }
}

// ── the step() cluster: poll + fast path + drag guard + one-shot consumer.
// Call from the HOST widget's step(), before drawing. takeGuestFlag reads AND
// clears a chain node's guest flag (one-shot — the StraitsBase consumer clears
// every lane's flag it consumed).
template <class FamilyFn, class TakeFlagFn>
inline void sandsSnapStep(rack::ModuleWidget* hostW, SandsSnapHostState& st,
                          FamilyFn isOwnFamily, TakeFlagFn takeGuestFlag) {
    rack::Module* hostMod = hostW ? hostW->module : nullptr;
    if (!hostMod) return;

    // ── the triggers (the StraitsBase trigger block: event fast path OR the
    // 0.1 s poll backstop, deferred while a drag owns the pointer) ──
    const bool eventDirty = st.topologyDirty.load(std::memory_order_relaxed);
    const double now = rack::system::getTime();
    const bool pollDue = st.lastAdjacencyCheck < 0.0 || now - st.lastAdjacencyCheck >= 0.1;
    if ((eventDirty || pollDue) && !APP->event->getDraggedWidget()) {
        if (eventDirty) st.topologyDirty.store(false, std::memory_order_relaxed);
        if (pollDue) st.lastAdjacencyCheck = now;
        if (!sandsChainIntact(hostMod, isOwnFamily))
            st.needsResnap = true;
    }

    // ── the one-shot consumer: the host's flag OR any family subpanel's own
    // flag (the hardening: a subpanel drag-end must reach the resnap even when
    // the chain still reads intact), consumed once, under the drag guard.
    bool flagged = st.needsResnap;
    if (!flagged && !APP->event->getDraggedWidget()) {
        rack::Module* curr = hostMod->rightExpander.module;
        for (int d = 0; curr && d < 12; ++d) {
            if (!isOwnFamily(curr)) break;
            if (takeGuestFlag(curr)) { flagged = true; break; }
            curr = curr->rightExpander.module;
        }
    }
    if (flagged && !APP->event->getDraggedWidget()) {
        st.needsResnap = false;
        sandsResnapSubpanels(hostW, isOwnFamily);
    }
}

} // namespace redDot
