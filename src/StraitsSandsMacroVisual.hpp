#pragma once
#include <rack.hpp>
#include "ui/SandsGrid.hpp"
#include <cmath>   // std::fabs (macroSpreadModulatesLane)
#include "Monsoon.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/StoreBound.hpp"   // redDot::StoreKnob (de-parammed controls)   // redDot::findMonsoonEitherSide (MACRO field accessors)
#include "ui/SweepSigGate.hpp"   // RESTORED: SweepSig — the prob-out sweep's input-change gate (CC_PROMPT_restore_sweepsig_gate_START)
#include "ui/ModuleLookup.hpp"   // THE module-lookup authority: lookupMonsoon (event-driven adjacency; the monLookupDiv poll is deleted)

using namespace rack;
using namespace MonsoonIds;

namespace StraitsMacroVisualIds {

    // ── Panel ──────────────────────────────────────────────────────────────
    // Macro does the same job as the East visual (spread control) but GLOBAL
    // rather than per-lane. SANDS CONSOLIDATION Step 3: widened 48→52HP (the extra
    // 10HP on the RIGHT carries the 6-column mix-in send row per lane); left/center column
    // geometry still matches East for consistency.
    // CC_PROMPT_04: +9mm for the w-level column (ED_X 88→97) + 9mm for the warp-edit
    // button column ("CRV", right of the dir cell).
    static constexpr float W_MM    = 312.64f;   // 58HP + 9mm (w-level) + 9mm (warp-edit)
    static constexpr float WLEVEL_X   = 88.f;   // w-level knob column (the OLD ED_X slot)
    static constexpr float OWNER_X    = 214.f;  // owner cell column (matches East; +9 with ED_X)
    static constexpr float DIR_X      = 221.f;  // direction cell column (matches East)
    static constexpr float WARPEDIT_X = 229.f;  // CC_PROMPT_04 STEP 2: "CRV" button column
    static constexpr float DIR_MOD_X  = 238.f;  // direction gate-mod jack column (+9 for CRV)
    static constexpr float PROB_OUT_X = 248.f;  // poly prob-out jack (moved L to clear the send cols)
    // ── RHS mix-in SEND grid (6 columns × 7 rows, row-aligned per lane). SANDS CONSOLIDATION
    //    Step 3: single 6-knob row per lane, 9mm pitch (matches left-side mod knobs).
    //    Cols 0..3 = LEN/OFF/ROT/SPR sends, col 4 = LOR tap, col 5 = SPR tap.
    static constexpr float SEND_COL0_X = 258.f;   // LEN send   (+9 grid shift, +9 CRV column)
    static constexpr float SEND_COL1_X = 267.f;   // OFF send
    static constexpr float SEND_COL2_X = 276.f;   // ROT send
    static constexpr float SEND_COL3_X = 285.f;   // SPR send
    static constexpr float SEND_COL4_X = 294.f;   // LOR tap (PRE/POST)
    static constexpr float SEND_COL5_X = 303.f;   // SPR tap (PRE/POST)
    // Bound to the grid so Mono/East/Macro cannot drift (mirrors East). 7 poly lanes
    // (MEL/OCT/QMIX/REST/ACC/VAR/LEG), LANE_H=13. Was a stale hardcoded ROW_BOT=108 /
    // N_ROWS=4 (pre-QMIX "REST/MEL/OCT/ACCENT"); keep them grid-derived so the file can
    // never re-introduce a 4-/5-lane assumption.
    static constexpr float ROW_TOP = dotModular::SandsGrid::LANE_TOP;      // 14
    static constexpr float ROW_BOT = dotModular::SandsGrid::polyBottom();  // 105 (7×13)
    static constexpr int   N_ROWS  = dotModular::SandsGrid::POLY_LANES;    // 7 (SANDS CONSOLIDATION Step 1)
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
    static constexpr float ED_X   = 97.f;   // editor (+9mm: the w-level column sits left of the grid)
    static constexpr float ED_W   = 111.f;  // editor width (fixed; no longer tied to PROB_OUT_X)
    // Mirror TAB_TOP_OFFSET_MM in gen_macro_mono.py (extra top margin; 0.5cm=5mm).
    // Base 18 matches the generator's editor recess (was 16 here — a small drift;
    // aligned now so the widget editor sits exactly on the drawn recess).
    static constexpr float TAB_TOP_OFFSET_MM = 5.f;   // (retained; tabs now sit ABOVE the grid)
    // Voice tabs moved into 3..13mm (above the grid) so lane 0 starts at LANE_TOP like Mono.
    static constexpr float ED_Y   = dotModular::SandsGrid::LANE_TOP;   // 14 (was 23)
    // Editor holds 7 poly lanes (MEL/OCT/QMIX/REST/ACC/VAR/LEG); 13mm each. ED_LANE_H
    // drives prob-out + send-knob vertical placement and must match the gen script's ED_H/N.
    static constexpr float ED_H      = dotModular::SandsGrid::polyHeight();  // 91 (7×13, was 65)
    static constexpr float ED_LANE_H = dotModular::SandsGrid::LANE_H;        // 13 (Option B)

