#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsMacroVisualIds {
    enum SpreadParamId {
        SPREAD_REST = 0,
        SPREAD_MELODY,
        SPREAD_OCTAVE,
        INTERP_TARGET,
        NUM_SPREAD_PARAMS  // = 4, safely below GLOBAL_REST_DNA_LEN (~368+)
    };
    // L/O/R at MonsoonIds GLOBAL_*_DNA positions — same as MonsoonStraitsSands knobs
    inline int lorId(int lane, int c) {
        if (lane == 0) return GLOBAL_REST_DNA_LEN    + c;
        if (lane == 1) return GLOBAL_MELODY_DNA_LEN  + c;
        return              GLOBAL_OCTAVE_DNA_LEN    + c;
        // GLOBAL_*_DNA_LEN/OFF/ROT are contiguous (LEN=+0 OFF=+1 ROT=+2)
    }
}

struct StraitsSandsMacroVisual : Module {
    StraitsSandsMacroVisual() {
        using namespace StraitsMacroVisualIds;
        config(MonsoonIds::NUM_PARAMS, 0, 0, 0);

        configParam(SPREAD_REST,    0.f, 1.f, 0.f, "Global Spread REST");
        configParam(SPREAD_MELODY,  0.f, 1.f, 0.f, "Global Spread MELODY");
        configParam(SPREAD_OCTAVE,  0.f, 1.f, 0.f, "Global Spread OCTAVE");
        configSwitch(INTERP_TARGET, 0.f, 1.f, 0.f, "Interp Target",
                     {"Avg Active Poly", "Mono Draw"});

        // Global L/O/R — same positions as MonsoonStraitsSands knobs
        configParam(GLOBAL_REST_DNA_LEN,    1.f, 16.f, 16.f, "Global REST Length");
        configParam(GLOBAL_REST_DNA_OFF,    0.f, 15.f,  0.f, "Global REST Offset");
        configParam(GLOBAL_REST_DNA_ROT,    0.f, 15.f,  0.f, "Global REST Rotation");
        configParam(GLOBAL_MELODY_DNA_LEN,  1.f, 16.f, 16.f, "Global MELODY Length");
        configParam(GLOBAL_MELODY_DNA_OFF,  0.f, 15.f,  0.f, "Global MELODY Offset");
        configParam(GLOBAL_MELODY_DNA_ROT,  0.f, 15.f,  0.f, "Global MELODY Rotation");
        configParam(GLOBAL_OCTAVE_DNA_LEN,  1.f, 16.f, 16.f, "Global OCTAVE Length");
        configParam(GLOBAL_OCTAVE_DNA_OFF,  0.f, 15.f,  0.f, "Global OCTAVE Offset");
        configParam(GLOBAL_OCTAVE_DNA_ROT,  0.f, 15.f,  0.f, "Global OCTAVE Rotation");
    }
    void process(const ProcessArgs&) override {}
};
