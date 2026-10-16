#pragma once
// ── DmcurveDevHost — DEV-ONLY standalone host for DistributionCurveWidget (Phase 2) ───────────
// Scaffolding so the .dmcurve curve widget can be dragged and viewed in Rack IN ISOLATION
// before Phase 3 integration (docs/design/DMCURVE_EDITOR.md Phase 2). It exercises BOTH
// widget modes:
//   • the universal draw (Kumaraswamy / arcsine / triangular render through the same
//     const Distribution& interface as everything else — display-only), and
//   • the drawn edit (a PchipDistribution whose MonotoneCurve is supplied as the widget's
//     editCurve → drag / click-add / double-click-remove all live).
// NO engine wiring, no audio, no lane integration, no live preview — render + interact only.
//
// LAYOUT: the panel is deliberately WIDE (30 HP) so the square diagram is sized by the
// panel HEIGHT (~most of it) with a control sidebar beside it — the diagram dominates, the
// test controls (distribution selector knob + name label, edit-handles toggle, curve reset)
// sit in a vertical strip to the right. The widget itself stays square/dual-axis.
//
// STATE: the distribution PARAM is the single source of truth for `which` (the sidebar knob
// and the context menu both write it; the widget derives `which` from it each frame; Rack
// auto-persists params). The edit-handles flag and the drawn curve persist via dataToJson
// (the tested .dmcurve JSON path), so an editing session survives save/load while iterating
// on feel.

#include <rack.hpp>

#include "dsp/Distribution.hpp"
#include "dsp/MonotoneCurve.hpp"
#include "ui/SvgPanelKit.hpp"

namespace dotModular { struct DistributionCurveWidget; }

struct DmcurveDevHost : rack::engine::Module {
    enum ParamIds { DISTRIBUTION_PARAM, N_PARAMS };
    enum Which { KUMARASWAMY = 0, ARCSINE, TRIANGULAR, DRAWN, N_WHICH };

    dotModular::PchipDistribution       pchip;      // THE editable curve (the drawn case)
    dotModular::KumaraswamyDistribution kum{2.0, 3.0};
    dotModular::ArcsineDistribution     arcsine;
    dotModular::TriangularDistribution  tri{0.3};
    int  which       = DRAWN;   // derived from DISTRIBUTION_PARAM by the widget each frame
    bool editEnabled = true;    // EDIT HANDLES toggle (only meaningful for the drawn case)

    DmcurveDevHost() {
        config(N_PARAMS, 0, 0, 0);
        configParam(DISTRIBUTION_PARAM, 0.f, (float)(N_WHICH - 1), (float)DRAWN, "Distribution");
        starterS();
    }

    // A starter S so the editable case is immediately meaningful to drag.
    void starterS() {
        pchip.curve.reset();
        pchip.curve.addPoint(0.25);
        pchip.curve.addPoint(0.5);
        pchip.curve.addPoint(0.75);
        pchip.curve.setPoint(1, 0.25, 0.1);
        pchip.curve.setPoint(3, 0.75, 0.9);
    }

    dotModular::Distribution* current() {
        switch (which) {
            case KUMARASWAMY: return &kum;
            case ARCSINE:     return &arcsine;
            case TRIANGULAR:  return &tri;
            default:          return &pchip;
        }
    }

    void process(const rack::engine::Module::ProcessArgs&) override {}

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "edit", json_boolean(editEnabled));
        json_object_set_new(root, "curve", pchip.curve.toJson());   // the full .dmcurve object
        return root;
    }
    void dataFromJson(json_t* root) override {
        if (json_t* j = json_object_get(root, "edit")) editEnabled = json_boolean_value(j);
        if (json_t* jc = json_object_get(root, "curve"))
            pchip.curve.fromJson(jc);   // MonotoneCurve's own validation + sanitize
    }
};

struct DmcurveDevHostWidget : rack::app::ModuleWidget,
    dotModular::Compose<DmcurveDevHostWidget, dotModular::ShapeQuery, dotModular::Bind> {

    dotModular::DistributionCurveWidget* curveWidget = nullptr;
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;   // -1 = unset, forces the first apply

    explicit DmcurveDevHostWidget(DmcurveDevHost* module);
    void step() override;
    void appendContextMenu(rack::ui::Menu* menu) override;
};

extern rack::Model* modelDmcurveDevHost;
