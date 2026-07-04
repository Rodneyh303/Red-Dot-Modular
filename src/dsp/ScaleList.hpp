#pragma once
//
// ScaleList — the data model for Shophouse's scale-list + boundary-quantised modulation
// (SHOPHOUSE_SPEC.md). Deliberately PURE and driver-agnostic: it holds a small list of
// (scale, root) entries and a pending→active index that commits ONLY on a phrase-boundary
// signal. It knows nothing about HOW the pending index is chosen (gate-queued vs CV-sampled —
// open question 2) or how it's presented (UI — open question 4); a driver sets `pending`, the
// engine calls commitAtBoundary() at the phrase boundary. This is the substrate the driver +
// UI + engine-wiring build on, and it's independently unit-testable.
//
// Unification (spec): a SINGLE scale is just a 1-entry list. Monsoon's existing single
// scaleRoot/lastSelectedScale maps to a 1-entry ScaleList (migration = seed entry 0).
//
#include <cstdint>
#include <vector>
#include <algorithm>

struct ScaleListEntry {
    int scaleIdx = -1;   // index into MONSOON_SCALES (-1 = none/chromatic, matches ScaleManager)
    int root     = 0;    // 0..11 semitone root
    bool operator==(const ScaleListEntry& o) const { return scaleIdx == o.scaleIdx && root == o.root; }
};

// Boundary-quantised list of (scale, root) entries. "Decide freely, apply at boundary":
// setPending() can be called any time (by any driver); the active index only changes when
// commitAtBoundary() is called (at the phrase boundary), so the scale the engine reads changes
// only at that clean cut — coherent with the phrase-boundary behaviour (LEGATO_TIE_REDESIGN.md).
class ScaleList {
public:
    // Construct with a fixed number of slots (default 4 per spec; length stays configurable so
    // open question 3 "4 or configurable" is sidestepped — it works for any N>=1).
    explicit ScaleList(int slots = 4) { resize(slots); }

    int  size() const { return (int)entries_.size(); }
    bool isSingle() const { return entries_.size() == 1; }   // 1-entry list = single scale

    void resize(int slots) {
        slots = slots < 1 ? 1 : slots;
        entries_.resize((size_t)slots);
        clampIndices_();
    }

    // ── Entry editing (by the UI / migration; independent of the driver) ──────────────
    void setEntry(int slot, int scaleIdx, int root) {
        if (slot < 0 || slot >= size()) return;
        entries_[(size_t)slot].scaleIdx = scaleIdx;
        entries_[(size_t)slot].root     = ((root % 12) + 12) % 12;
    }
    const ScaleListEntry& entry(int slot) const {
        static const ScaleListEntry kNone;
        if (slot < 0 || slot >= size()) return kNone;
        return entries_[(size_t)slot];
    }

    // ── Pending → active, committed at the boundary ──────────────────────────────────
    // A driver (gate/CV/UI) sets the pending slot at any time. Clamped/wrapped into range.
    void setPending(int slot) { pending_ = wrap_(slot); }
    void stepPending(int delta) { pending_ = wrap_(pending_ + delta); }  // fwd/back drivers
    int  pending() const { return pending_; }
    int  active()  const { return active_; }

    // Called by the engine AT the phrase boundary: the pending slot becomes active. Returns
    // true if the active entry actually changed (so the caller can updateScaleMask only then).
    bool commitAtBoundary() {
        if (active_ == pending_) return false;
        ScaleListEntry before = entries_[(size_t)active_];
        active_ = pending_;
        return !(entries_[(size_t)active_] == before);   // value-changed (not just index)
    }

    const ScaleListEntry& activeEntry() const { return entry(active_); }

    // Migration helper: seed a 1-entry list from Monsoon's existing single scale/root.
    void seedSingle(int scaleIdx, int root) {
        resize(1);
        setEntry(0, scaleIdx, root);
        pending_ = active_ = 0;
    }

private:
    std::vector<ScaleListEntry> entries_;
    int pending_ = 0;
    int active_  = 0;

    int  wrap_(int i) const { int n = size(); return ((i % n) + n) % n; }
    void clampIndices_() {
        pending_ = wrap_(pending_);
        active_  = wrap_(active_);
    }
};
