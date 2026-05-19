#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

/**
 * Straits Sands Expander - Poly DNA Control (Voices 2-16)
 * 
 * Provides fun scramble/reset buttons for randomizing poly voice DNA parameters.
 * Each of 15 voices (2-16) gets scramble and reset buttons.
 */

namespace StraitSandsExpanderIds {
    enum ParamId {
        // Scramble buttons: randomize length & offset per voice
        SCRAMBLE_ALL_PARAM = 0,
        SCRAMBLE_VOICE_2,
        SCRAMBLE_VOICE_3,
        SCRAMBLE_VOICE_4,
        SCRAMBLE_VOICE_5,
        SCRAMBLE_VOICE_6,
        SCRAMBLE_VOICE_7,
        SCRAMBLE_VOICE_8,
        SCRAMBLE_VOICE_9,
        SCRAMBLE_VOICE_10,
        SCRAMBLE_VOICE_11,
        SCRAMBLE_VOICE_12,
        SCRAMBLE_VOICE_13,
        SCRAMBLE_VOICE_14,
        SCRAMBLE_VOICE_15,
        SCRAMBLE_VOICE_16,
        
        // Reset buttons: restore defaults per voice
        RESET_ALL_PARAM,
        RESET_VOICE_2,
        RESET_VOICE_3,
        RESET_VOICE_4,
        RESET_VOICE_5,
        RESET_VOICE_6,
        RESET_VOICE_7,
        RESET_VOICE_8,
        RESET_VOICE_9,
        RESET_VOICE_10,
        RESET_VOICE_11,
        RESET_VOICE_12,
        RESET_VOICE_13,
        RESET_VOICE_14,
        RESET_VOICE_15,
        RESET_VOICE_16,
        
        NUM_PARAMS
    };
    
    enum InputId {
        // Gate inputs for scramble actions
        SCRAMBLE_ALL_INPUT = 0,
        SCRAMBLE_VOICE_2_INPUT,
        SCRAMBLE_VOICE_3_INPUT,
        SCRAMBLE_VOICE_4_INPUT,
        SCRAMBLE_VOICE_5_INPUT,
        SCRAMBLE_VOICE_6_INPUT,
        SCRAMBLE_VOICE_7_INPUT,
        SCRAMBLE_VOICE_8_INPUT,
        SCRAMBLE_VOICE_9_INPUT,
        SCRAMBLE_VOICE_10_INPUT,
        SCRAMBLE_VOICE_11_INPUT,
        SCRAMBLE_VOICE_12_INPUT,
        SCRAMBLE_VOICE_13_INPUT,
        SCRAMBLE_VOICE_14_INPUT,
        SCRAMBLE_VOICE_15_INPUT,
        SCRAMBLE_VOICE_16_INPUT,
        
        // Gate inputs for reset actions
        RESET_ALL_INPUT,
        RESET_VOICE_2_INPUT,
        RESET_VOICE_3_INPUT,
        RESET_VOICE_4_INPUT,
        RESET_VOICE_5_INPUT,
        RESET_VOICE_6_INPUT,
        RESET_VOICE_7_INPUT,
        RESET_VOICE_8_INPUT,
        RESET_VOICE_9_INPUT,
        RESET_VOICE_10_INPUT,
        RESET_VOICE_11_INPUT,
        RESET_VOICE_12_INPUT,
        RESET_VOICE_13_INPUT,
        RESET_VOICE_14_INPUT,
        RESET_VOICE_15_INPUT,
        RESET_VOICE_16_INPUT,
        
        NUM_INPUTS
    };
};

struct MonsoonStraitSandsExpander : Module {
    MonsoonStraitSandsExpander() {
        // Own params/inputs for buttons and gates
        config(StraitSandsExpanderIds::NUM_PARAMS, StraitSandsExpanderIds::NUM_INPUTS, 0, 0);

        // Scramble buttons
        configButton(StraitSandsExpanderIds::SCRAMBLE_ALL_PARAM, "Scramble ALL");
        for (int v = 0; v < 15; v++) {
            configButton(StraitSandsExpanderIds::SCRAMBLE_VOICE_2 + v,
                        "Scramble Voice " + std::to_string(v + 2));
        }
        
        // Reset buttons
        configButton(StraitSandsExpanderIds::RESET_ALL_PARAM, "Reset ALL");
        for (int v = 0; v < 15; v++) {
            configButton(StraitSandsExpanderIds::RESET_VOICE_2 + v,
                        "Reset Voice " + std::to_string(v + 2));
        }
        
        // Gate inputs for scramble
        configInput(StraitSandsExpanderIds::SCRAMBLE_ALL_INPUT, "Scramble ALL Gate");
        for (int v = 0; v < 15; v++) {
            configInput(StraitSandsExpanderIds::SCRAMBLE_VOICE_2_INPUT + v,
                       "Scramble Voice " + std::to_string(v + 2) + " Gate");
        }
        
        // Gate inputs for reset
        configInput(StraitSandsExpanderIds::RESET_ALL_INPUT, "Reset ALL Gate");
        for (int v = 0; v < 15; v++) {
            configInput(StraitSandsExpanderIds::RESET_VOICE_2_INPUT + v,
                       "Reset Voice " + std::to_string(v + 2) + " Gate");
        }
    }

    void process(const ProcessArgs& args) override {}
};
