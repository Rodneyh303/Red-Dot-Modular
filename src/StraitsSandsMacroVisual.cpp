#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitsSands.hpp"
#include "StraitsSandsMacroVisual.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/PolySandsParameterManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsMacroVisualIds;

extern Model* modelMonsoon;
extern Model* modelMonsoonStraitsSands;

struct StraitsSandsMacroVisualWidget : ModuleWidget {
    SandsVisualEditorV4*       visualEditor = nullptr;
    PolySandsParameterManager* paramMgr     = nullptr;
    bool                       initialized  = false;

    explicit StraitsSandsMacroVisualWidget(StraitsSandsMacroVisual* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/StraitsSandsMacroVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Handles are the L/O/R controls — no separate L/O/R knobs
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(4.f, 20.f));
        visualEditor->box.size = mm2px(Vec(114.f, 185.f));
        addChild(visualEditor);

        addParam(createParamCentered<VCVSlider>(mm2px(Vec(20.f,  218.f)), mod, SPREAD_REST));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(60.f,  218.f)), mod, SPREAD_MELODY));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(100.f, 218.f)), mod, SPREAD_OCTAVE));
        addParam(createParamCentered<CKSS>(mm2px(Vec(56.f, 246.f)), mod, INTERP_TARGET));

        paramMgr = new PolySandsParameterManager(nullptr, nullptr, nullptr, 7);
    }

    ~StraitsSandsMacroVisualWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    void saveLOR() {
        if (!module) return;
        for (int l = 0; l < 3; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            module->params[lorId(l, 0)].setValue((float)lane.length);
            module->params[lorId(l, 1)].setValue((float)lane.offset);
            module->params[lorId(l, 2)].setValue((float)lane.rotation);
        }
    }
    void loadLOR() {
        if (!module) return;
        for (int l = 0; l < 3; ++l) {
            auto& lane = visualEditor->currentState.lanes[l];
            lane.length   = std::max(1, (int)std::round(module->params[lorId(l, 0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorId(l, 1)].getValue());
            lane.rotation = (int)std::round(module->params[lorId(l, 2)].getValue());
        }
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        PatternEngine*   pe = &monsoon->engine.pe;
        SequencerEngine* se = &monsoon->engine;
        if (paramMgr->patternEngine != pe) {
            paramMgr->patternEngine   = pe;
            paramMgr->sequencerEngine = se;
            paramMgr->spreadMgr.patternEngine   = pe;
            paramMgr->spreadMgr.sequencerEngine = se;
        }

        if (!initialized) { loadLOR(); initialized = true; }

        auto* mod = static_cast<StraitsSandsMacroVisual*>(module);
        paramMgr->spreadMgr.setSpread(0, mod->params[SPREAD_REST  ].getValue());
        paramMgr->spreadMgr.setSpread(1, mod->params[SPREAD_MELODY].getValue());
        paramMgr->spreadMgr.setSpread(2, mod->params[SPREAD_OCTAVE].getValue());

        bool useMono = mod->params[INTERP_TARGET].getValue() > 0.5f;
        paramMgr->spreadMgr.setInterpolationTarget(
            useMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // Capture handle drags → params
        saveLOR();

        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // Per-lane playhead using our own global L/O/R params
        int gs = monsoon->engine.stepIndex;
        for (int l = 0; l < 3; ++l) {
            visualEditor->setLanePlayStep(l,
                calcPlayhead(gs,
                    readLenParam(mod, lorId(l, 0)),
                    readOffRotParam(mod, lorId(l, 1)),
                    readOffRotParam(mod, lorId(l, 2))));
        }
    }
};

Model* modelStraitsSandsMacroVisual =
    createModel<StraitsSandsMacroVisual, StraitsSandsMacroVisualWidget>(
        "StraitsSandsMacroVisual");
