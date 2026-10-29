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
// ── THE TWO STRAITS INVARIANTS (task 85 — Rodney's fix prompt
// docs/design/CC_PROMPT_SANDS_SNAP_FIX.md, commit bf1ccd9f: "Sands snap diverged
// from 2 Straits invariants (stable owned-ID list + unconditional reposition)").
// The task-84 port kept a runIds SNAPSHOT that was REFRESHED FROM THE LIVE
// CHAIN whenever the chain read intact (sandsSnapshotRun) — and that broke the
// ownership in both visible ways: an intruder splitting the family run reads
// "intact" at the split (the walk stops at the first non-family node), so the
// re-snapshot DROPPED every subpanel beyond the break and the host forgot its
// own panels; a host dragged off its run reads vacuously intact on an EMPTY
// chain, so the re-snapshot WIPED the list. StraitsBase never re-derives:
// orderedLaneIds_ is a STABLE OWNED LIST — recorded on attach, erased only on
// deliberate delete, NEVER re-derived from the live chain (the chain only
// DETECTS breakage, chainIntact() walks the owned list itself). Restored here
// as the law of this cluster:
//   INVARIANT 1 — runIds is the STABLE OWNED LIST. Grown ONLY by attach (the
//     context-menu spawn, the spawn-adopt, the near-miss magnet's grab, and the
//     ONE-TIME first-step adopt of the patch-restored run — StraitsBase's
//     lanesSpawned_ walk verbatim), shrunk ONLY by deliberate remove
//     (sandsRemoveSubpanel). Dead IDs linger and are skipped everywhere (the
//     StraitsBase law). No code path clears or re-derives it.
//   INVARIANT 2 — the resnap repositions every OWNED ID UNCONDITIONALLY
//     (sandsResnapSubpanels walks runIds and setModulePosForce each to its
//     seat, wherever it currently is — never gated on near-miss proximity), so
//     an owned subpanel dragged FAR is reclaimed from any distance and an
//     intruder seated between host and subpanel is SHOVED OUT by the force
//     placement. The StraitsBase feel: drag the host and the run follows
//     (drag-end flag), shove anything in the seam and it is ejected within the
//     0.1 s poll.
// The near-miss magnet (sandsAttractNearSubpanels) stays — it is the BROWSER-ADD
// adoption path Straits never needed (its lanes are base-spawned), and it is
// proximity-gated BY DESIGN because it only ever touches UNOWNED family panels;
// owned members are invariant 2's business. On a grab the ID enters the owned
// list (attach), so the magnet can never need to find the same panel twice.
//
// ── THE WHEEL ITSELF (task 84 — Rodney: "straitsbase and lane expander
// exemplify it, don't reinvent the wheel"): StraitsLane models are UNREGISTERED
// ("base-spawned only", src/plugin.cpp) — the user NEVER drops a lane; the
// base SPAWNS it docked (spawnLane: exact flush coords, y = the base's row, so
// addModule's updateExpanders links it from birth) and OWNS the ID forever
// (orderedLaneIds_), pulling it back flush from any distance within the 0.1 s
// poll. That ownership — not the drop — is the straits feel. The first port
// kept only the browser magnet and never made the hosts OWN anything: a
// subpanel that has never linked has an EMPTY snapshot, so nothing adopts it,
// nothing pulls it in, and a near-miss window is hopeless with a 52 HP panel's
// left edge far from the cursor. sandsToggleSubpanel / sandsSpawnSubpanel /
// sandsRemoveSubpanel below are StraitsBase's spawnLane / removeLane /
// toggleLane / hasLane, family-parameterised, with one sands addition: SPAWN
// ADOPTS a parked family panel of that model instead of duplicating it (the
// user's panels are browser-registered; a right-click "spawn" that already
// owns the model removes it — the checkbox law — so adoption is the only path
// a parked panel ever gets back into the dock). runIds stays the ONE owned
// list: spawn pushes the ID, remove erases it — and NOTHING ever re-derives
// it from the live chain (invariant 1).
// The StraitsLane differences, deliberate:
//   - the subpanels are ALSO browser-addable (registered models — the spec's
//     law), so ownership is bootstrapped three ways: the context-menu spawn
//     (the wheel — docked from birth), the near-miss magnet on a drop, and the
//     ONE-TIME first-step adopt of the restored run (a patch-load). The DOCK
//     ORDER (runIds) is the canonical order (the spec's "canonical ordering
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
// STABLE owned list — recorded on attach, erased only on deliberate remove,
// NEVER re-derived from the live chain (task-85 invariant 1), and the only
// memory of the run once links break.
struct SandsSnapHostState {
    std::atomic<bool> topologyDirty{false};  // the event fast path (onExpanderChange)
    bool   needsResnap = false;              // the host's own one-shot request
    double lastAdjacencyCheck = -1.0;        // the 0.1 s poll cadence (system::getTime)
    std::vector<int64_t> runIds;             // the STABLE owned list by module ID, dock order (invariant 1)
    bool   adoptedRun = false;               // the lanesSpawned_ gate: the one-time adopt ran
};

