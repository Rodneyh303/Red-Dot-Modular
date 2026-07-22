#pragma once
#include <rack.hpp>
#include "ui/SandsGrid.hpp"
#include <cmath>   // std::fabs (macroSpreadModulatesLane)
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide (MACRO field accessors)

using namespace rack;
using namespace MonsoonIds;

namespace StraitsMacroVisualIds {

    // ── Panel ──────────────────────────────────────────────────────────────
    // Macro does the same job as the East visual (spread control) but GLOBAL
    // rather than per-lane, so it shares East's 40HP width and column geometry
    // for consistency and to give the spread column proper room.
    static constexpr float W_MM    = 243.84f;   // 48HP (44 + 4HP for dir_mod + prob_out jack columns)
    static constexpr float OWNER_X    = 205.f;  // owner cell column (matches East)
    static constexpr float DIR_X      = 212.f;  // direction cell column (matches East)
    static constexpr float DIR_MOD_X  = 220.f;  // direction gate-mod jack column
    static constexpr float PROB_OUT_X = 236.f;  // poly prob-out jack column (aligned with East/Mono)
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
    static constexpr float ED_W   = 111.f;  // editor width (fixed; no longer tied to PROB_OUT_X)
    // Mirror TAB_TOP_OFFSET_MM in gen_macro_mono.py (extra top margin; 0.5cm=5mm).
    // Base 18 matches the generator's editor recess (was 16 here — a small drift;
    // aligned now so the widget editor sits exactly on the drawn recess).
    static constexpr float TAB_TOP_OFFSET_MM = 5.f;   // (retained; tabs now sit ABOVE the grid)
    // Voice tabs moved into 3..13mm (above the grid) so lane 0 starts at LANE_TOP like Mono.
    static constexpr float ED_Y   = dotModular::SandsGrid::LANE_TOP;   // 14 (was 23)
    // Editor holds 4 poly lanes (MEL/OCT/REST/ACCENT); ~12mm each. ED_LANE_H
    // drives prob-out vertical placement and must match the gen script's ED_H/4.
    static constexpr float ED_H      = dotModular::SandsGrid::polyHeight();  // 56 (was 48)
    static constexpr float ED_LANE_H = dotModular::SandsGrid::LANE_H;        // 14 (was 12)

    // Left-control rows align with the EDITOR lane centres (not the full panel),
    // so each lane's CV jacks + attens sit beside the visual lane they modulate.
    static inline float rowY(int r) {
        return ED_Y + (r + 0.5f) * ED_LANE_H;
    }

