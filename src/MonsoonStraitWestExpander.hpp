#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

// Output IDs local to Straits West expander (voices 9-16).
namespace StraitWestExpanderIds {
    enum OutputIds {
        POLY_GATE_OUT_1 = 0,   // Voice 9
        POLY_GATE_OUT_2,       // Voice 10
        POLY_GATE_OUT_3,
        POLY_GATE_OUT_4,
        POLY_GATE_OUT_5,
        POLY_GATE_OUT_6,
        POLY_GATE_OUT_7,
        POLY_GATE_OUT_8,       // Voice 16
        POLY_CV_OUT_1,
        POLY_CV_OUT_2,
        POLY_CV_OUT_3,
        POLY_CV_OUT_4,
        POLY_CV_OUT_5,
        POLY_CV_OUT_6,
        POLY_CV_OUT_7,
        POLY_CV_OUT_8,
        POLY_ACCENT_OUT_1,     // Voice 9
        POLY_ACCENT_OUT_2,
        POLY_ACCENT_OUT_3,
        POLY_ACCENT_OUT_4,
        POLY_ACCENT_OUT_5,
        POLY_ACCENT_OUT_6,
        POLY_ACCENT_OUT_7,
        POLY_ACCENT_OUT_8,     // Voice 16
        NUM_OUTPUTS
    };
}

struct MonsoonStraitWestExpander : Module {
    MonsoonStraitWestExpander() {
        // Size to parent's param/input counts (voices 9-16)
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, StraitWestExpanderIds::NUM_OUTPUTS, 0);

        // Rest probability params for voices 9-16 (POLY_REST_PARAM_8-15)
        for (int i = 0; i < 8; i++) {
            configParam(MonsoonIds::POLY_REST_PARAM_8 + i, 0.f, 1.f, 0.1f,
                        "Voice " + std::to_string(i + 9) + " Rest Probability");
        }
        configInput(MonsoonIds::POLY_REST_CV_INPUT, "Poly Rest CV");

        // Individual Poly DNA Controls for 8 voices (9-16)
        for (int i = 0; i < 8; i++) {
            std::string voiceLabel = "Voice " + std::to_string(i + 9);
            configParam(MonsoonIds::POLY_DNA_VOICE_8_LEN + i * 3, 1.f, 16.f, 16.f, voiceLabel + " DNA Length");
            configParam(MonsoonIds::POLY_DNA_VOICE_8_OFF + i * 3, 0.f, 15.f, 0.f, voiceLabel + " DNA Offset");
            configParam(MonsoonIds::POLY_DNA_VOICE_8_ROT + i * 3, 0.f, 15.f, 0.f, voiceLabel + " DNA Rotation");
        }

        // Configure outputs
        for (int i = 0; i < 8; i++) {
            configOutput(StraitWestExpanderIds::POLY_GATE_OUT_1 + i,
                         "Voice " + std::to_string(i + 9) + " Gate");
            configOutput(StraitWestExpanderIds::POLY_CV_OUT_1 + i,
                         "Voice " + std::to_string(i + 9) + " CV (Pitch)");
            configOutput(StraitWestExpanderIds::POLY_ACCENT_OUT_1 + i,
                         "Voice " + std::to_string(i + 9) + " Accent Gate");
        }
    }

    void process(const ProcessArgs& args) override {}
};
