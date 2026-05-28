#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "managers/MonoSandsParameterManager.hpp"

using namespace rack;
using namespace redDot;

extern Model* modelMonsoon;

// ── Param layout ──────────────────────────────────────────────────────────────
namespace SandsVisualIds {
    enum ParamId {
        SPREAD_REST = 0,
        SPREAD_MELODY,
        SPREAD_OCTAVE,
        SPREAD_LEGATO,
        SPREAD_ACCENT,
        SPREAD_VARIATION,
        // Toggle: 0 = avg-to-flat (toward 0.5), 1 = avg-to-poly
        // For mono, only flat makes sense for now
        INTERP_TARGET,
        NUM_PARAMS
    };
    enum InputId  { NUM_INPUTS  = 0 };
    enum OutputId { NUM_OUTPUTS = 0 };
    enum LightId  { NUM_LIGHTS  = 0 };
}

// ── Module ────────────────────────────────────────────────────────────────────
struct MonsoonSandsVisualExpander : Module {
    MonsoonSandsVisualExpander() {
        using namespace SandsVisualIds;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SPREAD_REST,      0.f, 1.f, 0.f, "Spread REST");
        configParam(SPREAD_MELODY,    0.f, 1.f, 0.f, "Spread MELODY");
        configParam(SPREAD_OCTAVE,    0.f, 1.f, 0.f, "Spread OCTAVE");
        configParam(SPREAD_LEGATO,    0.f, 1.f, 0.f, "Spread LEGATO");
        configParam(SPREAD_ACCENT,    0.f, 1.f, 0.f, "Spread ACCENT");
        configParam(SPREAD_VARIATION, 0.f, 1.f, 0.f, "Spread VARIATION");
        configSwitch(INTERP_TARGET, 0.f, 1.f, 0.f, "Interp Target",
                     {"Flat (0.5)", "Poly Avg"});
    }
    void process(const ProcessArgs&) override {}
};

// ── Widget ────────────────────────────────────────────────────────────────────
struct MonsoonSandsVisualExpanderWidget : ModuleWidget {
    SandsVisualEditorV4*       visualEditor = nullptr;
    MonoSandsParameterManager* paramMgr = nullptr;
    PatternEngine* lastPe = nullptr;

    explicit MonsoonSandsVisualExpanderWidget(MonsoonSandsVisualExpander* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/SandsMonoVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Visual Editor: 6 lanes, MONO mode ────────────────────────────────
        // Panel zone: X=4mm, Y=20mm, W=114mm, H=135mm
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::MONO);
        visualEditor->box.pos  = mm2px(Vec(4.f, 20.f));
        visualEditor->box.size = mm2px(Vec(114.f, 135.f));
        addChild(visualEditor);

        // ── Spread Sliders ────────────────────────────────────────────────────
        // Row 1 Y=167mm: REST | MELODY | OCTAVE
        // Row 2 Y=187mm: LEGATO | ACCENT | VARIATION
        using P = SandsVisualIds;
        const float c[3] = { 20.f, 60.f, 100.f };
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(c[0], 167.f)), mod, P::SPREAD_REST));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(c[1], 167.f)), mod, P::SPREAD_MELODY));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(c[2], 167.f)), mod, P::SPREAD_OCTAVE));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(c[0], 187.f)), mod, P::SPREAD_LEGATO));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(c[1], 187.f)), mod, P::SPREAD_ACCENT));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(c[2], 187.f)), mod, P::SPREAD_VARIATION));

        // ── Interp target toggle ──────────────────────────────────────────────
        addParam(createParamCentered<CKSS>(mm2px(Vec(60.f, 207.f)), mod, P::INTERP_TARGET));

        paramMgr = new MonoSandsParameterManager();
    }

    ~MonsoonSandsVisualExpanderWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        if (!module) return nullptr;
        return dynamic_cast<Monsoon*>(module->rightExpander.module);
    }

    // Spread: interpolate each probability toward target value
    // Mono target: 0.5 (flat) or poly-average (future)
    float applySpread(float prob, float spread, float target = 0.5f) const {
        return prob + (target - prob) * clamp(spread, 0.f, 1.f);
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        PatternEngine* pe = &monsoon->engine.pe;

        // Wire engine pointer if not yet done
        if (paramMgr->patternEngine != pe) {
            paramMgr->patternEngine = pe;
        }

        // 1. Pull raw probabilities from PatternEngine into editor
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // 2. Apply spread inline over the editor state for display
        auto* mod = static_cast<MonsoonSandsVisualExpander*>(module);
        using P = SandsVisualIds;
        const float spreads[6] = {
            mod->params[P::SPREAD_REST     ].getValue(),
            mod->params[P::SPREAD_MELODY   ].getValue(),
            mod->params[P::SPREAD_OCTAVE   ].getValue(),
            mod->params[P::SPREAD_LEGATO   ].getValue(),
            mod->params[P::SPREAD_ACCENT   ].getValue(),
            mod->params[P::SPREAD_VARIATION].getValue(),
        };

        const int laneMap[6] = {
            SandsVisualEditorV4::REST,
            SandsVisualEditorV4::MELODY,
            SandsVisualEditorV4::OCTAVE,
            SandsVisualEditorV4::LEGATO,
            SandsVisualEditorV4::ACCENT,
            SandsVisualEditorV4::VARIATION,
        };

        for (int l = 0; l < 6; ++l) {
            if (spreads[l] <= 0.f) continue;
            auto& lane = visualEditor->currentState.lanes[laneMap[l]];
            for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s) {
                lane.probabilities[s] =
                    applySpread(lane.probabilities[s], spreads[l]);
            }
        }
    }
};

Model* modelMonsoonSandsVisualExpander =
    createModel<MonsoonSandsVisualExpander, MonsoonSandsVisualExpanderWidget>(
        "MonsoonSandsVisualExpander");
