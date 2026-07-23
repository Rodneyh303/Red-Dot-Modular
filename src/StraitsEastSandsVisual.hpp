#pragma once
#include <rack.hpp>
#include "ui/SandsGrid.hpp"
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

namespace StraitsEastVisualIds {

    // ── Panel geometry (48HP — +4HP for dir_mod + deleg_mod + prob_out jack columns) ──
    static constexpr float W_MM        = 243.84f;  // 48HP
    static constexpr float ED_X        = 88.f;
    static constexpr float ED_W        = 111.f;
    static constexpr float OWNER_X     = 205.f;
    static constexpr float DIR_X       = 212.f;
    // Column order matches the TOGGLE order left->right (delegation cell, then direction
    // cell), so the jacks line up with the controls they modulate instead of crossing over.
    static constexpr float DELEG_MOD_X = 220.f;    // delegation gate-mod jack column
    static constexpr float DIR_MOD_X   = 228.f;    // direction gate-mod jack column
    static constexpr float PROB_OUT_X  = 236.f;    // prob-out jack column (shifted right)
    static constexpr int   N_ROWS      = dotModular::SandsGrid::POLY_LANES;  // 4 — 1 row per lane
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
    // Stage 1 of EAST_EXTRA_LANES.md: East shows 6 lanes (adds VARIATION/LEGATO), so its editor
    // spans the SAME band as Mono (14..98). Lanes 4/5 are locked/display-only for now.
    static constexpr float ED_H      = dotModular::SandsGrid::monoHeight();  // 84 (4-lane was 56)
    static constexpr int   N_EDITOR_LANES = dotModular::SandsGrid::EAST_LANES;  // 6
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
        // 6 VAR/LEG CV-depth DISPLAY proxies (LEN/OFF/ROT only — no SPR): lane 0=VAR,
        // 1=LEG; col 0..2. Selected-voice view; real depth in VARLEG_ATTEN_START.
        VARLEG_ATTEN_DISP_START = ATTEN_START + 16,   // = 20
        // 6 direction display proxies (one per editor lane 0..5). Value 0..3 =
        // Forward/Reverse/Pendulum/PingPong. Selected-voice view; the engine holds the
        // real per-voice state in laneDir_/laneDirV_.
        DIR_DISP_START = VARLEG_ATTEN_DISP_START + 6,  // = 26
        // 2 VAR/LEG delegation display proxies (lane 0=VAR, 1=LEG), re-homed here from
        // MonsoonIds::VARLEG_DELEG_DISP (NUM_PARAMS_MIGRATION.md): they drive a visible
        // OwnerCell so they MUST stay params, but in the EXPANDER's small namespace so they
        // don't inflate the shared MonsoonIds pool.
        VARLEG_DELEG_DISP_START = DIR_DISP_START + 6,   // = 32
        // 4 owner display proxies (lanes 0-3), re-homed from MonsoonIds::MACRO_OWN_DISP
        // (NUM_PARAMS_MIGRATION.md): drive visible owner cells so they stay params, but in
        // the expander namespace so they don't inflate the shared pool.
        OWN_DISP_START = VARLEG_DELEG_DISP_START + 2,    // = 34
        NUM_SPREAD_PARAMS = OWN_DISP_START + 4           // = 38
    };
    // Owner display proxy (selected-voice view). lane 0..3.
    static inline int ownerDispId(int lane) { return OWN_DISP_START + lane; }
    // VAR/LEG delegation display proxy (selected-voice view). lane 0=VAR, 1=LEG.
    static inline int varlegDelegDispId(int lane) { return VARLEG_DELEG_DISP_START + lane; }
    // Direction display proxy (selected-voice view). lane = editor lane 0..5.
    static inline int dirDispId(int lane) { return DIR_DISP_START + lane; }
    // Selected-voice display proxy (physical knob binds here).
    static inline int attenDispId(int lane, int col) { return ATTEN_START + lane*4 + col; }
    // Per-voice CV depth (real store) MIGRATED to Monsoon::editor.macroAtten -- use
    // monsoon->getMacroAtten/setMacroAtten(v, lane*4+col). (attenDispId above stays local.)
    // VAR/LEG CV-depth display proxy (selected-voice view). lane 0=VAR, 1=LEG; col 0..2.
    static inline int varlegAttDispId(int lane, int col) { return VARLEG_ATTEN_DISP_START + lane*3 + col; }
    // VAR/LEG per-voice CV depth (the real store) MIGRATED to Monsoon::editor.varlegAtten:
    // use monsoon->getVarlegAtten/setVarlegAtten(v,lane,col). (v=0..15, slot 0 = mono/V1.)

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        CV_START = 0,
        NUM_LANE_INPUTS = CV_START + 16,              // 4 lanes × 4 cols (LEN/OFF/ROT/SPR)
        // VAR/LEG poly CV inputs (LEN/OFF/ROT only — no SPR). lane 0=VAR, 1=LEG; col 0..2.
        VARLEG_CV_START = NUM_LANE_INPUTS,            // = 16
        DIR_MOD_START = VARLEG_CV_START + 6,          // = 22 — direction gate-mod (6 poly jacks)
        DELEG_MOD_START = DIR_MOD_START + 6,          // = 28 — delegation gate-mod (6 poly jacks, all lanes)
        NUM_INPUTS = DELEG_MOD_START + 6              // = 34
    };
    static inline int dirModId(int lane) { return DIR_MOD_START + lane; }
    static inline int delegModId(int lane) { return DELEG_MOD_START + lane; }
    static inline int cvId(int lane, int col) { return CV_START + lane*4 + col; }
    // VAR/LEG CV jack id. lane 0=VAR, 1=LEG; col 0=LEN,1=OFF,2=ROT.
    static inline int varlegCvId(int lane, int col) { return VARLEG_CV_START + lane*3 + col; }

    // ── LOR bank helper ───────────────────────────────────────────────────

    // Unified LOR bank for an EDITOR lane: engine lane for 0..3 (REST/MEL/OCT/ACC), self for
    // VAR(4)/LEG(5). Mirrors lorIdEditor's mapping so Monsoon::getLorBase/setLorBase index the
    // same per-voice slot the old POLY_*_VOICE_1_LEN params did.
    static inline int lorBank(int editorLane) {
        return (editorLane <= 3) ? dotModular::EDITOR_TO_ENGINE_LANE[editorLane] : editorLane;
    }

    // Macro/East base owner per (voice, lane): MonsoonIds::MACRO_OWN_START + v*4 + lane.
    // 0 = Macro owns (default), 1 = East owns. 4 lanes: REST/MEL/OCT/ACCENT.
    // ownerId MIGRATED to Monsoon::editor.macroOwn -- getMacroOwn/setMacroOwn(v,lane).
    // V1 (mono) per-lane Macro-delegation owner. The MACRO_OWN block reserves 64 params
    // but poly only uses v=0..14 (slots 0..59); slots 60..63 (v=15) are spare. Reuse them
    // as the mono owner store so V1 delegation persists in the patch like poly voices do
    // (real configSwitch params → Rack auto-saves them). 1.f = East owns, 0.f = Macro owns.
    // monoOwnerId MIGRATED -- getMonoMacroOwn/setMonoMacroOwn(lane).
    // VAR/LEG per-voice delegation toggle (EAST_EXTRA_LANES §4d). 0 = delegate to mono
    // (default), 1 = Local East (own LOR). 15 poly voices (v=0..14 = V2..V16) × 2 lanes
    // (lane 0 = VAR, 1 = LEG). V1 is mono → always follows, no toggle.
    // VAR/LEG delegation + attenuation MIGRATED to Monsoon::editor (NUM_PARAMS_MIGRATION.md):
    // use monsoon->getVarlegDeleg/setVarlegDeleg(v,lane) and
    // getVarlegAtten/setVarlegAtten(v,lane,col). The DISPLAY proxy varlegDelegDispId re-homed
    // to this expander's own namespace (VARLEG_DELEG_DISP_START, above) since it drives a
    // visible OwnerCell.
    // East's per-voice LANE DIRECTION bank (plans/lane_direction_homes.md). 4-state per lane:
    // 0=Forward 1=Reverse 2=Pendulum 3=PingPong (SequencerEngine::LaneDir). Poly-bank indexed
    // like ownerId/varlegDelegId (v = 0..14 = V2..V16); lane = STRAND index 0..5. The manager
    // reads this module-side and pushes it to the engine, so direction is expander-homed and
    // engine-derived like every other lane-addressing datum.
    // LANE_DIR migrated to Monsoon::editor.laneDir -- use monsoon->getLaneDir(v,lane) /
    // setLaneDir(v,lane,x) and getMonoLaneDir/setMonoLaneDir(lane) instead of dirId/monoDirId.
    // (v = 0..14 poly bank = V2..V16; mono is the spare slot, via the Mono accessors.)
    // (Macro mix-in send helpers relocated to StraitsMacroVisualIds under the control
    //  inversion — the send is a Macro concern now.)
    // Owner display proxy (selected-voice view; copied to/from per-voice on switch).
    // ownerDispId re-homed to expander namespace (OWN_DISP_START, above).

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
        // RIGHT-SIZED to the LOCAL param count (38). Verified by census: all 9 configured
        // param ids resolve to StraitsEastVisualIds, none to the shared MonsoonIds pool,
        // and nothing reads this module's params[] by a MonsoonIds index. Claiming
        // MonsoonIds::NUM_PARAMS (152) was pure over-allocation.
        config(StraitsEastVisualIds::NUM_SPREAD_PARAMS, StraitsEastVisualIds::NUM_INPUTS,
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

        // VARIATION / LEGATO poly CV (LEN/OFF/ROT only — no SPR, no spread). Display
        // proxies + per-voice depth store, same selected-voice pattern as the 4 lanes.
        // ch0 of each jack is the mono/V1 mix-in (slot 0); ch1.. feed poly voices.
        {
            static const char* vlNames[2] = {"VARIATION","LEGATO"};
            static const char* vlItems[3] = {"Len","Off","Rot"};
            for (int lane=0; lane<2; ++lane)
                for (int c=0; c<3; ++c) {
                    std::string nm = std::string(vlNames[lane])+" "+vlItems[c];
                    configParam(varlegAttDispId(lane,c), -1.f,1.f,0.f,
                                nm+" depth (selected voice)");
                    configInput(varlegCvId(lane,c),
                                nm+" CV (poly: ch1=mono mix-in, ch2+ voices)");
                }
            // Per-voice depth store (16-wide, slot 0 = mono/V1).
            // Per-voice VAR/LEG depth store MIGRATED to Monsoon::editor.varlegAtten
            // (NUM_PARAMS_MIGRATION.md) -- no configParam here; accessors carry it.
        }

        for (int v=0; v<15; ++v) {   // poly voices 2..16: owner/send/atten banks are 15-wide
            std::string vl = "V"+std::to_string(v+2)+" ";
            // (LOR base + spread migrated to Monsoon::editor.lorBase / .spread — no params here.)

            // Base owner (0=Macro default, 1=East) + Macro-CV blend sends (unity
            // default) per lane. Switch/snap so owner reads as discrete 0/1.
            // owner base MIGRATED to Monsoon::editor.macroOwn -- no configSwitch here.
            // VAR/LEG delegation (§4d): the only target is mono (no Macro), so this is a
            // clean binary — follow mono (default, silent) or Local East (own LOR). lane
            // 0 = VAR, 1 = LEG. Rendered as the lane-end toggle for editor lanes 4/5.
            // VAR/LEG delegation store MIGRATED to Monsoon::editor.varlegDeleg
            // (NUM_PARAMS_MIGRATION.md) -- no configSwitch; setVarlegDeleg carries it. The
            // visible selected-voice cell is varlegDelegDispId (expander-local, configured below).
        }
        // V1 (mono) per-lane owner store (spare MACRO_OWN slot v=15). Default 0.f =
        // inherit Macro, matching the poly ownerId and ownerDispId defaults (consistent
        // delegation convention; toggle per lane via the V1 owner cell).
        gateModDiv.setDivision(GATE_MOD_DIV);   // gate-mod scan runs at /8, not per sample
        probOutDiv.setDivision(16);             // prob CV outs recompute at /16 (~3 kHz @48k)
        // East LANE DIRECTION bank: 16 slots (0..14 = poly V2..V16, 15 = V1/mono spare slot,
        // the monoOwnerId trick) x 6 strands. Real params, so Rack persists them and the
        // LANE_DIR migrated out of params[] to Monsoon::editor.laneDir (NUM_PARAMS_MIGRATION.md).
        // No configSwitch here anymore -- the direction state is plain fields on Monsoon,
        // reached via getLaneDir/setLaneDir. (Default Forward = 0 = field default.)
        // V1 owner base MIGRATED to Monsoon::editor.macroOwn (mono slot) -- no configSwitch.
        // VAR/LEG delegation display proxies (selected-voice cells). lane 0=VAR, 1=LEG.
        for (int lane=0; lane<2; ++lane) {
            configSwitch(varlegDelegDispId(lane), 0.f,1.f,0.f,
                         std::string(lane==0?"VAR":"LEG")+" delegate: follow mono / local East",
                         {"Follow mono","Local East"});
        }
        // Per-voice CV depth for each of the 12 jacks — its own bank is 16-wide now
        // (slot 0 = voice 1/mono, slot v = voice v+1), so the mono mix-in's depth no longer
        // aliases poly voice 2's. The panel's 12 attenuverters are display proxies copied to
        // the selected voice's slice on voice switch.
        for (int v=0; v<16; ++v) {
            std::string vl = "V"+std::to_string(v+1)+" ";   // slot v = voice v+1
            // atten store MIGRATED to Monsoon::editor.macroAtten (getMacroAtten/setMacroAtten);
            // the per-voice depth params no longer live in params[]. The 12 visible attenuverters
            // are the display proxies (attenDispId), configured above.
            (void)vl;
        }
        // Direction display proxies (selected-voice view). lane = editor lane 0..5.
        // Value 0=Forward, 1=Reverse, 2=Pendulum, 3=PingPong. Default 0 (Forward).
        // Direction gate-mod inputs (poly: ch1=mono, ch2+=voices). Gate cycles Fwd→Rev→Pend→PingPong.
        // Delegation gate-mod inputs (poly: ch1=mono, ch2+=voices). Gate flips local/delegated.
        {
            const char* laneNm[6] = {"MEL","OCT","REST","ACC","VAR","LEG"};
            for (int lane=0; lane<6; ++lane) {
                configParam(dirDispId(lane), 0.f, 3.f, 0.f,
                            std::string(laneNm[lane])+" direction (selected voice)");
                configInput(dirModId(lane), std::string(laneNm[lane])+" direction gate-mod (poly)");
            }
            for (int lane=0; lane<6; ++lane)
                configInput(delegModId(lane), std::string(laneNm[lane])+" delegation gate-mod (poly)");
        }
   }

    void process(const ProcessArgs&) override;   // defined in .cpp (needs findMonsoonEitherSide)

    // Gate edge detection state for dir_mod and deleg_mod inputs.
    // dirModPrev[lane][channel] — 6 lanes × 16 channels (ch0=mono, ch1..15=voices 2..16)
    // delegModPrev[lane][channel] — 4 lanes × 16 channels
    // ── Gate-mod inputs (dir_mod / deleg_mod) ───────────────────────────────────
    // Deliberately split across threads. The AUDIO thread does one job: spot rising edges
    // and bump a counter. The UI thread (widget step()) interprets them — it already owns
    // the display-proxy <-> per-voice-store sync and it alone knows which tab is shown, so
    // nothing has to read the selected voice at audio rate.
    //
    // Counters are monotonic uint8 and the audio thread is their ONLY writer; the widget
    // keeps its own "seen" copy and applies the unsigned difference, so wrap is harmless and
    // no edge is lost or double-applied (a burst between frames applies as N cycles).
    //
    // The scan runs on a divider rather than every sample: a gate is milliseconds long
    // (hundreds of samples), so /8 still catches every edge at an eighth of the cost.
    static constexpr int GATE_MOD_DIV = 8;
    rack::dsp::ClockDivider gateModDiv;
    Monsoon* cachedMon_  = nullptr;   // refreshed on gateModDiv (topology is control-rate)
    bool     gateModScan_ = false;    // set when gateModDiv ticks; consumed by the gate scan
    rack::dsp::ClockDivider probOutDiv;   // prob CV outs: /16 (Rodney audit item 2)
    bool    dirModPrev[6][16]    = {};
    bool    delegModPrev[6][16]  = {};
    uint8_t dirModEdges[6][16]   = {};   // audio thread is the sole writer
    uint8_t delegModEdges[6][16] = {};
    // Channel count of each connected mod cable. A 1-channel cable into a poly input is
    // BROADCAST to every target (the VCV norm), rather than hitting only ch0 = V1 — a mono
    // gate means "apply this everywhere", not "apply it to voice 1".
    uint8_t dirModChans[6]   = {};
    uint8_t delegModChans[6] = {};

    json_t* dataToJson() override {
        json_t* r = json_object();
        return r;
    }
    void dataFromJson(json_t* root) override {
    }
};
