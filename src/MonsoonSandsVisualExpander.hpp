#pragma once
#include <rack.hpp>
#include "ui/SandsGrid.hpp"
#include "Monsoon.hpp"
#include "dsp/LaneMapping.hpp"   // ENGINE_LANE_TO_EDITOR / MONO_PARAM_TO_EDITOR — single source of truth
#include "ui/VisualExpanderHelpers.hpp"  // findMonsoonEitherSide (monoMacroOwnsEngineLane store read)

using namespace rack;

namespace SandsMonoVisualIds {

    // ── Panel ────────────────────────────────────────────────────────────
    static constexpr float W_MM     = 243.84f;  // 48HP (44 + 4HP for dir_mod + deleg_mod + prob_out)
    static constexpr float ED_X     = 88.f;
    static constexpr float ED_W     = 111.f;    // editor width (fixed; no longer tied to PROB_OUT_X)
    static constexpr float OWNER_X    = 205.f;  // owner cell column (matches East/Macro)
    static constexpr float DIR_X      = 212.f;  // direction cell column (matches East/Macro)
    static constexpr float DIR_MOD_X   = 220.f; // direction gate-mod jack column
    static constexpr float DELEG_MOD_X = 228.f; // delegation gate-mod jack column
    static constexpr float PROB_OUT_X = 236.f;  // output jack column (pushed right by mod columns)
    // Grid now comes from ui/SandsGrid.hpp so Mono, East and Macro cannot drift apart.
    // ROW_BOT 108 -> 98: lane height 15.667 -> 14, matching East/Macro's lanes exactly.
    static constexpr float ED_Y     = dotModular::SandsGrid::LANE_TOP;      // 14
    static constexpr float ROW_TOP  = dotModular::SandsGrid::LANE_TOP;      // 14
    static constexpr float ROW_BOT  = dotModular::SandsGrid::monoBottom();  // 98 (was 108)
    static constexpr int   N_LANES  = dotModular::SandsGrid::MONO_LANES;    // 6
    static constexpr int   N_SPREAD_LANES = 4;  // REST, MELODY, OCTAVE, ACCENT
    // Spread control index (0..3 = REST/MEL/OCT/ACCENT) → editor lane.
    // Shares the poly engine→editor mapping (dsp/LaneMapping.hpp): REST=2, MEL=0,
    // OCT=1, ACCENT=3. Single source of truth — do not redefine here.
    static constexpr const int* SPREAD_LANE_TO_EDITOR = dotModular::ENGINE_LANE_TO_EDITOR;

    // Column X positions (mm)
    // LOR CV jacks (all 6 lanes): LEN/OFF/ROT
    static constexpr float JACK_X[3]  = {6.f, 15.f, 24.f};
    // LOR attenuverters (all 6 lanes)
    static constexpr float ATTEN_X[3] = {34.f, 43.f, 52.f};
    // Spread group (REST/MEL/OCT lanes only): base trimpot, CV jack, atten
    static constexpr float SPR_BASE_X  = 62.f;
    static constexpr float SPR_CV_X    = 71.f;
    static constexpr float SPR_ATTEN_X = 80.f;

    static inline float rowY(int lane) {
        return ROW_TOP + (lane + 0.5f) * (ROW_BOT - ROW_TOP) / N_LANES;
    }

    // ── Param IDs ─────────────────────────────────────────────────────────
    enum ParamId {
        // LOR handle params: 6 lanes × 3 (LEN/OFF/ROT) = 18 (0-17).
        // EDITOR ORDER (MEL,OCT,REST,ACC,VAR,LEG) — same order the editor shows and
        // the engine strands use, so lenId(editorLane) reads directly with no remap.
        LEN_MELODY = 0, OFF_MELODY, ROT_MELODY,
        LEN_OCTAVE,     OFF_OCTAVE,     ROT_OCTAVE,
        LEN_REST,       OFF_REST,       ROT_REST,
        LEN_ACCENT,     OFF_ACCENT,     ROT_ACCENT,
        LEN_VARIATION,  OFF_VARIATION,  ROT_VARIATION,
        LEN_LEGATO,     OFF_LEGATO,     ROT_LEGATO,
        // Spread base trimpots: poly lanes in ENGINE order REST/MEL/OCT/ACCENT — kept
        // in engine order because the spread path shares SPREAD_LANE_TO_EDITOR with the
        // poly engine (which is NOT being renumbered in this step). sprId(l) takes a
        // spread index 0-3, mapped to editor via SPREAD_LANE_TO_EDITOR.
        SPR_REST, SPR_MELODY, SPR_OCTAVE, SPR_ACCENT,
        // Attenuverters: 18 LOR (6 lanes × 3) + 4 spread = 22
        ATTEN_START,                       // 22 .. 39  (18 LOR attens)
        SPR_ATTEN_START = ATTEN_START + 18, // 40 .. 43  (4 spread attens)
        // V1 ownership: per poly lane (MEL/OCT/REST/ACC, EDITOR order), latch
        // 0 = Macro owns V1's base for this lane (global base), 1 = Mono owns it
        // (this expander's own LOR edit). LEG/VAR are mono-only → always Mono-owned,
        // no owner param. Mono is single-voice (V1), so no per-voice bank needed.
        OWN_DISP_START = SPR_ATTEN_START + 4,   // 44 .. 47
        // Direction display proxy (mono direction, 6 lanes). DirCell writes here;
        // widget step() syncs to engine.laneDirPending_.
        DIR_DISP_START = OWN_DISP_START + 4,    // 48 .. 53
        NUM_PARAMS = DIR_DISP_START + 6
    };
    // V1 owner display proxy: poly lane (editor order 0=MEL 1=OCT 2=REST 3=ACC).
    inline int ownerDispId(int polyLaneEditor) { return OWN_DISP_START + polyLaneEditor; }
    // Direction display proxy: lane 0..5 (editor order MEL/OCT/REST/ACC/VAR/LEG).
    static inline int dirDispId(int lane) { return DIR_DISP_START + lane; }

