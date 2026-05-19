#pragma once

#include "rack.hpp"
#include "MeloDicer.hpp"

using namespace rack;
using namespace MeloDicerIds;

/**
 * Straits West Expander - Voices 9-16
 * 
 * Extends polyphony from 8 (East) to 16 total voices.
 * Provides per-voice outputs and DNA control for voices 9-16.
 * 
 * Uses parent MeloDicer's param/input IDs:
 * • POLY_REST_PARAM_8-15 (voices 9-16 rest probability)
 * • POLY_DNA_VOICE_8-15 with LEN/OFF/ROT (DNA controls)
 * 
 * NOTE: Parent MeloDicer::process() handles all logic (zero-delay)
 */

namespace StraitWestExpanderIds {
    enum Output {
        // Gate outputs (voices 9-16, 8 voices)
        POLY_GATE_OUT_9 = 0,
        POLY_GATE_OUT_10,
        POLY_GATE_OUT_11,
        POLY_GATE_OUT_12,
        POLY_GATE_OUT_13,
        POLY_GATE_OUT_14,
        POLY_GATE_OUT_15,
        POLY_GATE_OUT_16,
        
        // CV outputs (voices 9-16)
        POLY_CV_OUT_9,
        POLY_CV_OUT_10,
        POLY_CV_OUT_11,
        POLY_CV_OUT_12,
        POLY_CV_OUT_13,
        POLY_CV_OUT_14,
        POLY_CV_OUT_15,
        POLY_CV_OUT_16,
        
        // Accent outputs (voices 9-16)
        POLY_ACCENT_OUT_9,
        POLY_ACCENT_OUT_10,
        POLY_ACCENT_OUT_11,
        POLY_ACCENT_OUT_12,
        POLY_ACCENT_OUT_13,
        POLY_ACCENT_OUT_14,
        POLY_ACCENT_OUT_15,
        POLY_ACCENT_OUT_16,
        
        NUM_OUTPUTS
    };
};

struct MeloDicerStraitWestExpander : Module {
    MeloDicerStraitWestExpander() {
        // Size to parent module's param/input counts
        config(MeloDicerIds::NUM_PARAMS, MeloDicerIds::NUM_INPUTS, StraitWestExpanderIds::NUM_OUTPUTS);
        
        // Configure rest probability params for voices 9-16
        for (int i = 0; i < 8; i++) {
            configParam(POLY_REST_PARAM_8 + i, 0.f, 1.f, 0.1f,
                       "Voice " + std::to_string(9 + i) + " Rest Probability");
        }
        
        // Configure DNA params for voices 9-16 (8 voices × 3 = 24 params)
        for (int i = 0; i < 8; i++) {
            int voice = 9 + i;
            int paramBase = POLY_DNA_VOICE_8_LEN + i * 3;
            configParam(paramBase,     1.f, 16.f, 16.f, "Voice " + std::to_string(voice) + " DNA Length");
            configParam(paramBase + 1, 0.f, 15.f,  0.f, "Voice " + std::to_string(voice) + " DNA Offset");
            configParam(paramBase + 2, 0.f, 15.f,  0.f, "Voice " + std::to_string(voice) + " DNA Rotation");
        }
        
        // Configure outputs for voices 9-16
        for (int i = 0; i < 8; i++) {
            int voice = 9 + i;
            configOutput(StraitWestExpanderIds::POLY_GATE_OUT_9 + i,
                        "Voice " + std::to_string(voice) + " Gate");
            configOutput(StraitWestExpanderIds::POLY_CV_OUT_9 + i,
                        "Voice " + std::to_string(voice) + " CV");
            configOutput(StraitWestExpanderIds::POLY_ACCENT_OUT_9 + i,
                        "Voice " + std::to_string(voice) + " Accent");
        }
    }

    // Zero-delay: parent module writes outputs directly
    void process(const ProcessArgs& args) override {}
};

struct MeloDicerStraitWestExpanderWidget : ModuleWidget {
    MeloDicerStraitWestExpanderWidget(MeloDicerStraitWestExpander* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/straits_west.svg")));
        
        // TODO (UI): Add module controls
        // - 8 rest probability knobs
        // - 24 DNA knobs (length, offset, rotation × 8 voices)
        // - 24 outputs (gate, CV, accent × 8 voices)
    }
};

Model* modelMeloDicerStraitWestExpander = createModel<MeloDicerStraitWestExpander, MeloDicerStraitWestExpanderWidget>("MeloDicerStraitWestExpander");
