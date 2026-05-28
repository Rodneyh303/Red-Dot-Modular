#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;

// Param ID helpers — mirrors SandsMonoVisualIds in the .cpp
// Exposed here so MonsoonSandsManager can read params without including the full widget.
namespace SandsMonoVisualIds {
    enum ParamId {
        LEN_REST = 0, OFF_REST, ROT_REST,
        LEN_MELODY,   OFF_MELODY,   ROT_MELODY,
        LEN_OCTAVE,   OFF_OCTAVE,   ROT_OCTAVE,
        LEN_LEGATO,   OFF_LEGATO,   ROT_LEGATO,
        LEN_ACCENT,   OFF_ACCENT,   ROT_ACCENT,
        LEN_VARIATION,OFF_VARIATION,ROT_VARIATION,
        NUM_PARAMS
    };
    inline int lenId(int l) { return LEN_REST + l * 3; }
    inline int offId(int l) { return LEN_REST + l * 3 + 1; }
    inline int rotId(int l) { return LEN_REST + l * 3 + 2; }
}

struct MonsoonSandsVisualExpander : Module {
    MonsoonSandsVisualExpander() {
        using namespace SandsMonoVisualIds;
        config(NUM_PARAMS, 0, 0, 0);
        static const char* names[6] = {
            "REST","MELODY","OCTAVE","LEGATO","ACCENT","VARIATION"
        };
        for (int l = 0; l < 6; ++l) {
            configParam(lenId(l), 1.f, 16.f, 16.f, string::f("%s Length",   names[l]));
            configParam(offId(l), 0.f, 15.f,  0.f, string::f("%s Offset",   names[l]));
            configParam(rotId(l), 0.f, 15.f,  0.f, string::f("%s Rotation", names[l]));
        }
    }
    void process(const ProcessArgs&) override {}
};