// ── chainIntact: the exact StraitsBase predicate, ownership-aware — the walk
// runs over OUR OWNED LIST (st.runIds), NEVER over the live chain: each live
// owned subpanel's left link must be the previous node BY POINTER IDENTITY,
// starting at the host. A foreign module inserted between host and subpanel
// rewires that link (or nulls it) and the predicate fails; an owned subpanel
// dragged off leaves its left link non-host and the predicate fails. Dead IDs
// (deleted modules) are skipped, exactly like the StraitsBase poll (its
// chainIntact builds the lane list from orderedLaneIds_ and skips dead IDs); an
// empty owned set reads intact (nothing to defend yet). THE INVARIANT (task
// 85): the live chain is used to DETECT breakage, never to redefine ownership —
// membership changes only through attach and deliberate remove.
template <class FamilyFn>
inline bool sandsChainIntact(rack::Module* hostMod, FamilyFn isOwnFamily,
                             const SandsSnapHostState& st, int maxDepth = 12) {
    if (!hostMod) return true;
    rack::Module* prev = hostMod;
    int depth = 0;
    for (int64_t id : st.runIds) {
        rack::Module* m = APP->engine->getModule(id);
        if (!m || !isOwnFamily(m)) continue;       // dead IDs are skipped (the StraitsBase law)
        if (++depth > maxDepth) return false;      // bounded like every walk here
        if (m->leftExpander.module != prev) return false;
        prev = m;
    }
    return true;
}

// ── the ONE-TIME adopt (the StraitsBase lanesSpawned_ walk, verbatim shape):
// on the host widget's FIRST step after construction/patch-load, Rack has
// already restored the saved subpanels at their saved (flush) positions and
// re-linked them (updateExpanders runs inside every load-time addModule) —
// walk the live right chain ONCE and record the family members into the owned
// list. This is the ONLY place the live chain ever DEFINES membership (task-85
// invariant 1: attach grows the list; breakage never re-derives it).
// Already-owned IDs are skipped (idempotent) and the list is NEVER cleared.
template <class FamilyFn>
inline void sandsAdoptLinkedRun(rack::Module* hostMod, FamilyFn isOwnFamily,
                                SandsSnapHostState& st, int maxDepth = 12) {
    rack::Module* curr = hostMod ? hostMod->rightExpander.module : nullptr;
    for (int d = 0; curr && d < maxDepth; ++d) {
        if (!isOwnFamily(curr)) break;             // end of the family run
        bool owned = false;
        for (int64_t id : st.runIds) if (id == curr->id) { owned = true; break; }
        if (!owned) st.runIds.push_back(curr->id);
        curr = curr->rightExpander.module;
    }
}

