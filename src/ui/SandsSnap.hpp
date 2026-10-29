// SandsSnap — the host-side resnap cluster for the SANDS right-side subpanels.
// Spec: docs/design/CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md §HARD REQUIREMENT:
// "Reuse the Straits snapping code/pattern directly — do NOT reinvent."
//
// This is the StraitsBase cluster (resnapLanes + chainIntact + the 0.1 s periodic
// adjacency poll + the event-driven fast path + the drag guard + the one-shot
// consumer), family-parameterised so BOTH hosts — the Macro visual (cross-voice,
// may carry TWO subpanels: the per-voice-sends subpanel AND the Distribution
// expander) and the East visual (per-voice) — run the SAME code.
//
// ── THE RUN MEMORY (the StraitsBase orderedLaneIds_ equivalent — the piece the
// first port dropped, and the reason the subpanels "didn't snap together like
// the straits example"): StraitsBase OWNS its lanes — they are SPAWNED at exact
// flush positions, and resnapLanes() walks the base's owned-ID list, so the run
// survives every unlink (the base dragged away, a lane dragged off, a foreign
// insert rewiring the links). The Sands subpanels are BROWSER-ADDED (the spec's
// law), and Rack v2 links expanders ONLY when the ROUNDED grid boxes touch
// EXACTLY (RackWidget::updateExpanders: p2Left.equals(pRight), same row) — so a
// drop with even one pixel of gap never links, and a live-chain walk then sees
// NOTHING, forever. This cluster therefore keeps the two Straits-shaped pieces
// the live chain cannot provide:
//   - sandsAttractNearSubpanels: the near-miss adoption ("the subpanel snaps
//     adjacent to its host" — seated flush via setModulePosForce, which is also
//     the call that makes Rack LINK it, because updateExpanders runs inside);
//   - the runIds snapshot in SandsSnapHostState: refreshed from the live chain
//     whenever the chain reads intact, and the ONLY thing the resnap and the
//     guest-flag harvest walk afterwards — so an unlink (host dragged away,
//     subpanel dragged off, foreign insert) still re-seats the run, exactly
//     like StraitsBase's owned list. Dead IDs are skipped (the StraitsBase
//     law); the family predicate re-validates every resolve.
// The StraitsLane differences, deliberate:
//   - the subpanels are BROWSER-ADDED (not base-spawned), so there is no
//     ordered-ID ownership list from spawn: the RIGHT CHAIN — and the snapshot
//     taken from it — is the canonical order (the spec's "canonical ordering
//     resolves ties, but either order is valid" — the user's physical order
//     persists; the resnap preserves it, it never re-sorts);
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
#include <vector>
#include <algorithm>
#include "SandsSnapGeom.hpp"   // the pure near-miss decision (rack-free, unit-tested)

