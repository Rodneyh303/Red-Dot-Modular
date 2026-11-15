#pragma once
// ── redDot::PhaseDistStore — the EIGHT PARAMETER SETS of the phase-distortion
// expander model, plus the block-consistency mechanism (PHASE_DISTRIBUTION_SPEC §14,
// CC_PROMPT_PHASE_DISTORTION_EXPANDER Task 1) ─────────────────────────────────────────
//
// THE MODEL (§14, Rodney-adopted; do not change): the expander holds EIGHT parameter
// sets — (warp, tilt, gap, atom, delegation) each — and every Monsoon in the network
// pulls its OWN row (two indices: one for the phase tier, one for the subphase tier)
// from this store. Several Monsoons may hold the same index and so share a set. The
// expander produces no signal; nothing is pushed; the read is a plain memory read.
//
// TASK 1 — THE CONSISTENCY MECHANISM (the CC prompt's "inspect StoreBound.hpp and the
// store it writes"): the store the expander's knobs write is PLAIN FIELDS written by
// the UI thread (StoreKnob's setter runs on the UI thread — StoreBound.hpp) while the
// readers run on the engine thread. Un-synchronised, a reader can see a TORN set (the
// gap location of the next value beside the gap size of the previous one) and two
// Monsoons can disagree within one audio block. THE CHOSEN FIX — the prompt's option
// (a), PER-BLOCK SNAPSHOT, the smaller change with no per-sample locking:
//
//   · EVERY WRITE takes the store mutex. UI writes are rare (a drag event, a JSON
//     load), so the mutex is never contended in steady state and — the prompt's hard
//     rule — nothing locks PER SAMPLE.
//   · EACH READER copies the set it needs ONCE PER ITS OWN AUDIO BLOCK (at the block
//     start — Monsoon's Control-tier tick), under the same mutex, into its own working
//     copy, and uses THAT copy for the whole block. Within a block a reader's view is
//     frozen (its own copy); a mid-block write becomes visible at the reader's next
//     block boundary, never mid-block; a write BETWEEN blocks is picked up by both
//     readers at their same-indexed next boundary — never one before the other (the
//     Task-1 gate, pinned by test_phase_distortion's two-reader block test).
//   · THE MUTEX makes a multi-field write ATOMIC per set: a snapshot can never interleave
//     two writes (no torn gapLoc/gapSize pair, no torn member/depth pair). This is the
//     piece relaxed atomics alone do not give — the spec's own warning ("relaxed atomics
//     alone do not guarantee it") — and why the mutex is load-bearing even though the
//     copy it guards is ~60 bytes.
//
// WHY NOT OPTION (b) (the double-buffered store): it needs the back/front swap, a dirty
// flag and a publish cadence on the expander side, and the swap itself can straddle two
// readers' snapshot moments inside one engine frame (module order) — the reader-side
// snapshot is what actually delivers "both readers see the new value at the SAME block
// boundary", so the snapshot is the mechanism and the double buffer would be plumbing
// around it. Documented in the spec's §15 implementation record.
//
// THE SET (one channel): the warp member is a BARE palette curve (never a DualWarp —
// the READING lives in the sign of w, the bidirectional-warp law the branch already
// landed), the depth is SIGNED [−1,+1], and the tilt/gap/atom fields are the authored
// RAW values (the eval-side clamps live in WarpStages.hpp — the store trusts its
// writer, the same split the bank uses). "delegation" is the model's fifth element: it
// rides the set (persisted, shown by the future authoring UI) and is ENGINE-INERT —
// the transform depends only on (phase, the set's numbers, the reading).
//
// IDENTITY: a default-constructed store is EIGHT IDENTITY SETS (identity member, w=0,
// tilt 0, mass 0, gap width 0) — the Stage-1 gate's starting point ("all eight sets at
// identity, each Monsoon's output bit-identical to today").
//
// PURE MODULE: no Rack — plain C++ + jansson (the WarpF codec's own dependency), so the
// unit rig drives the mechanism directly (test/test_phase_distortion.cpp) and the
// expander embeds it as a member. The mutex is mutable: a const snapshot is a read.