// ── attract: the near-miss adoption — the browser-add snap. Scans the rack for
// UNOWNED family modules whose ROUNDED grid box sits in the snap window right of
// the OWNED seating edge (the host, then every owned member's widget — walked
// BY ID from runIds, not from the live chain: after a break the chain is empty
// but ownership remains), seats them flush with setModulePosForce (which is
// also the call that makes Rack LINK them, because updateExpanders runs
// inside), and — invariant 1 — RECORDS each grab into the owned list: adoption
// IS ownership. Owned members are excluded from the scan and are never
// proximity-gated: their reclaim is the resnap's unconditional business
// (invariant 2). Returns true when anything moved.
template <class FamilyFn>
inline bool sandsAttractNearSubpanels(rack::ModuleWidget* hostW, FamilyFn isOwnFamily,
                                      SandsSnapHostState& st) {
    if (!hostW || !hostW->module) return false;
    float expectedPx    = hostW->box.getTopRight().x;
    rack::math::Rect hb = hostW->getGridBox();
    const float expectedTop   = hb.getTop();
    float expectedRight = hb.getTopRight().x;
    // the OWNED run defines the seating edge (the StraitsBase owned-edge walk)
    for (int64_t id : st.runIds) {
        rack::Module* m = APP->engine->getModule(id);
        if (!m || !isOwnFamily(m)) continue;       // dead IDs are skipped
        if (rack::ModuleWidget* sw = APP->scene->rack->getModule(m->id)) {
            expectedPx    = std::max(expectedPx,    sw->box.getTopRight().x);
            expectedRight = std::max(expectedRight, sw->getGridBox().getTopRight().x);
        }
    }
    auto isOwned = [&](rack::Module* m) {
        for (int64_t id : st.runIds) if (id == m->id) return true;
        return false;
    };
    // the scan: UNOWNED family modules in the snap window, same row (Rack's
    // linker math)
    std::vector<rack::ModuleWidget*> grabbed;
    for (rack::ModuleWidget* mw : APP->scene->rack->getModules()) {
        if (!mw || !mw->module || mw->module == hostW->module) continue;
        if (isOwned(mw->module)) continue;
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
        st.runIds.push_back(mw->module->id);   // adoption = ownership (invariant 1: attach grows the list)
        expectedPx    = mw->box.getTopRight().x;
        expectedRight += mw->getGridSize().x;
        moved = true;
    }
    return moved;
}

// ── resnap (INVARIANT 2 — the StraitsBase resnapLanes, verbatim shape): seat
// every OWNED subpanel flush on the host's right edge, in owned-list order,
// stepping rightward, UNCONDITIONALLY — each owned ID is setModulePosForce'd to
// its seat REGARDLESS of where it currently sits (a subpanel shoved FAR is
// reclaimed from any distance; never gated on near-miss proximity) and any
// bystander on a seat (an intruder) is SHOVED aside by the force placement —
// the eject behaviour falls out of the unconditional walk, exactly as Straits
// resnapLanes walking orderedLaneIds_ ejects an inserted module.
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

// ══ THE STRAITS WHEEL (task 84): StraitsBase's spawnLane / removeLane /
// toggleLane / hasLane, family-parameterised. The hosts OWN their subpanels —
// the context menu spawns them DOCKED (linked from birth, never a drop skill
// test), the owned ID list is st.runIds (the ONE stable owned list — invariant 1).

// ── the spawnLane edge walk, verbatim shape: right of the host, but past every
// OWNED member's widget (StraitsBase: "right of the rightmost owned lane").
template <class FamilyFn>
inline float sandsOwnedEdgePx(rack::ModuleWidget* hostW, const SandsSnapHostState& st,
                              FamilyFn isOwnFamily) {
    float rightX = hostW->box.getTopRight().x;
    for (int64_t id : st.runIds) {
        rack::Module* m = APP->engine->getModule(id);
        if (!m || !isOwnFamily(m)) continue;
        if (rack::ModuleWidget* lw = APP->scene->rack->getModule(m->id))
            rightX = std::max(rightX, lw->box.getTopRight().x);
    }
    return rightX;
}

