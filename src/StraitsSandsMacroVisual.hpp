#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsMacroVisualIds {

    // ── Panel ──────────────────────────────────────────────────────────────
    // Macro does the same job as the East visual (spread control) but GLOBAL
    // rather than per-lane, so it shares East's 40HP width and column geometry
    // for consistency and to give the spread column proper room.
    static constexpr float W_MM    = 203.2f;    // 40HP (matches East visual)
    static constexpr float ROW_TOP = 14.f;
    static constexpr float ROW_BOT = 108.f;
    static constexpr int   N_ROWS  = 6;
    // Columns mirror StraitsEastSandsVisual exactly: j1, j2, a1, a2, spread, editor
    static constexpr float COL_J1 = 8.f;
    static constexpr float COL_J2 = 18.f;
    static constexpr float COL_A1 = 30.f;
    static constexpr float COL_A2 = 39.f;
    static constexpr float SPREAD_X = 49.f;     // per-lane spread trimpot column (matches East)
    static constexpr float ED_X   = 58.f;       // editor starts after the spread column
    static constexpr float ED_W   = W_MM - ED_X - 4.f;  // ~141.2mm (matches East)
    static constexpr float ED_Y   = 16.f;
    // Editor height sized so the 3 poly lanes are close to the Mono lane height
    // (~16mm) rather than ~30mm; frees the lower panel for decoration/logos.
    static constexpr float ED_LANE_H = 16.f;
    static constexpr float ED_H   = 3.f * ED_LANE_H;    // 48mm

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

    // CV jack / attenuverter index for (lane, param) where param 0=LEN 1=OFF
    // 2=ROT 3=SPR. The 12 jacks are laid out 2 rows per lane:
    //   row lane*2+0 : col0=LEN col1=OFF
    //   row lane*2+1 : col0=ROT col1=SPR
    // so row = lane*2 + (param>=2), col = param&1. (The old code used
    // cvId(lane,param) directly, which mis-indexed ROT/SPR onto other lanes'
    // jacks — the macro spread/LOR CV routing bug.)
    inline int macroJackRow(int lane, int param) { return lane * 2 + (param >= 2 ? 1 : 0); }
    inline int macroJackCol(int param)           { return param & 1; }
    inline int macroCvId   (int lane, int param) { return cvId   (macroJackRow(lane, param), macroJackCol(param)); }
    inline int macroAttenId(int lane, int param) { return attenId(macroJackRow(lane, param), macroJackCol(param)); }
}

struct StraitsSandsMacroVisual : Module {

    StraitsSandsMacroVisual() {
        using namespace StraitsMacroVisualIds;
        config(MonsoonIds::NUM_PARAMS, StraitsMacroVisualIds::NUM_INPUTS, 0, 0);

        configParam(SPREAD_REST,   -1.f,1.f,0.f,"Global Spread REST");   // bipolar, matches MEL/OCT (was 0..1 — inconsistent)
        configParam(SPREAD_MELODY, -1.f,1.f,0.f,"Global Spread MELODY");
        configParam(SPREAD_OCTAVE, -1.f,1.f,0.f,"Global Spread OCTAVE");

        static const char* rowNames[6][2] = {
            {"REST Len","REST Off"}, {"REST Rot","REST Spr"},
            {"MEL Len", "MEL Off"}, {"MEL Rot", "MEL Spr"},
            {"OCT Len", "OCT Off"}, {"OCT Rot", "OCT Spr"},
        };
        for (int r=0; r<6; ++r)
            for (int c=0; c<2; ++c) {
                configParam(attenId(r,c), -1.f,1.f,0.f,
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

    // CV-applied global spread per lane (0=REST,1=MEL,2=OCT). processDNA writes
    // these from base + spread CV; the display reads them so spread CV is visible
    // WITHOUT moving the base knob (the old code wrote the modulated value back to
    // the SPREAD_* param, which dragged the knob — fixed).
    float spreadEffective[3] = {0.f, 0.f, 0.f};

    // Per (lane, item) split of Macro's global contribution, published by
    // processDNA::applyGlobal for the Macro/East blend equation. item: 0=LEN
    // 1=OFF 2=ROT 3=SPR. macroBase = the knob value (no CV); macroCVDelta = the
    // CV-only contribution (already scaled by Macro's own attenuverter). East's
    // sync reads these: value = base(owner) + eastCV + macroCVDelta·blendSend.
    float macroBase[3][4]    = {};
    float macroCVDelta[3][4] = {};

    json_t* dataToJson() override {
        json_t* r = json_object();
        return r;
    }
    void dataFromJson(json_t* root) override {
        (void)root;  // interpUseMono moved to Monsoon::spreadInterpMono
    }
};
