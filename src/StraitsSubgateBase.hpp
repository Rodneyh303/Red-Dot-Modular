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
//   StraitsSubgatePanel{Level}{Decision} — the per-voice tier (tier 1), ONE
//                               panel per (LEVEL × DECISION) — CC_PROMPT_FIX_
//                               SUBGATE_PANELS, Rodney DECIDED option (a):
//                               {RATCHET,GHOST} × {REST,LEGATO,ACCENT,ADVANCE}
//                               = EIGHT panels, each 16 voices × ONE decision
//                               (the lane-expander law — one panel = one bank
//                               of 16, the settled 6/6/4 165-wide layout from
//                               gen_straits_lane.py; NO crammed multi-decision
//                               panel). Store-backed knobs (touch = claim —
//                               the ownActive opt-in), one 16-channel mod jack
//                               per panel (ch v = voice slot v). Registered +
//                               HIDDEN — base-spawned only (the StraitsLane
//                               law).
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
extern rack::Model* modelStraitsSubgatePanelRatchetRest;
extern rack::Model* modelStraitsSubgatePanelRatchetLegato;
extern rack::Model* modelStraitsSubgatePanelRatchetAccent;
extern rack::Model* modelStraitsSubgatePanelRatchetAdvance;
extern rack::Model* modelStraitsSubgatePanelGhostRest;
extern rack::Model* modelStraitsSubgatePanelGhostLegato;
extern rack::Model* modelStraitsSubgatePanelGhostAccent;
extern rack::Model* modelStraitsSubgatePanelGhostAdvance;

namespace StraitsSubgateIds {
    namespace SK = dotModular::SubgateKnobs;

    // The BASE's mod inputs: one per GLOBAL threshold, mono, ±10V → ±1.0 delta
    // (×0.1 — the distribution expander's jack convention). Level-major order.
    enum InputIds {
        MOD_GLOBAL_0 = 0,
        NUM_MOD_INPUTS = SK::LEVELS * SK::DECS
    };
    inline int globalModId(int level, int dec) { return level * SK::DECS + dec; }

    // The SUBPANEL's mod input: ONE 16-channel poly jack per panel (ch v = voice
    // storage slot v — 0 = V1's mono row). ±10V → ±1.0 delta. Option (a): one
    // decision per panel, so one jack per panel.
    enum PanelInputIds {
        MOD_PANEL_0 = 0,
        PANEL_NUM_MOD_INPUTS = 1
    };

    // Panel slugs (persistence + spawn dispatch — the StraitsBase lane-slug
    // shape), now per (level × decision). Old saves carried the per-LEVEL slugs
    // ("RATCHET"/"GHOST" — the crammed panels, retired): parsePanelSlug returns
    // false for them so a restore SKIPS the legacy entries gracefully.
    inline const char* panelSlug(int level, int dec) {
        switch (level * SK::DECS + dec) {
            case SK::RATCHET * SK::DECS + 0: return "RATCHET_REST";
            case SK::RATCHET * SK::DECS + 1: return "RATCHET_LEGATO";
            case SK::RATCHET * SK::DECS + 2: return "RATCHET_ADVANCE";
            case SK::RATCHET * SK::DECS + 3: return "RATCHET_ACCENT";
            case SK::GHOST   * SK::DECS + 0: return "GHOST_REST";
            case SK::GHOST   * SK::DECS + 1: return "GHOST_LEGATO";
            case SK::GHOST   * SK::DECS + 2: return "GHOST_ADVANCE";
            case SK::GHOST   * SK::DECS + 3: return "GHOST_ACCENT";
        }
        return "";
    }
    inline bool parsePanelSlug(const std::string& s, int& levelOut, int& decOut) {
        if (s == "RATCHET_REST")   { levelOut = SK::RATCHET; decOut = 0; return true; }
        if (s == "RATCHET_LEGATO") { levelOut = SK::RATCHET; decOut = 1; return true; }
        if (s == "RATCHET_ADVANCE"){ levelOut = SK::RATCHET; decOut = 2; return true; }
        if (s == "RATCHET_ACCENT") { levelOut = SK::RATCHET; decOut = 3; return true; }
        if (s == "GHOST_REST")     { levelOut = SK::GHOST;   decOut = 0; return true; }
        if (s == "GHOST_LEGATO")   { levelOut = SK::GHOST;   decOut = 1; return true; }
        if (s == "GHOST_ADVANCE")  { levelOut = SK::GHOST;   decOut = 2; return true; }
        if (s == "GHOST_ACCENT")   { levelOut = SK::GHOST;   decOut = 3; return true; }
        return false;   // legacy per-LEVEL slugs ("RATCHET"/"GHOST") land here — skipped
    }
    // The panel SVG key (res/panels/StraitsSubgatePanel_{level}_{dec}_{theme}.svg).
    inline std::string panelSvgKey(int level, int dec) {
        return std::string(level == SK::GHOST ? "ghost" : "ratchet") + "_"
             + (dec == 0 ? "rest" : dec == 1 ? "legato" : dec == 2 ? "advance" : "accent");
    }
}