// ── hasLane, verbatim shape: an OWNED member of this model?
template <class FamilyFn>
inline bool sandsHasOwnedSubpanel(const SandsSnapHostState& st, FamilyFn isOwnFamily,
                                  rack::Model* model) {
    for (int64_t id : st.runIds) {
        rack::Module* m = APP->engine->getModule(id);
        if (m && isOwnFamily(m) && m->model == model) return true;
    }
    return false;
}

// ── removeLane, verbatim shape: find the OWNED member of this model, remove its
// widget from the rack, remove the engine module, erase the ID, resnap (close
// the gap — the remaining owned members re-seat flush).
template <class FamilyFn>
inline bool sandsRemoveSubpanel(rack::ModuleWidget* hostW, SandsSnapHostState& st,
                                FamilyFn isOwnFamily, rack::Model* model) {
    if (!hostW || !hostW->module) return false;
    for (size_t i = 0; i < st.runIds.size(); ++i) {
        rack::Module* m = APP->engine->getModule(st.runIds[i]);
        if (!m || !isOwnFamily(m) || m->model != model) continue;
        if (rack::ModuleWidget* lw = APP->scene->rack->getModule(m->id))
            APP->scene->rack->removeModule(lw);      // updateExpanders inside detaches the links
        APP->engine->removeModule(m);
        st.runIds.erase(st.runIds.begin() + i);
        sandsResnapSubpanels(hostW, isOwnFamily, st);
        return true;
    }
    return false;
}

// ── spawnLane, with one sands addition: ADOPT a parked family panel of this
// model instead of spawning a duplicate. Seat = the owned edge (sandsOwnedEdgePx)
// on the host's row; setModulePosForce there links the adoptee THIS frame
// (updateExpanders runs inside it). A parked panel qualifies when it is family,
// unowned, and its LEFT neighbour is LOOSE — null, or neither family nor a host
// visual (never steal from another host's dock; never break into a parked
// pair's middle — its loose head is the adoptable one). No loose candidate →
// CREATE: the StraitsBase spawnLane order verbatim (engine module first, the ID
// pushed, the widget created, box.pos set BEFORE rack addModule — addModule
// preserves pos and runs updateExpanders, so the panel is LINKED FROM BIRTH),
// then one setModulePosForce to shove any bystander off the seat.
template <class FamilyFn, class LooseFn>
inline bool sandsSpawnSubpanel(rack::ModuleWidget* hostW, SandsSnapHostState& st,
                               FamilyFn isOwnFamily, LooseFn isLooseLeft,
                               rack::Model* model) {
    if (!hostW || !hostW->module || !model) return false;
    if (sandsHasOwnedSubpanel(st, isOwnFamily, model)) return false;   // checkbox guard
    const float seatX = sandsOwnedEdgePx(hostW, st, isOwnFamily);
    const float seatY = hostW->box.pos.y;

    rack::ModuleWidget* adopt = nullptr;
    for (rack::ModuleWidget* mw : APP->scene->rack->getModules()) {
        if (!mw || !mw->module || mw->module->model != model) continue;
        rack::Module* m = mw->module;
        bool owned = false;
        for (int64_t id : st.runIds) if (id == m->id) { owned = true; break; }
        if (owned) continue;
        if (!isLooseLeft(m->leftExpander.module)) continue;
        adopt = mw;
        break;
    }
    if (adopt) {
        APP->scene->rack->setModulePosForce(adopt, rack::Vec(seatX, seatY));
        st.runIds.push_back(adopt->module->id);
        return true;
    }

    // CREATE (StraitsBase spawnLane verbatim order)
    rack::engine::Module* subMod = model->createModule();
    APP->engine->addModule(subMod);
    st.runIds.push_back(subMod->id);
    rack::ModuleWidget* subW = model->createModuleWidget(subMod);
    if (!subW) {   // never leave a tracked zombie
        APP->engine->removeModule(subMod);
        st.runIds.pop_back();
        return false;
    }
    subW->box.pos = rack::Vec(seatX, seatY);
    APP->scene->rack->addModule(subW);   // updateExpanders inside → LINKED from birth
    APP->scene->rack->setModulePosForce(subW, rack::Vec(seatX, seatY));   // shove any bystander off the seat
    return true;
}

