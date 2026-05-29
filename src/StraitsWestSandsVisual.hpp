#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"
#include "StraitsEastSandsVisual.hpp"   // shares InputId/helpers

using namespace rack;
using namespace MonsoonIds;

// West has NO inputs of its own — it shares East's poly jacks.
// East channels 0-6  → voices 2-8  (East)
// East channels 7-14 → voices 9-16 (West)

namespace StraitsWestVisualIds {
    static constexpr float W_MM = 182.88f;   // 36HP, matches East

    // Only spread display trimpots needed (no CV jacks)
    enum SpreadParamId {
        SPREAD_R = 0, SPREAD_M, SPREAD_O,
        NUM_SPREAD_PARAMS
    };

    // Voice helpers — West local v=0..7, global v=7..14
    inline int lorId(int localV, int lane, int c) {
        int gv = localV + 7;
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + gv*3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + gv*3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + gv*3 + c;
    }
    inline int restInterpId  (int localV) { return POLY_REST_INTERP_1   + localV + 7; }
    inline int melodyInterpId(int localV) { return POLY_MELODY_INTERP_1 + localV + 7; }
    inline int octaveInterpId(int localV) { return POLY_OCTAVE_INTERP_1 + localV + 7; }
    inline int interpId(int localV, int lane) {
        if (lane == 0) return restInterpId(localV);
        if (lane == 1) return melodyInterpId(localV);
        return              octaveInterpId(localV);
    }
    inline int rowLorId(int localV, int row) { return lorId(localV, row/3, row%3); }
    inline int rowInterpId(int localV, int row) { return interpId(localV, row/3); }
}

struct StraitsWestSandsVisual : Module {
    bool interpUseMono     = false;

    StraitsWestSandsVisual() {
        using namespace StraitsWestVisualIds;
        config(MonsoonIds::NUM_PARAMS, 0, 0, 0);   // NO inputs

        configParam(SPREAD_R, 0.f,1.f,0.f,"Spread REST");
        configParam(SPREAD_M, 0.f,1.f,0.f,"Spread MELODY");
        configParam(SPREAD_O, 0.f,1.f,0.f,"Spread OCTAVE");

        // LOR params — West uses global voices 7-14
        for (int lv=0; lv<8; ++lv)
            for (int lane=0; lane<3; ++lane)
                for (int c=0; c<3; ++c)
                    configParam(lorId(lv,lane,c), 0.f,16.f, c==0?16.f:0.f,
                                "V"+std::to_string(lv+9)+" l"+std::to_string(lane)+" c"+std::to_string(c));

        // INTERP params
        for (int lv=0; lv<8; ++lv) {
            configParam(restInterpId(lv),   0.f,1.f,0.f,"V"+std::to_string(lv+9)+" Spread REST");
            configParam(melodyInterpId(lv), 0.f,1.f,0.f,"V"+std::to_string(lv+9)+" Spread MEL");
            configParam(octaveInterpId(lv), 0.f,1.f,0.f,"V"+std::to_string(lv+9)+" Spread OCT");
        }
    }

    void process(const ProcessArgs&) override {}

    json_t* dataToJson() override {
        json_t* r = json_object();
        json_object_set_new(r,"interpUseMono",    json_boolean(interpUseMono));
        return r;
    }
    void dataFromJson(json_t* root) override {
        if (auto* j=json_object_get(root,"interpUseMono"))    interpUseMono    =json_boolean_value(j);
    }
};
