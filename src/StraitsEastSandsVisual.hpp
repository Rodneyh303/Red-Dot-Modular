#pragma once
#include <rack.hpp>
#include "ui/SandsGrid.hpp"
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsEastVisualIds {

    // ── Panel ──────────────────────────────────────────────────────────────
    static constexpr float W_MM    = 218.44f;   // 43HP (42 + 1HP for the per-lane
                                                 // owner-source block right of the editor)
    static constexpr float ED_X   = 88.f;   // editor (matches SandsMonoVisual)
    static constexpr float ED_W   = 111.f;  // editor width (fixed; no longer tied to PROB_OUT_X)
    // Owner-source block: per-lane cells just right of the editor, before the
    // prob-out strip. OWNER_X is the cell-column centre; the block has a faint
    // separator + backing (drawn in the gen) so it doesn't read as a 17th step.
    static constexpr float OWNER_X    = 205.f;   // owner cell column (editor right edge = 199)
    static constexpr float PROB_OUT_X = 212.f;   // poly prob-out jack column (right strip, pushed right by the owner block)
    static constexpr int   N_ROWS  = dotModular::SandsGrid::POLY_LANES;  // 4 — 1 row per lane
    // ROW_TOP/ROW_BOT were DEAD here (nothing read them; rows come from ED_Y + ED_LANE_H) yet still
    // said 14..108, which is what made this look aligned with Mono when it wasn't. Bound to the grid.
    static constexpr float ROW_TOP = dotModular::SandsGrid::LANE_TOP;      // 14
    static constexpr float ROW_BOT = dotModular::SandsGrid::polyBottom();  // 70
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
    // Mirror TAB_TOP_OFFSET_MM in panel_src/gen_east_clean.py (extra top margin so
    // the tab row clears the panel top edge; 0.5cm = 5mm).
    static constexpr float TAB_TOP_OFFSET_MM = 5.f;   // (retained; tabs now sit ABOVE the grid)
    // Voice tabs moved into 3..13mm (above the grid) so lane 0 starts at LANE_TOP like Mono.
    static constexpr float ED_Y   = dotModular::SandsGrid::LANE_TOP;   // 14 (was 23)
    // Editor holds 4 poly lanes (MEL/OCT/REST/ACCENT); ~12mm each. ED_LANE_H
    // drives prob-out vertical placement and must match the gen script's ED_H/4.
    static constexpr float ED_H      = dotModular::SandsGrid::polyHeight();  // 56 (was 48)
    static constexpr float ED_LANE_H = dotModular::SandsGrid::LANE_H;        // 14 (was 12)

    // Left-control rows align with the EDITOR lane centres (not the full panel),
    // so each lane's CV jacks + attens sit beside the visual lane they modulate —
    // matching SandsMonoVisual's behaviour.
    static inline float rowY(int r) {
        return ED_Y + (r + 0.5f) * ED_LANE_H;
    }

    // ── Row → lane mapping (mono-style: row == lane) ──────────────────────
    // Each row is one lane. 3 jacks (LEN/OFF/ROT) + 3 attens + 1 spread per row.
    // col: 0=LEN, 1=OFF, 2=ROT
    static inline int rowLane(int r)           { return r; }
    static inline int rowParam(int r, int col) { return col; }
    // param: 0=LEN, 1=OFF, 2=ROT, 3=SPR

    // ── Param IDs ─────────────────────────────────────────────────────────
    enum SpreadParamId {
        // 4 spread display trimpots: REST/MEL/OCT/ACCENT (0-3)
        SPREAD_R = 0, SPREAD_M, SPREAD_O, SPREAD_A,
        // 16 attenuverter DISPLAY proxies — lane l, col c → ATTEN_START + l*4 + c
        // (4-19). Selected-voice view; real depth in MonsoonIds::MACRO_ATTEN_START.
        ATTEN_START,
        NUM_SPREAD_PARAMS = ATTEN_START + 16  // = 20
    };
    // Selected-voice display proxy (physical knob binds here).
    static inline int attenDispId(int lane, int col) { return ATTEN_START + lane*4 + col; }
    // Per-voice CV depth (the real store): voice v, lane l, col c (0=LEN,1=OFF,2=ROT).
    static inline int attenId(int v, int lane, int col) {
        return MonsoonIds::MACRO_ATTEN_START + v*16 + (lane*4 + col);
    }

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        CV_START = 0,
        NUM_INPUTS = CV_START + 16   // 4 lanes × 4 cols
    };
    static inline int cvId(int lane, int col) { return CV_START + lane*4 + col; }

    // ── LOR / Interp param ID helpers ─────────────────────────────────────
    inline int lorId(int v, int lane, int c) {
        if (lane == 0) return POLY_DNA_VOICE_1_LEN    + v*3 + c;
        if (lane == 1) return POLY_MELODY_VOICE_1_LEN + v*3 + c;
        if (lane == 3) return POLY_ACCENT_VOICE_1_LEN + v*3 + c;
        return              POLY_OCTAVE_VOICE_1_LEN   + v*3 + c;
    }
    inline int restInterpId  (int v) { return POLY_REST_INTERP_1   + v; }
    inline int melodyInterpId(int v) { return POLY_MELODY_INTERP_1 + v; }
    inline int octaveInterpId(int v) { return POLY_OCTAVE_INTERP_1 + v; }
    inline int accentInterpId(int v) { return POLY_ACCENT_INTERP_1 + v; }
    inline int interpId(int v, int lane) {
        if (lane == 0) return restInterpId(v);
        if (lane == 1) return melodyInterpId(v);
        if (lane == 3) return accentInterpId(v);
        return              octaveInterpId(v);
    }
    // param 0=LEN,1=OFF,2=ROT: lorId; param 3=SPR: interpId
    inline int targetId(int v, int lane, int param) {
        if (param < 3) return lorId(v, lane, param);
        return interpId(v, lane);
    }
    inline float targetLo(int param) { return (param == 0) ? 1.f : 0.f; }
    inline float targetHi(int param) { return (param == 0) ? 16.f : (param < 3) ? 15.f : 1.f; }

    // Macro/East base owner per (voice, lane): MonsoonIds::MACRO_OWN_START + v*4 + lane.
    // 0 = Macro owns (default), 1 = East owns. 4 lanes: REST/MEL/OCT/ACCENT.
    inline int ownerId(int v, int lane) { return MonsoonIds::MACRO_OWN_START + v*4 + lane; }
    // V1 (mono) per-lane Macro-delegation owner. The MACRO_OWN block reserves 64 params
    // but poly only uses v=0..14 (slots 0..59); slots 60..63 (v=15) are spare. Reuse them
    // as the mono owner store so V1 delegation persists in the patch like poly voices do
    // (real configSwitch params → Rack auto-saves them). 1.f = East owns, 0.f = Macro owns.
    inline int monoOwnerId(int lane) { return MonsoonIds::MACRO_OWN_START + 15*4 + lane; }
    // (Macro mix-in send helpers relocated to StraitsMacroVisualIds under the control
    //  inversion — the send is a Macro concern now.)
    // Owner display proxy (selected-voice view; copied to/from per-voice on switch).
    inline int ownerDispId(int lane)           { return MonsoonIds::MACRO_OWN_DISP_START + lane; }

    // Local lights for this module (East visual has its own light space, separate
    // from Monsoon's). Owner latch lights — lit when the lane is East-owned.
    enum LightIds {
        OWNER_LIGHT_START = 0,          // 4 lights, one per lane (REST/MEL/OCT/ACCENT)
        OWNER_LIGHT_END = OWNER_LIGHT_START + 4,
        NUM_LIGHTS = OWNER_LIGHT_END
    };
    inline int ownerLightId(int lane) { return OWNER_LIGHT_START + lane; }

    // Per-lane POLY probability CV outs (REST/MEL/OCT). Each is a poly cable:
    // channel 1 = master value, channels 2..1+nVoices = the per-voice ensemble.
    enum OutputId {
        PROB_OUT_REST = 0, PROB_OUT_MEL, PROB_OUT_OCT, PROB_OUT_ACCENT,
        NUM_OUTPUTS
    };
}

