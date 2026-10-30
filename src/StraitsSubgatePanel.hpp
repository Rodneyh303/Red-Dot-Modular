#pragma once
// ── StraitsSubgatePanel — the subgate per-voice panels (the subgate tier-1
// surfaces). CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_BASE: the subgate GLOBAL
// tier (the 8 knobs + their mod jacks, the source switches, the wheel) lives
// on STRAITSBASE now — the separate StraitsSubgateBase module is RETIRED
// (deleted); these panels attach to the unified StraitsBase, docking after
// the lane run.
//
//   StraitsSubgatePanel{Level}{Decision} — the per-voice tier (tier 1), ONE
//                               panel per (LEVEL × DECISION) — CC_PROMPT_FIX_
//                               SUBGATE_PANELS, Rodney DECIDED option (a):
//                               {RATCHET,GHOST} × {REST,LEGATO,ACCENT,ADVANCE}
//                               = EIGHT panels, each 16 voices × ONE decision
//                               (the lane-expander law — one panel = one bank
//                               of 16, the settled 6/6/4 165-wide layout from
//                               gen_straits_lane.py; NO crammed multi-decision
//                               panel). Store-backed knobs (touch = claim —
//                               the ownActive opt-in). NO mod jack
//                               (CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_BASE:
//                               modulation is Causeway, not per-panel jacks).
//                               Registered + HIDDEN — base-spawned only (the
//                               StraitsLane law).
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

#include "Monsoon.hpp"
#include "ui/ModuleLookupCore.hpp"   // THE lookup authority: the topology epoch (onExpanderChange bump)

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
// scan's hop-through list). SUBGATE MERGE (CC_PROMPT_SUBGATE_MERGE_ONTO_
// STRAITS_BASE): the panels dock on the UNIFIED Straits chain — their left
// neighbour is the StraitsBase, a lane expander, or another subgate panel.
inline bool isStraitsSubgateChainModel(rack::Model* m) {
    return m == modelStraitsBase
        || m == modelStraitsLaneQMIX
        || m == modelStraitsLaneREST
        || m == modelStraitsLaneACCENT
        || m == modelStraitsLaneVARIATION
        || m == modelStraitsLaneLEGATO
        || m == modelStraitsSubgatePanelRatchetRest
        || m == modelStraitsSubgatePanelRatchetLegato
        || m == modelStraitsSubgatePanelRatchetAccent
        || m == modelStraitsSubgatePanelRatchetAdvance
        || m == modelStraitsSubgatePanelGhostRest
        || m == modelStraitsSubgatePanelGhostLegato
        || m == modelStraitsSubgatePanelGhostAccent
        || m == modelStraitsSubgatePanelGhostAdvance;
}

// ── the SUBPANEL module — the per-voice tier for ONE (level × decision) ─────
// CC_PROMPT_FIX_SUBGATE_PANELS option (a): ONE decision's 16 voices per panel
// (like a lane expander is one lane's 16 voices), NOT one level's all-decisions.
struct StraitsSubgatePanelModule : rack::engine::Module {
    int level = 0;   // dotModular::SubgateKnobs::RATCHET/GHOST — set by the per-panel subclass
    int dec   = 0;   // 0..3 = REST/LEGATO/ADVANCE/ACCENT (the DEC_CAPTIONS order)
    std::atomic<bool> needsResnap{false};   // writers: widget drag-end (UI) + the left-link check (engine); reader/clearer: the base's step

    // A pure attach-surface (CC_PROMPT_SUBGATE_MERGE_ONTO_STRAITS_BASE): no
    // params, no inputs, no process — the StoreKnob authoring goes through
    // Monsoon's store via the widget; per-voice deltas were the mod jack's
    // path, which is gone (modulation is Causeway).
    StraitsSubgatePanelModule() {
        config(0, 0, 0, 0);
    }

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
    StraitsSubgateRatchetRestModule()    { level = dotModular::SubgateKnobs::RATCHET; dec = 0; } };
struct StraitsSubgateRatchetLegatoModule  : StraitsSubgatePanelModule {
    StraitsSubgateRatchetLegatoModule()  { level = dotModular::SubgateKnobs::RATCHET; dec = 1; } };
struct StraitsSubgateRatchetAdvanceModule : StraitsSubgatePanelModule {
    StraitsSubgateRatchetAdvanceModule() { level = dotModular::SubgateKnobs::RATCHET; dec = 2; } };
struct StraitsSubgateRatchetAccentModule  : StraitsSubgatePanelModule {
    StraitsSubgateRatchetAccentModule()  { level = dotModular::SubgateKnobs::RATCHET; dec = 3; } };
struct StraitsSubgateGhostRestModule      : StraitsSubgatePanelModule {
    StraitsSubgateGhostRestModule()      { level = dotModular::SubgateKnobs::GHOST;   dec = 0; } };
struct StraitsSubgateGhostLegatoModule    : StraitsSubgatePanelModule {
    StraitsSubgateGhostLegatoModule()    { level = dotModular::SubgateKnobs::GHOST;   dec = 1; } };
struct StraitsSubgateGhostAdvanceModule   : StraitsSubgatePanelModule {
    StraitsSubgateGhostAdvanceModule()   { level = dotModular::SubgateKnobs::GHOST;   dec = 2; } };
struct StraitsSubgateGhostAccentModule    : StraitsSubgatePanelModule {
    StraitsSubgateGhostAccentModule()    { level = dotModular::SubgateKnobs::GHOST;   dec = 3; } };
