// SandsPhaseDistSubpanel — the EXPANDER MODEL's phase-distortion authoring panel
// (PHASE_DISTRIBUTION_SPEC §14 store + docs/design/CC_PROMPT_PHASE_DISTORTION_PANEL.md
// TASK 1). Built alongside the standalone module (§13 build B) — neither replaces the
// other; the expander build A stays untouched (the prompt's DO-NOT).
//
// ONE snapped subpanel authors the EIGHT PARAMETER SETS the expander's PhaseDistStore
// holds — the sets every Monsoon in the network pulls by its two indices. The panel is
// modelled on the Sands Distribution module's own grid: EIGHT ROWS (one per set)
// instead of seven lanes, the SAME param axis (P1 P2 TILT LEVEL MASS | GAPLOC GAPSIZE
// | WARP — the gap pair beside the atom pair, WARP trailing), the same StoreKnob art,
// a focus button per row, and ONE permanent DistributionCurveWidget showing the
// SELECTED row's effective curve (spec §10 — the reading follows the sign of w).
// Header/footer lettering: "PHASE DISTORTION" + the selected set number.
//
// THE STORE RESOLUTION: the panel edits the store the Monsoons ACTUALLY read — the
// SAME rack-wide first-found scan Monsoon's updateExpanderPointers runs (the LOWEST
// module id among the SandsDistributionExpanders — deterministic under any module
// order; one store, one reading). A left-walk would risk editing a store instance no
// Monsoon pulls when two expanders exist. Memoized by the topology epoch (the lookup
// authority's law: the scan runs on a topology event, never per frame). No owner
// reachable ⟹ the knobs are inert (StoreKnob's null-store path) — the panel is a
// working UI wherever the family expander exists.
//
// DE-PARAM LAW: the module exposes NOTHING — config(0, 0, 0, 0); every control is a
// store-backed StoreKnob onto the owner's PhaseDistStore (the family's convention).
// DELEGATION: OMITTED from the rows (the panel prompt's DECISION TO CONFIRM #1,
// Rodney's own proposal — a phase set has no Macro/East to delegate to; the store's
// `delegated` field stays, engine-inert, persisted by the store codec).
//
// SNAPPING: the SandsSnap cluster (the Macro visual seats this panel beside its
// subpanel run — the context-menu dock toggle + the adopt path, the send subpanel's
// idiom). NO AUDIO-THREAD WORK: the knobs write the owner's store directly
// (writeSet — atomic per set); the Monsoons' per-block snapshots pick changes up at
// their next boundary (the landed §14 mechanism, untouched).
#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"   // the family model externs (modelSandsDistributionExpander...)
#include "dsp/PhaseDistStore.hpp"   // the set shape (shared with A and B — untouched)
#include "ui/SandsSnap.hpp"              // the guest mixin (the host's harvest reads it)
#include "ui/ModuleLookupCore.hpp"       // the topology epoch + lookupMemoizedT

using namespace rack;

struct SandsDistributionExpander;   // the store's owner (full type in its header, included by the cpp)

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

    // ── store-owner badge (UI-only): whether a store owner is reachable. Plain
    // bool: the widget's step() is the only writer and the only reader's thread.
    bool storeOwnerLive = false;

    SandsPhaseDistSubpanel() {
        // the de-param law: no params, no jacks — the whole panel is store-backed
        // knobs onto the OWNER's store (the family's convention).
        config(0, 0, 0, 0);
    }

    // ── the store owner: the rack-wide LOWEST-ID SandsDistributionExpander — the
    // SAME scan Monsoon's updateExpanderPointers runs, so this panel and the
    // Monsoon network always address the SAME store instance. Memoized by the
    // topology epoch (the scan runs once per topology event, never per frame —
    // the L5 law; the resolveFollowedT precedent is a getModuleIds walk too).
    SandsDistributionExpander* resolveStoreOwner();

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
