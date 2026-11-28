// SandsFold — the seamless-fold RESIZE SPIKE's model (CC_PROMPT_FOLD_RESIZE_SPIKE.md,
// the WIDGET HALF). One NEW module whose width changes at runtime, saved in the
// patch, neighbours pushed aside, undoable, hosting the Sends band (98 store-backed
// knobs — no ports, no Rack params; the de-param law).
//
// This header is the MODEL half only (state + the JSON law), kept Monsoon-free so
// the width law is exactly as testable as the Rack-free core it rides
// (ui/FoldPanelWidthCore.hpp — every number comes from there; test_sands_fold_width
// pins the round trip + the garbage set with INDEPENDENT oracles). The widget
// (src/SandsFold.cpp) owns the Rack surface.
#pragma once
#include <rack.hpp>
#include "ui/FoldPanelWidthCore.hpp"

using namespace rack;

// THE NAME — ONE constant (Rodney renames; Singapore-geography naming is brand
// identity, so the slug lives here and nowhere else).
constexpr const char* kSandsFoldSlug = "SandsFold";

struct SandsFold : Module {
    // ── the Sends band's geometry (mm) — MUST match SandsMacroSendSubpanel.cpp /
    //    panel_src/gen_sands_macro_send.py (the subpanel's copies are file-local;
    //    the widget static_asserts the COUNTS against SandsMacroSendIds) ──
    static constexpr float SEND_X0 = 20.f, TAP_X0 = 58.f, DIST_X0 = 78.f, COL_DX = 9.f;
    static constexpr float ROW_TOP = 27.0f, ROW_PITCH = 12.6f;
    static constexpr int   LANES = 7, NITEM = 4, NTAP = 2, NPAR = 8;   // 7 x (4+2+8) = 98 knobs
    // the band's content extent: the last column + a knob-radius margin (the
    // min-content rule's input — the width can never squeeze below this).
    static constexpr float BAND_RIGHT_MM  = DIST_X0 + (NPAR - 1) * COL_DX + 6.f;

    // ── the width state (HP) ──
    int panelHp = 0;   // constructed below to the default; ALWAYS an allowed width

    static int minContentHp() { return redDot::fold::contentMinHp(BAND_RIGHT_MM); }
    static int defaultHp()    { return redDot::fold::clampHp(40, minContentHp()); }

    void onReset() override { panelHp = defaultHp(); }
    // Every write goes through the clamp — the module's width is ALWAYS an allowed
    // width (the core's law: anything else is snapped, never trusted).
    void setPanelHp(int hp) { panelHp = redDot::fold::clampHp(hp, minContentHp()); }

    // ── the patch JSON ──
    // Saved as a real (the patch's number type). MISSING -> the default; garbage
    // (NaN / inf / huge / off-grid) -> decodeSavedHp's snap — never a crash.
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "panelHp", json_real((double)panelHp));
        return rootJ;
    }
    void dataFromJson(json_t* rootJ) override {
        json_t* j = json_object_get(rootJ, "panelHp");
        const bool present = (j != nullptr && json_is_number(j));
        const double value = present ? json_number_value(j) : 0.0;
        panelHp = redDot::fold::decodeSavedHp(present, value, defaultHp(), minContentHp());
    }

    SandsFold() {
        config(0, 0, 0, 0);   // the de-param law: no params, no ports, no lights
        panelHp = defaultHp();
    }
};

// the model pointer (defined in src/SandsFold.cpp; declared here beside the model,
// the Monsoon.hpp extern family's convention — plugin.cpp reaches it via Monsoon.hpp)
extern Model* modelSandsFold;
