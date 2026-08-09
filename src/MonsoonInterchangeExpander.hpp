#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// Left expander for Monsoon.
// Monsoon reads this module's inputs and params directly via the
// cachedExpander pointer — no message-passing protocol is used.
// The messages[2] double-buffer has been removed as it was dead code.
struct MonsoonInterchangeExpander : Module {
    // 3C-ii: this Interchange can also CV-modulate a Colonnades/Duo MICRO's scale-mask faders (not just
    // Monsoon's own SEMI faders). It binds to a Micro by pairId (reuse of the shared redDot pairing):
    //   followTarget == 0  -> AUTO: the nearest Micro hub either side (findPairHubEitherSide).
    //   followTarget >  0  -> the Micro whose pairId == followTarget, anywhere in the rack.
    // targetHalf says WHICH 12 of a 24-degree Micro these 12 CV inputs drive:
    //   1 -> degrees 0..11,  2 -> degrees 12..23.  For a 12-degree Micro, half 2 addresses degrees
    //   12..23 which don't exist → inert (satisfies "second Interchange on a Micro-12 is inert").
    // The Micro READS these (its own process() is the single writer of weight[]); this stays passive.
    int followTarget = 0;   // 0 = adjacency; >0 = Micro pairId to follow (persisted)
    int targetHalf   = 1;   // 1 or 2 (persisted)

    MonsoonInterchangeExpander() {
        config(MonsoonIds::NUM_EXPANDER_PARAMS, MonsoonIds::NUM_EXPANDER_INPUTS, 0, 0);

        for (int i = 0; i < 12; i++) {
            configInput(MonsoonIds::EXPANDER_SEMI_CV_INPUT_0 + i,
                        string::f("Semitone %d CV", i + 1));
            configParam(MonsoonIds::EXPANDER_SEMI_ATTENUVERTER_0 + i,
                        -1.f, 1.f, 0.f, string::f("Semitone %d CV Attenuverter", i + 1));
        }

        configInput(MonsoonIds::EXPANDER_OCT_LO_CV_INPUT, "Octave Low CV");
        configParam(MonsoonIds::EXPANDER_OCT_LO_ATTENUVERTER, -1.f, 1.f, 0.f, "Octave Low CV Attenuverter");

        configInput(MonsoonIds::EXPANDER_OCT_HI_CV_INPUT, "Octave High CV");
        configParam(MonsoonIds::EXPANDER_OCT_HI_ATTENUVERTER, -1.f, 1.f, 0.f, "Octave High CV Attenuverter");
    }

    void process(const ProcessArgs& args) override {}

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "followTarget", json_integer(followTarget));
        json_object_set_new(root, "targetHalf",   json_integer(targetHalf));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* j = json_object_get(root, "followTarget")) followTarget = (int)json_integer_value(j);
        if (json_t* j = json_object_get(root, "targetHalf"))   targetHalf   = (int)json_integer_value(j);
    }
};
