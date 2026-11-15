// SandsPhaseDistSubpanel — the EXPANDER MODEL's phase-distortion authoring panel
// (PHASE_DISTRIBUTION_SPEC §14 store + docs/design/CC_PROMPT_PHASE_DISTORTION_PANEL.md
// TASK 1). Built alongside the standalone module (§13 build B) — neither replaces the
// other; the expander build A stays untouched (the prompt's DO-NOT).
//
// ONE snapped subpanel AUTHORS the EIGHT PARAMETER SETS — and OWNS them
// (CC_FIX_PHASE_STORE_ON_SUBPANEL): the PhaseDistStore is THIS module's own member,
// the subpanel is self-contained like the standalone PhaseDistortion module already
// is. No Distribution expander is required to author or use phase sets — the old
// arrangement (the store a member of the lowest-id SandsDistributionExpander, the
// panel resolving it rack-wide) was a surprise dependency between two modules that
// share only the distribution MATHS, not a reason to share a MODULE: Distribution
// modifies probabilities; phase distortion modifies transport. Monsoon's rack-wide
// scan resolves THIS panel model as the source (the phase AUTHORING surface, so
// requiring IT is not a surprise); several subpanels ⟹ the lowest module id wins
// (one store, one reading — the same determinism rule as before).
//
// The panel is modelled on the Sands Distribution module's own grid: EIGHT ROWS
// (one per set) instead of seven lanes, the SAME param axis (P1 P2 TILT LEVEL MASS |
// GAPLOC GAPSIZE | WARP — the gap pair beside the atom pair, WARP trailing), the
// same StoreKnob art, a focus button per row, and ONE permanent
// DistributionCurveWidget showing the SELECTED row's effective curve (spec §10 —
// the reading follows the sign of w). Header/footer lettering: "PHASE DISTORTION"
// + the selected set number.
//
// DE-PARAM LAW: the module exposes NOTHING — config(0, 0, 0, 0); every control is a
// store-backed StoreKnob onto THIS module's own PhaseDistStore (the family's
// convention).
// DELEGATION: OMITTED from the rows (the panel prompt's DECISION TO CONFIRM #1,
// Rodney's own proposal — a phase set has no Macro/East to delegate to; the store's
// `delegated` field stays, engine-inert, persisted by the store codec).
//
// SNAPPING: the SandsSnap cluster (the Macro visual seats this panel beside its
// subpanel run — the context-menu dock toggle + the adopt path, the send subpanel's
// idiom). NO AUDIO-THREAD WORK: the knobs write THIS module's store directly
// (writeSet — atomic per set); the Monsoons' per-block snapshots pick changes up at
// their next boundary (the landed §14 mechanism, untouched — it moved here WITH the
// store: mutex-guarded writes, per-block snapshot reads, the same codec).
#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"   // the family model externs (modelSandsDistributionExpander...)
#include "dsp/PhaseDistStore.hpp"   // the set shape (shared with A and B — untouched)
#include "ui/SandsSnap.hpp"              // the guest mixin (the host's harvest reads it)
#include "ui/ModuleLookupCore.hpp"       // the topology epoch (the epoch bump stays:
                                         // Monsoon's owner scan is epoch-memoized)

using namespace rack;

namespace SandsPhaseDistIds {
    static constexpr int SETS     = redDot::PhaseDistStore::kSets;   // 8 rows
    static constexpr int N_PARAMS = 8;   // p1, p2, tilt, level(ℓ), mass(m), warp(w), gaploc(a), gapsize(b−a)
    // the expander's VISUAL order (the gap pair beside the atom pair, WARP trailing)
    static constexpr int VISUAL_ORDER[N_PARAMS] = {0, 1, 2, 3, 4, 6, 7, 5};
    static const char* const PARAM_CAPTIONS[N_PARAMS] = {
        "P1", "P2", "TILT", "LEVEL", "MASS", "WARP", "GAPLOC", "GAPSIZE",
    };
    // the SVG anchor prefixes (panel_src/gen_sands_phase_dist.py emits them;
    // test/audit_anchor_bind.py gates the binds)
    static const char* const KNOB_ANCHOR  = "param_pds_";    // + set "_" param
    static const char* const FOCUS_ANCHOR = "param_pdsfocus_";  // + set
}

struct SandsPhaseDistSubpanel : rack::Module, redDot::SandsSnapGuest {

    // ── THE PHASE-DISTORTION STORE — THIS module's own state
    // (CC_FIX_PHASE_STORE_ON_SUBPANEL): EIGHT parameter sets — (warp, tilt, gap,
    // atom, delegation) each — that ANY Monsoon in the network pulls by its two
    // indices (phaseSet 1..8 / subphaseSet 1..8, Monsoon-side params). The store is
    // rack-free (dsp/PhaseDistStore.hpp) and unit-tested directly — the TYPE is
    // untouched; only its OWNER moved (off the SandsDistributionExpander, which
    // keeps its distribution role: probabilities). The consistency mechanism rides
    // with it unchanged: the UI writes under the store mutex (writeSet — atomic per
    // set), each Monsoon copies its set ONCE per audio block under the same mutex
    // (snapshotSet) and reads its private copy for the whole block. Nothing is
    // pushed — the subpanel produces no signal.
    redDot::PhaseDistStore phaseDistStore;

    SandsPhaseDistSubpanel() {
        // the de-param law: no params, no jacks — the whole panel is store-backed
        // knobs onto THIS module's own store (the family's convention).
        config(0, 0, 0, 0);
    }

    // ── persistence: the sets ride THIS module's patch state under the SAME
    // additive key ("phaseDistSets" — the key moves with the store; missing or
    // malformed ⟹ the eight identity defaults, the house contract). Patches saved
    // before the store moved carried the key on the Distribution expander's JSON,
    // where nothing reads it anymore — those sets need re-authoring here (flagged).
    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "phaseDistSets", phaseDistStore.toJson());
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (!root) return;
        json_t* j = json_object_get(root, "phaseDistSets");
        if (j) phaseDistStore.fromJson(j);   // absent/malformed ⟹ the identity defaults survive
    }

    // ── the Sands subpanel family (seam continuity + the left-link check): the
    // two host visuals + the distribution expander + both subpanels. Seating is
    // the HOSTS' decision (Macro seats this panel; East is unchanged).
    static bool isFamilyModel(const rack::Model* m);

    // ── topology events: bump the epoch (the ONLY legal bump site) + the
    // left-link hardening check (the StraitsLane idiom, family swapped).
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
        rack::Module* left = leftExpander.module;
        if (!(left && isFamilyModel(left->model)))
            needsResnap.store(true, std::memory_order_relaxed);
    }
};
