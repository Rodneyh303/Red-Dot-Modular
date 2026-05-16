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
        POLY_ACCENT_OUT_1,     // New: accent gate per voice
        POLY_ACCENT_OUT_2,
        POLY_ACCENT_OUT_3,
        POLY_ACCENT_OUT_4,
        POLY_ACCENT_OUT_5,
        POLY_ACCENT_OUT_6,
        POLY_ACCENT_OUT_7,
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

        // Individual Poly DNA Controls for 7 voices
        for (int i = 0; i < 7; i++) {
            std::string voiceLabel = "Voice " + std::to_string(i + 2);
            configParam(MeloDicerIds::POLY_DNA_VOICE_1_LEN + i * 3, 1.f, 16.f, 16.f, voiceLabel + " DNA Length");
            configParam(MeloDicerIds::POLY_DNA_VOICE_1_OFF + i * 3, 0.f, 15.f, 0.f, voiceLabel + " DNA Offset");
            configParam(MeloDicerIds::POLY_DNA_VOICE_1_ROT + i * 3, 0.f, 15.f, 0.f, voiceLabel + " DNA Rotation");
        }

        for (int i = 0; i < 7; i++) {
            configOutput(PolyVoiceExpanderIds::POLY_GATE_OUT_1 + i,
                         "Voice " + std::to_string(i + 2) + " Gate");
            configOutput(PolyVoiceExpanderIds::POLY_CV_OUT_1 + i,
                         "Voice " + std::to_string(i + 2) + " CV (Pitch)");
            configOutput(PolyVoiceExpanderIds::POLY_ACCENT_OUT_1 + i,
                         "Voice " + std::to_string(i + 2) + " Accent Gate");
        }
    }

    // MeloDicer writes gate and CV outputs directly via the cached pointer.
    void process(const ProcessArgs& args) override {}
};
