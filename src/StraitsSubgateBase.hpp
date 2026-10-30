#pragma once
// ── StraitsSubgateBase + subpanels — the subgate consumer-group surfaces ─────
// CC_PROMPT_STRAITS_SUBGATE_KNOBS_SPEC on its STRAITS home (Rodney: nothing on
// the distribution expander — Sands/distribution is the DRAW-generation plane,
// Straits the decision plane reacting to the draws; "Only panel changes are on
// the new straits expander base and new subpanels").
//
//   StraitsSubgateBase        — the GLOBAL tier: one store-backed knob per
//                               subgate decision ({RATCHET,GHOST}×{REST,LEG,
//                               ADV,ACC}), one mod jack per knob (the Causeway
//                               path — a Straits knob without its mod path is a
//                               modulation-layer seam), the per-decision source
//                               SWITCH as a context menu (follow main / global,
//                               default follow main), and the SUBPANEL WHEEL:
//                               it spawns/owns the per-voice subpanels exactly
//                               the way StraitsBase owns its lanes (stable
//                               owned-ID list + unconditional reposition — the
//                               two Straits invariants, task-84 law).
//   StraitsSubgatePanel{Ratchet,Ghost} — the per-voice tier (tier 1): 16 voice
//                               rows × the 4 decisions, store-backed knobs
//                               (touch = claim — the ownActive opt-in), one
//                               16-channel mod jack per decision column
//                               (ch v = voice slot v). Registered + HIDDEN —
//                               base-spawned only (the StraitsLane law).
//
// DE-PARAM LAW: every control is store-backed (StoreKnob onto Monsoon's
// editor.subgate — the bank holds the authored cells; PARAM_CLASSIFICATION.md).
// The bank lives in MONSOON (the engine host both families attach to); these
// surfaces address it through the Monsoon accessors via redDot::lookupMonsoon.
// Defaults are the byte-identity pin: follow-main with nothing authored resolves
// to the voice's main thresholds — until a knob is touched or a switch flips,
// every subgate decision plays exactly as before.
#include <rack.hpp>
#include <atomic>
#include <string>
#include <vector>

#include "Monsoon.hpp"
#include "ui/ModuleLookupCore.hpp"   // THE lookup authority: the topology epoch (onExpanderChange bump)

extern rack::Model* modelStraitsSubgateBase;
extern rack::Model* modelStraitsSubgatePanelRatchet;
extern rack::Model* modelStraitsSubgatePanelGhost;

namespace StraitsSubgateIds {
    namespace SK = dotModular::SubgateKnobs;

    // The BASE's mod inputs: one per GLOBAL threshold, mono, ±10V → ±1.0 delta
    // (×0.1 — the distribution expander's jack convention). Level-major order.
    enum InputIds {
        MOD_GLOBAL_0 = 0,
        NUM_MOD_INPUTS = SK::LEVELS * SK::DECS
    };
    inline int globalModId(int level, int dec) { return level * SK::DECS + dec; }

    // The SUBPANEL's mod inputs: one per DECISION column, 16-channel poly,
    // ch v = voice storage slot v (0 = V1's mono row). ±10V → ±1.0 delta.
    enum PanelInputIds {
        MOD_PANEL_0 = 0,
        PANEL_NUM_MOD_INPUTS = SK::DECS
    };

    // Panel slugs (persistence + spawn dispatch — the StraitsBase lane-slug shape).
    inline const char* panelSlug(int level) {
        return level == SK::GHOST ? "GHOST" : "RATCHET";
    }
}

// The family predicate (seam detection + the resnap chain checks). Subgate chain
// only — the StraitsBase lane chain has its OWN predicate; the two families dock
// as separate runs (a subgate surface parked inside a lane run gets shoved out
// by the lanes' resnap, and vice versa — deliberate, mirrors foreign inserts).
inline bool isStraitsSubgateChainModel(rack::Model* m) {
    return m == modelStraitsSubgateBase
        || m == modelStraitsSubgatePanelRatchet
        || m == modelStraitsSubgatePanelGhost;
}

