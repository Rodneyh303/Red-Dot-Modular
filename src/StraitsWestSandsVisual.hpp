#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"
#include "StraitsEastSandsVisual.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsWestVisualIds {
    static constexpr float W_MM = 182.88f;
    // West uses East's layout constants for editor position
    using E = StraitsEastVisualIds;

    enum SpreadParamId {
        SPREAD_R = 0, SPREAD_M, SPREAD_O,
        NUM_SPREAD_PARAMS
    };

    // West: localV=0..7 → global v=7..14 (voices 9-16)
    inline int lorId(int lv, int lane, int c) {
        int gv = lv+7;
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + gv*3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + gv*3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + gv*3 + c;
    }
    inline int restInterpId  (int lv) { return POLY_REST_INTERP_1   + lv+7; }
    inline int melodyInterpId(int lv) { return POLY_MELODY_INTERP_1 + lv+7; }
    inline int octaveInterpId(int lv) { return POLY_OCTAVE_INTERP_1 + lv+7; }
    inline int interpId(int lv, int lane) {
        if (lane == 0) return restInterpId(lv);
        if (lane == 1) return melodyInterpId(lv);
        return              octaveInterpId(lv);
    }
}

struct StraitsWestSandsVisual : Module {
    bool interpUseMono = false;

    StraitsWestSandsVisual() {
        using namespace StraitsWestVisualIds;
        config(MonsoonIds::NUM_PARAMS, 0, 0, 0);

        configParam(SPREAD_R, 0.f,1.f,0.f,"Spread REST");
        configParam(SPREAD_M, 0.f,1.f,0.f,"Spread MELODY");
        configParam(SPREAD_O, 0.f,1.f,0.f,"Spread OCTAVE");

        for (int lv=0; lv<8; ++lv) {
            std::string vl = "V"+std::to_string(lv+9)+" ";
            for (int lane=0; lane<3; ++lane)
                for (int c=0; c<3; ++c)
                    configParam(lorId(lv,lane,c), 0.f,16.f,
                                c==0?16.f:0.f, vl+"l"+std::to_string(lane)+"c"+std::to_string(c));
            configParam(restInterpId(lv),   0.f,1.f,0.f,vl+"Spread REST");
            configParam(melodyInterpId(lv), 0.f,1.f,0.f,vl+"Spread MEL");
            configParam(octaveInterpId(lv), 0.f,1.f,0.f,vl+"Spread OCT");
        }
    }

    void process(const ProcessArgs&) override {}

    json_t* dataToJson() override {
        json_t* r = json_object();
        json_object_set_new(r,"interpUseMono",json_boolean(interpUseMono));
        return r;
    }
    void dataFromJson(json_t* root) override {
        if (auto* j=json_object_get(root,"interpUseMono")) interpUseMono=json_boolean_value(j);
    }
};