    // Left-control rows align with the EDITOR lane centres (not the full panel),
    // so each lane's CV jacks + attens sit beside the visual lane they modulate.
    static inline float rowY(int r) {
        return ED_Y + (r + 0.5f) * ED_LANE_H;
    }

    // ── Param IDs ─────────────────────────────────────────────────────────
    enum SpreadParamId {
        // Display spread trimpots (0-4)
        SPREAD_REST = 0, SPREAD_MELODY, SPREAD_OCTAVE, SPREAD_ACCENT, SPREAD_QMIX,
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
        // against the file-scope using namespace MonsoonIds. Lane order matches the
        // MonsoonIds layout that was replaced: REST, MELODY, OCTAVE, ACCENT; c = 0 LEN,
        // 1 OFF, 2 ROT.
        GLOBAL_DNA_START = TAP_START + 8,       // = 48 .. 59
        NUM_SPREAD_PARAMS = GLOBAL_DNA_START + 12   // = 60
    };
    // Global LOR/DNA param pool blocks are keyed by lane IDENTITY (fixed layout:
    // REST=0, MELODY=3, OCTAVE=6, ACCENT=9). Lane index is the NEW order
    // (MELODY=0, OCTAVE=1, QMIX=2, REST=3, ACCENT=4). Map new lane -> its identity block.
    static inline int globalDnaId(int lane, int c) {
        const int blk = (lane == 0) ? 3   // PL_MELODY -> MELODY block
                      : (lane == 1) ? 6   // PL_OCTAVE -> OCTAVE block
                      : (lane == 3) ? 0   // PL_REST   -> REST block
                      : (lane == 4) ? 9   // PL_ACCENT -> ACCENT block
                      :               6;  // PL_QMIX (lane 2) shares OCTAVE block (as before: old 'else')
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
        CV_START = 0,                    // 7 poly lanes × 4 cols (LEN/OFF/ROT/SPR) = 28 (0..27)
        DIR_MOD_START = CV_START + dotModular::SandsGrid::POLY_LANES * 4,   // = 28 — direction gate-mod (7 poly lanes)
        NUM_INPUTS = DIR_MOD_START + dotModular::SandsGrid::POLY_LANES   // = 35
    };
    static inline int dirModId(int lane) { return DIR_MOD_START + lane; }
    static inline int cvId(int lane, int c) { return CV_START + lane*4 + c; }

