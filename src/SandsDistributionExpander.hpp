// SandsDistributionExpander — the distribution sub-expander (dual role).
// Spec: docs/design/CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC.md (branch off
// lane-expander-refactor when scheduled — THIS is that build).
//
// THE ORGANIZING PRINCIPLE (spec §1): without this expander the Sands body is the
// pure moving-average GAUSSIAN COPULA (λ=0, no tail dependence); with it, warp,
// tilt, atoms (ℓ,m) and the shape params + MOD JACKS unlock the full
// reachable-structures space (REACHABLE_STRUCTURES_THEORY.md). ALL
// distribution-shaping controls live HERE — warp included (warp is already
// "leaving pure Gaussian", so it belongs with the layer the expander unlocks).
//
// DUAL ROLE BY HOST (spec §3) — ONE panel design, role by attachment:
//   - leftward host is the MACRO visual (modelStraitsSandsMacroVisual) →
//     CROSS-VOICE role: the knobs author Macro's per-lane global defaults
//     (editor.warp.globalWarp*) — the delegation source; the MOD jacks become
//     PER-VOICE SENDS on the Macro→East send bus (the decided §"MACRO:
//     distribution param mods BECOME per-voice sends").
//   - otherwise (East visual / Monsoon / standalone) → PER-VOICE role: the knobs
//     author the SHARED-SELECTED voice's own slots (editor.warp.warp*, the slot
//     the Sands main panel's voice tabs select — read LIVE each use, never
//     cached: VoiceResolver/PatternEngine::displayedVoice is the single source
//     of truth for voice identity/addressing); the MOD jacks write that voice's
//     own-mod plane.
// Controls are IDENTICAL in both roles; only the SCOPE differs (the spec's
// "one mechanism, addressing by context").
//
// SNAPPING (spec §HARD REQUIREMENT): the Straits subpanel pattern — chain via
// leftExpander from the host, resnap force-reposition (setModulePosForce) on
// drag-end + the periodic adjacency poll on the HOST side (ui/SandsSnap.hpp —
// the StraitsBase cluster, family-parameterised), hardening: the subpanel
// flags its own drag-end + left-link breaks (StraitsLaneExpander idiom).
// Attachment is RIGHT of the host main panel (the Straits lanes chain right —
// same direction, no flip).
//
// DE-PARAM LAW: every control is store-backed (StoreKnob onto Monsoon's
// editor.warp — the NUM_PARAMS_MIGRATION convention). The module exposes NO
// host params: config(0, NUM_MOD_INPUTS, 0, 0) — the 42 are the MOD jack CV
// INPUTS (one per param per lane; "modulation sent to a param a distribution
// doesn't use is HARMLESS — a no-op" — the spec's unused-param ruling).
// NB the Rack config() signature is (numParams, numInputs, numOutputs,
// numLights) — an earlier draft passed the 42 in the numOutputs slot, which
// left inputs.size()==0 and made the ctor's first configInput() fire Rack's
// inputInfo() assert (portId < inputs.size()) — the browser-add crash on
// Rodney's Windows build (wassert -> abort under ModelBox::onButton).
#pragma once
#include <rack.hpp>
#include <atomic>
#include "Monsoon.hpp"
#include "dsp/LaneMapping.hpp"   // SandsLaneNames (the canonical lane-name table)
#include "ui/ModuleLookupCore.hpp"  // the topology epoch + lookupMemoizedT
#include "dsp/SandsDistributionRole.hpp"  // the dual-role walk (rack-free, unit-tested)
#include "ui/SandsSnap.hpp"      // the guest mixin (the host's snap harvest reads it)

using namespace rack;

namespace SandsDistributionIds {
    // ── the param set per lane (spec §2: five general params + warp) ──────────
    // 0 P1, 1 P2 — the per-kind distribution shape params (the popup's p1/p2;
    //   windows are per-kind — WarpCurvePopup's spec tables are the authority);
    //   kinds without the param make the control a harmless no-op (the spec's
    //   unused-param ruling — no special-casing).
    // 2 TILT — the rail tilt p (every distribution has it; warpStages::kTiltMax).
    // 3 LEVEL — atom location ℓ ∈ [0,1] (interior default 0.5; inert at m=0).
    // 4 MASS — atom size m ∈ [0, kAtomMassMax] (0 = the neutral body).
    // 5 WARP — the w-mix depth ∈ [0,1] (0 = pure member curve).
    static constexpr int LANES           = 7;   // dotModular::SandsGrid::POLY_LANES
    static constexpr int PARAMS_PER_LANE = 6;
    static constexpr int NUM_MOD_INPUTS  = LANES * PARAMS_PER_LANE;   // 42
    inline int warpModId(int lane, int p) { return lane * PARAMS_PER_LANE + p; }
    // the SVG anchor prefixes (panel_src/gen_sands_distribution.py emits them)
    static const char* const KNOB_ANCHOR = "param_dexp_";
    static const char* const MOD_ANCHOR  = "input_dexpmod_";
    static const char* const FOCUS_ANCHOR = "param_dfocus_";
    // column captions (widget-drawn lettering — nanosvg drops <text>)
    static const char* const PARAM_CAPTIONS[PARAMS_PER_LANE] = {
        "P1", "P2", "TILT", "LEVEL", "MASS", "WARP",
    };
} // namespace SandsDistributionIds

