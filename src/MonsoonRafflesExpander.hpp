#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// Raffles: dice/draw-generation modulation expander for Monsoon.
// Passive (like Interchange) — Monsoon reads its CV (×attenuverter) and fires
// its gate edges via the cached expander pointer. No message-passing.
//   4 CV + attenuverters: slew R/M, mix R/M  (sum into the same targets as CV3)
//   10 dedicated die-action gates (see RafflesInputIds order)
struct MonsoonRafflesExpander : Module {
    MonsoonRafflesExpander() {
        config(MonsoonIds::NUM_RAFFLES_PARAMS, MonsoonIds::NUM_RAFFLES_INPUTS, 0, 0);
        configParam(MonsoonIds::RAFFLES_SLEW_R_ATT, -1.f, 1.f, 0.f, "Rhythm slew CV attenuverter");
        configParam(MonsoonIds::RAFFLES_SLEW_M_ATT, -1.f, 1.f, 0.f, "Melody slew CV attenuverter");
        configParam(MonsoonIds::RAFFLES_MIX_R_ATT,  -1.f, 1.f, 0.f, "Rhythm A>B mix CV attenuverter");
        configParam(MonsoonIds::RAFFLES_MIX_M_ATT,  -1.f, 1.f, 0.f, "Melody A>B mix CV attenuverter");
        configInput(MonsoonIds::RAFFLES_SLEW_R_CV, "Rhythm slew CV");
        configInput(MonsoonIds::RAFFLES_SLEW_M_CV, "Melody slew CV");
        configInput(MonsoonIds::RAFFLES_MIX_R_CV,  "Rhythm A>B mix CV");
        configInput(MonsoonIds::RAFFLES_MIX_M_CV,  "Melody A>B mix CV");
        // Labels reflect ACTUAL behaviour (see kRafflesGateAction in Monsoon.hpp). Gates whose
        // mechanism was removed are inert and labelled "(unused)" so the tooltip doesn't lie.
        configInput(MonsoonIds::RAFFLES_GATE_TRIAL_R,       "Trial rhythm die — unused (Trial removed)");
        configInput(MonsoonIds::RAFFLES_GATE_TRIAL_M,       "Trial melody die — unused (Trial removed)");
        configInput(MonsoonIds::RAFFLES_GATE_REDICE_R,      "Re-dice rhythm (gate)");
        configInput(MonsoonIds::RAFFLES_GATE_REDICE_M,      "Re-dice melody (gate)");
        configInput(MonsoonIds::RAFFLES_GATE_LIVESRC_R,     "Live source rhythm — unused (Trial removed)");
        configInput(MonsoonIds::RAFFLES_GATE_LIVESRC_M,     "Live source melody — unused (Trial removed)");
        configInput(MonsoonIds::RAFFLES_GATE_LIVESTATIC_R,  "Toggle rhythm dice\u2194live (gate)");
        configInput(MonsoonIds::RAFFLES_GATE_LIVESTATIC_M,  "Toggle melody dice\u2194live (gate)");
        configInput(MonsoonIds::RAFFLES_GATE_RESEED_ROLL,   "Reseed-on-roll — unused (reseed lives on RESET)");
        configInput(MonsoonIds::RAFFLES_GATE_RESEED_RESTART,"Toggle reseed-on-restart (gate)");
        configInput(MonsoonIds::RAFFLES_GATE_LASTDICE_R,    "Last rhythm die (gate)");
        configInput(MonsoonIds::RAFFLES_GATE_LASTDICE_M,    "Last melody die (gate)");
        configInput(MonsoonIds::RAFFLES_GATE_LASTTRIAL_R,   "Last rhythm trial — unused (Trial removed)");
        configInput(MonsoonIds::RAFFLES_GATE_LASTTRIAL_M,   "Last melody trial — unused (Trial removed)");
    }
    void process(const ProcessArgs& args) override {}
};
