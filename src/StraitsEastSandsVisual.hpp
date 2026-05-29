#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsEastVisualIds {
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
        CV_SPREAD_R_INPUT,   // poly: modulates spread REST per voice
        CV_SPREAD_M_INPUT,   // poly: modulates spread MELODY per voice
        CV_SPREAD_O_INPUT,   // poly: modulates spread OCTAVE per voice
        NUM_INPUTS
    };

    // Panel width: 28HP
    static constexpr float W_MM = 142.24f;

    // L/O/R param IDs (same positions as DeepStraitsSandsEast)
    inline int lorId(int v, int lane, int c) {
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + v * 3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + v * 3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + v * 3 + c;
    }

    // Spread/interp param IDs — Monsoon reads these directly from cached pointer
    // East uses voices 0-6 (voices 2-8), same global indices
    inline int restInterpId  (int v) { return POLY_REST_INTERP_1   + v; }
    inline int melodyInterpId(int v) { return POLY_MELODY_INTERP_1 + v; }
    inline int octaveInterpId(int v) { return POLY_OCTAVE_INTERP_1 + v; }
    inline int interpId(int v, int lane) {
        if (lane == 0) return restInterpId(v);
        if (lane == 1) return melodyInterpId(v);
        return              octaveInterpId(v);
    }
}

struct StraitsEastSandsVisual : Module {
    // Context-menu state (serialised)
    bool  interpUseMono     = false;    // false=AVERAGE_POLY, true=MONO_DRAW
    int   cvVoiceMask       = 0b1111111; // LOR CV: bits 0-6
    int   cvLaneMask        = 0b111;     // LOR CV: lanes 0-2
    int   cvSpreadVoiceMask = 0b1111111; // spread CV: bits 0-6 (all on = apply to all)

    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        config(MonsoonIds::NUM_PARAMS, NUM_INPUTS, 0, 0);

        // 3 display trimpots (selected voice's spread)
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

        // L/O/R params (92+)
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

        // Spread/interp params — Monsoon reads these directly via cached pointer
        // REST_INTERP (242+v), MELODY_INTERP (257+v), OCTAVE_INTERP (272+v) for v=0..6
        static const char* ln[3] = {"REST","MELODY","OCTAVE"};
        for (int v = 0; v < 7; ++v) {
            std::string vl = "Voice " + std::to_string(v+2);
            configParam(restInterpId(v),   0.f, 1.f, 0.f, vl + " Spread REST");
            configParam(melodyInterpId(v), 0.f, 1.f, 0.f, vl + " Spread MELODY");
            configParam(octaveInterpId(v), 0.f, 1.f, 0.f, vl + " Spread OCTAVE");
        }
    }

    void process(const ProcessArgs&) override {}

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "interpUseMono",     json_boolean(interpUseMono));
        json_object_set_new(root, "cvVoiceMask",       json_integer(cvVoiceMask));
        json_object_set_new(root, "cvLaneMask",        json_integer(cvLaneMask));
        json_object_set_new(root, "cvSpreadVoiceMask", json_integer(cvSpreadVoiceMask));
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (auto* j = json_object_get(root, "interpUseMono"))     interpUseMono     = json_boolean_value(j);
        if (auto* j = json_object_get(root, "cvVoiceMask"))       cvVoiceMask       = json_integer_value(j);
        if (auto* j = json_object_get(root, "cvLaneMask"))        cvLaneMask        = json_integer_value(j);
        if (auto* j = json_object_get(root, "cvSpreadVoiceMask")) cvSpreadVoiceMask = json_integer_value(j);
    }
};
