#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "managers/PolySandsParameterManager.hpp"

using namespace rack;
using namespace redDot;

extern Model* modelMonsoon;

// ── Param layout ──────────────────────────────────────────────────────────────
// 3 global spread params (apply to all voices) + 1 target toggle
namespace StraitsMacroVisualIds {
    enum ParamId {
        SPREAD_REST = 0,
        SPREAD_MELODY,
        SPREAD_OCTAVE,
        INTERP_TARGET,
        NUM_PARAMS
    };
    enum InputId  { NUM_INPUTS  = 0 };
    enum OutputId { NUM_OUTPUTS = 0 };
    enum LightId  { NUM_LIGHTS  = 0 };
}

// ── Module ────────────────────────────────────────────────────────────────────
struct StraitsSandsMacroVisual : Module {
    StraitsSandsMacroVisual() {
        using namespace StraitsMacroVisualIds;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SPREAD_REST,    0.f, 1.f, 0.f, "Global Spread REST (all voices)");
        configParam(SPREAD_MELODY,  0.f, 1.f, 0.f, "Global Spread MELODY (all voices)");
        configParam(SPREAD_OCTAVE,  0.f, 1.f, 0.f, "Global Spread OCTAVE (all voices)");
        configSwitch(INTERP_TARGET, 0.f, 1.f, 0.f, "Interp Target",
                     {"Avg Active Poly", "Mono Draw"});
    }
    void process(const ProcessArgs&) override {}
};

// ── Widget ────────────────────────────────────────────────────────────────────
struct StraitsSandsMacroVisualWidget : ModuleWidget {
    SandsVisualEditorV4* visualEditor = nullptr;
    PolySandsParameterManager* paramMgr = nullptr;

    explicit StraitsSandsMacroVisualWidget(StraitsSandsMacroVisual* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/StraitsSandsMacroVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Visual editor: 3 lanes, POLY mode (representative voice 2) ──────
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(4.f, 26.f));
        visualEditor->box.size = mm2px(Vec(114.f, 130.f));
        addChild(visualEditor);

        // ── 3 global spread sliders ───────────────────────────────────────────
        using P = StraitsMacroVisualIds;
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(20.f,  172.f)), mod, P::SPREAD_REST));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(60.f,  172.f)), mod, P::SPREAD_MELODY));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(100.f, 172.f)), mod, P::SPREAD_OCTAVE));

        // ── Interp target toggle ──────────────────────────────────────────────
        addParam(createParamCentered<CKSS>(mm2px(Vec(56.f, 199.f)), mod, P::INTERP_TARGET));

        paramMgr = new PolySandsParameterManager(nullptr, nullptr, nullptr, 7);
    }

    ~StraitsSandsMacroVisualWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        if (!module) return nullptr;
        return dynamic_cast<Monsoon*>(module->rightExpander.module);
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        PatternEngine*   pe = &monsoon->engine.pe;
        SequencerEngine* se = &monsoon->engine;

        // Wire engines lazily
        if (paramMgr->patternEngine != pe) {
            paramMgr->patternEngine   = pe;
            paramMgr->sequencerEngine = se;
            paramMgr->spreadMgr.patternEngine   = pe;
            paramMgr->spreadMgr.sequencerEngine = se;
        }

        // Read global spread knobs and push to MacroSpreadManager
        auto* mod = static_cast<StraitsSandsMacroVisual*>(module);
        using P = StraitsMacroVisualIds;
        paramMgr->spreadMgr.setSpread(0, mod->params[P::SPREAD_REST   ].getValue());
        paramMgr->spreadMgr.setSpread(1, mod->params[P::SPREAD_MELODY ].getValue());
        paramMgr->spreadMgr.setSpread(2, mod->params[P::SPREAD_OCTAVE ].getValue());

        // Interp target
        bool useMono = mod->params[P::INTERP_TARGET].getValue() > 0.5f;
        paramMgr->spreadMgr.setInterpolationTarget(
            useMono ? SpreadManager::MONO_DRAW
                    : SpreadManager::AVERAGE_POLY);

        // Sync representative voice → visual editor
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);
    }
};

Model* modelStraitsSandsMacroVisual =
    createModel<StraitsSandsMacroVisual, StraitsSandsMacroVisualWidget>(
        "StraitsSandsMacroVisual");
