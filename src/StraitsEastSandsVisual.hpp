#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsEastVisualIds {

    static constexpr float W_MM    = 182.88f;   // 36HP
    static constexpr int   N_ROWS  = 12;        // 3 lanes × 4 params
    static constexpr float ROW_TOP = 13.f;
    static constexpr float ROW_BOT = 125.f;
    static constexpr float COL_JACK  = 10.f;
    static constexpr float COL_ATTEN = 22.f;
    static constexpr float ED_X      = 30.f;
    static constexpr float ED_W      = W_MM - ED_X - 4.f;  // 148.9mm

    // ── Param IDs ─────────────────────────────────────────────────────────
    enum SpreadParamId {
        // 3 display trimpots for selected voice's spread (0-2)
        SPREAD_R = 0, SPREAD_M, SPREAD_O,
        // 12 attenuverters — one per row (3-14)
        ATTEN_0, ATTEN_1, ATTEN_2,  ATTEN_3,   // REST  LEN/OFF/ROT/SPR
        ATTEN_4, ATTEN_5, ATTEN_6,  ATTEN_7,   // MEL   LEN/OFF/ROT/SPR
        ATTEN_8, ATTEN_9, ATTEN_10, ATTEN_11,  // OCT   LEN/OFF/ROT/SPR
        NUM_SPREAD_PARAMS   // = 15, well below POLY_DNA_VOICE_1_LEN (92)
    };

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        // 12 poly jacks — lane group × param, ch N = voice N
        CV_0, CV_1, CV_2,  CV_3,   // REST  LEN/OFF/ROT/SPR
        CV_4, CV_5, CV_6,  CV_7,   // MEL   LEN/OFF/ROT/SPR
        CV_8, CV_9, CV_10, CV_11,  // OCT   LEN/OFF/ROT/SPR
        NUM_INPUTS
    };

    // ── Row geometry ──────────────────────────────────────────────────────
    static inline float rowY(int r) {
        return ROW_TOP + (r + 0.5f) * (ROW_BOT - ROW_TOP) / N_ROWS;
    }

    // ── Param ID helpers ──────────────────────────────────────────────────
    inline int lorId(int v, int lane, int c) {
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + v*3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + v*3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + v*3 + c;
    }
    inline int restInterpId  (int v) { return POLY_REST_INTERP_1   + v; }
    inline int melodyInterpId(int v) { return POLY_MELODY_INTERP_1 + v; }
    inline int octaveInterpId(int v) { return POLY_OCTAVE_INTERP_1 + v; }
    inline int interpId(int v, int lane) {
        if (lane == 0) return restInterpId(v);
        if (lane == 1) return melodyInterpId(v);
        return              octaveInterpId(v);
    }

    // Row → target param.
    // Rows 0-3=REST, 4-7=MEL, 8-11=OCT. Within each group: 0=LEN,1=OFF,2=ROT,3=SPR.
    inline bool rowIsSpread(int row) { return (row % 4) == 3; }
    inline int  rowLane(int row)     { return row / 4; }
    inline int  rowComp(int row)     { return row % 4; }  // 0=LEN,1=OFF,2=ROT (3=SPR handled separately)

    inline int rowTargetId(int v, int row) {
        int lane = rowLane(row);
        if (rowIsSpread(row)) return interpId(v, lane);
        return lorId(v, lane, rowComp(row));
    }
    // Scale for LOR CV (spread scale is always 1.0, handled separately)
    inline float rowScale(int row) {
        int c = rowComp(row);
        if (rowIsSpread(row)) return 1.f;
        return (c == 0) ? 15.f : 15.f;  // LEN max=16 (1+15), OFF/ROT max=15
    }
    inline float rowLo(int row) { return (rowIsSpread(row) || rowComp(row)>0) ? 0.f : 1.f; }
    inline float rowHi(int row) { return rowIsSpread(row) ? 1.f : (rowComp(row)==0 ? 16.f : 15.f); }
}

struct StraitsEastSandsVisual : Module {
    bool interpUseMono = false;

    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        config(MonsoonIds::NUM_PARAMS, NUM_INPUTS, 0, 0);

        configParam(SPREAD_R, 0.f,1.f,0.f,"Spread REST");
        configParam(SPREAD_M, 0.f,1.f,0.f,"Spread MELODY");
        configParam(SPREAD_O, 0.f,1.f,0.f,"Spread OCTAVE");

        static const char* rowNames[12] = {
            "REST Len","REST Off","REST Rot","REST Spread",
            "MEL Len", "MEL Off", "MEL Rot", "MEL Spread",
            "OCT Len", "OCT Off", "OCT Rot", "OCT Spread"
        };
        for (int r=0; r<12; ++r) {
            configParam(ATTEN_0+r, -1.f,1.f,1.f,
                        std::string(rowNames[r])+" depth");
            configInput(CV_0+r,
                        std::string(rowNames[r])+" CV (poly)");
        }

        for (int v=0; v<7; ++v) {
            std::string vl = "V"+std::to_string(v+2)+" ";
            for (int lane=0; lane<3; ++lane)
                for (int c=0; c<3; ++c)
                    configParam(lorId(v,lane,c), 0.f,16.f,
                                c==0?16.f:0.f, vl+"l"+std::to_string(lane)+"c"+std::to_string(c));
            configParam(restInterpId(v),   0.f,1.f,0.f,vl+"Spread REST");
            configParam(melodyInterpId(v), 0.f,1.f,0.f,vl+"Spread MEL");
            configParam(octaveInterpId(v), 0.f,1.f,0.f,vl+"Spread OCT");
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
