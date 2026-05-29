#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsEastVisualIds {
    enum SpreadParamId {
        // Spread trimpots: one per lane per voice (0-20)
        SPREAD_V0_R = 0, SPREAD_V0_M, SPREAD_V0_O,
        SPREAD_V1_R,     SPREAD_V1_M, SPREAD_V1_O,
        SPREAD_V2_R,     SPREAD_V2_M, SPREAD_V2_O,
        SPREAD_V3_R,     SPREAD_V3_M, SPREAD_V3_O,
        SPREAD_V4_R,     SPREAD_V4_M, SPREAD_V4_O,
        SPREAD_V5_R,     SPREAD_V5_M, SPREAD_V5_O,
        SPREAD_V6_R,     SPREAD_V6_M, SPREAD_V6_O,
        // CV depth attenuverter (21)
        CV_DEPTH_PARAM,
        NUM_SPREAD_PARAMS  // = 22, safely below POLY_DNA_VOICE_1_LEN (152)
    };

    enum InputId {
        CV_LEN_INPUT = 0,
        CV_OFF_INPUT,
        CV_ROT_INPUT,
        NUM_INPUTS
    };

    inline int lorId(int v, int lane, int c) {
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + v * 3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + v * 3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + v * 3 + c;
    }
}

struct StraitsEastSandsVisual : Module {
    // Context-menu state (serialised)
    bool  interpUseMono = false;       // false=AVERAGE_POLY, true=MONO_DRAW
    int   cvVoiceMask   = 0b1111111;   // bits 0-6 = voices 0-6 (all on)
    int   cvLaneMask    = 0b111;       // bits 0-2 = REST/MELODY/OCTAVE (all on)

    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        config(MonsoonIds::NUM_PARAMS, NUM_INPUTS, 0, 0);

        static const char* ln[3] = {"REST","MELODY","OCTAVE"};
        for (int v = 0; v < 7; ++v)
            for (int l = 0; l < 3; ++l)
                configParam(SPREAD_V0_R + v*3 + l, 0.f, 1.f, 0.f,
                    string::f("Voice %d Spread %s", v+2, ln[l]));

        configParam(CV_DEPTH_PARAM, -1.f, 1.f, 0.f, "CV Depth");

        configInput(CV_LEN_INPUT, "CV Length (poly)");
        configInput(CV_OFF_INPUT, "CV Offset (poly)");
        configInput(CV_ROT_INPUT, "CV Rotation (poly)");

        for (int v = 0; v < 7; ++v) {
            std::string vl = "Voice " + std::to_string(v+2);
            configParam(POLY_DNA_VOICE_1_LEN    + v*3,     1.f, 16.f, 16.f, vl + " REST Length");
            configParam(POLY_DNA_VOICE_1_LEN    + v*3 + 1, 0.f, 15.f,  0.f, vl + " REST Offset");
            configParam(POLY_DNA_VOICE_1_LEN    + v*3 + 2, 0.f, 15.f,  0.f, vl + " REST Rotation");
            configParam(POLY_MELODY_VOICE_1_LEN + v*3,     1.f, 16.f, 16.f, vl + " MELODY Length");
            configParam(POLY_MELODY_VOICE_1_LEN + v*3 + 1, 0.f, 15.f,  0.f, vl + " MELODY Offset");
            configParam(POLY_MELODY_VOICE_1_LEN + v*3 + 2, 0.f, 15.f,  0.f, vl + " MELODY Rotation");
            configParam(POLY_OCTAVE_VOICE_1_LEN + v*3,     1.f, 16.f, 16.f, vl + " OCTAVE Length");
            configParam(POLY_OCTAVE_VOICE_1_LEN + v*3 + 1, 0.f, 15.f,  0.f, vl + " OCTAVE Offset");
            configParam(POLY_OCTAVE_VOICE_1_LEN + v*3 + 2, 0.f, 15.f,  0.f, vl + " OCTAVE Rotation");
        }
    }

    void process(const ProcessArgs&) override {}

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "interpUseMono", json_boolean(interpUseMono));
        json_object_set_new(root, "cvVoiceMask",   json_integer(cvVoiceMask));
        json_object_set_new(root, "cvLaneMask",    json_integer(cvLaneMask));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (auto* j = json_object_get(root, "interpUseMono")) interpUseMono = json_boolean_value(j);
        if (auto* j = json_object_get(root, "cvVoiceMask"))   cvVoiceMask   = json_integer_value(j);
        if (auto* j = json_object_get(root, "cvLaneMask"))    cvLaneMask    = json_integer_value(j);
    }
};
