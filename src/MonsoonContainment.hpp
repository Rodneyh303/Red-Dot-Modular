#pragma once
// MonsoonContainment — the articulation-envelope routing module (the "containment" sibling of
// Change Alley). Authority: CC_PROMPT_CONTAINMENT_MODULE_SPEC.md + ARTICULATION_ENVELOPE_VS_
// CORRELATION.md, amended by Rodney's rulings 2026-10-26 (multi-parent acyclic DAG with
// intersection semantics; disconnected vertices valid; EXPAND = trajectory memory). Build plan:
// docs/design/CC_PROMPT_CONTAINMENT_BUILD_PLAN.md.
//
// WHAT THIS MODULE IS: an EDITOR + DRIVER for the containment graph — per-voice gate-support
// nesting (gate(v) = clamp(own, ∩ resolved parents)), which is cycle-FATAL and therefore owns
// its own routing, structurally acyclic, CA-untouched. The graph state is ENGINE-SIDE
// authoritative (mirroring the CA seam: PatternEngine owns caRhythmSrc, the CA module edits and
// drives it); this module edits pins (cycle-rejecting) and drives the DAG-closed gesture verbs
// (INVERT / ROTATE ROLES / REFLECT ROLES / COLLAPSE / COLLAPSE-ALL / EXPAND) armed as pending
// and applied at the PHRASE BOUNDARY. The gate-clamp recursion that CONSUMES the graph is the
// decision manager's job (separate work — MONO_SPECIAL_IN_LOGIC_AUDIT.md); this module only
// PRODUCES the graph.
//
// PANEL (Rodney correction 2026-10-26): "a 16 by 16 panel like change alley (square near full
// module height with controls jacks to the side not underneath). Full Polyphony is 16 voices."
// So the matrix is CA V2's square at CA scale (99.6mm, 6.225mm cells, top at 16.0mm — 40HP
// module), ALWAYS 16x16: rows = voices, columns = parents; rows/cols >= activeCount get dimmed
// by the UI at step 4, the geometry never shrinks. Trig jacks run down the LEFT flank, fire
// buttons + pending lamps down the RIGHT (one row per verb, two groups of three). Art is
// GENERATED: panel_src/gen_containment.py (house pipeline, dark+light); ALL lettering is
// widget-drawn here because nanosvg — the parser Rack uses — silently drops <text>.
//
// ZERO PARAM SLOTS BY DESIGN (DAW_PARAM_AUDIT.md, the ChangeAlleyV2 rule): transforms are
// triggered verbs (buttons/CV), config lives in context menus — nothing here is
// automation-visible. Lands with the step-3/4 UI work.
//
// STEP 0 SCAFFOLD: registration + panel + the pure dsp headers (ContainmentGraph.hpp /
// ContainmentTransforms.hpp). STEP 1: the module now HOLDS the editor-side graph state and
// serializes it (raw uint16 array round-trip via ContainmentGraph::serializeTo/deserializeFrom,
// the WARPDIAG round-trip lesson; reject-invalid-blob falls back to the R2 all-disconnected
// default). Pending/boundary machinery = step 3; pin-matrix UI + store-backed edits = step 4;
// trajectory ring = follow-up. Kept honest: process() is a no-op until the machinery lands —
// no silent fake work.

#include <cstdio>
#include <rack.hpp>
#include <jansson.h>
#include "Monsoon.hpp"                        // extern Plugin* pluginInstance
#include "ui/SvgPanelKit.hpp"                 // Option B-full: loadPanel/kitStep/findNamed
#include "ui/ModuleLookup.hpp"                // THE lookup authority: redDot::lookupMonsoon
#include "ui/RedScrew.hpp"                    // house screws
#include "dsp/ContainmentGraph.hpp"           // the state (pure, rack-free)
#include "dsp/ContainmentTransforms.hpp"      // the DAG-closed verbs (pure, rack-free)

using namespace rack;
// Qualified ids on purpose: Monsoon.hpp exposes MonsoonIds/ChangeAlleyIds with the same
// NUM_PARAMS/... names (the MonsoonChangeAlleyV2.hpp rule).

