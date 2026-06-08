#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// Causeway: dice/draw-generation modulation expander for Monsoon.
// Passive (like Interchange) — Monsoon reads its CV (×attenuverter) and fires
// its gate edges via the cached expander pointer. No message-passing.
//   4 CV + attenuverters: slew R/M, mix R/M  (sum into the same targets as CV3)
//   10 dedicated die-action gates (see CausewayInputIds order)
struct MonsoonCausewayExpander : Module {
    MonsoonCausewayExpander() {
        config(MonsoonIds::NUM_CAUSEWAY_PARAMS, MonsoonIds::NUM_CAUSEWAY_INPUTS, 0, 0);
        configParam(MonsoonIds::CAUSEWAY_SLEW_R_ATT, -1.f, 1.f, 0.f, "Rhythm slew CV attenuverter");
        configParam(MonsoonIds::CAUSEWAY_SLEW_M_ATT, -1.f, 1.f, 0.f, "Melody slew CV attenuverter");
        configParam(MonsoonIds::CAUSEWAY_MIX_R_ATT,  -1.f, 1.f, 0.f, "Rhythm A>B mix CV attenuverter");
        configParam(MonsoonIds::CAUSEWAY_MIX_M_ATT,  -1.f, 1.f, 0.f, "Melody A>B mix CV attenuverter");
        configInput(MonsoonIds::CAUSEWAY_SLEW_R_CV, "Rhythm slew CV");
        configInput(MonsoonIds::CAUSEWAY_SLEW_M_CV, "Melody slew CV");
        configInput(MonsoonIds::CAUSEWAY_MIX_R_CV,  "Rhythm A>B mix CV");
        configInput(MonsoonIds::CAUSEWAY_MIX_M_CV,  "Melody A>B mix CV");
        configInput(MonsoonIds::CAUSEWAY_GATE_TRIAL_R,       "Trial rhythm die (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_TRIAL_M,       "Trial melody die (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_REDICE_R,      "Re-dice rhythm (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_REDICE_M,      "Re-dice melody (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_LIVESRC_R,     "Toggle rhythm live source (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_LIVESRC_M,     "Toggle melody live source (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_LIVESTATIC_R,  "Toggle rhythm live/static (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_LIVESTATIC_M,  "Toggle melody live/static (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_RESEED_ROLL,   "Toggle reseed-on-roll (gate)");
        configInput(MonsoonIds::CAUSEWAY_GATE_RESEED_RESTART,"Toggle reseed-on-restart (gate)");
    }
    void process(const ProcessArgs& args) override {}
};
