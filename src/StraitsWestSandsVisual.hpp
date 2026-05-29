#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsWestVisualIds {
    enum SpreadParamId {
        // Spread trimpots — always show selected voice (0-2)
        SPREAD_R = 0,
        SPREAD_M,
        SPREAD_O,
        // CV depth attenuverter (3)
        CV_DEPTH_PARAM,
        NUM_SPREAD_PARAMS  // = 4, safely below POLY_DNA_VOICE_1_LEN (92)
    };

    enum InputId {
        CV_LEN_INPUT = 0,
        CV_OFF_INPUT,
        CV_ROT_INPUT,
        CV_SPREAD_R_INPUT,
        CV_SPREAD_M_INPUT,
        CV_SPREAD_O_INPUT,
        NUM_INPUTS
    };

    // Panel width: 28HP
    static constexpr float W_MM = 142.24f;

    // West voices: local v=0..7 → global v=7..14 (voices 9-16)
    inline int lorId(int localV, int lane, int c) {
        int gv = localV + 7;
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + gv * 3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + gv * 3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + gv * 3 + c;
    }

    // Spread/interp param IDs — Monsoon reads these directly from cached pointer
    // West uses global voices 7-14 for INTERP params
    inline int restInterpId  (int localV) { return POLY_REST_INTERP_1   + localV + 7; }
    inline int melodyInterpId(int localV) { return POLY_MELODY_INTERP_1 + localV + 7; }
    inline int octaveInterpId(int localV) { return POLY_OCTAVE_INTERP_1 + localV + 7; }
    inline int interpId(int localV, int lane) {
        if (lane == 0) return restInterpId(localV);
        if (lane == 1) return melodyInterpId(localV);
        return              octaveInterpId(localV);
    }
}

struct StraitsWestSandsVisual : Module {
    bool  interpUseMono = false;
    int   cvVoiceMask   = 0b11111111;  // bits 0-7 = voices 0-7 (all on)
    int   cvLaneMask        = 0b111;
    int   cvSpreadVoiceMask = 0b11111111; // bits 0-7 = voices 0-7

    StraitsWestSandsVisual() {
        using namespace StraitsWestVisualIds;
        config(MonsoonIds::NUM_PARAMS, NUM_INPUTS, 0, 0);

        configParam(SPREAD_R, 0.f, 1.f, 0.f, "Spread REST");
        configParam(SPREAD_M, 0.f, 1.f, 0.f, "Spread MELODY");
        configParam(SPREAD_O, 0.f, 1.f, 0.f, "Spread OCTAVE");
        configParam(CV_DEPTH_PARAM, -1.f, 1.f, 0.f, "CV Depth");

        configInput(CV_LEN_INPUT,      "CV Length (poly)");
        configInput(CV_OFF_INPUT,      "CV Offset (poly)");
        configInput(CV_ROT_INPUT,      "CV Rotation (poly)");
        configInput(CV_SPREAD_R_INPUT, "CV Spread REST (poly)");
        configInput(CV_SPREAD_M_INPUT, "CV Spread MELODY (poly)");
        configInput(CV_SPREAD_O_INPUT, "CV Spread OCTAVE (poly)");

        // L/O/R params — West uses global voice indices 7-14
        for (int localV = 0; localV < 8; ++localV) {
            int gv = localV + 7;
            std::string vl = "Voice " + std::to_string(localV + 9);
            configParam(POLY_DNA_VOICE_1_LEN    + gv*3,     1.f, 16.f, 16.f, vl + " REST Length");
            configParam(POLY_DNA_VOICE_1_LEN    + gv*3 + 1, 0.f, 15.f,  0.f, vl + " REST Offset");
            configParam(POLY_DNA_VOICE_1_LEN    + gv*3 + 2, 0.f, 15.f,  0.f, vl + " REST Rotation");
            configParam(POLY_MELODY_VOICE_1_LEN + gv*3,     1.f, 16.f, 16.f, vl + " MELODY Length");
            configParam(POLY_MELODY_VOICE_1_LEN + gv*3 + 1, 0.f, 15.f,  0.f, vl + " MELODY Offset");
            configParam(POLY_MELODY_VOICE_1_LEN + gv*3 + 2, 0.f, 15.f,  0.f, vl + " MELODY Rotation");
            configParam(POLY_OCTAVE_VOICE_1_LEN + gv*3,     1.f, 16.f, 16.f, vl + " OCTAVE Length");
            configParam(POLY_OCTAVE_VOICE_1_LEN + gv*3 + 1, 0.f, 15.f,  0.f, vl + " OCTAVE Offset");
            configParam(POLY_OCTAVE_VOICE_1_LEN + gv*3 + 2, 0.f, 15.f,  0.f, vl + " OCTAVE Rotation");
        }

        // Spread/interp params — West global voices 7-14 → INTERP IDs 249-256 / 264-271 / 279-286
        for (int localV = 0; localV < 8; ++localV) {
            std::string vl = "Voice " + std::to_string(localV + 9);
            configParam(restInterpId(localV),   0.f, 1.f, 0.f, vl + " Spread REST");
            configParam(melodyInterpId(localV), 0.f, 1.f, 0.f, vl + " Spread MELODY");
            configParam(octaveInterpId(localV), 0.f, 1.f, 0.f, vl + " Spread OCTAVE");
        }
    }

    void process(const ProcessArgs&) override {}

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "interpUseMono", json_boolean(interpUseMono));
        json_object_set_new(root, "cvVoiceMask",   json_integer(cvVoiceMask));
        json_object_set_new(root, "cvLaneMask",    json_integer(cvLaneMask));
        json_object_set_new(root, "cvSpreadVoiceMask", json_integer(cvSpreadVoiceMask));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (auto* j = json_object_get(root, "interpUseMono")) interpUseMono = json_boolean_value(j);
        if (auto* j = json_object_get(root, "cvVoiceMask"))   cvVoiceMask   = json_integer_value(j);
        if (auto* j = json_object_get(root, "cvLaneMask"))    cvLaneMask    = json_integer_value(j);
        if (auto* j = json_object_get(root, "cvSpreadVoiceMask")) cvSpreadVoiceMask = json_integer_value(j);
    }
};
