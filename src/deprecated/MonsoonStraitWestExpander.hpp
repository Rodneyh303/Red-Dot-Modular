#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

// Output IDs local to Straits West expander (voices 9-16).
namespace StraitWestExpanderIds {
    enum OutputIds {
        // Individual outputs per voice (voices 9-16)
        POLY_GATE_OUT_1 = 0,
        POLY_GATE_OUT_2,
        POLY_GATE_OUT_3,
        POLY_GATE_OUT_4,
        POLY_GATE_OUT_5,
        POLY_GATE_OUT_6,
        POLY_GATE_OUT_7,
        POLY_GATE_OUT_8,
        POLY_CV_OUT_1,
        POLY_CV_OUT_2,
        POLY_CV_OUT_3,
        POLY_CV_OUT_4,
        POLY_CV_OUT_5,
        POLY_CV_OUT_6,
        POLY_CV_OUT_7,
        POLY_CV_OUT_8,
        POLY_ACCENT_OUT_1,
        POLY_ACCENT_OUT_2,
        POLY_ACCENT_OUT_3,
        POLY_ACCENT_OUT_4,
        POLY_ACCENT_OUT_5,
        POLY_ACCENT_OUT_6,
        POLY_ACCENT_OUT_7,
        POLY_ACCENT_OUT_8,
        
        // Poly outputs for voices 1-16 (all voices aggregated)
        POLY_GATE_1_16_OUT,
        POLY_CV_1_16_OUT,
        
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
        
        // Rest probability modulation inputs with attenuverters (voices 9-16)
        for (int i = 0; i < 8; i++) {
            configParam(MonsoonIds::POLY_REST_MOD_ATT_8 + i, -1.f, 1.f, 0.f,
                       "Voice " + std::to_string(i + 9) + " Rest Mod Attenuverter");
            configInput(MonsoonIds::POLY_REST_MOD_CV_INPUT_8 + i,
                       "Voice " + std::to_string(i + 9) + " Rest Mod CV");
        }
        
        configInput(MonsoonIds::POLY_REST_CV_INPUT, "Poly Rest CV");

        // Accent as a poly lane — per-voice accent probability + mod att + mod CV (voices 9-16)
        for (int i = 0; i < 8; i++) {
            configParam(MonsoonIds::POLY_ACCENT_PARAM_8 + i, 0.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 9) + " Accent Probability");
            configParam(MonsoonIds::POLY_ACCENT_MOD_ATT_8 + i, -1.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 9) + " Accent Mod Attenuverter");
            configInput(MonsoonIds::POLY_ACCENT_MOD_CV_INPUT_8 + i,
                        "Voice " + std::to_string(i + 9) + " Accent Mod CV");
        }
        configInput(MonsoonIds::POLY_ACCENT_CV_INPUT, "Poly Accent CV");

        // Configure individual outputs
        for (int i = 0; i < 8; i++) {
            configOutput(StraitWestExpanderIds::POLY_GATE_OUT_1 + i,
                         "Voice " + std::to_string(i + 9) + " Gate");
            configOutput(StraitWestExpanderIds::POLY_CV_OUT_1 + i,
                         "Voice " + std::to_string(i + 9) + " CV (Pitch)");
            configOutput(StraitWestExpanderIds::POLY_ACCENT_OUT_1 + i,
                         "Voice " + std::to_string(i + 9) + " Accent Gate");
        }
        
        // Configure poly outputs for voices 1-16 (aggregated)
        configOutput(StraitWestExpanderIds::POLY_GATE_1_16_OUT, "Poly Gate 1-16");
        configOutput(StraitWestExpanderIds::POLY_CV_1_16_OUT, "Poly CV 1-16");
    }

    void process(const ProcessArgs& args) override {}
};