// (the role walk lives in dsp/SandsDistributionRole.hpp — rack-free and
// unit-tested there; the module feeds it the production predicates below.)
struct SandsDistributionExpander : rack::Module, redDot::SandsSnapGuest {

    // ── the role (host detection, memoized by the topology epoch) ────────────
    std::atomic<bool> macroRole{false};   // CROSS-VOICE (Macro host) vs PER-VOICE

    // ── snapping (the StraitsLane idiom: the subpanel FLAGS, the host decides) ─
    // The flag lives in SandsSnapGuest (needsResnap): set by the widget's
    // onDragEnd (the hardening's "resnap also triggers on the SUBPANEL's own
    // drag-end") and by onExpanderChange when the left link is not family.
    // Consumed by the HOST's step() poll/consumer (ui/SandsSnap.hpp).

    // ── CROSS-VOICE publisher state: the deltas themselves live in MONSOON's bank
    // (WarpBank::distGlobalDelta / distOwnDelta — see the send-bus plumbing block
    // below for why the bank owns them, not this module); here only the
    // change-gate shadows (what THIS expander last published) + the bookkeeping.
    // POST-form only: the distribution expander's jacks are raw unit CV (no
    // Macro-side attenuverter), so there is no pre/post pair to carry — the
    // per-voice scaling lives in the receiving side's send amounts (the Macro
    // send subpanel, when it lands).

    // ── PER-VOICE bookkeeping: the last selected slot whose own-mod row this
    // expander wrote. On a voice-switch the previously written row is CLEARED
    // (a CV modulating voice 3 must not keep modulating it after the user
    // selects voice 4 — the mod addresses the shared selection, so it follows
    // it; nothing stale stays behind).
    int lastOwnSlot_ = -1;

    // ── THE MOD-JACK PLUMBING (the send-bus commit — spec §"MACRO: distribution
    // param mods BECOME per-voice sends"): process() publishes the 42 live jack
    // deltas into Monsoon's bank planes (WarpBank::distGlobalDelta for the
    // CROSS-VOICE role — every voice receives them, delegated or not;
    // distOwnDelta[slot] rows for the PER-VOICE role — the shared-selected
    // voice's own cells) and performs the matching scoped mirror rebuilds inline
    // (audio thread — the same thread Monsoon::process rebuilds on, so warped_
    // keeps its single writer). Publish is change-gated (kPubEps on param units —
    // a steady CV costs nothing; the publish overwrites, nothing latches), the
    // heartbeat bumps every block, and a frozen heartbeat is Monsoon's watchdog
    // signal to drop the plane (module deleted/bypassed). CV scale: volts/10 —
    // the CvTerm unit convention (±10 V = a full unit of the param).
    static constexpr float kPubEps = 1e-4f;
    float lastGlob_[SandsDistributionIds::LANES][SandsDistributionIds::PARAMS_PER_LANE] = {};
    float lastOwn_[SandsDistributionIds::LANES][SandsDistributionIds::PARAMS_PER_LANE]  = {};
    void publishMacroSends(Monsoon* m);
    void publishOwnMods(Monsoon* m);

    SandsDistributionExpander() {
        config(0, SandsDistributionIds::NUM_MOD_INPUTS, 0, 0);
        // configInput with the canonical lane names + the param captions — the
        // tooltips the DAW/host sees for the 42 mod jacks.
        static const char* const* laneNames = dotModular::SandsLaneNames::EDITOR;
        for (int l = 0; l < SandsDistributionIds::LANES; ++l)
            for (int p = 0; p < SandsDistributionIds::PARAMS_PER_LANE; ++p)
                configInput(SandsDistributionIds::warpModId(l, p),
                    std::string(laneNames[l]) + " distribution "
                    + SandsDistributionIds::PARAM_CAPTIONS[p] + " mod");
    }

    // The role walk, memoized: raw walks must not run per-sample (the L5 law) —
    // lookupMemoizedT caches per (self, epoch) and the epoch moves ONLY on
    // onExpanderChange events, so adjacency changes re-resolve immediately and
    // steady state costs one compare.
    bool resolveMacroRole();
    void process(const ProcessArgs& args) override;

    // ── topology events: bump the epoch (the ONLY legal bump site) + the
    // left-link hardening check (StraitsLaneExpander's hook, family swapped).
    static bool isFamilyModel(const rack::Model* m);
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
        rack::Module* left = leftExpander.module;
        if (!(left && isFamilyModel(left->model)))
            needsResnap.store(true, std::memory_order_relaxed);
    }
};
