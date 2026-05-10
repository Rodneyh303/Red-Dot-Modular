#pragma once
#include <rack.hpp>
#include "MeloDicer.hpp"

using namespace rack;

struct MeloDicerDNAExpander : Module {
    MeloDicerDNAExpander() {
        config(MeloDicerIds::NUM_PARAMS, MeloDicerIds::NUM_INPUTS, 0, 0);

        auto configS = [&](int pLen, int pOff, int pRot, const char* name) {
            configParam(pLen, 1.f, 16.f, 16.f, name + std::string(" Length"));
            configParam(pOff, 0.f, 15.f, 0.f, name + std::string(" Offset"));
            configParam(pRot, 0.f, 15.f, 0.f, name + std::string(" Rotation (Non-destructive)"));
        };

        configS(MeloDicerIds::DNA_R_LEN_PARAM, MeloDicerIds::DNA_R_OFF_PARAM, MeloDicerIds::DNA_R_ROT_PARAM, "Rhythm");
        configS(MeloDicerIds::DNA_V_LEN_PARAM, MeloDicerIds::DNA_V_OFF_PARAM, MeloDicerIds::DNA_V_ROT_PARAM, "Variation");
        configS(MeloDicerIds::DNA_L_LEN_PARAM, MeloDicerIds::DNA_L_OFF_PARAM, MeloDicerIds::DNA_L_ROT_PARAM, "Legato");
        configS(MeloDicerIds::DNA_M_LEN_PARAM, MeloDicerIds::DNA_M_OFF_PARAM, MeloDicerIds::DNA_M_ROT_PARAM, "Melody");
        configS(MeloDicerIds::DNA_O_LEN_PARAM, MeloDicerIds::DNA_O_OFF_PARAM, MeloDicerIds::DNA_O_ROT_PARAM, "Octave");

        // DNA Action Buttons (Scramble)
        configButton(MeloDicerIds::DNA_SCRAMBLE_ALL_PARAM, "Scramble ALL");
        configButton(MeloDicerIds::DNA_SCRAMBLE_R_PARAM,   "Scramble Rhythm");
        configButton(MeloDicerIds::DNA_SCRAMBLE_V_PARAM,   "Scramble Variation");
        configButton(MeloDicerIds::DNA_SCRAMBLE_L_PARAM,   "Scramble Legato");
        configButton(MeloDicerIds::DNA_SCRAMBLE_M_PARAM,   "Scramble Melody");
        configButton(MeloDicerIds::DNA_SCRAMBLE_O_PARAM,   "Scramble Octave");

        // DNA Action Buttons (Reset)
        configButton(MeloDicerIds::DNA_RESET_ALL_PARAM,    "Reset ALL");
        configButton(MeloDicerIds::DNA_RESET_R_PARAM,      "Reset Rhythm");
        configButton(MeloDicerIds::DNA_RESET_V_PARAM,      "Reset Variation");
        configButton(MeloDicerIds::DNA_RESET_L_PARAM,      "Reset Legato");
        configButton(MeloDicerIds::DNA_RESET_M_PARAM,      "Reset Melody");
        configButton(MeloDicerIds::DNA_RESET_O_PARAM,      "Reset Octave");

        // Note: The DNA Gate Inputs are defined in MeloDicer.hpp and read by the main module.
        // The expander module itself does not need to configInput() for them,
        // but the expander widget will add InputPort widgets that refer to these IDs.
        // This is consistent with how the main module reads expander inputs.
    }

    void process(const ProcessArgs& args) override {}
};