// The family predicate (seam detection + the resnap chain checks + the suite
// scan's hop-through list). Subgate chain only — the StraitsBase lane chain has
// its OWN predicate; the two families dock as separate runs (a subgate surface
// parked inside a lane run gets shoved out by the lanes' resnap, and vice versa
// — deliberate, mirrors foreign inserts).
inline bool isStraitsSubgateChainModel(rack::Model* m) {
    return m == modelStraitsSubgateBase
        || m == modelStraitsSubgatePanelRatchetRest
        || m == modelStraitsSubgatePanelRatchetLegato
        || m == modelStraitsSubgatePanelRatchetAccent
        || m == modelStraitsSubgatePanelRatchetAdvance
        || m == modelStraitsSubgatePanelGhostRest
        || m == modelStraitsSubgatePanelGhostLegato
        || m == modelStraitsSubgatePanelGhostAccent
        || m == modelStraitsSubgatePanelGhostAdvance;
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

// ── the SUBPANEL module — the per-voice tier for ONE (level × decision) ─────
// CC_PROMPT_FIX_SUBGATE_PANELS option (a): ONE decision's 16 voices per panel
// (like a lane expander is one lane's 16 voices), NOT one level's all-decisions.
struct StraitsSubgatePanelModule : rack::engine::Module {
    int level = 0;   // dotModular::SubgateKnobs::RATCHET/GHOST — set by the per-panel subclass
    int dec   = 0;   // 0..3 = REST/LEGATO/ADVANCE/ACCENT (the DEC_CAPTIONS order)
    float lastOwn_[dotModular::SubgateKnobs::VOICES] = {};
    std::atomic<bool> needsResnap{false};   // writers: widget drag-end (UI) + the left-link check (engine); reader/clearer: the base's step

    StraitsSubgatePanelModule() {
        config(0, StraitsSubgateIds::PANEL_NUM_MOD_INPUTS, 0, 0);
    }

    // Per-panel ctor finish: the ONE decision jack's tooltip.
    void configPanelInputs() {
        configInput(StraitsSubgateIds::MOD_PANEL_0,
            std::string("Per-voice ") + dotModular::SubgateKnobs::LEVEL_CAPTIONS[level]
            + " " + dotModular::SubgateKnobs::DEC_CAPTIONS[dec]
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

// The eight per-(level × decision) subclasses — one slug each, so the user can
// attach exactly the decisions they want (the StraitsLane subclass law).
struct StraitsSubgateRatchetRestModule    : StraitsSubgatePanelModule {
    StraitsSubgateRatchetRestModule()    { level = dotModular::SubgateKnobs::RATCHET; dec = 0; configPanelInputs(); } };
struct StraitsSubgateRatchetLegatoModule  : StraitsSubgatePanelModule {
    StraitsSubgateRatchetLegatoModule()  { level = dotModular::SubgateKnobs::RATCHET; dec = 1; configPanelInputs(); } };
struct StraitsSubgateRatchetAdvanceModule : StraitsSubgatePanelModule {
    StraitsSubgateRatchetAdvanceModule() { level = dotModular::SubgateKnobs::RATCHET; dec = 2; configPanelInputs(); } };
struct StraitsSubgateRatchetAccentModule  : StraitsSubgatePanelModule {
    StraitsSubgateRatchetAccentModule()  { level = dotModular::SubgateKnobs::RATCHET; dec = 3; configPanelInputs(); } };
struct StraitsSubgateGhostRestModule      : StraitsSubgatePanelModule {
    StraitsSubgateGhostRestModule()      { level = dotModular::SubgateKnobs::GHOST;   dec = 0; configPanelInputs(); } };
struct StraitsSubgateGhostLegatoModule    : StraitsSubgatePanelModule {
    StraitsSubgateGhostLegatoModule()    { level = dotModular::SubgateKnobs::GHOST;   dec = 1; configPanelInputs(); } };
struct StraitsSubgateGhostAdvanceModule   : StraitsSubgatePanelModule {
    StraitsSubgateGhostAdvanceModule()   { level = dotModular::SubgateKnobs::GHOST;   dec = 2; configPanelInputs(); } };
struct StraitsSubgateGhostAccentModule    : StraitsSubgatePanelModule {
    StraitsSubgateGhostAccentModule()    { level = dotModular::SubgateKnobs::GHOST;   dec = 3; configPanelInputs(); } };
