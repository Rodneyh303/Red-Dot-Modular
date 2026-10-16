#pragma once
// ── DmcurveDevHost — DEV-ONLY standalone host for DistributionCurveWidget (Phase 2 / 2.5) ──────
// Scaffolding so the .dmcurve curve widget can be dragged and viewed in Rack IN ISOLATION
// before Phase 3 integration (docs/design/DMCURVE_EDITOR.md Phase 2 + "Phase 2.5
// additions"). It exercises the full widget surface:
//   • the universal draw — ALL seven built-ins render through the same const Distribution&
//     interface (kumaraswamy, arcsine, triangular, power, logit-normal, truncated-normal,
//     and the drawn PchipDistribution),
//   • parametric param editing — two GENERIC param knobs that remap + relabel per
//     distribution (knob1 = a/mu/mode/k, knob2 = b/sigma; the label shows the live meaning
//     + value; arcsine and the drawn case leave them inert), reshaping the curve LIVE, and
//   • the drawn edit — a PchipDistribution whose MonotoneCurve is the widget's editCurve
//     (drag / click-add / double-click-remove), plus CONVERT → EDITABLE: any parametric is
//     one click from MonotoneCurve::fromDistribution (the start-from-a-shape workflow).
// NO engine wiring, no audio, no lane integration, no live preview — render + interact only.
//
// LAYOUT: the panel is deliberately WIDE (30 HP) so the square diagram is sized by the
// panel HEIGHT (~most of it) with a control sidebar beside it.
//
// STATE: the distribution PARAM is the single source of truth for `which` (the sidebar knob
// and the context menu both write it; the widget derives `which` from it each frame; Rack
// auto-persists params). The two param knobs are NORMALIZED [0,1] — the per-distribution
// ranges live in paramSlot(), so one knob covers a/mu/mode/k and the other b/sigma; on a
// distribution switch the knobs back-fill from the new distribution's current values. The
// edit-handles flag and the drawn curve persist via dataToJson (the tested .dmcurve JSON
// path).

#include <rack.hpp>

#include "dsp/Distribution.hpp"
#include "dsp/MonotoneCurve.hpp"
#include "ui/SvgPanelKit.hpp"

namespace dotModular { struct DistributionCurveWidget; }

struct DmcurveDevHost : rack::engine::Module {
    enum ParamIds { DISTRIBUTION_PARAM, PARAM1_PARAM, PARAM2_PARAM, N_PARAMS };
    enum Which {
        KUMARASWAMY = 0, ARCSINE, TRIANGULAR, POWER,
        LOGIT_NORMAL, TRUNCATED_NORMAL, DRAWN, N_WHICH,
    };

    dotModular::PchipDistribution           pchip;      // THE editable curve (the drawn case)
    dotModular::KumaraswamyDistribution     kum{2.0, 3.0};
    dotModular::ArcsineDistribution         arcsine;
    dotModular::TriangularDistribution      tri{0.3};
    dotModular::PowerDistribution           pw{2.0};
    dotModular::LogitNormalDistribution     ln{0.0, 1.0};
    dotModular::TruncatedNormalDistribution tn{0.5, 0.2};
    int  which       = DRAWN;   // derived from DISTRIBUTION_PARAM by the widget each frame
    bool editEnabled = true;    // EDIT HANDLES toggle (only meaningful for the drawn case)

    DmcurveDevHost() {
        config(N_PARAMS, 0, 0, 0);
        configParam(DISTRIBUTION_PARAM, 0.f, (float)(N_WHICH - 1), (float)DRAWN, "Distribution");
        configParam(PARAM1_PARAM, 0.f, 1.f, 0.25f, "Param 1 (normalized)");
        configParam(PARAM2_PARAM, 0.f, 1.f, 0.35f, "Param 2 (normalized)");
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
            case KUMARASWAMY:      return &kum;
            case ARCSINE:          return &arcsine;
            case TRIANGULAR:       return &tri;
            case POWER:            return &pw;
            case LOGIT_NORMAL:     return &ln;
            case TRUNCATED_NORMAL: return &tn;
            default:               return &pchip;
        }
    }

    // ── the generic param knobs' per-distribution mapping ───────────────────────────────
    // Knobs are normalized [0,1]; this is the per-distribution meaning + value range.
    // knob1 = a/mu/mode/k; knob2 = b/sigma. used=false → inert (arcsine, the single-param
    // distributions' second slot, and the drawn case — the curve is edited by dragging).
    struct ParamSlot {
        const char* sym;
        double lo, hi;
        bool used;
    };
    static ParamSlot paramSlot(int which, int slot) {
        const bool first = (slot == 0);
        switch (which) {
            case KUMARASWAMY:      return first ? ParamSlot{"a",    0.2, 8.0, true}
                                                : ParamSlot{"b",    0.2, 8.0, true};
            case TRIANGULAR:       return first ? ParamSlot{"mode", 0.0, 1.0, true}
                                                : ParamSlot{"-",    0.0, 1.0, false};
            case POWER:            return first ? ParamSlot{"k",    0.2, 8.0, true}
                                                : ParamSlot{"-",    0.0, 1.0, false};
            case LOGIT_NORMAL:     return first ? ParamSlot{"mu",  -3.0, 3.0, true}
                                                : ParamSlot{"sigma", 0.1, 3.0, true};
            case TRUNCATED_NORMAL: return first ? ParamSlot{"mu",  -3.0, 3.0, true}
                                                : ParamSlot{"sigma", 0.1, 3.0, true};
            default:               return ParamSlot{"-", 0.0, 1.0, false};   // arcsine / drawn
        }
    }
    double paramValue(int slot) const {
        switch (which) {
            case KUMARASWAMY:      return slot == 0 ? kum.a : kum.b;
            case TRIANGULAR:       return slot == 0 ? tri.mode : 0.0;
            case POWER:            return slot == 0 ? pw.k : 0.0;
            case LOGIT_NORMAL:     return slot == 0 ? ln.mu : ln.sigma;
            case TRUNCATED_NORMAL: return slot == 0 ? tn.mu : tn.sigma;
            default:               return 0.0;
        }
    }
    void setParamValue(int slot, double v) {
        switch (which) {
            case KUMARASWAMY:      if (slot == 0) kum.a = v; else kum.b = v; break;
            case TRIANGULAR:       if (slot == 0) tri.mode = v; break;
            case POWER:            if (slot == 0) pw.k = v; break;
            case LOGIT_NORMAL:     if (slot == 0) ln.mu = v; else ln.sigma = v; break;
            case TRUNCATED_NORMAL: if (slot == 0) tn.mu = v; else tn.sigma = v; break;
            default: break;
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
    int lastWhich = -1;        // back-fill the param knobs when the distribution changes

    explicit DmcurveDevHostWidget(DmcurveDevHost* module);
    void step() override;
    void appendContextMenu(rack::ui::Menu* menu) override;
};

extern rack::Model* modelDmcurveDevHost;