struct StraitsEastSandsVisual : Module {

    // Per-voice, per-lane EFFECTIVE spread (interp param + per-voice/lane CV·att,
    // clamped, after combineSpread) — published by MonsoonExpanderManager each
    // sync so the East spread trimpot mod-arc can show the viewed voice's value.
    // lane: 0=REST 1=MELODY 2=OCTAVE. Bipolar-ish 0..1 interp domain.
    float polySpreadEffective[15][4] = {};   // 4 lanes (REST/MEL/OCT/ACCENT)

    // Probability CV out config (persisted): scale 0=0..1V,1=0..5V,2=0..10V; S&H vs
    // continuous. probHeld/probLastStep per (lane, channel) for S&H (ch0=master, 1..15
    // = voices → index [lane][0..15]).
    float probHeld[4][16] = {};
    int   probLastStep[4][16];

    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        config(MonsoonIds::NUM_PARAMS, StraitsEastVisualIds::NUM_INPUTS,
               StraitsEastVisualIds::NUM_OUTPUTS, StraitsEastVisualIds::NUM_LIGHTS);
        for (auto& a : probLastStep) for (auto& x : a) x = -1;
        { static const char* ln[4] = {"REST","MEL","OCT","ACC"};
          for (int l = 0; l < 4; ++l)
            configOutput(StraitsEastVisualIds::PROB_OUT_REST + l,
                std::string("Probability ") + ln[l] + " (poly: ch1 master, ch2+ voices)"); }

