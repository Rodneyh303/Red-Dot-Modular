#pragma once
#include <rack.hpp>
#include "MeloDicer.hpp"

using namespace rack;

// Left expander for MeloDicer.
// MeloDicer reads this module's inputs and params directly via the
// cachedExpander pointer — no message-passing protocol is used.
// The messages[2] double-buffer has been removed as it was dead code.
struct MeloDicerExpander : Module {
    MeloDicerExpander() {
        config(MeloDicerIds::NUM_EXPANDER_PARAMS, MeloDicerIds::NUM_EXPANDER_INPUTS, 0, 0);

        for (int i = 0; i < 12; i++) {
            configInput(MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + i,
                        string::f("Semitone %d CV", i + 1));
            configParam(MeloDicerIds::EXPANDER_SEMI_ATTENUVERTER_0 + i,
                        -1.f, 1.f, 0.f, string::f("Semitone %d CV Attenuverter", i + 1));
        }

        configInput(MeloDicerIds::EXPANDER_OCT_LO_CV_INPUT, "Octave Low CV");
        configParam(MeloDicerIds::EXPANDER_OCT_LO_ATTENUVERTER, -1.f, 1.f, 0.f, "Octave Low CV Attenuverter");

        configInput(MeloDicerIds::EXPANDER_OCT_HI_CV_INPUT, "Octave High CV");
        configParam(MeloDicerIds::EXPANDER_OCT_HI_ATTENUVERTER, -1.f, 1.f, 0.f, "Octave High CV Attenuverter");
    }

    void process(const ProcessArgs& args) override {}
};
