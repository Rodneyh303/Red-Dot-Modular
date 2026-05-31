#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsEastVisualIds {

    // ── Panel ──────────────────────────────────────────────────────────────
    static constexpr float W_MM    = 182.88f;   // 36HP
    static constexpr int   N_ROWS  = 6;         // 2 rows per lane × 3 lanes
    static constexpr float ROW_TOP = 14.f;
    static constexpr float ROW_BOT = 108.f;
    // 4 columns: jack1, jack2, atten1, atten2
    static constexpr float COL_J1 = 8.f;
    static constexpr float COL_J2 = 18.f;
    static constexpr float COL_A1 = 30.f;
    static constexpr float COL_A2 = 39.f;
    static constexpr float ED_X   = 46.f;
    static constexpr float ED_W   = W_MM - ED_X - 4.f;  // 132.9mm

    static inline float rowY(int r) {
        return ROW_TOP + (r + 0.5f) * (ROW_BOT - ROW_TOP) / N_ROWS;
    }

    // ── Row → (lane, param) mapping ───────────────────────────────────────
    // Each row has 2 jacks. Row r: lane = r/2, sub = r%2
    // sub=0: LEN (col1) + OFF (col2)
    // sub=1: ROT (col1) + SPR (col2)
    // col: 0=col1, 1=col2
    static inline int rowLane(int r)           { return r / 2; }
    static inline int rowParam(int r, int col) { return (r % 2) * 2 + col; }
    // param: 0=LEN, 1=OFF, 2=ROT, 3=SPR

    // ── Param IDs ─────────────────────────────────────────────────────────
    enum SpreadParamId {
        // 3 display trimpots for selected voice's spread (0-2)
        SPREAD_R = 0, SPREAD_M, SPREAD_O,
        // 12 attenuverters — row r, col c → ATTEN_START + r*2 + c (3-14)
        ATTEN_START,
        NUM_SPREAD_PARAMS = ATTEN_START + 12  // = 15
    };
    static inline int attenId(int r, int col) { return ATTEN_START + r*2 + col; }

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        CV_START = 0,
        NUM_INPUTS = CV_START + 12   // 6 rows × 2 cols
    };
    static inline int cvId(int r, int col) { return CV_START + r*2 + col; }

    // ── LOR / Interp param ID helpers ─────────────────────────────────────
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
    // param 0=LEN,1=OFF,2=ROT: lorId; param 3=SPR: interpId
    inline int targetId(int v, int lane, int param) {
        if (param < 3) return lorId(v, lane, param);
        return interpId(v, lane);
    }
    inline float targetLo(int param) { return (param == 0) ? 1.f : 0.f; }
    inline float targetHi(int param) { return (param == 0) ? 16.f : (param < 3) ? 15.f : 1.f; }
}

struct StraitsEastSandsVisual : Module {
    bool interpUseMono = false;

    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        config(MonsoonIds::NUM_PARAMS, NUM_INPUTS, 0, 0);

        configParam(SPREAD_R, 0.f,1.f,0.f,"Spread REST");
        configParam(SPREAD_M, 0.f,1.f,0.f,"Spread MELODY");
        configParam(SPREAD_O, 0.f,1.f,0.f,"Spread OCTAVE");

        static const char* rowNames[6][2] = {
            {"REST Len","REST Off"}, {"REST Rot","REST Spr"},
            {"MEL Len", "MEL Off"}, {"MEL Rot", "MEL Spr"},
            {"OCT Len", "OCT Off"}, {"OCT Rot", "OCT Spr"},
        };
        for (int r=0; r<6; ++r)
            for (int c=0; c<2; ++c) {
                configParam(attenId(r,c), -1.f,1.f,1.f,
                            std::string(rowNames[r][c])+" depth");
                configInput(cvId(r,c),
                            std::string(rowNames[r][c])+" CV (poly)");
            }

        for (int v=0; v<7; ++v) {
            std::string vl = "V"+std::to_string(v+2)+" ";
            for (int lane=0; lane<3; ++lane) {
                for (int c=0; c<3; ++c)
                    configParam(lorId(v,lane,c), 0.f,16.f,
                                c==0?16.f:0.f, vl+"l"+std::to_string(lane)+"c"+std::to_string(c));
            }
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
