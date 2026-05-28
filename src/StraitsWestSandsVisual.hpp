#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsWestVisualIds {
    enum SpreadParamId {
        SPREAD_V0_R = 0, SPREAD_V0_M, SPREAD_V0_O,
        SPREAD_V1_R,     SPREAD_V1_M, SPREAD_V1_O,
        SPREAD_V2_R,     SPREAD_V2_M, SPREAD_V2_O,
        SPREAD_V3_R,     SPREAD_V3_M, SPREAD_V3_O,
        SPREAD_V4_R,     SPREAD_V4_M, SPREAD_V4_O,
        SPREAD_V5_R,     SPREAD_V5_M, SPREAD_V5_O,
        SPREAD_V6_R,     SPREAD_V6_M, SPREAD_V6_O,
        SPREAD_V7_R,     SPREAD_V7_M, SPREAD_V7_O,
        INTERP_TARGET,
        NUM_SPREAD_PARAMS  // = 25, safely below POLY_DNA_VOICE_1_LEN (152)
    };
    // West voices: v=7-14 in global index, v=0-7 in local (widget) index
    // Param IDs use global voice index
    inline int lorId(int localV, int lane, int c) {
        int globalV = localV + 7;  // West = voices 9-16 → indices 7-14
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + globalV * 3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + globalV * 3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + globalV * 3 + c;
    }
}

struct StraitsWestSandsVisual : Module {
    StraitsWestSandsVisual() {
        using namespace StraitsWestVisualIds;
        config(MonsoonIds::NUM_PARAMS, 0, 0, 0);

        static const char* ln[3] = {"REST","MELODY","OCTAVE"};
        for (int v = 0; v < 8; ++v) {
            for (int l = 0; l < 3; ++l)
                configParam(SPREAD_V0_R + v*3 + l, 0.f, 1.f, 0.f,
                    string::f("Voice %d Spread %s", v+9, ln[l]));
        }
        configSwitch(INTERP_TARGET, 0.f, 1.f, 0.f, "Interp Target",
                     {"Avg Active Poly", "Mono Draw"});

        // L/O/R params at MonsoonIds positions — West voices are global indices 7-14
        for (int localV = 0; localV < 8; ++localV) {
            int globalV = localV + 7;
            std::string vl = "Voice " + std::to_string(localV + 9);
            configParam(POLY_DNA_VOICE_1_LEN    + globalV*3, 1.f, 16.f, 16.f, vl + " REST Length");
            configParam(POLY_DNA_VOICE_1_LEN    + globalV*3 + 1, 0.f, 15.f, 0.f, vl + " REST Offset");
            configParam(POLY_DNA_VOICE_1_LEN    + globalV*3 + 2, 0.f, 15.f, 0.f, vl + " REST Rotation");
            configParam(POLY_MELODY_VOICE_1_LEN + globalV*3, 1.f, 16.f, 16.f, vl + " MELODY Length");
            configParam(POLY_MELODY_VOICE_1_LEN + globalV*3 + 1, 0.f, 15.f, 0.f, vl + " MELODY Offset");
            configParam(POLY_MELODY_VOICE_1_LEN + globalV*3 + 2, 0.f, 15.f, 0.f, vl + " MELODY Rotation");
            configParam(POLY_OCTAVE_VOICE_1_LEN + globalV*3, 1.f, 16.f, 16.f, vl + " OCTAVE Length");
            configParam(POLY_OCTAVE_VOICE_1_LEN + globalV*3 + 1, 0.f, 15.f, 0.f, vl + " OCTAVE Offset");
            configParam(POLY_OCTAVE_VOICE_1_LEN + globalV*3 + 2, 0.f, 15.f, 0.f, vl + " OCTAVE Rotation");
        }
    }
    void process(const ProcessArgs&) override {}
};