        configParam(SPREAD_R, -1.f,1.f,0.f,"Spread REST");
        configParam(SPREAD_M, -1.f,1.f,0.f,"Spread MELODY");
        configParam(SPREAD_O, -1.f,1.f,0.f,"Spread OCTAVE");
        configParam(SPREAD_A, -1.f,1.f,0.f,"Spread ACCENT");

        // Display proxies for the selected-voice owner/send controls (copied
        // to/from the per-voice MACRO_OWN/SEND params on voice switch). Owner is
        // an on/off switch (off=Macro owns base, on=East owns). Sends -1..1
        // default unity (Macro CV reaches the voice; turn down to localise).
        const char* laneNm[4] = {"REST","MEL","OCT","ACC"};
        for (int lane=0; lane<4; ++lane) {
            configSwitch(ownerDispId(lane), 0.f,1.f,0.f,
                         std::string(laneNm[lane])+" base: inherit Macro / local East", {"Inherit Macro","Local East"});
        }

        static const char* laneNames[4] = {"REST","MEL","OCT","ACC"};
        static const char* paramNames[4] = {"Len","Off","Rot","Spr"};
        for (int lane=0; lane<4; ++lane)
            for (int c=0; c<4; ++c) {
                std::string nm = std::string(laneNames[lane])+" "+paramNames[c];
                configParam(attenDispId(lane,c), -1.f,1.f,0.f, nm+" depth (selected voice)");
                configInput(cvId(lane,c), nm+" CV (poly, per-voice depth)");
            }

        for (int v=0; v<15; ++v) {   // poly voices 2..16: lorId/owner/interp banks are 15-wide
            std::string vl = "V"+std::to_string(v+2)+" ";
            for (int lane=0; lane<3; ++lane) {
                for (int c=0; c<3; ++c)
                    configParam(lorId(v,lane,c), 0.f,16.f,
                                c==0?16.f:0.f, vl+"l"+std::to_string(lane)+"c"+std::to_string(c));
            }
            // Accent LOR (lane 3) — own base params; LEN default 16 (identity window). Panel
            // controls for these come with the East/Macro 4th-lane relayout; until then they
            // hold the identity default so poly accent has a valid LOR.
            for (int c=0; c<3; ++c)
                configParam(lorId(v,3,c), 0.f,16.f, c==0?16.f:0.f, vl+"l3c"+std::to_string(c));
            configParam(restInterpId(v),   -1.f,1.f,0.f,vl+"Spread REST");
            configParam(melodyInterpId(v), -1.f,1.f,0.f,vl+"Spread MEL");
            configParam(octaveInterpId(v), -1.f,1.f,0.f,vl+"Spread OCT");
            configParam(accentInterpId(v), -1.f,1.f,0.f,vl+"Spread ACC");

            // Base owner (0=Macro default, 1=East) + Macro-CV blend sends (unity
            // default) per lane. Switch/snap so owner reads as discrete 0/1.
            for (int lane=0; lane<4; ++lane) {
                configSwitch(ownerId(v,lane), 0.f,1.f,0.f,
                             vl+"L"+std::to_string(lane)+" base: inherit Macro / local East", {"Inherit Macro","Local East"});
            }
        }
        // V1 (mono) per-lane owner store (spare MACRO_OWN slot v=15). Default 0.f =
        // inherit Macro, matching the poly ownerId and ownerDispId defaults (consistent
        // delegation convention; toggle per lane via the V1 owner cell).
        for (int lane=0; lane<4; ++lane) {
            configSwitch(monoOwnerId(lane), 0.f,1.f,0.f,
                         "V1 L"+std::to_string(lane)+" base: inherit Macro / local East", {"Inherit Macro","Local East"});
        }
        // Per-voice CV depth for each of the 12 jacks — its own bank is 16-wide now
        // (slot 0 = voice 1/mono, slot v = voice v+1), so the mono mix-in's depth no longer
        // aliases poly voice 2's. The panel's 12 attenuverters are display proxies copied to
        // the selected voice's slice on voice switch.
        for (int v=0; v<16; ++v) {
            std::string vl = "V"+std::to_string(v+1)+" ";   // slot v = voice v+1
            for (int lane=0; lane<4; ++lane)
                for (int c=0; c<4; ++c)
                    configParam(attenId(v,lane,c), -1.f,1.f,0.f,
                                vl+"depth l"+std::to_string(lane)+"c"+std::to_string(c));
        }
    }

    void process(const ProcessArgs&) override;   // defined in .cpp (needs findMonsoonEitherSide)

    json_t* dataToJson() override {
        json_t* r = json_object();
        return r;
    }
    void dataFromJson(json_t* root) override {
    }
};
