#pragma once

#include "rack.hpp"
#include "MeloDicer.hpp"

using namespace rack;
using namespace MeloDicerIds;

/**
 * Straits Sands Expander - Poly DNA Control (Voices 2-16)
 * 
 * Provides DNA parameter control and fun randomization across all 15 poly voices.
 * 
 * Uses parent MeloDicer's param/input IDs:
 * • POLY_DNA_VOICE_1-15: Length, Offset, Rotation controls per voice
 * • POLY_DNA_VOICE_1-15_LEN/OFF_INPUT: CV modulation inputs
 * 
 * Local params/inputs (expander-specific):
 * • Scramble buttons: Randomize length & offset (fun exploration!)
 * • Reset buttons: Restore defaults
 * • Gate inputs for external control
 * 
 * NOTE: Parent MeloDicer::process() handles all logic (zero-delay)
 */

namespace StraitSandsExpanderIds {
    enum ParamId {
        // Scramble buttons: randomize length & offset per voice (16 total)
        SCRAMBLE_ALL_PARAM = 0,
        SCRAMBLE_V2_PARAM,
        SCRAMBLE_V3_PARAM,
        SCRAMBLE_V4_PARAM,
        SCRAMBLE_V5_PARAM,
        SCRAMBLE_V6_PARAM,
        SCRAMBLE_V7_PARAM,
        SCRAMBLE_V8_PARAM,
        SCRAMBLE_V9_PARAM,
        SCRAMBLE_V10_PARAM,
        SCRAMBLE_V11_PARAM,
        SCRAMBLE_V12_PARAM,
        SCRAMBLE_V13_PARAM,
        SCRAMBLE_V14_PARAM,
        SCRAMBLE_V15_PARAM,
        SCRAMBLE_V16_PARAM,
        
        // Reset buttons: restore defaults per voice (16 total)
        RESET_ALL_PARAM,
        RESET_V2_PARAM,
        RESET_V3_PARAM,
        RESET_V4_PARAM,
        RESET_V5_PARAM,
        RESET_V6_PARAM,
        RESET_V7_PARAM,
        RESET_V8_PARAM,
        RESET_V9_PARAM,
        RESET_V10_PARAM,
        RESET_V11_PARAM,
        RESET_V12_PARAM,
        RESET_V13_PARAM,
        RESET_V14_PARAM,
        RESET_V15_PARAM,
        RESET_V16_PARAM,
        
        NUM_PARAMS
    };
    
    enum InputId {
        // Gate inputs for scramble actions (16 total)
        SCRAMBLE_ALL_INPUT = 0,
        SCRAMBLE_V2_INPUT,
        SCRAMBLE_V3_INPUT,
        SCRAMBLE_V4_INPUT,
        SCRAMBLE_V5_INPUT,
        SCRAMBLE_V6_INPUT,
        SCRAMBLE_V7_INPUT,
        SCRAMBLE_V8_INPUT,
        SCRAMBLE_V9_INPUT,
        SCRAMBLE_V10_INPUT,
        SCRAMBLE_V11_INPUT,
        SCRAMBLE_V12_INPUT,
        SCRAMBLE_V13_INPUT,
        SCRAMBLE_V14_INPUT,
        SCRAMBLE_V15_INPUT,
        SCRAMBLE_V16_INPUT,
        
        // Gate inputs for reset actions (16 total)
        RESET_ALL_INPUT,
        RESET_V2_INPUT,
        RESET_V3_INPUT,
        RESET_V4_INPUT,
        RESET_V5_INPUT,
        RESET_V6_INPUT,
        RESET_V7_INPUT,
        RESET_V8_INPUT,
        RESET_V9_INPUT,
        RESET_V10_INPUT,
        RESET_V11_INPUT,
        RESET_V12_INPUT,
        RESET_V13_INPUT,
        RESET_V14_INPUT,
        RESET_V15_INPUT,
        RESET_V16_INPUT,
        
        NUM_INPUTS
    };
};

struct MeloDicerStraitSandsExpander : Module {
    MeloDicerStraitSandsExpander() {
        // Config for expander's own button/gate inputs
        config(StraitSandsExpanderIds::NUM_PARAMS, StraitSandsExpanderIds::NUM_INPUTS, 0);
        
        // Scramble buttons - randomize length and offset for each voice
        configButton(StraitSandsExpanderIds::SCRAMBLE_ALL_PARAM, "Scramble ALL");
        for (int v = 0; v < 15; v++) {
            int voice = v + 2;  // Voices 2-16
            configButton(StraitSandsExpanderIds::SCRAMBLE_V2_PARAM + v,
                        "Scramble Voice " + std::to_string(voice));
        }
        
        // Reset buttons - restore defaults
        configButton(StraitSandsExpanderIds::RESET_ALL_PARAM, "Reset ALL");
        for (int v = 0; v < 15; v++) {
            int voice = v + 2;
            configButton(StraitSandsExpanderIds::RESET_V2_PARAM + v,
                        "Reset Voice " + std::to_string(voice));
        }
        
        // Gate inputs for scramble/reset external control
        configInput(StraitSandsExpanderIds::SCRAMBLE_ALL_INPUT, "Scramble ALL Gate");
        for (int v = 0; v < 15; v++) {
            int voice = v + 2;
            configInput(StraitSandsExpanderIds::SCRAMBLE_V2_INPUT + v,
                       "Scramble Voice " + std::to_string(voice) + " Gate");
        }
        
        configInput(StraitSandsExpanderIds::RESET_ALL_INPUT, "Reset ALL Gate");
        for (int v = 0; v < 15; v++) {
            int voice = v + 2;
            configInput(StraitSandsExpanderIds::RESET_V2_INPUT + v,
                       "Reset Voice " + std::to_string(voice) + " Gate");
        }
    }

    // Zero-delay: parent module handles scramble/reset logic
    void process(const ProcessArgs& args) override {}
};

struct MeloDicerStraitSandsExpanderWidget : ModuleWidget {
    MeloDicerStraitSandsExpanderWidget(MeloDicerStraitSandsExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/strait_sands.svg")));
        
        // TODO (UI): Add DNA control interface
        // - Displays for current DNA settings: length, offset, rotation (per voice)
        // - "Scramble ALL" button + 15 individual scramble buttons (fun randomization!)
        // - "Reset ALL" button + 15 individual reset buttons
        // - Gate inputs for each scramble/reset (external trigger control)
    }
};

Model* modelMeloDicerStraitSandsExpander = createModel<MeloDicerStraitSandsExpander, MeloDicerStraitSandsExpanderWidget>("MeloDicerStraitSandsExpander");
