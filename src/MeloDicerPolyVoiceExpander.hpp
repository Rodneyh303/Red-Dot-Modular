#pragma once
#include <rack.hpp>
#include "MeloDicer.hpp"

using namespace rack;
using namespace MeloDicerIds;

struct MeloDicerPolyVoiceExpander : Module {
    MeloDicerPolyVoiceExpander() {
        config(7, 1, 0, 0); // 7 rest knobs, 1 poly rest CV input
        for (int i = 0; i < 7; i++) { // POLY_REST_PARAM_1 is for voice 2, so loop 7 times for voices 2-8
            configParam(MeloDicerIds::POLY_REST_PARAM_1 + i, 0.f, 1.f, 0.1f, "Voice " + std::to_string(i + 2) + " Rest Probability");
        }
        configInput(MeloDicerIds::POLY_REST_CV_INPUT, "Poly Rest CV");
    }

    void process(const ProcessArgs& args) override {}
};