    // Owner lookup BY ENGINE LANE (REST=0,MEL=1,OCT=2,ACC=3). Mono's ownerDispId is
    // EDITOR-ordered, so callers working in engine/spread lane order MUST convert via
    // ENGINE_LANE_TO_EDITOR — forgetting this caused the recurring "owner of lane N+1
    // gates lane N" off-by-one (found 5×). This helper bakes the conversion in: given a
    // Mono module and an ENGINE lane, returns true iff Macro owns that lane (delegated).
    // Returns false if mod is null. (param <= 0.5 == Macro owns; > 0.5 == Mono/local owns.)
    inline bool monoMacroOwnsEngineLane(rack::Module* mod, int engineLane) {
        if (!mod || engineLane < 0 || engineLane >= 4) return false;
        int editorLane = dotModular::ENGINE_LANE_TO_EDITOR[engineLane];
        // MVC step 1d: owner is STORE-BACKED (editor.monoOwner via getMonoOwner). Was params[ownerDispId].
        // Returns true iff Macro owns (delegated); no Monsoon → treat as Mono-owned (not delegated).
        Monsoon* m = redDot::findMonsoonEitherSide(mod);
        return !(m ? m->getMonoOwner(editorLane) : true);
    }

    // ── Input IDs ─────────────────────────────────────────────────────────
    enum InputId {
        // 18 LOR CV jacks (6 lanes × 3) + 4 spread CV jacks (REST/MEL/OCT/ACCENT) = 22
        CV_START = 0,                       // 0 .. 17
        SPR_CV_START = CV_START + 18,       // 18 .. 21
        DIR_MOD_START = SPR_CV_START + 4,   // = 22 — direction gate-mod (6 mono jacks)
        DELEG_MOD_START = DIR_MOD_START + 6, // = 28 — delegation gate-mod (4 mono jacks)
        NUM_INPUTS = DELEG_MOD_START + 4    // = 32
    };
    static inline int dirModId(int lane) { return DIR_MOD_START + lane; }
    static inline int delegModId(int lane) { return DELEG_MOD_START + lane; }

    // ── Output IDs ────────────────────────────────────────────────────────
    enum OutputId {
        // Per-lane probability CV out (editor lane order REST/MEL/OCT/LEG/ACC/VAR):
        // the final post-everything (A/B mix + spread + LOR) probability the playhead
        // goes over at the current step for that lane. S&H or continuous (menu).
        PROB_OUT_START = 0,                 // 0 .. 5
        NUM_OUTPUTS = PROB_OUT_START + 6
    };

    // ── Helpers ───────────────────────────────────────────────────────────
    inline int lenId(int l) { return LEN_MELODY + l * 3; }     // l = EDITOR lane now
    inline int offId(int l) { return LEN_MELODY + l * 3 + 1; }
    inline int rotId(int l) { return LEN_MELODY + l * 3 + 2; }
    inline int sprId(int l) { return SPR_REST + l; }          // l: 0-3 spread index (engine order)

    // LOR atten/CV: lane 0-5, param 0=LEN,1=OFF,2=ROT
    inline int attenId(int lane, int param) { return ATTEN_START + lane*3 + param; }
    inline int cvId   (int lane, int param) { return CV_START    + lane*3 + param; }
    // Spread atten/CV: lane 0-2 (REST/MEL/OCT)
    inline int sprAttenId(int l) { return SPR_ATTEN_START + l; }
    inline int sprCvId   (int l) { return SPR_CV_START    + l; }
}

