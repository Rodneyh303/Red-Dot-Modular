#pragma once

#include "rack.hpp"

/**
 * Straits Sands Expander - Poly DNA (Voices 2-16)
 * 
 * Provides DNA (length offset + rotation) modulation for all 15 poly voices (voices 2-16).
 * Each voice has independent:
 *   • Length offset modulation input + attenuverter
 *   • Rotation modulation input + attenuverter
 * 
 * Modulation: Continuous (not quantized)
 * 
 * Behavior:
 *   • Works with Straits East (voices 2-8)
 *   • Works with Straits West (voices 9-16)
 *   • Works with both (all 15 voices)
 *   • No effect for voices without corresponding expanders
 */

namespace StraitSandsExpanderIds {
    // Modulation inputs for DNA parameters (15 voices × 2 params = 30 inputs)
    enum Input {
        // Voices 2-8 (Straits East compatible)
        VOICE_2_LENGTH_MOD_IN = 0,
        VOICE_2_ROTATION_MOD_IN,
        VOICE_3_LENGTH_MOD_IN,
        VOICE_3_ROTATION_MOD_IN,
        VOICE_4_LENGTH_MOD_IN,
        VOICE_4_ROTATION_MOD_IN,
        VOICE_5_LENGTH_MOD_IN,
        VOICE_5_ROTATION_MOD_IN,
        VOICE_6_LENGTH_MOD_IN,
        VOICE_6_ROTATION_MOD_IN,
        VOICE_7_LENGTH_MOD_IN,
        VOICE_7_ROTATION_MOD_IN,
        VOICE_8_LENGTH_MOD_IN,
        VOICE_8_ROTATION_MOD_IN,
        
        // Voices 9-16 (Straits West compatible)
        VOICE_9_LENGTH_MOD_IN,
        VOICE_9_ROTATION_MOD_IN,
        VOICE_10_LENGTH_MOD_IN,
        VOICE_10_ROTATION_MOD_IN,
        VOICE_11_LENGTH_MOD_IN,
        VOICE_11_ROTATION_MOD_IN,
        VOICE_12_LENGTH_MOD_IN,
        VOICE_12_ROTATION_MOD_IN,
        VOICE_13_LENGTH_MOD_IN,
        VOICE_13_ROTATION_MOD_IN,
        VOICE_14_LENGTH_MOD_IN,
        VOICE_14_ROTATION_MOD_IN,
        VOICE_15_LENGTH_MOD_IN,
        VOICE_15_ROTATION_MOD_IN,
        VOICE_16_LENGTH_MOD_IN,
        VOICE_16_ROTATION_MOD_IN,
        NUM_INPUTS
    };

    // Attenuverters for DNA modulation (15 voices × 2 = 30 attenuverters)
    enum Param {
        // Voices 2-8
        VOICE_2_LENGTH_ATT = 0,
        VOICE_2_ROTATION_ATT,
        VOICE_3_LENGTH_ATT,
        VOICE_3_ROTATION_ATT,
        VOICE_4_LENGTH_ATT,
        VOICE_4_ROTATION_ATT,
        VOICE_5_LENGTH_ATT,
        VOICE_5_ROTATION_ATT,
        VOICE_6_LENGTH_ATT,
        VOICE_6_ROTATION_ATT,
        VOICE_7_LENGTH_ATT,
        VOICE_7_ROTATION_ATT,
        VOICE_8_LENGTH_ATT,
        VOICE_8_ROTATION_ATT,
        
        // Voices 9-16
        VOICE_9_LENGTH_ATT,
        VOICE_9_ROTATION_ATT,
        VOICE_10_LENGTH_ATT,
        VOICE_10_ROTATION_ATT,
        VOICE_11_LENGTH_ATT,
        VOICE_11_ROTATION_ATT,
        VOICE_12_LENGTH_ATT,
        VOICE_12_ROTATION_ATT,
        VOICE_13_LENGTH_ATT,
        VOICE_13_ROTATION_ATT,
        VOICE_14_LENGTH_ATT,
        VOICE_14_ROTATION_ATT,
        VOICE_15_LENGTH_ATT,
        VOICE_15_ROTATION_ATT,
        VOICE_16_LENGTH_ATT,
        VOICE_16_ROTATION_ATT,
        NUM_PARAMS
    };

    // No outputs - modulation only affects internal DNA state
    enum Output {
        NUM_OUTPUTS = 0
    };
};

struct MeloDicerStraitSandsExpander : Module {
    MeloDicerStraitSandsExpander() {
        config(StraitSandsExpanderIds::NUM_PARAMS, StraitSandsExpanderIds::NUM_INPUTS, StraitSandsExpanderIds::NUM_OUTPUTS);
        
        // Configure all 30 attenuverter params
        for (int v = 0; v < 15; v++) {
            int voice = v + 2;  // Voice 2-16
            configParam(StraitSandsExpanderIds::VOICE_2_LENGTH_ATT + v*2, -1.f, 1.f, 0.f, 
                       "Voice " + std::to_string(voice) + " Length Offset Attenuation");
            configParam(StraitSandsExpanderIds::VOICE_2_ROTATION_ATT + v*2, -1.f, 1.f, 0.f, 
                       "Voice " + std::to_string(voice) + " Rotation Attenuation");
        }
    }

    // TODO (Phase 6): Implement process() with:
    // - Read modulation inputs for all 15 voices
    // - Apply attenuverter scaling
    // - Pass modulation to parent MeloDicer's DNA state
    // - Only affect voices where corresponding East/West expanders are present

    void process(const ProcessArgs& args) override {
        // Placeholder for Phase 6 implementation
        // Will communicate DNA modulation to parent MeloDicer
    }
};

struct MeloDicerStraitSandsExpanderWidget : ModuleWidget {
    MeloDicerStraitSandsExpanderWidget(MeloDicerStraitSandsExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/strait_sands.svg")));
        
        // TODO (Phase 6): Add UI elements
        // - 30 attenuverter knobs (15 voices × 2 controls)
        // - 30 input jacks (15 voices × 2 controls)
        // - Arranged in 2 columns (one per DNA parameter)
    }
};

Model* modelMeloDicerStraitSandsExpander = createModel<MeloDicerStraitSandsExpander, MeloDicerStraitSandsExpanderWidget>("MeloDicerStraitSandsExpander");