struct MonsoonContainment : Module {
    enum ParamIds { NUM_PARAMS };
    enum InputIds { NUM_INPUTS };
    enum OutputIds { NUM_OUTPUTS };
    enum LightIds { NUM_LIGHTS };

    // The editor-side containment graph (the engine-side authoritative publish lands at step 4,
    // mirroring the CA seam: PatternEngine owns caRhythmSrc, the CA module edits and drives it).
    // Until then this member is the ONLY state, and it round-trips through the Rack save.
    dotModular::ct::ContainmentGraph graph;

    MonsoonContainment() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    }

    void process(const ProcessArgs& args) override {
        // Step 3 lands the pending machinery here: armed verbs (CAPendingOrder reuse, stamped
        // chronologically) applied at the Rate::Phrase tier fed from Monsoon::process (the CA
        // pattern — the module never owns a clock; the R7 grep-gate discipline). Until then:
        // nothing. The graph is pure state; silence is the correct scaffold behaviour.
    }

    // ── serialization (step 1): the raw uint16 array IS the state ─────────────
    // THE WARPDIAG LESSON: no bit-packing, no derived fields, no cleverness — 16 plain words
    // in, the same 16 words out. The consumer side re-validates and re-derives nothing.
    json_t* dataToJson() override {
        json_t* rootJ = json_object();
        json_t* arrJ = json_array();
        uint16_t raw[dotModular::ct::VOICES];
        graph.serializeTo(raw);
        for (int v = 0; v < dotModular::ct::VOICES; ++v)
            json_array_append_new(arrJ, json_integer(raw[v]));
        json_object_set_new(rootJ, "containmentParents", arrJ);
        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override {
        // Reject-invalid-blob policy: ANY malformation — missing/short/long array, non-integer
        // or out-of-range element, or a blob that fails re-validation (acyclic + no self-bits) —
        // falls back to the R2 all-disconnected default. A corrupt or hand-edited save never
        // promotes garbage into the editor state (the WARPDIAG round-trip lesson). A wholly
        // ABSENT key keeps whatever the module already holds (partial-patch load semantics).
        auto setDefault = [this]() { graph = dotModular::ct::ContainmentGraph{}; };
        json_t* arrJ = json_object_get(rootJ, "containmentParents");
        if (!arrJ || !json_is_array(arrJ)) return;
        if (json_array_size(arrJ) != (size_t)dotModular::ct::VOICES) { setDefault(); return; }
        uint16_t raw[dotModular::ct::VOICES];
        for (int v = 0; v < dotModular::ct::VOICES; ++v) {
            json_t* eJ = json_array_get(arrJ, v);
            if (!json_is_integer(eJ)) { setDefault(); return; }
            int x = json_integer_value(eJ);
            if (x < 0 || x > 0xFFFF) { setDefault(); return; }
            raw[v] = (uint16_t)x;
        }
        if (!graph.deserializeFrom(raw)) setDefault();
    }
};

// ── Panel geometry in mm — MUST MATCH panel_src/gen_containment.py ──────────────
// (the gen_change_alley_v2.py GRID_Y rule: one authority per number, cross-declared)
namespace ContainmentPanelMm {
    constexpr float PW = 203.2f;                   // 40HP
    constexpr float GRID_X = 51.8f, GRID_Y = 16.0f, GRID_W = 99.6f;
    constexpr float CELL = GRID_W / 16.0f;         // 6.225
    constexpr float J_X = 18.75f, L_X = 27.75f;    // trig-jack col (left); lamp col (right)
    constexpr float ROW_Y[6] = {31.0f, 45.0f, 59.0f, 78.0f, 92.0f, 106.0f};
    constexpr const char* VERB_ID[6]    = {"rotate", "reflect", "invert", "collapse", "collapseall", "expand"};
    constexpr const char* VERB_LABEL[6] = {"ROTATE", "REFLECT", "INVERT", "COLLAPSE", "COLLAPSE-ALL", "EXPAND"};
    constexpr float LABEL_X = 47.4f, NUM_X = 50.4f;
    constexpr float TITLE_Y = 9.8f, SIDE_Y = 14.8f, CAP1_Y = 121.0f, CAP2_Y = 125.2f;
} // namespace ContainmentPanelMm

