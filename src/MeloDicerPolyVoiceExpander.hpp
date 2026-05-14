#pragma once
#include <rack.hpp>
#include "MeloDicer.hpp"

using namespace rack;
using namespace MeloDicerIds;

// Output IDs local to the PolyVoice expander.
// Gates 0-6 = voices 2-8, CVs 7-13 = voices 2-8.
namespace PolyVoiceExpanderIds {
    enum OutputIds {
        POLY_GATE_OUT_1 = 0,
        POLY_GATE_OUT_2,
        POLY_GATE_OUT_3,
        POLY_GATE_OUT_4,
        POLY_GATE_OUT_5,
        POLY_GATE_OUT_6,
        POLY_GATE_OUT_7,
        POLY_CV_OUT_1,
        POLY_CV_OUT_2,
        POLY_CV_OUT_3,
        POLY_CV_OUT_4,
        POLY_CV_OUT_5,
        POLY_CV_OUT_6,
        POLY_CV_OUT_7,
        NUM_OUTPUTS
    };
}

struct MeloDicerPolyVoiceExpander : Module {
    MeloDicerPolyVoiceExpander() {
        // Sizing to NUM_PARAMS and NUM_INPUTS is required because the expander
        // uses IDs from the main MeloDicerIds namespace, which exceed the 
        // local count of controls on this specific panel.
        config(MeloDicerIds::NUM_PARAMS, MeloDicerIds::NUM_INPUTS, PolyVoiceExpanderIds::NUM_OUTPUTS, 0);

        for (int i = 0; i < 7; i++) {
            configParam(MeloDicerIds::POLY_REST_PARAM_1 + i, 0.f, 1.f, 0.1f,
                        "Voice " + std::to_string(i + 2) + " Rest Probability");
        }
        configInput(MeloDicerIds::POLY_REST_CV_INPUT, "Poly Rest CV");

        // Poly DNA Controls: Setting 1-16 range prevents the 50% rounding jump
        configParam(MeloDicerIds::POLY_DNA_LEN_PARAM, 1.f, 16.f, 16.f, "Poly DNA Length");
        configParam(MeloDicerIds::POLY_DNA_OFF_PARAM, 0.f, 15.f, 0.f, "Poly DNA Offset");
        configParam(MeloDicerIds::POLY_DNA_ROT_PARAM, 0.f, 15.f, 0.f, "Poly DNA Rotation");

        for (int i = 0; i < 7; i++) {
            configOutput(PolyVoiceExpanderIds::POLY_GATE_OUT_1 + i,
                         "Voice " + std::to_string(i + 2) + " Gate");
            configOutput(PolyVoiceExpanderIds::POLY_CV_OUT_1 + i,
                         "Voice " + std::to_string(i + 2) + " CV (Pitch)");
        }
    }

    // MeloDicer writes gate and CV outputs directly via the cached pointer.
    void process(const ProcessArgs& args) override {}
};