    // ── Param IDs ─────────────────────────────────────────────────────────
    enum SpreadParamId {
        // Display spread trimpots (0-2)
        SPREAD_REST = 0, SPREAD_MELODY, SPREAD_OCTAVE, SPREAD_ACCENT,  // already added
        // 16 attenuverters: lane l, col c → ATTEN_START + l*4 + c (4-19)
        ATTEN_START,
        // Direction display proxy: 4 lanes (MEL/OCT/REST/ACC). DirCell writes here;
        // widget step() syncs to engine.laneDirPending_ (Macro = global = mono direction).
        DIR_DISP_START = ATTEN_START + 16,   // 20 .. 23
        // 16 send display proxies (lane*4+item), re-homed from MonsoonIds::MACRO_SEND_DISP
        // (NUM_PARAMS_MIGRATION.md): drive visible send cells so they stay params, in the
        // expander namespace to keep them out of the shared pool.
        SEND_DISP_START = DIR_DISP_START + 4,   // = 24
        // 8 PRE/POST tap params (lane*2 + which), re-homed from MonsoonIds::MACRO_TAP
        // (widgeted trimpots, so they stay params in the expander namespace).
        TAP_START = SEND_DISP_START + 16,       // = 40
        // 12 global LOR params (4 lanes x LEN/OFF/ROT), re-homed OUT of the shared
        // MonsoonIds pool so Macro can be right-sized. Deliberately ONE symbol +
        // arithmetic: individual names (GLOBAL_REST_DNA_LEN ...) would be ambiguous
        // against the file-scope `using namespace MonsoonIds`. Lane order matches the
        // MonsoonIds layout that was replaced: REST, MELODY, OCTAVE, ACCENT; c = 0 LEN,
        // 1 OFF, 2 ROT.
        GLOBAL_DNA_START = TAP_START + 8,       // = 48 .. 59
        NUM_SPREAD_PARAMS = GLOBAL_DNA_START + 12   // = 60
    };
    // lane: 0 REST, 1 MELODY, 2 OCTAVE, 3 ACCENT -> block offset
    static inline int globalDnaId(int lane, int c) {
        const int blk = (lane == 0) ? 0 : (lane == 1) ? 3 : (lane == 3) ? 9 : 6;
        return GLOBAL_DNA_START + blk + c;
    }
    static inline int sendDispId(int lane, int item) { return SEND_DISP_START + lane*4 + item; }
    static inline int tapLorId(int lane) { return TAP_START + lane*2 + 0; }
    static inline int tapSprId(int lane) { return TAP_START + lane*2 + 1; }
    static inline int tapIdForItem(int lane, int item) { return (item==3) ? tapSprId(lane) : tapLorId(lane); }
    static inline int attenId(int lane, int c) { return ATTEN_START + lane*4 + c; }
    static inline int dirDispId(int lane) { return DIR_DISP_START + lane; }

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
    // sendId (MACRO_SEND) MIGRATED to Monsoon::editor.macroSend -- use
    // monsoon->getMacroSend/setMacroSend(v, lane, item). sendDispId + tap re-homed above.

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        CV_START = 0,
        DIR_MOD_START = CV_START + 16,   // = 16 — direction gate-mod (4 mono jacks)
        NUM_INPUTS = DIR_MOD_START + 4   // = 20
    };
    static inline int dirModId(int lane) { return DIR_MOD_START + lane; }
    static inline int cvId(int lane, int c) { return CV_START + lane*4 + c; }

    // ── Output IDs ────────────────────────────────────────────────────────
    // 3 poly probability CV outs (REST/MEL/OCT): ch1 reserved (future mono tab),
    // ch2..1+nVoices = per-voice ensemble.
    enum OutputId {
        PROB_OUT_REST = 0, PROB_OUT_MEL, PROB_OUT_OCT, PROB_OUT_ACC,
        NUM_OUTPUTS
    };

    // ── Row mapping (same as East/West) ───────────────────────────────────
    // Row r: lane=r/2, sub=r%2
    //   sub=0: LEN(col0) + OFF(col1)
    //   sub=1: ROT(col0) + SPR(col1)

    // ── Global LOR param helpers ──────────────────────────────────────────
    inline int lorId(int lane, int c) {
        return StraitsMacroVisualIds::globalDnaId(lane, c);   // local pool, was MonsoonIds
    }
    // Spread: stored in SPREAD_REST/MELODY/OCTAVE display params
    inline int sprId(int lane) {
        if (lane == 0) return SPREAD_REST;
        if (lane == 1) return SPREAD_MELODY;
        if (lane == 3) return SPREAD_ACCENT;
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
    // Mono-style: lane == row, col == param (0=LEN, 1=OFF, 2=ROT).
    inline int macroCvId   (int lane, int param) { return cvId   (lane, param); }
    inline int macroAttenId(int lane, int param) { return attenId(lane, param); }  // param 0..3 = LEN/OFF/ROT/SPR

    // Does Macro DYNAMICALLY modulate this lane's SPREAD (item 3)? Used by all three
    // spread mod-arc isActive predicates (East poly, East V1, Mono) — previously hand-
    // written 3× with subtle drift. The rule (engine lane):
    //   • delegated lane → active iff Macro's spread CV (cvId(lane,3)) is connected.
    //   • owned lane     → active iff send(sendSlot,lane,3) is non-zero AND Macro's
    //                      spread CV is connected. (Static blend excluded — a non-zero
    //                      send with no live CV is a constant offset, not modulation, and
    //                      gating on it raced the manual-knob turn → red residue arc.)
    // Caller resolves `delegated` (owner storage differs per module) and `sendSlot`
    // (mono slot 0 for V1/Mono; the voice slot for poly). macro may be null → false.
    inline bool macroSpreadModulatesLane(rack::Module* macro, int engineLane,
                                         bool delegated, int sendSlot) {
        if (!macro || engineLane < 0 || engineLane >= 4) return false;
        bool sprCvLive = macro->inputs[cvId(engineLane, 3)].isConnected();
        if (delegated) return sprCvLive;
        float send = (redDot::findMonsoonEitherSide(macro)
            ? redDot::findMonsoonEitherSide(macro)->getMacroSend(sendSlot, engineLane, 3) : 0.f);
        return std::fabs(send) > 1e-4f && sprCvLive;
    }
    // P9b: PRE/POST tap params re-homed to this expander's namespace (TAP_START, above):
    // tapLorId/tapSprId/tapIdForItem. Still params (visible trimpots).
}

struct StraitsSandsMacroVisual : Module {

