#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsEastVisualIds {

    // ── Panel ──────────────────────────────────────────────────────────────
    static constexpr float W_MM    = 213.36f;   // 42HP (was 40HP; +2HP right strip for
                                                 // the 3 poly probability CV outs)
    static constexpr float PROB_OUT_X = 207.f;   // poly prob-out jack column (right strip)
    static constexpr int   N_ROWS  = 6;         // 2 rows per lane × 3 lanes
    static constexpr float ROW_TOP = 14.f;
    static constexpr float ROW_BOT = 108.f;
    // 5 columns: jack1, jack2, atten1, atten2, spread (per-lane)
    static constexpr float COL_J1 = 8.f;
    static constexpr float COL_J2 = 18.f;
    static constexpr float COL_A1 = 30.f;
    static constexpr float COL_A2 = 39.f;
    static constexpr float SPREAD_X = 49.f;      // per-lane spread trimpot column
    static constexpr float ED_X   = 58.f;        // editor starts after spread column
    // Mirror TAB_TOP_OFFSET_MM in panel_src/gen_east_clean.py (extra top margin so
    // the tab row clears the panel top edge; 0.5cm = 5mm).
    static constexpr float TAB_TOP_OFFSET_MM = 5.f;
    static constexpr float ED_Y   = 18.f + TAB_TOP_OFFSET_MM;  // editor top (below 2-row tab band)
    static constexpr float ED_W   = PROB_OUT_X - ED_X - 8.f;  // editor stops left of prob outs
    // Editor height sized so the 3 poly lanes are close to the Mono lane height
    // (~16mm), not ~30mm. Leaves the lower panel free for decoration / logos.
    static constexpr float ED_LANE_H = 16.f;
    static constexpr float ED_H   = 3.f * ED_LANE_H;    // 48mm (3 poly lanes)

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
        // 12 attenuverter DISPLAY proxies — row r, col c → ATTEN_START + r*2 + c
        // (3-14). These are the selected-voice view; the real depth is stored
        // per-voice in MonsoonIds::MACRO_ATTEN_START (see attenId below).
        ATTEN_START,
        NUM_SPREAD_PARAMS = ATTEN_START + 12  // = 15
    };
    // Selected-voice display proxy (physical knob binds here).
    static inline int attenDispId(int r, int col) { return ATTEN_START + r*2 + col; }
    // Per-voice CV depth (the real store): voice v, jack (r,c).
    static inline int attenId(int v, int r, int col) {
        return MonsoonIds::MACRO_ATTEN_START + v*12 + (r*2 + col);
    }

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

    // Macro/East base owner per (voice, lane): MonsoonIds::MACRO_OWN_START + v*3 + lane.
    // 0 = Macro owns (default), 1 = East owns.
    inline int ownerId(int v, int lane) { return MonsoonIds::MACRO_OWN_START + v*3 + lane; }
    // (Macro mix-in send helpers relocated to StraitsMacroVisualIds under the control
    //  inversion — the send is a Macro concern now.)
    // Owner display proxy (selected-voice view; copied to/from per-voice on switch).
    inline int ownerDispId(int lane)           { return MonsoonIds::MACRO_OWN_DISP_START + lane; }

    // Local lights for this module (East visual has its own light space, separate
    // from Monsoon's). Owner latch lights — lit when the lane is East-owned.
    enum LightIds {
        OWNER_LIGHT_START = 0,          // 3 lights, one per lane (REST/MEL/OCT)
        OWNER_LIGHT_END = OWNER_LIGHT_START + 3,
        NUM_LIGHTS = OWNER_LIGHT_END
    };
    inline int ownerLightId(int lane) { return OWNER_LIGHT_START + lane; }

    // Per-lane POLY probability CV outs (REST/MEL/OCT). Each is a poly cable:
    // channel 1 = master value, channels 2..1+nVoices = the per-voice ensemble.
    enum OutputId {
        PROB_OUT_REST = 0, PROB_OUT_MEL, PROB_OUT_OCT,
        NUM_OUTPUTS
    };
}

struct StraitsEastSandsVisual : Module {