    // ── Output IDs ────────────────────────────────────────────────────────
    // 3 poly probability CV outs (REST/MEL/OCT): ch1 reserved (future mono tab),
    // ch2..1+nVoices = per-voice ensemble.
    enum OutputId {
        // Unified lane order (MEL=0 OCT=1 QMIX=2 REST=3 ACC=4 VAR=5 LEG=6) — matches the
        // panel rows, the resolver writes and the labels. (Was REST-first from the OLD
        // spread order, so every id's NAME was crossed with the data it carries.)
        PROB_OUT_START = 0, PROB_OUT_MEL, PROB_OUT_OCT, PROB_OUT_QMIX, PROB_OUT_REST,
        PROB_OUT_ACC, PROB_OUT_VAR, PROB_OUT_LEG,   // SANDS CONSOLIDATION Step 1
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
    // Spread: stored in SPREAD_* display params. Lane index is the NEW visual/engine
    // order (MELODY=0, OCTAVE=1, QMIX=2, REST=3, ACCENT=4); map each to its fixed
    // SPREAD_* slot (enum order REST=0, MELODY=1, OCTAVE=2, ACCENT=3, QMIX=4).
    inline int sprId(int lane) {
        if (lane == 0) return SPREAD_MELODY;  // PL_MELODY
        if (lane == 1) return SPREAD_OCTAVE;  // PL_OCTAVE
        if (lane == 2) return SPREAD_QMIX;    // PL_QMIX
        if (lane == 3) return SPREAD_REST;    // PL_REST
        return              SPREAD_ACCENT;    // PL_ACCENT (lane 4)
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
    // Caller resolves delegated (owner storage differs per module) and sendSlot
    // (mono slot 0 for V1/Mono; the voice slot for poly). macro may be null → false.
    inline bool macroSpreadModulatesLane(rack::Module* macro, int engineLane,
                                         bool delegated, int sendSlot) {
        // SANDS CONSOLIDATION: 7 poly lanes (REST/MEL/OCT/ACC/QMIX/VAR/LEG), not 4. The old
        // >= 4 bound silently rejected QMIX (4), VARIATION (5), LEGATO (6) so their spread
        // mod-arcs never lit (a UI regression after the consolidation added the newer lanes).
        if (!macro || engineLane < 0 || engineLane >= 7) return false;
        bool sprCvLive = macro->inputs[cvId(engineLane, 3)].isConnected();
        if (delegated) return sprCvLive;
        // LOOKUP AUTHORITY (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START): ONE memoized resolve
        // (was a DOUBLE raw walk — the host was chain-walked twice per call).
        Monsoon* mon = redDot::lookupMonsoon(macro);
        float send = mon ? mon->getMacroSend(sendSlot, engineLane, 3) : 0.f;
        return std::fabs(send) > 1e-4f && sprCvLive;
    }
    // P9b: PRE/POST tap params re-homed to this expander's namespace (TAP_START, above):
    // tapLorId/tapSprId/tapIdForItem. Still params (visible trimpots).
}

struct StraitsSandsMacroVisual : Module {

    StraitsSandsMacroVisual() {
        using namespace StraitsMacroVisualIds;
        // config() PARAMS = 0 (MVC step 1: Macro fully de-parammed). Every group --
        // attenuverters, spread, taps, LOR, direction, sends -- is store-backed now
        // (StoreKnob/store-backed DirCell bound by SVG shape name, not paramId), so Macro
        // exposes NO host params. The id constants stay declared (they name SVG shapes like
        // "param_SPREAD_REST" and index the store), but config() reserves no param slots.
        // Macro has left the host/DAW param list entirely -- the point of the de-param.
        config(0, StraitsMacroVisualIds::NUM_INPUTS,
               StraitsMacroVisualIds::NUM_OUTPUTS, 0);
        // (the monLookupDiv /8 topology poll is DELETED — CC_PROMPT_MODULE_LOOKUP_
        //  AUTHORITY_START: adjacency is event-driven now, onExpanderChange bumps the
        //  topology epoch and lookupMonsoon's memo re-resolves; a static rack walks
        //  ZERO times. See ui/ModuleLookup.hpp.)
        // RESTORED (CC_PROMPT_restore_sweepsig_gate_START): the /32 tick the prob-CV
        // sweep's input-change gate runs on. 70dd837 deleted it for a step-cache that
        // resolved on step edges unconditionally — Rodney's bisect: Macro-only 0.4% →
        // ~3%. The gate (ui/SweepSigGate.hpp) skips the whole resolve when its inputs
        // are bit-unchanged; the /32 cadence ALONE is not the fix (e0981d5, reverted).
        probOutDiv.setDivision(32);
        // Labels from the canonical table — the old local literal was OLD-order, so
        // every jack tooltip named the wrong lane (id 0 carries MEL, was labelled "REST").
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l)
            configOutput(StraitsMacroVisualIds::PROB_OUT_START + l,
                std::string("Probability ") + dotModular::SandsLaneNames::EDITOR[l] + " (poly: ch2+ voices)");

