#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

// Spread param indices (sit below poly DNA params which start at ~152)
namespace StraitsEastVisualIds {
    enum SpreadParamId {
        SPREAD_V0_R = 0, SPREAD_V0_M, SPREAD_V0_O,
        SPREAD_V1_R,     SPREAD_V1_M, SPREAD_V1_O,
        SPREAD_V2_R,     SPREAD_V2_M, SPREAD_V2_O,
        SPREAD_V3_R,     SPREAD_V3_M, SPREAD_V3_O,
        SPREAD_V4_R,     SPREAD_V4_M, SPREAD_V4_O,
        SPREAD_V5_R,     SPREAD_V5_M, SPREAD_V5_O,
        SPREAD_V6_R,     SPREAD_V6_M, SPREAD_V6_O,
        INTERP_TARGET,
        NUM_SPREAD_PARAMS  // = 22, all safely below POLY_DNA_VOICE_1_LEN (152)
    };
    // L/O/R param IDs sit at MonsoonIds positions (152+) — same as DeepStraitsSandsEast
    // REST:   POLY_DNA_VOICE_1_LEN    + v*3 + {0=LEN, 1=OFF, 2=ROT}
    // MELODY: POLY_MELODY_VOICE_1_LEN + v*3 + {0=LEN, 1=OFF, 2=ROT}
    // OCTAVE: POLY_OCTAVE_VOICE_1_LEN + v*3 + {0=LEN, 1=OFF, 2=ROT}
    inline int lorId(int v, int lane, int c) {
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + v * 3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + v * 3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + v * 3 + c;
    }
}

struct StraitsEastSandsVisual : Module {
    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        // Must size to NUM_PARAMS so poly DNA param IDs (152+) are in range
        config(MonsoonIds::NUM_PARAMS, 0, 0, 0);

        // Spread params (0-21)
        static const char* ln[3] = {"REST","MELODY","OCTAVE"};
        for (int v = 0; v < 7; ++v) {
            for (int l = 0; l < 3; ++l)
                configParam(SPREAD_V0_R + v*3 + l, 0.f, 1.f, 0.f,
                    string::f("Voice %d Spread %s", v+2, ln[l]));
        }
        configSwitch(INTERP_TARGET, 0.f, 1.f, 0.f, "Interp Target",
                     {"Avg Active Poly", "Mono Draw"});

        // L/O/R params (152+) — same positions as DeepStraitsSandsEast
        for (int v = 0; v < 7; ++v) {
            std::string vl = "Voice " + std::to_string(v+2);
            configParam(POLY_DNA_VOICE_1_LEN    + v*3, 1.f, 16.f, 16.f, vl + " REST Length");
            configParam(POLY_DNA_VOICE_1_LEN    + v*3 + 1, 0.f, 15.f, 0.f, vl + " REST Offset");
            configParam(POLY_DNA_VOICE_1_LEN    + v*3 + 2, 0.f, 15.f, 0.f, vl + " REST Rotation");
            configParam(POLY_MELODY_VOICE_1_LEN + v*3, 1.f, 16.f, 16.f, vl + " MELODY Length");
            configParam(POLY_MELODY_VOICE_1_LEN + v*3 + 1, 0.f, 15.f, 0.f, vl + " MELODY Offset");
            configParam(POLY_MELODY_VOICE_1_LEN + v*3 + 2, 0.f, 15.f, 0.f, vl + " MELODY Rotation");
            configParam(POLY_OCTAVE_VOICE_1_LEN + v*3, 1.f, 16.f, 16.f, vl + " OCTAVE Length");
            configParam(POLY_OCTAVE_VOICE_1_LEN + v*3 + 1, 0.f, 15.f, 0.f, vl + " OCTAVE Offset");
            configParam(POLY_OCTAVE_VOICE_1_LEN + v*3 + 2, 0.f, 15.f, 0.f, vl + " OCTAVE Rotation");
        }
    }
    void process(const ProcessArgs&) override {}
};