#include <mutex>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>

#include "WarpF.hpp"   // WarpF, warpToJson/warpFromJson, IdentityWarp

namespace redDot {

// ONE parameter set (a §14 "channel"). Trivially copyable apart from the WarpF
// variant (itself trivially copyable — pinned by test_warp_f_bank) — the reader's
// per-block copy is a plain struct copy.
struct PhaseDistSet {
    dotModular::WarpF f = {};   // the BARE member (never DualWarp: the reading is
                                // the SIGN of w — the amended bidirectional law)
    float w          = 0.f;     // SIGNED depth [−1,+1] (0 = identity; + member
                                // quantile, − the dual — the mix's own convention)
    float tilt       = 0.f;     // rail tilt p (neutral 0; eval clamps to ±0.95)
    float atomLevel  = 0.5f;    // atom location ℓ — INTERIOR default (inert at
                                // mass 0), the bank's kAtomLevelDefault convention
    float atomMass   = 0.f;     // atom mass m (neutral 0; eval clamps to ≤ 0.9)
    float gapLoc     = 0.5f;    // gap location a — INTERIOR default (inert at
                                // width 0), the bank's kGapLocDefault convention
    float gapSize    = 0.f;     // gap size b−a (neutral 0; eval clamps b ≤ 1)
    bool  delegated  = false;   // the model's fifth element; engine-inert (the
                                // transform reads only the numbers above)

    // THE IDENTITY PREDICATE — the load-bearing law of the Stage-1 gate: a set that
    // satisfies this maps u to u BIT-EXACTLY through the primitive (the neutral
    // early-outs), so the caller may — and Monsoon does — take the raw path verbatim.
    // Field-by-field: the same neutrality the composed chain's early-outs test.
    bool isIdentity() const {
        return std::holds_alternative<dotModular::IdentityWarp>(f)
            && w == 0.f && tilt == 0.f && atomMass == 0.f && gapSize == 0.f;
    }

    // The set AS a resolved cell — the primitive's input shape (the same struct the
    // lane draws resolve onto; field order = WarpCell's own). WarpCell is complete
    // here: WarpF.hpp's bottom include pulls SandsModifierResolver.hpp (the guard-safe
    // cycle that header documents).
    redDot::WarpCell cell() const {
        return redDot::WarpCell{f, w, tilt, atomLevel, atomMass, gapLoc, gapSize};
    }

    bool operator==(const PhaseDistSet& o) const {
        return f == o.f && w == o.w && tilt == o.tilt && atomLevel == o.atomLevel
            && atomMass == o.atomMass && gapLoc == o.gapLoc && gapSize == o.gapSize
            && delegated == o.delegated;
    }
    bool operator!=(const PhaseDistSet& o) const { return !(*this == o); }
};

// ── the store ────────────────────────────────────────────────────────────────────────
struct PhaseDistStore {
    static constexpr int kSets = 8;   // the model's EIGHT parameter sets (1..8 outside)

    // The sets. Default construction = eight identity sets (the Stage-1 gate's base).
    // std::array (the array-guard's P-B form — no bare C-array members).
    std::array<PhaseDistSet, kSets> sets_{};

    // The write/snapshot lock. mutable so a const store can hand out snapshots.
    mutable std::mutex mu_;

    // ── the WRITE path (the UI thread — knobs, menus, JSON load; also the tests) ──
    // ONE call per committed write of a whole set. Under the mutex: the write is
    // atomic per set (a concurrent snapshot sees either the whole old set or the
    // whole new one, never a mix), and the reader's next block-boundary snapshot
    // picks it up. i is clamped (a hostile/legacy index writes nothing).
    void writeSet(int i, const PhaseDistSet& s) {
        if (i < 0 || i >= kSets) return;
        std::lock_guard<std::mutex> lk(mu_);
        sets_[i] = s;
    }

