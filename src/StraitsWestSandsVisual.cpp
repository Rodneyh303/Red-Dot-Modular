#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonDeepStraitsSands.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/PolyVoiceSandsParameterManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;

extern Model* modelMonsoon;
extern Model* modelMonsoonDeepStraitsSandsWest;

namespace StraitsWestVisualIds {
    enum ParamId {
        SPREAD_V0_R = 0, SPREAD_V0_M, SPREAD_V0_O,
        SPREAD_V1_R,     SPREAD_V1_M, SPREAD_V1_O,
        SPREAD_V2_R,     SPREAD_V2_M, SPREAD_V2_O,
        SPREAD_V3_R,     SPREAD_V3_M, SPREAD_V3_O,
        SPREAD_V4_R,     SPREAD_V4_M, SPREAD_V4_O,
        SPREAD_V5_R,     SPREAD_V5_M, SPREAD_V5_O,
        SPREAD_V6_R,     SPREAD_V6_M, SPREAD_V6_O,
        SPREAD_V7_R,     SPREAD_V7_M, SPREAD_V7_O,
        INTERP_TARGET,
        NUM_PARAMS
    };
}

struct StraitsWestSandsVisual : Module {
    StraitsWestSandsVisual() {
        using namespace StraitsWestVisualIds;
        config(NUM_PARAMS, 0, 0, 0);
        for (int v = 0; v < 8; ++v) {
            const char* ln[3] = {"REST","MELODY","OCTAVE"};
            for (int l = 0; l < 3; ++l)
                configParam(SPREAD_V0_R + v*3 + l, 0.f, 1.f, 0.f,
                    string::f("Voice %d Spread %s", v+9, ln[l]));
        }
        configSwitch(INTERP_TARGET, 0.f, 1.f, 0.f, "Interp Target",
                     {"Avg Active Poly", "Mono Draw"});
    }
    void process(const ProcessArgs&) override {}
};

struct StraitsWestSandsVisualWidget : ModuleWidget {
    SandsVisualEditorV4* visualEditor = nullptr;
    TabButtonGroup*      tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr = nullptr;
    int selectedVoice = 0;

    explicit StraitsWestSandsVisualWidget(StraitsWestSandsVisual* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/StraitsWestSandsVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // 8 tabs, V9–V16, narrower (12mm) to fit in 24HP
        tabGroup = new TabButtonGroup(8, 9, mm2px(12.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos = mm2px(Vec(4.f, 22.f));
        addChild(tabGroup);

        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(4.f, 32.f));
        visualEditor->box.size = mm2px(Vec(114.f, 120.f));
        addChild(visualEditor);

        using P = StraitsWestVisualIds;
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(20.f,  168.f)), mod, P::SPREAD_V0_R));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(60.f,  168.f)), mod, P::SPREAD_V0_M));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(100.f, 168.f)), mod, P::SPREAD_V0_O));
        addParam(createParamCentered<CKSS>(mm2px(Vec(56.f, 199.f)), mod, P::INTERP_TARGET));

        // West voices 9-16 map to polyRandom indices 7-14
        // (voice 2=0, 3=1, ... 9=7, ... 16=14)
        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 8, 7);
    }

    ~StraitsWestSandsVisualWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    rack::Module* getDeepSandsWest() {
        if (!module) return nullptr;
        Module* curr = module->rightExpander.module;
        for (int d = 0; curr && d < 8; ++d) {
            if (curr->model == modelMonsoonDeepStraitsSandsWest) return curr;
            curr = curr->rightExpander.module;
        }
        return nullptr;
    }

    void onVoiceTabChanged(int newVoice) {
        if (!paramMgr || !visualEditor) return;
        paramMgr->syncEditorToPatternEngine(selectedVoice, visualEditor->currentState);
        selectedVoice = newVoice;
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
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

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        auto* mod = static_cast<StraitsWestSandsVisual*>(module);
        using P = StraitsWestVisualIds;
        for (int v = 0; v < 8; ++v)
            for (int l = 0; l < 3; ++l)
                paramMgr->setSpread(v, l, mod->params[P::SPREAD_V0_R + v*3 + l].getValue());

        bool useMono = mod->params[P::INTERP_TARGET].getValue() > 0.5f;
        paramMgr->setInterpolationTarget(
            useMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        // Per-lane playhead: West voices in DeepSandsWest start at voice index 7
        int globalStep   = monsoon->engine.stepIndex;
        rack::Module* dp = getDeepSandsWest();
        setPolyVoicePlayheads(visualEditor, globalStep, dp, selectedVoice + 7);
    }
};

Model* modelStraitsWestSandsVisual =
    createModel<StraitsWestSandsVisual, StraitsWestSandsVisualWidget>(
        "StraitsWestSandsVisual");