    // Per-voice, per-lane EFFECTIVE spread (interp param + per-voice/lane CV·att,
    // clamped, after combineSpread) — published by MonsoonExpanderManager each
    // sync so the East spread trimpot mod-arc can show the viewed voice's value.
    // lane: 0=REST 1=MELODY 2=OCTAVE. Bipolar-ish 0..1 interp domain.
    float polySpreadEffective[15][3] = {};

    // Probability CV out config (persisted): scale 0=0..1V,1=0..5V,2=0..10V; S&H vs
    // continuous. probHeld/probLastStep per (lane, channel) for S&H (ch0=master, 1..15
    // = voices → index [lane][0..15]).
    float probHeld[3][16] = {};
    int   probLastStep[3][16];

    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        config(MonsoonIds::NUM_PARAMS, StraitsEastVisualIds::NUM_INPUTS,
               StraitsEastVisualIds::NUM_OUTPUTS, StraitsEastVisualIds::NUM_LIGHTS);
        for (auto& a : probLastStep) for (auto& x : a) x = -1;
        { static const char* ln[3] = {"REST","MEL","OCT"};
          for (int l = 0; l < 3; ++l)
            configOutput(StraitsEastVisualIds::PROB_OUT_REST + l,
                std::string("Probability ") + ln[l] + " (poly: ch1 master, ch2+ voices)"); }

        configParam(SPREAD_R, -1.f,1.f,0.f,"Spread REST");
        configParam(SPREAD_M, -1.f,1.f,0.f,"Spread MELODY");
        configParam(SPREAD_O, -1.f,1.f,0.f,"Spread OCTAVE");

        // Display proxies for the selected-voice owner/send controls (copied
        // to/from the per-voice MACRO_OWN/SEND params on voice switch). Owner is
        // an on/off switch (off=Macro owns base, on=East owns). Sends -1..1
        // default unity (Macro CV reaches the voice; turn down to localise).
        const char* laneNm[3] = {"REST","MEL","OCT"};
        for (int lane=0; lane<3; ++lane) {
            configSwitch(ownerDispId(lane), 0.f,1.f,0.f,
                         std::string(laneNm[lane])+" base: inherit Macro / local East", {"Inherit Macro","Local East"});
            // (Macro mix-in send display proxies relocated to Macro under the control
            //  inversion — East no longer configures them.)
        }

        static const char* rowNames[6][2] = {
            {"REST Len","REST Off"}, {"REST Rot","REST Spr"},
            {"MEL Len", "MEL Off"}, {"MEL Rot", "MEL Spr"},
            {"OCT Len", "OCT Off"}, {"OCT Rot", "OCT Spr"},
        };
        for (int r=0; r<6; ++r)
            for (int c=0; c<2; ++c) {
                configParam(attenDispId(r,c), -1.f,1.f,0.f,
                            std::string(rowNames[r][c])+" depth (selected voice)");
                configInput(cvId(r,c),
                            std::string(rowNames[r][c])+" CV (poly, per-voice depth)");
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
            for (int lane=0; lane<3; ++lane) {
                configSwitch(ownerId(v,lane), 0.f,1.f,0.f,
                             vl+"L"+std::to_string(lane)+" base: inherit Macro / local East", {"Inherit Macro","Local East"});
                // (per-voice Macro mix-in sends relocated to Macro — see
                //  StraitsMacroVisualIds::sendId.)
            }
        }
        // Per-voice CV depth for each of the 12 jacks — its own bank is 16-wide now
        // (slot 0 = voice 1/mono, slot v = voice v+1), so the mono mix-in's depth no longer
        // aliases poly voice 2's. The panel's 12 attenuverters are display proxies copied to
        // the selected voice's slice on voice switch.
        for (int v=0; v<16; ++v) {
            std::string vl = "V"+std::to_string(v+1)+" ";   // slot v = voice v+1
            for (int r=0; r<6; ++r)
                for (int c=0; c<2; ++c)
                    configParam(attenId(v,r,c), -1.f,1.f,0.f,
                                vl+"depth r"+std::to_string(r)+"c"+std::to_string(c));
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