    // ── the READER's block start (the engine thread, ONCE per audio block) ────────
    // Copies the whole set under the mutex into the caller's working copy. The copy
    // is the reader's private view for the whole block — the mechanism itself.
    void snapshotSet(int i, PhaseDistSet& out) const {
        if (i < 0 || i >= kSets) { out = PhaseDistSet{}; return; }
        std::lock_guard<std::mutex> lk(mu_);
        out = sets_[i];
    }

    // The store stays a plain aggregate: no copy of the mutex — the expander embeds
    // it by value and never copies; the tests construct it in place.
    PhaseDistStore() = default;
    PhaseDistStore(const PhaseDistStore&) = delete;
    PhaseDistStore& operator=(const PhaseDistStore&) = delete;

    // ── persistence (the expander's dataToJson/dataFromJson — additive key, the
    // house missing-key-⟹-defaults contract: a patch without the key loads eight
    // identity sets; a malformed slot loads as identity, never as garbage) ────────
    // NOTE the WarpF member serialises through the slot codec (warpToJson). A drawn
    // slot cannot ENTER the store (the expander's commit path lowers through
    // lowerWarp, which maps "drawn"/unknown to IdentityWarp — the phase tier has no
    // drawn-curve pool), and warpToJson of a parametric is the curve's own codec, so
    // the round trip is exact for everything that can be stored.
    json_t* toJson() const {
        json_t* arr = json_array();
        std::lock_guard<std::mutex> lk(mu_);
        for (int i = 0; i < kSets; ++i) {
            const PhaseDistSet& s = sets_[i];
            json_t* o = json_object();
            json_object_set_new(o, "f", warpToJson(s.f));
            json_object_set_new(o, "w", json_real((double)s.w));
            json_object_set_new(o, "tilt", json_real((double)s.tilt));
            json_object_set_new(o, "atomLevel", json_real((double)s.atomLevel));
            json_object_set_new(o, "atomMass", json_real((double)s.atomMass));
            json_object_set_new(o, "gapLoc", json_real((double)s.gapLoc));
            json_object_set_new(o, "gapSize", json_real((double)s.gapSize));
            json_object_set_new(o, "delegated", json_boolean(s.delegated));
            json_array_append_new(arr, o);
        }
        return arr;
    }

    // RESETS-then-overwrites (the WarpBank::parse convention — deterministic, never
    // merges into stale state). Returns true when the key was present AND full-length.
    bool fromJson(json_t* root) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            for (int i = 0; i < kSets; ++i) sets_[i] = PhaseDistSet{};
        }
        if (!root || !json_is_array(root)) return false;
        const size_t n = json_array_size(root);
        bool full = n >= (size_t)kSets;
        const size_t m = n < (size_t)kSets ? n : (size_t)kSets;
        for (size_t i = 0; i < m; ++i) {
            json_t* o = json_array_get(root, i);
            if (!json_is_object(o)) continue;
            PhaseDistSet s{};
            json_t* jf = json_object_get(o, "f");
            if (jf) s.f = dotModular::warpFromJson(jf);   // malformed ⟹ IdentityWarp (the codec's contract)
            auto num = [&](const char* k, float& dst, float lo, float hi) {
                json_t* jv = json_object_get(o, k);
                if (jv && json_is_number(jv)) {
                    const double v = json_number_value(jv);
                    if (v == v) dst = (float)std::max((double)lo, std::min((double)hi, v));  // NaN ⟹ keep default
                }
            };
            num("w",          s.w,         -1.f, 1.f);
            num("tilt",       s.tilt,      -1.f, 1.f);
            num("atomLevel",  s.atomLevel,  0.f, 1.f);
            num("atomMass",   s.atomMass,   0.f, 1.f);
            num("gapLoc",     s.gapLoc,     0.f, 1.f);
            num("gapSize",    s.gapSize,    0.f, 1.f);
            json_t* jd = json_object_get(o, "delegated");
            if (jd) s.delegated = json_boolean_value(jd) != 0;
            std::lock_guard<std::mutex> lk(mu_);
            sets_[(int)i] = s;
        }
        return full;
    }
};

} // namespace redDot