        // SANDS CONSOLIDATION Step 1: 7 poly lanes (unified order; matches cvId(lane,c)).
        // (Historical: a 5-initializer local literal left lanes 5/6 nullptr → the
        // std::string(nullptr) instantiation crash. Canonical table since.)
        static const char* const* laneNames = dotModular::SandsLaneNames::EDITOR;
        static const char* paramNames[4] = {"Len","Off","Rot","Spr"};
        for (int lane=0; lane<dotModular::SandsGrid::POLY_LANES; ++lane) {
            // P9b: TWO PRE/POST taps per lane — LOR (LEN/OFF/ROT) and SPREAD. Default
            // 1.0 (POST = send draws attenuated CV). 0.0 = PRE (raw CV pre-atten).
            for (int c=0; c<4; ++c) {
                std::string nm = std::string(laneNames[lane])+" "+paramNames[c];
                // attenId: STORE-BACKED (StoreKnob) - no configParam, not host-exposed.
                configInput(StraitsMacroVisualIds::cvId(lane,c), nm+" CV"); //check sandsmono visual version 3 lanes only?
            }
        }

        // Global L/O/R params
        // globalDnaId (12 LOR params) are STORE-BACKED (MVC step 1: LOR de-param) - no
        // configParam, not host-exposed. Macro's global LOR lives in editor.globalLor[12],
        // read by the engine (getGlobalLor) and persisted (editorGlobalLor); saveLOR/loadLOR
        // are the single reader/writer. The 12 ID slots stay declared (renumbering the enum
        // would shift every later id) until a later cleanup reclaims them, matching how the
        // attenuverters were de-parammed.

        // Per-voice Macro→voice mix-in sends (relocated from East). 12 display proxies
        // (selected-voice view, bound on the panel) + 180 per-voice store. Default 0
        // = opt-in: Macro global CV reaches a voice only when dialed in for that voice.
        // sendDispId (16 send params) are STORE-BACKED (MVC step 1: sends de-param) -- no
        // configParam, not host-exposed. Sends live per-voice in editor.macroSend[256]
        // (getMacroSend/setMacroSend), engine-read and persisted; the send StoreKnobs read/
        // write the live view voice's slot directly, so no display proxy and no sync dance.
        // Ids kept declared so the enum does not renumber, matching LOR/direction/attenuverters.
        // Direction display proxies (4 poly lanes). DirCell writes 0..3 = Fwd/Rev/Pend/PingPong.
        // SANDS CONSOLIDATION Step 1: 7 poly lanes (unified order; matches dirModId(l)).
        static const char* const* dirNames = dotModular::SandsLaneNames::EDITOR;
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
            // dirDispId is STORE-BACKED (MVC step 1: direction de-param) -- no configParam,
            // not host-exposed. Direction lives in editor.globalDir[4] (engine-read,
            // persisted); the store-backed DirCell is the reader/writer. Id kept declared so
            // the enum does not renumber, matching the LOR/attenuverter de-param.
            configInput(dirModId(l), std::string(dirNames[l]) + " direction gate-mod");
        }
    }

    void process(const ProcessArgs&) override;   // defined in .cpp (adjacency via redDot::lookupMonsoon)
    // THE LOOKUP AUTHORITY (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START): the event bumps
    // the global topology epoch — this module's adjacency (the lookupMonsoon memo)
    // re-resolves on the next read, from BOTH threads (engine process + widget
    // lambdas). The old /8 divider poll is deleted; a static rack walks zero times.
    void onExpanderChange(const ExpanderChangeEvent& e) override {
        redDot::bumpTopologyEpoch();
    }

    // Gate edge detection state for dir_mod inputs (mono, 1 channel per jack).
    // LOOKUP AUTHORITY: cachedMon_ + the /8 monLookupDiv poll are DELETED — the host
    // resolves through redDot::lookupMonsoon (the memoized, event-driven authority);
    // the dir-mod edge scan reads the same accessor.
    rack::dsp::ClockDivider probOutDiv;   // the /32 tick the SweepSig gate runs on (RESTORED — 70dd837 deleted it with the gate; Rodney's bisect put the 0.4% here)
    bool dirModPrev[dotModular::SandsGrid::POLY_LANES] = {};

