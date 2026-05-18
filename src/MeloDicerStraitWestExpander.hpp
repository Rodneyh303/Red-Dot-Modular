#pragma once

#include "rack.hpp"

/**
 * Straits West Expander - Voices 9-16
 * 
 * Extends MeloDicer polyphony from 8 voices (East) to 16 total voices.
 * Provides:
 *   • 8-voice poly outputs for voices 9-16
 *   • 16-voice aggregated outputs (all voices 1-16)
 *   • Voice probability modulation inputs (8 voices)
 * 
 * VALIDATION: Only valid if Straits East expander is present
 * If East is not present, all outputs are zeroed and visual warning shown
 */

namespace StraitWestExpanderIds {
    // Modulation inputs for voice probability (voices 9-16, 8 inputs)
    enum Input {
        VOICE_9_PROB_MOD_IN = 0,
        VOICE_10_PROB_MOD_IN,
        VOICE_11_PROB_MOD_IN,
        VOICE_12_PROB_MOD_IN,
        VOICE_13_PROB_MOD_IN,
        VOICE_14_PROB_MOD_IN,
        VOICE_15_PROB_MOD_IN,
        VOICE_16_PROB_MOD_IN,
        NUM_INPUTS
    };

    // Attenuverters for voice probability (8 attenuverters)
    enum Param {
        VOICE_9_PROB_ATT = 0,
        VOICE_10_PROB_ATT,
        VOICE_11_PROB_ATT,
        VOICE_12_PROB_ATT,
        VOICE_13_PROB_ATT,
        VOICE_14_PROB_ATT,
        VOICE_15_PROB_ATT,
        VOICE_16_PROB_ATT,
        NUM_PARAMS
    };

    // Outputs
    enum Output {
        POLY_GATE_9_16_OUT = 0,      // 8-voice poly gate (voices 9-16)
        POLY_CV_9_16_OUT,             // 8-voice poly CV (voices 9-16)
        POLY_GATE_1_16_OUT,           // 16-voice aggregated gate (all voices)
        POLY_CV_1_16_OUT,             // 16-voice aggregated CV (all voices)
        NUM_OUTPUTS
    };
};

struct MeloDicerStraitWestExpander : Module {
    MeloDicerStraitWestExpander() {
        config(StraitWestExpanderIds::NUM_PARAMS, StraitWestExpanderIds::NUM_INPUTS, StraitWestExpanderIds::NUM_OUTPUTS);
        
        // Configure attenuverter params for voice probability modulation
        for (int i = 0; i < 8; i++) {
            configParam(StraitWestExpanderIds::VOICE_9_PROB_ATT + i, -1.f, 1.f, 0.f, "Voice " + std::to_string(9 + i) + " Prob Attenuation");
        }
    }

    // TODO (Phase 4): Implement process() with:
    // - Check for Straits East expander presence
    // - Read probability modulation inputs (voices 9-16)
    // - Output 8-voice and 16-voice poly signals
    // - Aggregate voice 1-8 from East with voices 9-16

    void process(const ProcessArgs& args) override {
        // Placeholder for Phase 4 implementation
        // Will read from parent MeloDicer via expander pointer
    }

    bool hasEastExpander() {
        // TODO: Validate that Straits East expander is connected
        return true; // Placeholder
    }
};

struct MeloDicerStraitWestExpanderWidget : ModuleWidget {
    MeloDicerStraitWestExpanderWidget(MeloDicerStraitWestExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/straits_west.svg")));
        
        // TODO (Phase 4): Add UI elements
        // - 8 attenuverter knobs for voice probability
        // - 8 input jacks for modulation
        // - 4 output jacks (8-voice gate/CV, 16-voice gate/CV)
    }
};

Model* modelMeloDicerStraitWestExpander = createModel<MeloDicerStraitWestExpander, MeloDicerStraitWestExpanderWidget>("MeloDicerStraitWestExpander");
