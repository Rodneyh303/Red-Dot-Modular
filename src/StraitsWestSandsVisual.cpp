#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonDeepStraitsSands.hpp"
#include "StraitsWestSandsVisual.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/PolyVoiceSandsParameterManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsWestVisualIds;

extern Model* modelMonsoon;
extern Model* modelMonsoonDeepStraitsSandsWest;

struct StraitsWestSandsVisualWidget : ModuleWidget {
    SandsVisualEditorV4*            visualEditor = nullptr;
    TabButtonGroup*                 tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr     = nullptr;
    int  selectedVoice = 0;
    bool initialized   = false;

    explicit StraitsWestSandsVisualWidget(StraitsWestSandsVisual* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/StraitsWestSandsVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        tabGroup = new TabButtonGroup(8, 9, mm2px(12.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos = mm2px(Vec(4.f, 22.f));
        addChild(tabGroup);

        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(4.f, 32.f));
        visualEditor->box.size = mm2px(Vec(114.f, 175.f));
        addChild(visualEditor);

        addParam(createParamCentered<VCVSlider>(mm2px(Vec(20.f,  218.f)), mod, SPREAD_V0_R));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(60.f,  218.f)), mod, SPREAD_V0_M));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(100.f, 218.f)), mod, SPREAD_V0_O));
        addParam(createParamCentered<CKSS>(mm2px(Vec(56.f, 246.f)), mod, INTERP_TARGET));

        // West voices 9-16 → local indices 0-7 → global indices 7-14
        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 8, 7);
    }

    ~StraitsWestSandsVisualWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    void saveVoiceLOR(int localV) {
        if (!module) return;
        for (int l = 0; l < 3; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            module->params[lorId(localV, l, 0)].setValue((float)lane.length);
            module->params[lorId(localV, l, 1)].setValue((float)lane.offset);
            module->params[lorId(localV, l, 2)].setValue((float)lane.rotation);
        }
    }
    void loadVoiceLOR(int localV) {
        if (!module) return;
        for (int l = 0; l < 3; ++l) {
            auto& lane = visualEditor->currentState.lanes[l];
            lane.length   = std::max(1, (int)std::round(module->params[lorId(localV, l, 0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorId(localV, l, 1)].getValue());
            lane.rotation = (int)std::round(module->params[lorId(localV, l, 2)].getValue());
        }
    }

    void onVoiceTabChanged(int newVoice) {
        if (!paramMgr || !visualEditor) return;
        paramMgr->syncEditorToPatternEngine(selectedVoice, visualEditor->currentState);
        saveVoiceLOR(selectedVoice);
        selectedVoice = newVoice;
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
        loadVoiceLOR(selectedVoice);
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

        if (!initialized) { loadVoiceLOR(selectedVoice); initialized = true; }

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        auto* mod = static_cast<StraitsWestSandsVisual*>(module);
        for (int v = 0; v < 8; ++v)
            for (int l = 0; l < 3; ++l)
                paramMgr->setSpread(v, l, mod->params[SPREAD_V0_R + v*3 + l].getValue());

        bool useMono = mod->params[INTERP_TARGET].getValue() > 0.5f;
        paramMgr->setInterpolationTarget(
            useMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        saveVoiceLOR(selectedVoice);
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        int gs = monsoon->engine.stepIndex;
        for (int l = 0; l < 3; ++l) {
            visualEditor->setLanePlayStep(l,
                calcPlayhead(gs,
                    readLenParam(mod, lorId(selectedVoice, l, 0)),
                    readOffRotParam(mod, lorId(selectedVoice, l, 1)),
                    readOffRotParam(mod, lorId(selectedVoice, l, 2))));
        }
    }
};

Model* modelStraitsWestSandsVisual =
    createModel<StraitsWestSandsVisual, StraitsWestSandsVisualWidget>(
        "StraitsWestSandsVisual");