// ── toggleLane, verbatim shape: owned → remove; else spawn (adopt-or-create).
// The caller sets st.needsResnap on a true return so the one-shot consumer
// re-seats the run cleanly on the next frame.
template <class FamilyFn, class LooseFn>
inline bool sandsToggleSubpanel(rack::ModuleWidget* hostW, SandsSnapHostState& st,
                                FamilyFn isOwnFamily, LooseFn isLooseLeft,
                                rack::Model* model) {
    if (sandsHasOwnedSubpanel(st, isOwnFamily, model))
        return sandsRemoveSubpanel(hostW, st, isOwnFamily, model);
    return sandsSpawnSubpanel(hostW, st, isOwnFamily, isLooseLeft, model);
}

// ── the step() cluster: the one-time adopt + poll + fast path + drag guard +
// one-shot consumer. Call from the HOST widget's step(), before drawing.
// takeGuestFlag reads AND clears an owned node's guest flag (one-shot — the
// StraitsBase consumer clears every lane's flag it consumed).
template <class FamilyFn, class TakeFlagFn>
inline void sandsSnapStep(rack::ModuleWidget* hostW, SandsSnapHostState& st,
                          FamilyFn isOwnFamily, TakeFlagFn takeGuestFlag) {
    rack::Module* hostMod = hostW ? hostW->module : nullptr;
    if (!hostMod) return;
    const bool dragged = APP->event->getDraggedWidget() != nullptr;

    // ── the ONE-TIME adopt (the StraitsBase lanesSpawned_ walk): the first step
    // after construction/patch-load records the restored, linked family run into
    // the owned list. From here on the list NEVER re-derives from the live
    // chain — attach (spawn / spawn-adopt / magnet grab / this walk) grows it,
    // deliberate remove shrinks it, and breakage only DETECTS (invariant 1). ──
    if (!st.adoptedRun) {
        st.adoptedRun = true;
        sandsAdoptLinkedRun(hostMod, isOwnFamily, st);
    }

    // ── the triggers (the StraitsBase trigger block: event fast path OR the
    // 0.1 s poll backstop, deferred while a drag owns the pointer) ──
    const bool eventDirty = st.topologyDirty.load(std::memory_order_relaxed);
    const double now = rack::system::getTime();
    const bool pollDue = st.lastAdjacencyCheck < 0.0 || now - st.lastAdjacencyCheck >= 0.1;
    if ((eventDirty || pollDue) && !dragged) {
        if (eventDirty) st.topologyDirty.store(false, std::memory_order_relaxed);
        if (pollDue) st.lastAdjacencyCheck = now;
        // THE ATTRACTION PASS (the browser-add snap — the piece Straits never
        // needed because its lanes are SPAWNED flush): adopt UNOWNED family
        // modules in the snap window right of the owned edge; each grab ENTERS
        // the owned list (attach). setModulePosForce is also what makes Rack
        // LINK them (updateExpanders runs inside it).
        if (sandsAttractNearSubpanels(hostW, isOwnFamily, st))
            st.needsResnap = true;
        // THE BREAK DETECTOR (the StraitsBase poll, ownership-aware — it walks
        // the OWNED list, not the chain): an intact read is a NO-OP (the list
        // is never refreshed — invariant 1), a broken read only RAISES the
        // flag; the consumer's unconditional reposition then re-seats the run
        // and shoves the intruder out (invariant 2).
        if (!sandsChainIntact(hostMod, isOwnFamily, st))
            st.needsResnap = true;
    }

    // ── the one-shot consumer: the host's flag OR any owned member's own
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
        // its snap window first (grabs enter the owned list), then re-seat the
        // run from the owned list — UNCONDITIONALLY, wherever the members are.
        sandsAttractNearSubpanels(hostW, isOwnFamily, st);
        sandsResnapSubpanels(hostW, isOwnFamily, st);
    }
}

} // namespace redDot