namespace redDot {

// ── the guest mixin: what a Sands subpanel module carries so the host's harvest
// can read+clear its flag without knowing the concrete type (C1: the Distribution
// expander; the Macro send subpanel joins with the same base).
struct SandsSnapGuest {
    std::atomic<bool> needsResnap{false};
};

// ── the host state: each host module embeds one; the widget's step() drives it.
// runIds is UI-thread-only data (read/written only from the host widget's step,
// the same single-thread context as StraitsBaseWidget's orderedLaneIds_): the
// family run captured by module ID while the live chain last read intact, and
// the only memory of the run once links break.
struct SandsSnapHostState {
    std::atomic<bool> topologyDirty{false};  // the event fast path (onExpanderChange)
    bool   needsResnap = false;              // the host's own one-shot request
    double lastAdjacencyCheck = -1.0;        // the 0.1 s poll cadence (system::getTime)
    std::vector<int64_t> runIds;             // the family run by module ID, chain order
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

// ── the run snapshot: capture the family chain by module ID, in chain order —
// the StraitsBase orderedLaneIds_ capture (its adopt walk), run every trigger
// while the chain reads intact. Bounded like every walk here.
template <class FamilyFn>
inline void sandsSnapshotRun(rack::Module* hostMod, FamilyFn isOwnFamily,
                             SandsSnapHostState& st, int maxDepth = 12) {
    st.runIds.clear();
    rack::Module* curr = hostMod ? hostMod->rightExpander.module : nullptr;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (!isOwnFamily(curr)) break;             // end of the family run
        st.runIds.push_back(curr->id);
        curr = curr->rightExpander.module;
    }
}

// ── attract: the near-miss adoption — the browser-add snap. Scans the rack for
// OWN-family modules whose ROUNDED grid box sits in the snap window right of the
// run's LIVE seating edge (the host, then each already-linked family node — so a
// new arrival seats AFTER the canonical run, never between its members), and
// seats them flush with setModulePosForce, stepping the edge. Returns true when
// anything moved. The row test + window are Rack's own linker math
// (ui/SandsSnapGeom.hpp): a seat lands on exact grid adjacency, so the
// updateExpanders call inside setModulePosForce links it THIS frame.
template <class FamilyFn>
inline bool sandsAttractNearSubpanels(rack::ModuleWidget* hostW, FamilyFn isOwnFamily) {
    if (!hostW || !hostW->module) return false;
    float expectedPx    = hostW->box.getTopRight().x;
    rack::math::Rect hb = hostW->getGridBox();
    const float expectedTop   = hb.getTop();
    float expectedRight = hb.getTopRight().x;
    // the already-linked run (excluded from the scan; its members define the edge)
    rack::Module* run[12];
    int runLen = 0;
    rack::Module* curr = hostW->module->rightExpander.module;
    for (int d = 0; curr && d < 12; ++d) {
        if (!isOwnFamily(curr)) break;
        run[runLen++] = curr;
        if (rack::ModuleWidget* sw = APP->scene->rack->getModule(curr->id)) {
            expectedPx    = sw->box.getTopRight().x;
            expectedRight += sw->getGridSize().x;
        }
        curr = curr->rightExpander.module;
    }
    auto isRunMember = [&](rack::Module* m) {
        for (int i = 0; i < runLen; ++i) if (run[i] == m) return true;
        return false;
    };
    // the scan: family modules in the snap window, same row (Rack's linker math)
    std::vector<rack::ModuleWidget*> grabbed;
    for (rack::ModuleWidget* mw : APP->scene->rack->getModules()) {
        if (!mw || !mw->module || mw->module == hostW->module) continue;
        if (isRunMember(mw->module)) continue;
        if (!isOwnFamily(mw->module)) continue;
        rack::math::Rect gb = mw->getGridBox();
        if (!sandsNearMiss(gb.getTop(), gb.getLeft(), expectedTop, expectedRight)) continue;
        grabbed.push_back(mw);
    }
    std::sort(grabbed.begin(), grabbed.end(), [](rack::ModuleWidget* a, rack::ModuleWidget* b) {
        return a->getGridBox().getLeft() < b->getGridBox().getLeft();
    });
    bool moved = false;
    for (rack::ModuleWidget* mw : grabbed) {
        // re-test against the STEPPED edge (two near-misses queue up in order)
        rack::math::Rect gb = mw->getGridBox();
        if (!sandsNearMiss(gb.getTop(), gb.getLeft(), expectedTop, expectedRight)) continue;
        APP->scene->rack->setModulePosForce(mw, rack::Vec(expectedPx, hostW->box.pos.y));
        expectedPx    = mw->box.getTopRight().x;
        expectedRight += mw->getGridSize().x;
        moved = true;
    }
    return moved;
}

// ── resnap: seat the RUN flush on the host's right edge, in snapshot order,
// stepping rightward — Rack's setModulePosForce shoves bystanders aside without
// overlap (the StraitsBase placement, verbatim shape — but walking the SNAPSHOT,
// because after an unlink the live chain is empty, and the StraitsBase original
// walks its owned list for exactly that reason).
template <class FamilyFn>
inline void sandsResnapSubpanels(rack::ModuleWidget* hostW, FamilyFn isOwnFamily,
                                 const SandsSnapHostState& st) {
    if (!hostW || !hostW->module) return;
    float expectedX = hostW->box.getTopRight().x;
    for (int64_t id : st.runIds) {
        rack::Module* m = APP->engine->getModule(id);
        if (!m || !isOwnFamily(m)) continue;   // dead IDs are skipped (the StraitsBase law)
        rack::ModuleWidget* sw = APP->scene->rack->getModule(m->id);
        if (sw) {
            APP->scene->rack->setModulePosForce(sw, rack::Vec(expectedX, hostW->box.pos.y));
            expectedX = sw->box.getTopRight().x;
        }
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
    const bool dragged = APP->event->getDraggedWidget() != nullptr;

    // ── the triggers (the StraitsBase trigger block: event fast path OR the
    // 0.1 s poll backstop, deferred while a drag owns the pointer) ──
    const bool eventDirty = st.topologyDirty.load(std::memory_order_relaxed);
    const double now = rack::system::getTime();
    const bool pollDue = st.lastAdjacencyCheck < 0.0 || now - st.lastAdjacencyCheck >= 0.1;
    if ((eventDirty || pollDue) && !dragged) {
        if (eventDirty) st.topologyDirty.store(false, std::memory_order_relaxed);
        if (pollDue) st.lastAdjacencyCheck = now;
        // THE ATTRACTION PASS (the browser-add snap — the piece Straits never
        // needed because its lanes are SPAWNED flush): adopt family modules in
        // the snap window right of the run's edge. setModulePosForce is also
        // what makes Rack LINK them (updateExpanders runs inside it).
        if (sandsAttractNearSubpanels(hostW, isOwnFamily))
            st.needsResnap = true;
        // THE RUN MEMORY: refresh the snapshot whenever the chain reads intact;
        // on a break, KEEP it — the snapshot is the only memory of the run once
        // the links are gone (the StraitsBase orderedLaneIds_ semantics).
        if (sandsChainIntact(hostMod, isOwnFamily))
            sandsSnapshotRun(hostMod, isOwnFamily, st);
        else
            st.needsResnap = true;
    }

    // ── the one-shot consumer: the host's flag OR any snapshot member's own
    // flag (the hardening: a subpanel drag-end must reach the resnap even when
    // the chain no longer reads intact — which is exactly the unlinked case),
    // consumed once, under the drag guard.
    bool flagged = st.needsResnap;
    if (!flagged && !dragged) {
        for (int64_t id : st.runIds) {
            rack::Module* m = APP->engine->getModule(id);
            if (!m || !isOwnFamily(m)) continue;
            if (takeGuestFlag(m)) { flagged = true; break; }
        }
    }
    if (flagged && !dragged) {
        st.needsResnap = false;
        // the host itself moved (its drag-end flag): adopt whatever now sits in
        // its snap window first, then re-seat the run from the snapshot.
        sandsAttractNearSubpanels(hostW, isOwnFamily);
        sandsResnapSubpanels(hostW, isOwnFamily, st);
    }
}

} // namespace redDot
