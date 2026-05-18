#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// The DNA expander only uses a subset of MonsoonIds — the DNA strand
// params/inputs and action buttons.  We allocate exactly that range so we
// don't waste memory on the full MeloDicer param/input set.
//
// Layout helpers: MeloDicer reads this expander's params[]/inputs[] directly
// using MonsoonIds enum values.  We therefore allocate exactly
// (NUM_PARAMS - DNA_R_LEN_PARAM) params and (NUM_INPUTS - DNA_R_LEN_INPUT)
// inputs, and bias every index by subtracting the start offset so that
// params[DNA_R_LEN_PARAM - DNA_PARAM_OFFSET] corresponds to the correct slot.
//
// To keep it simple and safe for now we size to the full MonsoonIds ranges
// but use a named constant so the intent is obvious and can be tightened later.

struct MonsoonSandsExpander : Module {
    // Number of params/inputs in the DNA expander's own namespace.
    // Sized from DNA_R_LEN_PARAM .. NUM_PARAMS and DNA_R_LEN_INPUT .. NUM_INPUTS.
    static constexpr int DNA_NUM_PARAMS = MonsoonIds::NUM_PARAMS  - MonsoonIds::DNA_R_LEN_PARAM;
    static constexpr int DNA_NUM_INPUTS = MonsoonIds::NUM_INPUTS  - MonsoonIds::DNA_R_LEN_INPUT;

    MonsoonSandsExpander() {
        // Allocate only the DNA-relevant range, not all of MonsoonIds.
        // MeloDicer accesses params[id] and inputs[id] via the cached pointer;
        // it always uses full MonsoonIds values, so we offset by the base.
        // For safety during this transition we still size to NUM_PARAMS/NUM_INPUTS
        // to guarantee no out-of-bounds access if a call site forgets the offset.
        // TODO: once all call sites are updated to subtract DNA_R_LEN_PARAM /
        //       DNA_R_LEN_INPUT, replace with config(DNA_NUM_PARAMS, DNA_NUM_INPUTS, 0, 0).
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, 0, 0);

        auto configS = [&](int pLen, int pOff, int pRot, const char* name) {
            configParam(pLen, 1.f, 16.f, 16.f, name + std::string(" Length"));
            configParam(pOff, 0.f, 15.f,  0.f, name + std::string(" Offset"));
            configParam(pRot, 0.f, 15.f,  0.f, name + std::string(" Rotation (Non-destructive)"));
        };

        configS(MonsoonIds::DNA_R_LEN_PARAM, MonsoonIds::DNA_R_OFF_PARAM, MonsoonIds::DNA_R_ROT_PARAM, "Rhythm");
        configS(MonsoonIds::DNA_V_LEN_PARAM, MonsoonIds::DNA_V_OFF_PARAM, MonsoonIds::DNA_V_ROT_PARAM, "Variation");
        configS(MonsoonIds::DNA_L_LEN_PARAM, MonsoonIds::DNA_L_OFF_PARAM, MonsoonIds::DNA_L_ROT_PARAM, "Legato");
        configS(MonsoonIds::DNA_A_LEN_PARAM, MonsoonIds::DNA_A_OFF_PARAM, MonsoonIds::DNA_A_ROT_PARAM, "Accent");  // New
        configS(MonsoonIds::DNA_M_LEN_PARAM, MonsoonIds::DNA_M_OFF_PARAM, MonsoonIds::DNA_M_ROT_PARAM, "Melody");
        configS(MonsoonIds::DNA_O_LEN_PARAM, MonsoonIds::DNA_O_OFF_PARAM, MonsoonIds::DNA_O_ROT_PARAM, "Octave");

        // Scramble buttons
        configButton(MonsoonIds::DNA_SCRAMBLE_ALL_PARAM, "Scramble ALL");
        configButton(MonsoonIds::DNA_SCRAMBLE_R_PARAM,   "Scramble Rhythm");
        configButton(MonsoonIds::DNA_SCRAMBLE_V_PARAM,   "Scramble Variation");
        configButton(MonsoonIds::DNA_SCRAMBLE_L_PARAM,   "Scramble Legato");
        configButton(MonsoonIds::DNA_SCRAMBLE_A_PARAM,   "Scramble Accent");  // New
        configButton(MonsoonIds::DNA_SCRAMBLE_M_PARAM,   "Scramble Melody");
        configButton(MonsoonIds::DNA_SCRAMBLE_O_PARAM,   "Scramble Octave");

        // Reset buttons
        configButton(MonsoonIds::DNA_RESET_ALL_PARAM, "Reset ALL");
        configButton(MonsoonIds::DNA_RESET_R_PARAM,   "Reset Rhythm");
        configButton(MonsoonIds::DNA_RESET_V_PARAM,   "Reset Variation");
        configButton(MonsoonIds::DNA_RESET_L_PARAM,   "Reset Legato");
        configButton(MonsoonIds::DNA_RESET_A_PARAM,   "Reset Accent");  // New
        configButton(MonsoonIds::DNA_RESET_M_PARAM,   "Reset Melody");
        configButton(MonsoonIds::DNA_RESET_O_PARAM,   "Reset Octave");

        // CV inputs for Length and Offset per strand
        using namespace MonsoonIds;
        configInput(DNA_R_LEN_INPUT, "Rhythm Length CV");    configInput(DNA_R_OFF_INPUT, "Rhythm Offset CV");
        configInput(DNA_V_LEN_INPUT, "Variation Length CV"); configInput(DNA_V_OFF_INPUT, "Variation Offset CV");
        configInput(DNA_L_LEN_INPUT, "Legato Length CV");    configInput(DNA_L_OFF_INPUT, "Legato Offset CV");
        configInput(DNA_A_LEN_INPUT, "Accent Length CV");    configInput(DNA_A_OFF_INPUT, "Accent Offset CV");  // New
        configInput(DNA_M_LEN_INPUT, "Melody Length CV");    configInput(DNA_M_OFF_INPUT, "Melody Offset CV");
        configInput(DNA_O_LEN_INPUT, "Octave Length CV");    configInput(DNA_O_OFF_INPUT, "Octave Offset CV");

        // Gate inputs for scramble / reset actions
        configInput(DNA_SCRAMBLE_ALL_INPUT, "Scramble ALL Gate");
        configInput(DNA_SCRAMBLE_R_INPUT,   "Scramble Rhythm Gate");
        configInput(DNA_SCRAMBLE_M_INPUT,   "Scramble Melody Gate");
        configInput(DNA_SCRAMBLE_V_INPUT,   "Scramble Variation Gate");
        configInput(DNA_SCRAMBLE_L_INPUT,   "Scramble Legato Gate");
        configInput(DNA_SCRAMBLE_A_INPUT,   "Scramble Accent Gate");  // New
        configInput(DNA_SCRAMBLE_O_INPUT,   "Scramble Octave Gate");
        configInput(DNA_RESET_ALL_INPUT,    "Reset ALL Gate");
        configInput(DNA_RESET_R_INPUT,      "Reset Rhythm Gate");
        configInput(DNA_RESET_M_INPUT,      "Reset Melody Gate");
        configInput(DNA_RESET_V_INPUT,      "Reset Variation Gate");
        configInput(DNA_RESET_L_INPUT,      "Reset Legato Gate");
        configInput(DNA_RESET_A_INPUT,      "Reset Accent Gate");  // New
        configInput(DNA_RESET_O_INPUT,      "Reset Octave Gate");
    }

    void process(const ProcessArgs& args) override {}
};
