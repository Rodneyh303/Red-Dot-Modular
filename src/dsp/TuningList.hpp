#pragma once
//
// TuningList — the data model for Shophouse Micro's tuning-slot + boundary-quantised modulation
// (SHOPHOUSE_MICRO_SPEC.md). The microtonal generalisation of ScaleList: instead of (scale, root)
// entries it holds full per-degree TUNING SLOTS (cents[] + weight[] for N degrees), and a pending→
// active index that commits ONLY on a phrase-boundary signal. Same "decide freely, apply at boundary"
// contract as ScaleList — a driver sets pending; the engine calls commitAtBoundary() at the loop edge.
//
// PURE / driver-agnostic / no Rack (uses the plain TuningTable::MAXN size), so it's unit-testable.
// It knows nothing about .dmtune JSON (that's TuningPreset, jansson-backed) — a slot just holds the
// already-decoded cents/weight/name. The module layer decodes a .dmtune into a slot via loadSlot().
//
// 12/24 MODE (SHOPHOUSE_MICRO_SPEC §66): the list carries an explicit degree count `n_` (12 or 24);
// every loaded slot must match it. loadSlot() rejects a mismatched n so a mixed-N slot set is
// impossible. Front COUNT is caller-chosen (4 at 12, 2 at 24) via resize().
//
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include "../tuning/TuningTable.hpp"   // dotModular::TuningTable::MAXN

struct TuningSlot {
    bool  loaded = false;                         // false = empty front (placeholder name)
    // TUNING SIZE of THIS front (ROUND 10 full model): a front is a .dmtune of ANY n in 1..capacity —
    // slots may differ in n. Drives the host's tuningN when this front is active. 0 = empty.
    int   n = 0;
    float cents  [dotModular::TuningTable::MAXN] = {};
    // SCALE-MEMBERSHIP mask (ENABLED_MASK_BUILD_BRIEF v2): a .dmtune front carries cents + enabled, NOT
    // weight (weight is the live fader mix, never in a preset). Overrides the host's cents+enabled.
    bool  enabled[dotModular::TuningTable::MAXN] = {};
    std::string name;                             // .dmtune name for the band readout
};

class TuningList {
public:
    // slots = front count (4 at 12-mode, 2 at 24-mode per spec); n = degree count (12 or 24).
    explicit TuningList(int slots = 4, int n = 12) { n_ = clampN_(n); resize(slots); }

    int  size() const { return (int)slots_.size(); }
    int  degrees() const { return n_; }           // the CAPACITY / mode (12 or 24), NOT a front's size

    void resize(int slots) {
        slots = slots < 1 ? 1 : slots;
        slots_.resize((size_t)slots);
        clampIndices_();
    }

    // Set the degree mode. Only allowed while NO slot is loaded (spec: changing mode with loaded slots
    // requires an explicit clear at the module layer). Returns true if applied.
    bool setDegrees(int n) {
        n = clampN_(n);
        if (n == n_) return true;
        if (anyLoaded()) return false;            // caller must clear() first (explicit confirm)
        n_ = n;
        return true;
    }
    bool anyLoaded() const {
        for (const auto& s : slots_) if (s.loaded) return true;
        return false;
    }

    // ── Slot loading (by the module after decoding a .dmtune) ─────────────────────────────────────
    // ROUND 10 full model: a front's degree count `n` may be ANY value in 1..capacity — slots may differ
    // (the old all-fronts-same-n rejection is DROPPED). Only bound is the capacity (n_). If UNATTACHED
    // and adoptModeIfEmpty, the first load can BUMP the capacity/mode up (e.g. a 24-degree file adopts
    // 24-mode) — but never rejects an n <= capacity. Stores the front's own n for the scene-drive.
    bool loadSlot(int slot, int n, const float* cents, const bool* enabled,
                  const std::string& name, bool adoptModeIfEmpty = true) {
        if (slot < 0 || slot >= size()) return false;
        if (n < 1) return false;
        if (n > n_) {                              // file bigger than the current capacity/mode
            if (adoptModeIfEmpty && !anyLoaded()) { n_ = clampN_(n); }  // empty → adopt a larger mode
            if (n > n_) return false;              // still over capacity (e.g. 17 vs a fixed-12 host) → reject
        }
        TuningSlot& s = slots_[(size_t)slot];
        s.loaded = true;
        s.n      = n;                              // THIS front's tuning size (1..capacity)
        s.name   = name;
        for (int i = 0; i < dotModular::TuningTable::MAXN; ++i) {
            s.cents[i]   = (i < n && cents)   ? cents[i]   : 0.f;
            s.enabled[i] = (i < n && enabled) ? enabled[i] : (i < n);   // no mask → all in-scale within n
        }
        return true;
    }
    void clearSlot(int slot) {
        if (slot < 0 || slot >= size()) return;
        slots_[(size_t)slot] = TuningSlot{};
    }
    void clear() { for (auto& s : slots_) s = TuningSlot{}; }

    const TuningSlot& slot(int i) const {
        static const TuningSlot kEmpty;
        if (i < 0 || i >= size()) return kEmpty;
        return slots_[(size_t)i];
    }

    // ── Pending → active, committed at the boundary (same contract as ScaleList) ──────────────────
    void setPending(int slot) { pending_ = wrap_(slot); }
    void stepPending(int delta) { pending_ = wrap_(pending_ + delta); }
    int  pending() const { return pending_; }
    int  active()  const { return active_; }

    // Called by the engine AT the phrase boundary: pending becomes active. Returns true if the active
    // slot's CONTENT actually changed (so the caller re-publishes only then).
    bool commitAtBoundary() {
        if (active_ == pending_) return false;
        TuningSlot before = slots_[(size_t)active_];
        active_ = pending_;
        return !sameContent_(slots_[(size_t)active_], before);
    }
    const TuningSlot& activeSlot() const { return slot(active_); }

private:
    std::vector<TuningSlot> slots_;
    int n_       = 12;
    int pending_ = 0;
    int active_  = 0;

    static int clampN_(int n) { return (n >= 24) ? 24 : 12; }   // only 12 or 24 (matches the family)
    int  wrap_(int i) const { int m = size(); return ((i % m) + m) % m; }
    void clampIndices_() { pending_ = wrap_(pending_); active_ = wrap_(active_); }

    static bool sameContent_(const TuningSlot& a, const TuningSlot& b) {
        if (a.loaded != b.loaded) return false;
        if (a.n != b.n) return false;                  // different tuning size → re-publish
        for (int i = 0; i < dotModular::TuningTable::MAXN; ++i) {
            if (a.cents[i]   != b.cents[i])   return false;
            if (a.enabled[i] != b.enabled[i]) return false;
        }
        return true;
    }
};