struct MonsoonContainmentWidget : ModuleWidget,
    dotModular::Compose<MonsoonContainmentWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;

    MonsoonContainmentWidget(MonsoonContainment* module) {
        setModule(module);
        // Dual-panel house convention (theme follows the host Monsoon, the junction idiom);
        // loadPanel = kit: sets the panel AND caches the #components anchors for bind-by-name
        // at step 3/4 (input_trig_* / param_btn_* / light_pending_* from gen_containment.py).
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Containment_panel_dark.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/Containment_panel_light.svg"));
        loadPanel(asset::plugin(pluginInstance, "res/panels/Containment_panel_dark.svg"));
        redDot::addRedScrews(this);
    }

    void step() override {
        ModuleWidget::step();
        kitStep();          // kit: dev live-reload poll (Option B-full)
        if (!module) return;
        Monsoon* m = redDot::lookupMonsoon(module);
        const int wantLight = (m && m->lightTheme) ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }
    }

    // Lettering is widget-drawn at the generator's mm coordinates (nanosvg drops <text>):
    // title, side captions, group captions, verb labels, and the 1..16 vertex numbers on both
    // matrix axes. ASCII only — uiFont glyph coverage is not guaranteed beyond that.
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font) return;
        NVGcontext* vg = args.vg;
        nvgFontFaceId(vg, font->handle);
        using namespace ContainmentPanelMm;
        const bool lit = (lastThemeLight > 0);
        const NVGcolor ink = lit ? nvgRGB(0x2a, 0x2a, 0x2e) : nvgRGB(0xe8, 0xe2, 0xd0);
        const NVGcolor dim = lit ? nvgRGB(0x88, 0x8d, 0x96) : nvgRGB(0x8a, 0x85, 0x78);

        // Title — centred, CA's widget-drawn-title idiom.
        nvgFillColor(vg, ink);
        nvgFontSize(vg, mm2px(3.6f));
        nvgTextLetterSpacing(vg, mm2px(0.4f));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, box.size.x * 0.5f, mm2px(TITLE_Y), "CONTAINMENT", NULL);
        nvgTextLetterSpacing(vg, 0.f);

        // Side captions over the control columns; group captions over each verb trio.
        nvgFillColor(vg, dim);
        nvgFontSize(vg, mm2px(2.0f));
        nvgText(vg, mm2px(J_X), mm2px(SIDE_Y), "TRIG", NULL);
        nvgText(vg, mm2px(PW - J_X), mm2px(SIDE_Y), "FIRE", NULL);
        nvgText(vg, mm2px(J_X), mm2px(ROW_Y[0] - 7.5f), "ROLES", NULL);
        nvgText(vg, mm2px(J_X), mm2px(ROW_Y[3] - 7.5f), "FOLD", NULL);

        // Verb labels — right-aligned inboard of the voice-number column.
        nvgFillColor(vg, ink);
        nvgFontSize(vg, mm2px(2.8f));
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        for (int k = 0; k < 6; ++k)
            nvgText(vg, mm2px(LABEL_X), mm2px(ROW_Y[k]), VERB_LABEL[k], NULL);

        // Vertex numbers: voices 1..16 down the left edge, parents 1..16 across the top edge.
        nvgFillColor(vg, dim);
        nvgFontSize(vg, mm2px(2.0f));
        for (int i = 0; i < 16; ++i) {
            char b[4];
            snprintf(b, sizeof(b), "%d", i + 1);
            nvgText(vg, mm2px(NUM_X), mm2px(GRID_Y + (i + 0.5f) * CELL), b, NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, mm2px(GRID_X + (i + 0.5f) * CELL), mm2px(SIDE_Y), b, NULL);
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        }

        // Bottom captions (ASCII separators only).
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, box.size.x * 0.5f, mm2px(CAP1_Y),
                "ROWS = VOICE    COLUMNS = PARENT (CONTAINED-BY)", NULL);
        nvgText(vg, box.size.x * 0.5f, mm2px(CAP2_Y),
                "MULTI-PARENT ACYCLIC DAG    INTERSECTION OF CONTAINMENTS", NULL);
    }
};
