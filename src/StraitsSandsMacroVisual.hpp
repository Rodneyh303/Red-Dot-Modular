#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsMacroVisualIds {

    // ── Panel ──────────────────────────────────────────────────────────────
    static constexpr float W_MM    = 132.08f;   // 26HP
    static constexpr float ROW_TOP = 14.f;
    static constexpr float ROW_BOT = 108.f;
    static constexpr int   N_ROWS  = 6;
    // 4 columns: j1, j2, a1, a2 — same positions as East/West
    static constexpr float COL_J1 = 8.f;
    static constexpr float COL_J2 = 18.f;
    static constexpr float COL_A1 = 30.f;
    static constexpr float COL_A2 = 39.f;
    static constexpr float ED_X   = 46.f;
    static constexpr float ED_W   = W_MM - ED_X - 4.f;  // 82.1mm

    static inline float rowY(int r) {
        return ROW_TOP + (r + 0.5f) * (ROW_BOT - ROW_TOP) / N_ROWS;
    }

    // ── Param IDs ─────────────────────────────────────────────────────────
    enum SpreadParamId {
        // Display spread trimpots (0-2)
        SPREAD_REST = 0, SPREAD_MELODY, SPREAD_OCTAVE,
        // 12 attenuverters: row r, col c → ATTEN_START + r*2 + c (3-14)
        ATTEN_START,
        NUM_SPREAD_PARAMS = ATTEN_START + 12  // = 15
    };
    static inline int attenId(int r, int c) { return ATTEN_START + r*2 + c; }

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        CV_START = 0,
        NUM_INPUTS = CV_START + 12   // 6 rows × 2 cols
    };
    static inline int cvId(int r, int c) { return CV_START + r*2 + c; }

    // ── Row mapping (same as East/West) ───────────────────────────────────
    // Row r: lane=r/2, sub=r%2
    //   sub=0: LEN(col0) + OFF(col1)
    //   sub=1: ROT(col0) + SPR(col1)

    // ── Global LOR param helpers ──────────────────────────────────────────
    inline int lorId(int lane, int c) {
        if (lane == 0) return GLOBAL_REST_DNA_LEN   + c;
        if (lane == 1) return GLOBAL_MELODY_DNA_LEN + c;
        return              GLOBAL_OCTAVE_DNA_LEN   + c;
    }
    // Spread: stored in SPREAD_REST/MELODY/OCTAVE display params
    inline int sprId(int lane) {
        if (lane == 0) return SPREAD_REST;
        if (lane == 1) return SPREAD_MELODY;
        return              SPREAD_OCTAVE;
    }
    // param 0=LEN,1=OFF,2=ROT → lorId; param 3=SPR → sprId
    inline float targetLo(int param) { return param == 0 ? 1.f : 0.f; }
    inline float targetHi(int param) {
        return param == 0 ? 16.f : param < 3 ? 15.f : 1.f;
    }
}

struct StraitsSandsMacroVisual : Module {
    bool interpUseMono = false;  // context menu: Avg Poly / Mono Draw

    StraitsSandsMacroVisual() {
        using namespace StraitsMacroVisualIds;
        config(MonsoonIds::NUM_PARAMS, StraitsMacroVisualIds::NUM_INPUTS, 0, 0);

        configParam(SPREAD_REST,   0.f,1.f,0.f,"Global Spread REST");
        configParam(SPREAD_MELODY, 0.f,1.f,0.f,"Global Spread MELODY");
        configParam(SPREAD_OCTAVE, 0.f,1.f,0.f,"Global Spread OCTAVE");

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
                            std::string(rowNames[r][c])+" CV");
            }

        // Global L/O/R params
        configParam(GLOBAL_REST_DNA_LEN,   1.f,16.f,16.f,"Global REST Length");
        configParam(GLOBAL_REST_DNA_OFF,   0.f,15.f, 0.f,"Global REST Offset");
        configParam(GLOBAL_REST_DNA_ROT,   0.f,15.f, 0.f,"Global REST Rotation");
        configParam(GLOBAL_MELODY_DNA_LEN, 1.f,16.f,16.f,"Global MELODY Length");
        configParam(GLOBAL_MELODY_DNA_OFF, 0.f,15.f, 0.f,"Global MELODY Offset");
        configParam(GLOBAL_MELODY_DNA_ROT, 0.f,15.f, 0.f,"Global MELODY Rotation");
        configParam(GLOBAL_OCTAVE_DNA_LEN, 1.f,16.f,16.f,"Global OCTAVE Length");
        configParam(GLOBAL_OCTAVE_DNA_OFF, 0.f,15.f, 0.f,"Global OCTAVE Offset");
        configParam(GLOBAL_OCTAVE_DNA_ROT, 0.f,15.f, 0.f,"Global OCTAVE Rotation");
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
