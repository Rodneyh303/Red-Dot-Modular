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
    static constexpr float W_MM    = 213.36f;   // 42HP (was 40HP; +2HP for 3 poly prob outs)
    static constexpr float PROB_OUT_X = 207.f;  // poly prob-out jack column (right strip)
    static constexpr float ROW_TOP = 14.f;
    static constexpr float ROW_BOT = 108.f;
    static constexpr int   N_ROWS  = 4;         // 1 row per lane (REST/MEL/OCT/ACCENT)
    // Mono-style: 4 CV jacks (LEN/OFF/ROT/SPR-cv) + 4 attens + 1 spread-base trimpot per lane.
    // Column layout and ED_X=88 match SandsMonoVisual exactly.
    static constexpr float COL_J1 = 6.f;    // LEN CV in
    static constexpr float COL_J2 = 15.f;   // OFF CV in
    static constexpr float COL_J3 = 24.f;   // ROT CV in
    static constexpr float COL_J4 = 33.f;   // SPR CV in
    static constexpr float COL_A1 = 43.f;   // LEN depth
    static constexpr float COL_A2 = 52.f;   // OFF depth
    static constexpr float COL_A3 = 61.f;   // ROT depth
    static constexpr float COL_A4 = 70.f;   // SPR depth
    static constexpr float SPREAD_X = 80.f; // per-lane spread base trimpot
    static constexpr float ED_X   = 88.f;   // editor (matches SandsMonoVisual)
    //static constexpr float ED_X   = 58.f;       // editor starts after the spread column
    static constexpr float ED_W   = PROB_OUT_X - ED_X - 8.f;  // editor stops left of prob outs
    // Mirror TAB_TOP_OFFSET_MM in gen_macro_mono.py (extra top margin; 0.5cm=5mm).
    // Base 18 matches the generator's editor recess (was 16 here — a small drift;
    // aligned now so the widget editor sits exactly on the drawn recess).
    static constexpr float TAB_TOP_OFFSET_MM = 5.f;
    static constexpr float ED_Y   = 18.f + TAB_TOP_OFFSET_MM;
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
        SPREAD_REST = 0, SPREAD_MELODY, SPREAD_OCTAVE, SPREAD_ACCENT,
        // 12 attenuverters: row r, col c → ATTEN_START + r*2 + c (3-14)
        ATTEN_START,
        NUM_SPREAD_PARAMS = ATTEN_START + 12  // = 15
    };
    static inline int attenId(int r, int c) { return ATTEN_START + r*2 + c; }

    // ── Per-voice Macro→voice mix-in send (RELOCATED from East under the control
    //    inversion). Conceptually a MACRO concern: "per voice, how much of Macro's
    //    global CV modulation reaches that voice." Stored in the shared MonsoonIds
    //    bank (both Macro and East configure NUM_PARAMS), but now OWNED by Macro —
    //    Macro configures + binds them, consumption reads them off the Macro module.
    //    180 = 15 voices × 3 lanes × 4 items (LEN/OFF/ROT/SPR). 12 display proxies
    //    are the selected-voice view (sendDispId), synced on voice switch.
    // Per-voice mix-in send. v is a VOICE SLOT (0 = mono/V1, k = V(k+1)) per
    // dotModular::VoiceResolver::voiceSlot — NOT a poly bank index. Callers must derive it via
    // voiceSlot(voiceNumber) so the mono slice (slot 0) can't collide with poly V2 (the N→N
    // bug). Bank is 3-lane-strided × 4 items, 16 slots wide.
    // NOTE: bank is still 3-lane-strided; accent (lane 3) uses POLY_ACCENT_VOICE_* directly
    //       until the bank resize in Stage 6b.
    inline int sendId(int v, int lane, int item) { return MonsoonIds::MACRO_SEND_START + (v*4 + lane)*4 + item; }
    inline int sendDispId(int lane, int item)    { return MonsoonIds::MACRO_SEND_DISP_START + lane*4 + item; }

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        CV_START = 0,
        NUM_INPUTS = CV_START + 16   // 4 lanes × 4 cols
    };
    static inline int cvId(int lane, int c) { return CV_START + lane*4 + c; }

    // ── Output IDs ────────────────────────────────────────────────────────
    // 3 poly probability CV outs (REST/MEL/OCT): ch1 reserved (future mono tab),
    // ch2..1+nVoices = per-voice ensemble.
    enum OutputId {
        PROB_OUT_REST = 0, PROB_OUT_MEL, PROB_OUT_OCT, PROB_OUT_ACCENT,
        NUM_OUTPUTS
    };

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
        config(MonsoonIds::NUM_PARAMS, StraitsMacroVisualIds::NUM_INPUTS,
               StraitsMacroVisualIds::NUM_OUTPUTS, 0);
        for (auto& a : probLastStep) for (auto& x : a) x = -1;
        { static const char* ln[4] = {"REST","MEL","OCT","ACC"};
          for (int l = 0; l < 4; ++l)
            configOutput(StraitsMacroVisualIds::PROB_OUT_REST + l,
                std::string("Probability ") + ln[l] + " (poly: ch2+ voices)"); }

        configParam(SPREAD_REST,   -1.f,1.f,0.f,"Global Spread REST");
        configParam(SPREAD_MELODY, -1.f,1.f,0.f,"Global Spread MELODY");
        configParam(SPREAD_OCTAVE, -1.f,1.f,0.f,"Global Spread OCTAVE");
        configParam(SPREAD_ACCENT, -1.f,1.f,0.f,"Global Spread ACCENT");

        static const char* laneNames[4] = {"REST","MEL","OCT","ACC"};
        static const char* paramNames[4] = {"Len","Off","Rot","Spr"};
        for (int lane=0; lane<4; ++lane)
            for (int c=0; c<4; ++c) {
                std::string nm = std::string(laneNames[lane])+" "+paramNames[c];
                configParam(attenId(lane,c), -1.f,1.f,0.f, nm+" depth");
                configInput(cvId(lane,c), nm+" CV");
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

        // Per-voice Macro→voice mix-in sends (relocated from East). 12 display proxies
        // (selected-voice view, bound on the panel) + 180 per-voice store. Default 0
        // = opt-in: Macro global CV reaches a voice only when dialed in for that voice.
        for (int lane=0; lane<4; ++lane)
            for (int item=0; item<4; ++item)
                configParam(sendDispId(lane,item), -1.f,1.f,0.f,
                            "L"+std::to_string(lane)+" mix-in "+std::to_string(item)+" (selected voice)");
        for (int v=0; v<16; ++v)   // 16 voice slots: 0=mono(V1), 1..15=poly(V2..V16)
            for (int lane=0; lane<4; ++lane)
                for (int item=0; item<4; ++item)
                    configParam(sendId(v,lane,item), -1.f,1.f,0.f,
                                "V"+std::to_string(v+1)+" L"+std::to_string(lane)+" mix-in "+std::to_string(item));
    }

    void process(const ProcessArgs&) override;   // defined in .cpp (needs findMonsoonEitherSide)

    // S&H latch state for the poly prob outs: [lane][channel] (ch0 reserved, 1..15 voices).
    float probHeld[4][16] = {};
    int   probLastStep[4][16];

    // CV-applied global spread per lane (0=REST,1=MEL,2=OCT). processDNA writes
    // these from base + spread CV; the display reads them so spread CV is visible
    // WITHOUT moving the base knob (the old code wrote the modulated value back to
    // the SPREAD_* param, which dragged the knob — fixed).
    float spreadEffective[4] = {0.f, 0.f, 0.f, 0.f};

    // Per (lane, item) split of Macro's global contribution, published by
    // processDNA::applyGlobal for the Macro/East blend equation. item: 0=LEN
    // 1=OFF 2=ROT 3=SPR. macroBase = the knob value (no CV); macroCVDelta = the
    // CV-only contribution (already scaled by Macro's own attenuverter). East's
    // sync reads these: value = base(owner) + eastCV + macroCVDelta·blendSend.
    float macroBase[4][4]    = {};
    float macroCVDelta[4][4] = {};

    json_t* dataToJson() override {
        json_t* r = json_object();
        return r;
    }
    void dataFromJson(json_t* root) override {
        (void)root;  // interpUseMono moved to Monsoon::spreadInterpMono
    }
};
