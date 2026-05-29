#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsEastVisualIds {

    // ── Panel ──────────────────────────────────────────────────────────────
    static constexpr float W_MM = 182.88f;   // 36HP
    static constexpr int   N_VOICES_EAST = 7;
    static constexpr int   N_VOICES_WEST = 8;
    static constexpr int   N_ROWS        = 9;

    // ── Param IDs ─────────────────────────────────────────────────────────
    // Kept in SpreadParamId enum (values 0..35), safely below POLY_DNA (92)
    enum SpreadParamId {
        // 3 display trimpots for selected voice's spread (0-2)
        SPREAD_R = 0, SPREAD_M, SPREAD_O,

        // 9 attenuverters for LOR CV jacks (3-11)
        ATTEN_LOR_0, ATTEN_LOR_1, ATTEN_LOR_2,
        ATTEN_LOR_3, ATTEN_LOR_4, ATTEN_LOR_5,
        ATTEN_LOR_6, ATTEN_LOR_7, ATTEN_LOR_8,

        // 9 attenuverters for Spread CV jacks (12-20)
        ATTEN_SPR_0, ATTEN_SPR_1, ATTEN_SPR_2,
        ATTEN_SPR_3, ATTEN_SPR_4, ATTEN_SPR_5,
        ATTEN_SPR_6, ATTEN_SPR_7, ATTEN_SPR_8,

        NUM_SPREAD_PARAMS   // = 21
    };

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        // 9 poly LOR jacks (outer left column)
        CV_LOR_0 = 0, CV_LOR_1, CV_LOR_2,   // REST  LEN/OFF/ROT
        CV_LOR_3,     CV_LOR_4, CV_LOR_5,   // MEL   LEN/OFF/ROT
        CV_LOR_6,     CV_LOR_7, CV_LOR_8,   // OCT   LEN/OFF/ROT

        // 9 poly Spread jacks (outer right column)
        CV_SPR_0,     CV_SPR_1, CV_SPR_2,   // REST  LEN/OFF/ROT spread
        CV_SPR_3,     CV_SPR_4, CV_SPR_5,   // MEL   LEN/OFF/ROT spread
        CV_SPR_6,     CV_SPR_7, CV_SPR_8,   // OCT   LEN/OFF/ROT spread

        NUM_INPUTS
    };

    // ── Param ID helpers ──────────────────────────────────────────────────
    // LOR: voice v (0-based), lane 0=REST 1=MEL 2=OCT, component 0=L 1=O 2=R
    inline int lorId(int v, int lane, int c) {
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + v*3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + v*3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + v*3 + c;
    }

    // Row → LOR param: row 0-8 maps to REST/MEL/OCT × LEN/OFF/ROT
    inline int rowLorId(int v, int row) {
        int lane = row / 3, c = row % 3;
        return lorId(v, lane, c);
    }

    // Spread/interp params — Monsoon reads directly from cached pointer
    inline int restInterpId  (int v) { return POLY_REST_INTERP_1   + v; }
    inline int melodyInterpId(int v) { return POLY_MELODY_INTERP_1 + v; }
    inline int octaveInterpId(int v) { return POLY_OCTAVE_INTERP_1 + v; }
    inline int interpId(int v, int lane) {
        if (lane == 0) return restInterpId(v);
        if (lane == 1) return melodyInterpId(v);
        return              octaveInterpId(v);
    }
    // Row → INTERP param: rows 0-2→REST, 3-5→MEL, 6-8→OCT (grouped by lane)
    inline int rowInterpId(int v, int row) { return interpId(v, row / 3); }
}

struct StraitsEastSandsVisual : Module {
    // Context-menu state
    bool  interpUseMono     = false;
    int   cvLorVoiceMask    = 0b1111111;   // LOR CV voice opt-out (East V2-V8)
    int   cvSpreadVoiceMask = 0b1111111;   // Spread CV voice opt-out

    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        config(MonsoonIds::NUM_PARAMS, NUM_INPUTS, 0, 0);

        // Display trimpots
        configParam(SPREAD_R, 0.f,1.f,0.f,"Spread REST");
        configParam(SPREAD_M, 0.f,1.f,0.f,"Spread MELODY");
        configParam(SPREAD_O, 0.f,1.f,0.f,"Spread OCTAVE");

        // LOR attenuverters
        static const char* lorNames[9] = {
            "REST Len","REST Off","REST Rot",
            "MEL Len", "MEL Off", "MEL Rot",
            "OCT Len", "OCT Off", "OCT Rot"
        };
        for (int r=0; r<9; ++r)
            configParam(ATTEN_LOR_0+r, -1.f,1.f,1.f,
                        std::string(lorNames[r])+" CV depth");

        // Spread attenuverters
        static const char* sprNames[9] = {
            "REST Len Spr","REST Off Spr","REST Rot Spr",
            "MEL Len Spr", "MEL Off Spr", "MEL Rot Spr",
            "OCT Len Spr", "OCT Off Spr", "OCT Rot Spr"
        };
        for (int r=0; r<9; ++r)
            configParam(ATTEN_SPR_0+r, -1.f,1.f,1.f,
                        std::string(sprNames[r])+" CV depth");

        // LOR inputs
        for (int r=0; r<9; ++r)
            configInput(CV_LOR_0+r, std::string(lorNames[r])+" (poly)");

        // Spread inputs
        for (int r=0; r<9; ++r)
            configInput(CV_SPR_0+r, std::string(sprNames[r])+" (poly)");

        // LOR params per voice
        for (int v=0; v<7; ++v) {
            std::string vl = "V"+std::to_string(v+2);
            for (int lane=0; lane<3; ++lane)
                for (int c=0; c<3; ++c)
                    configParam(lorId(v,lane,c), 0.f,16.f, c==0?16.f:0.f,
                                vl+" lane"+std::to_string(lane)+" c"+std::to_string(c));
        }

        // Spread/INTERP params per voice
        for (int v=0; v<7; ++v) {
            std::string vl = "V"+std::to_string(v+2);
            configParam(restInterpId(v),   0.f,1.f,0.f, vl+" Spread REST");
            configParam(melodyInterpId(v), 0.f,1.f,0.f, vl+" Spread MELODY");
            configParam(octaveInterpId(v), 0.f,1.f,0.f, vl+" Spread OCTAVE");
        }
    }

    void process(const ProcessArgs&) override {}

    json_t* dataToJson() override {
        json_t* r = json_object();
        json_object_set_new(r,"interpUseMono",    json_boolean(interpUseMono));
        json_object_set_new(r,"cvLorVoiceMask",   json_integer(cvLorVoiceMask));
        json_object_set_new(r,"cvSpreadVoiceMask",json_integer(cvSpreadVoiceMask));
        return r;
    }
    void dataFromJson(json_t* root) override {
        if (auto* j=json_object_get(root,"interpUseMono"))    interpUseMono    =json_boolean_value(j);
        if (auto* j=json_object_get(root,"cvLorVoiceMask"))   cvLorVoiceMask   =json_integer_value(j);
        if (auto* j=json_object_get(root,"cvSpreadVoiceMask"))cvSpreadVoiceMask=json_integer_value(j);
    }
};
