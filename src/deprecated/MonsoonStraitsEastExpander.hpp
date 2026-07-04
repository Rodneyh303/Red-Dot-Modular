#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

// Output IDs local to the Straits East expander (voices 2-8, plus voice 1 in polyphonic output).
namespace PolyVoiceExpanderIds {
    enum OutputIds {
        // Individual outputs per voice (voices 2-8)
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
        POLY_ACCENT_OUT_1,
        POLY_ACCENT_OUT_2,
        POLY_ACCENT_OUT_3,
        POLY_ACCENT_OUT_4,
        POLY_ACCENT_OUT_5,
        POLY_ACCENT_OUT_6,
        POLY_ACCENT_OUT_7,
        
        // Poly outputs for voices 1-8 (voice 1 mono + voices 2-8)
        POLY_GATE_1_8_OUT,
        POLY_CV_1_8_OUT,
        
        NUM_OUTPUTS
    };
}

struct MonsoonStraitsEastExpander : Module {
    MonsoonStraitsEastExpander() {
        // Sizing to NUM_PARAMS and NUM_INPUTS is required because the expander
        // uses IDs from the main MonsoonIds namespace, which exceed the 
        // local count of controls on this specific panel.
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, PolyVoiceExpanderIds::NUM_OUTPUTS, 0);

        for (int i = 0; i < 7; i++) {
            configParam(MonsoonIds::POLY_REST_PARAM_1 + i, 0.f, 1.f, 0.1f,
                        "Voice " + std::to_string(i + 2) + " Rest Probability");
        }
        
        // Rest probability modulation inputs with attenuverters (voices 2-8)
        for (int i = 0; i < 7; i++) {
            configParam(MonsoonIds::POLY_REST_MOD_ATT_1 + i, -1.f, 1.f, 0.f,
                       "Voice " + std::to_string(i + 2) + " Rest Mod Attenuverter");
            configInput(MonsoonIds::POLY_REST_MOD_CV_INPUT_1 + i,
                       "Voice " + std::to_string(i + 2) + " Rest Mod CV");
        }
        
        configInput(MonsoonIds::POLY_REST_CV_INPUT, "Poly Rest CV");

        // Accent as a poly lane — per-voice accent probability + mod att + mod CV (voices 2-8)
        for (int i = 0; i < 7; i++) {
            configParam(MonsoonIds::POLY_ACCENT_PARAM_1 + i, 0.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " Accent Probability");
            configParam(MonsoonIds::POLY_ACCENT_MOD_ATT_1 + i, -1.f, 1.f, 0.f,
                        "Voice " + std::to_string(i + 2) + " Accent Mod Attenuverter");
            configInput(MonsoonIds::POLY_ACCENT_MOD_CV_INPUT_1 + i,
                        "Voice " + std::to_string(i + 2) + " Accent Mod CV");
        }
        configInput(MonsoonIds::POLY_ACCENT_CV_INPUT, "Poly Accent CV");

        // Configure individual outputs
        for (int i = 0; i < 7; i++) {
            configOutput(PolyVoiceExpanderIds::POLY_GATE_OUT_1 + i,
                         "Voice " + std::to_string(i + 2) + " Gate");
            configOutput(PolyVoiceExpanderIds::POLY_CV_OUT_1 + i,
                         "Voice " + std::to_string(i + 2) + " CV (Pitch)");
            configOutput(PolyVoiceExpanderIds::POLY_ACCENT_OUT_1 + i,
                         "Voice " + std::to_string(i + 2) + " Accent Gate");
        }
        
        // Configure poly outputs for voices 1-8
        configOutput(PolyVoiceExpanderIds::POLY_GATE_1_8_OUT, "Poly Gate 1-8");
        configOutput(PolyVoiceExpanderIds::POLY_CV_1_8_OUT, "Poly CV 1-8");
    }

    // Monsoon writes gate and CV outputs directly via the cached pointer.
    void process(const ProcessArgs& args) override {}
};