    StraitsSandsMacroVisual() {
        using namespace StraitsMacroVisualIds;
        // RIGHT-SIZED: local param count only, now that the 12 global LOR params were
        // re-homed out of the shared MonsoonIds pool (see GLOBAL_DNA_START).
        config(StraitsMacroVisualIds::NUM_SPREAD_PARAMS, StraitsMacroVisualIds::NUM_INPUTS,
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
        for (int lane=0; lane<4; ++lane) {
            // P9b: TWO PRE/POST taps per lane — LOR (LEN/OFF/ROT) and SPREAD. Default
            // 1.0 (POST = send draws attenuated CV). 0.0 = PRE (raw CV pre-atten).
            configParam(tapLorId(lane), 0.f,1.f,1.f, std::string(laneNames[lane])+" LOR send tap (PRE-POST)");
            configParam(tapSprId(lane), 0.f,1.f,1.f, std::string(laneNames[lane])+" spread send tap (PRE-POST)");
            for (int c=0; c<4; ++c) {
                std::string nm = std::string(laneNames[lane])+" "+paramNames[c];
                configParam(StraitsMacroVisualIds::attenId(lane,c), -1.f,1.f,0.f, nm+" depth");
                configInput(StraitsMacroVisualIds::cvId(lane,c), nm+" CV"); //check sandsmono visual version 3 lanes only?
            }
        }

        // Global L/O/R params
        configParam(globalDnaId(0,0),   1.f,16.f,16.f,"Global REST Length");
        configParam(globalDnaId(0,1),   0.f,15.f, 0.f,"Global REST Offset");
        configParam(globalDnaId(0,2),   0.f,15.f, 0.f,"Global REST Rotation");
        configParam(globalDnaId(1,0), 1.f,16.f,16.f,"Global MELODY Length");
        configParam(globalDnaId(1,1), 0.f,15.f, 0.f,"Global MELODY Offset");
        configParam(globalDnaId(1,2), 0.f,15.f, 0.f,"Global MELODY Rotation");
        configParam(globalDnaId(2,0), 1.f,16.f,16.f,"Global OCTAVE Length");
        configParam(globalDnaId(2,1), 0.f,15.f, 0.f,"Global OCTAVE Offset");
        configParam(globalDnaId(2,2), 0.f,15.f, 0.f,"Global OCTAVE Rotation");
        configParam(globalDnaId(3,0), 1.f,16.f,16.f,"Global ACCENT Length");
        configParam(globalDnaId(3,1), 0.f,15.f, 0.f,"Global ACCENT Offset");
        configParam(globalDnaId(3,2), 0.f,15.f, 0.f,"Global ACCENT Rotation");

        // Per-voice Macro→voice mix-in sends (relocated from East). 12 display proxies
        // (selected-voice view, bound on the panel) + 180 per-voice store. Default 0
        // = opt-in: Macro global CV reaches a voice only when dialed in for that voice.
        for (int lane=0; lane<4; ++lane)
            for (int item=0; item<4; ++item)
                configParam(sendDispId(lane,item), -1.f,1.f,0.f,
                            "L"+std::to_string(lane)+" mix-in "+std::to_string(item)+" (selected voice)");
        // send store MIGRATED to Monsoon::editor.macroSend (getMacroSend/setMacroSend) --
        // no per-voice configParam here; the visible send cells are sendDispId (configured above).
        // Direction display proxies (4 poly lanes). DirCell writes 0..3 = Fwd/Rev/Pend/PingPong.
        static const char* dirNames[4] = {"MEL","OCT","REST","ACC"};
        for (int l = 0; l < 4; ++l) {
            configParam(dirDispId(l), 0.f, 3.f, 0.f,
                        std::string(dirNames[l]) + " direction");
            configInput(dirModId(l), std::string(dirNames[l]) + " direction gate-mod");
        }
    }

    void process(const ProcessArgs&) override;   // defined in .cpp (needs findMonsoonEitherSide)

    // Gate edge detection state for dir_mod inputs (mono, 1 channel per jack).
    bool dirModPrev[4] = {};

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
    // P9: the send PRE/POST tap applies ONLY to what the sends distribute, not to
    // Macro's own LOR/spread display (which always uses the true POST macroCVDelta).
    // macroSendDelta = the tapped CV delta the East/Mono send mix-ins read.
    float macroSendDelta[4][4] = {};

    json_t* dataToJson() override {
        json_t* r = json_object();
        return r;
    }
    void dataFromJson(json_t* root) override {
        (void)root;  // spread target is always voice 1 (no mode to persist)
    }
};