    // ── The prob-out sweep: the input-change gate + the value cache ────────────────
    // RESTORED (CC_PROMPT_restore_sweepsig_gate_START): WHEN the rows re-resolve is
    // decided by the SweepSig fingerprint gate (ui/SweepSigGate.hpp) at the /32 tick —
    // input-driven (idle ⇒ ≈ 0 resolves; playing ⇒ step-rate), replacing 70dd837's
    // per-row step/nV trigger (keyed on the step NUMBER, it fired at every step edge
    // and could skip nothing else — Rodney's bisect: Macro-only ~3%). The rows are the
    // VALUE cache + the out-write epoch (CC_PROMPT_UI_TO_RESOLVER_FINISH2 — the CPU
    // fix, KEPT): the per-channel bit-diff walk runs only when the gate re-resolved
    // the row or the scale/channel count changed (outFresh/outScale/outCh); steady
    // state is one compare per lane per sample. val[] is voiceSlot-indexed
    // (val[ch] = channel ch's voice); one gate run fills ALL 7 rows.
    struct StepProbRow {
        bool  valid = false;
        int   step = -1;        // the lane's Macro-lens masked playhead the row holds
        int   nVoices = -1;     // eng.numPolyVoices when resolved (a change refreshes)
        float val[16] = {};     // [voiceSlot] — the ONE accessor's MacroOwn-lens values
        bool  outFresh = false; // the val[] row was written to the port at outScale/outCh
        float outScale = -1.f;  // the scaleV the last out walk used
        int   outCh = -1;       // the channel count the last out walk covered
    } stepProb_[dotModular::SandsGrid::POLY_LANES];
    // The gate's shadow signature — every input the resolve reads, nothing it doesn't
    // (the exhaustive enumeration + the deliberate drops: ui/SweepSigGate.hpp).
    SweepSig sweepSig_;
    // NOTE probOutSampleHold is inert on THIS module's outs now (the value between
    // steps is held by design); the store option stays for East's outs.

    // CV-applied global spread per lane (0=REST,1=MEL,2=OCT). processDNA writes
    // these from base + spread CV; the display reads them so spread CV is visible
    // WITHOUT moving the base knob (the old code wrote the modulated value back to
    // the SPREAD_* param, which dragged the knob — fixed).
    float spreadEffective[dotModular::SandsGrid::POLY_LANES] = {0.f, 0.f, 0.f, 0.f, 0.f};

    // Per (lane, item) split of Macro's global contribution, published by
    // processDNA::applyGlobal for the Macro/East blend equation. item: 0=LEN
    // 1=OFF 2=ROT 3=SPR. macroBase = the knob value (no CV); macroCVDelta = the
    // CV-only contribution (already scaled by Macro's own attenuverter). East's
    // sync reads these: value = base(owner) + eastCV + macroCVDelta·blendSend.
    // SANDS CONSOLIDATION Step 1: 7 poly lanes (REST/MELODY/OCTAVE/ACCENT/QMIX/VAR/LEG, engine order).
    float macroBase[7][4]    = {};
    float macroCVDelta[7][4] = {};
    // P9: the send PRE/POST tap applies ONLY to what the sends distribute, not to
    // Macro's own LOR/spread display (which always uses the true POST macroCVDelta).
    // (The old single globally-tapped macroSendDelta is DELETED — the P9b per-voice fix
    // publishes the endpoint pair below; each receiving voice selects via its own tap.)

    // PER-VOICE SEND-TAP (P9b fix): the publish carries BOTH endpoints of the
    // pre/post lerp; each receiving VOICE selects via its own macroTap (Monsoon store).
    //   macroPreDelta  = raw cv·(hi-lo)   (tap=0, PRE the attenuverter)
    //   macroPostDelta = cv·att·(hi-lo)   (tap=1, POST — the old tappedDelta at tap=1)
    // A voice's delta = tappedDelta(its tap) — see the helper below. Macro's OWN display
    // always reads macroPostDelta (Macro has no receiving voice).
    float macroPreDelta[7][4]  = {};
    float macroPostDelta[7][4] = {};

    // The delta a receiving voice gets: lerp between the published endpoints by the
    // voice's own tap (0..1). Pure arithmetic on this visual's published arrays; the
    // caller supplies the tap (getMacroTap(slot, lane, item==3 ? 1 : 0)).
    float tappedDelta(float tap, int lane, int item) const {
        const float pre  = macroPreDelta[lane][item];
        const float post = macroPostDelta[lane][item];
        return pre + (post - pre) * tap;
    }

    json_t* dataToJson() override {
        json_t* r = json_object();
        return r;
    }
    void dataFromJson(json_t* root) override {
        (void)root;  // spread target is always voice 1 (no mode to persist)
    }
};