// ── the BASE module — globals + switches + the owned-subpanel list ──────────
struct StraitsSubgateBaseModule : rack::engine::Module {
    // THE OWNED LIST (task-84 invariant 1): grown only by attach (menu spawn,
    // spawn-adopt, one-time restore adopt), shrunk only by deliberate remove;
    // NOTHING re-derives it from the live chain.
    std::vector<int64_t> orderedPanelIds_;
    std::vector<std::string> pendingPanels_;   // slugs to spawn on the widget's first step (restore)
    bool needsResnap = false;                  // consumed + cleared by the widget's step (UI thread)
    // RESNAP FAST PATH (the StraitsBase hook): Rack's own adjacency-change signal.
    std::atomic<bool> topologyDirty{false};

    // The last-published global deltas (change-gated publishing; audio thread only).
    float lastGlob_[dotModular::SubgateKnobs::LEVELS][dotModular::SubgateKnobs::DECS] = {};

    StraitsSubgateBaseModule() {
        // THE b0012162 LAW: config(numParams, numInputs, numOutputs, numLights) —
        // the mod jacks are INPUTS (the browser-crash assert was a mis-slotted 42).
        config(0, StraitsSubgateIds::NUM_MOD_INPUTS, 0, 0);
        // The jack tooltips (the Causeway mod path's labels — the captions are the
        // one authority shared with the panels' strips).
        for (int l = 0; l < dotModular::SubgateKnobs::LEVELS; ++l)
            for (int d = 0; d < dotModular::SubgateKnobs::DECS; ++d)
                configInput(StraitsSubgateIds::globalModId(l, d),
                    std::string(dotModular::SubgateKnobs::LEVEL_CAPTIONS[l]) + " "
                    + dotModular::SubgateKnobs::DEC_CAPTIONS[d] + " mod (±10V → ±1 threshold)");
    }

    void process(const ProcessArgs& args) override;   // .cpp — the global-delta publish + heartbeat
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
        topologyDirty.store(true, std::memory_order_relaxed);
    }

    // Persistence: the OWNED subpanel list by slug (the straitLanes shape). The
    // knob values live in MONSOON's bank (MonsoonPersistenceManager); the panels
    // are registered+hidden so Rack restores them natively — the base adopts them
    // on the widget's first step and re-spawns any the restore missed.
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;
};

// ── the SUBPANEL module — the per-voice tier for ONE level ───────────────────
struct StraitsSubgatePanelModule : rack::engine::Module {
    int level = 0;   // dotModular::SubgateKnobs::RATCHET/GHOST — set by the per-level subclass
    float lastOwn_[dotModular::SubgateKnobs::VOICES][dotModular::SubgateKnobs::DECS] = {};
    std::atomic<bool> needsResnap{false};   // writers: widget drag-end (UI) + the left-link check (engine); reader/clearer: the base's step

    StraitsSubgatePanelModule() {
        config(0, StraitsSubgateIds::PANEL_NUM_MOD_INPUTS, 0, 0);
    }

    // Per-level ctor finish: the decision-column jack tooltips.
    void configPanelInputs() {
        for (int d = 0; d < dotModular::SubgateKnobs::DECS; ++d)
            configInput(StraitsSubgateIds::MOD_PANEL_0 + d,
                std::string("Per-voice ") + dotModular::SubgateKnobs::DEC_CAPTIONS[d]
                + " mod (16ch: ch v = voice v; ±10V → ±1 threshold)");
    }

    void process(const ProcessArgs& args) override;   // .cpp — the per-voice delta publish + heartbeat
    // The lane's left-link fast path: a mid-chain insertion never changes the
    // BASE's own expanders, so it fires HERE (the StraitsLaneExpander hook
    // verbatim, family-parameterised).
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
        rack::Module* left = leftExpander.module;
        if (!left || !isStraitsSubgateChainModel(left->model))
            needsResnap.store(true, std::memory_order_relaxed);
    }
};
struct StraitsSubgateRatchetModule : StraitsSubgatePanelModule {
    StraitsSubgateRatchetModule() { level = dotModular::SubgateKnobs::RATCHET; configPanelInputs(); }
};
struct StraitsSubgateGhostModule : StraitsSubgatePanelModule {
    StraitsSubgateGhostModule() { level = dotModular::SubgateKnobs::GHOST; configPanelInputs(); }
};