struct MonsoonSandsVisualExpander : Module {
    // Effective spread per lane — written by processDNA at control rate,
    // read by widget at UI rate. Same cross-thread pattern as Rack params.
    // Base spread lives in sprId(lane) params (set by trimpot, never mutated by CV).
    // (spreadEffective removed — spread is now ENGINE state, engine.spread[slot][lane], written by
    // the manager via SpreadResolver and read via spreadE(). The Mono visual reads engine.spread for
    // its display arcs; it no longer caches its own copy. MVC: the view reads the model.)

    // Probability CV out config (persisted): scale 0=0..1V, 1=0..5V, 2=0..10V;
    // sampleHold true = latch the value at each 16th step start (the decision value),
    // false = continuous (the live modulated surface within the bar).
    float probHeld[6] = {};         // latched per-lane value for S&H mode
    int   probLastStep[6] = {-1,-1,-1,-1,-1,-1};  // last step latched per lane

    MonsoonSandsVisualExpander() {
        using namespace SandsMonoVisualIds;
        // MVC step 1d DONE: all 54 Mono params are store-backed (spread/atten/LOR/dir/owner).
        // config() reserves ZERO host param slots — Mono has left the host/DAW param list. The
        // id enums (LEN_MELODY…NUM_PARAMS) stay declared (they name panel slots) but reserve none.
        config(0, SandsMonoVisualIds::NUM_INPUTS, SandsMonoVisualIds::NUM_OUTPUTS, 0);
        monLookupDiv.setDivision(8);   // topology changes are control-rate
        for (int l = 0; l < 6; ++l)
            configOutput(PROB_OUT_START + l, std::string("Probability ") +
                (const char*[]){"MEL","OCT","REST","ACC","VAR","LEG"}[l]);

        static const char* names[6]  = {"MEL","OCT","REST","ACC","VAR","LEG"};
        static const char* lnames[3] = {"Len","Off","Rot"};

        // LOR + atten group: ALL STORE-BACKED (MVC step 1d). lenId/offId/rotId/attenId ids are
        // KEPT (they name panel slots) but reserve NO param slots — LOR base lives in
        // editor.lorBase[kMonoSlot] (grid-edited via the visualEditor), attens in editor.monoAtten.
        // Only the CV jacks (inputs, not host-exposed) are configured here.
        for (int l = 0; l < 6; ++l) {
            for (int p = 0; p < 3; ++p) {  // LEN/OFF/ROT
                configInput(cvId(l, p),
                            std::string(names[l])+" "+lnames[p]+" CV");
            }
        }
        // Spread group: STORE-BACKED (MVC step 1d). sprId/sprAttenId ids are KEPT (they name
        // the panel slots) but reserve NO param slots — the widgets are StoreKnobs bound to
        // editor.spread[kMonoSlot,l] (base) and editor.monoAtten[editorLane,3] (atten). Only the
        // CV jack (an input, not host-exposed) is configured here.
        for (int l = 0; l < N_SPREAD_LANES; ++l) {
            const char* nm = names[SPREAD_LANE_TO_EDITOR[l]];
            configInput(sprCvId(l), std::string(nm)+" Spread CV");
        }
        // V1 ownership (poly lanes 0..3): STORE-BACKED (MVC step 1d). ownerDispId ids KEPT
        // (name panel slots) but reserve NO param slots — OwnerCell reads/writes editor.monoOwner
        // via get/setMonoOwner (persisted as editorMonoOwner, default Mono owns).
        // Direction (6 lanes): STORE-BACKED (MVC step 1d). dirDispId ids KEPT (name panel slots)
        // but reserve NO param slots — DirCell reads/writes editor.laneDir[15*6+lane] via
        // get/setMonoLaneDir (persisted as editorLaneDir). Only the gate-mod jack (input) is here.
        for (int l = 0; l < 6; ++l) {
            configInput(dirModId(l), std::string(names[l]) + " direction gate-mod");
        }
        const char* delegNm[4] = {"MEL","OCT","REST","ACC"};
        for (int l = 0; l < 4; ++l)
            configInput(delegModId(l), std::string(delegNm[l]) + " delegation gate-mod");
    }
    void process(const ProcessArgs&) override;   // defined in .cpp (needs calcPlayhead)

    // Gate edge detection state (mono, 1 channel per jack).
    // PERF: chain walk is control-rate work (Rodney audit item 3).
    Monsoon* cachedMon_ = nullptr;
    rack::dsp::ClockDivider monLookupDiv;
    bool dirModPrev[6] = {};
    bool delegModPrev[4] = {};

    json_t* dataToJson() override {
        json_t* root = json_object();
        return root;
    }
    void dataFromJson(json_t* root) override { (void)root; }
};
