#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonDeepStraitsSands.hpp"
#include "StraitsEastSandsVisual.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/PolyVoiceSandsParameterManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsEastVisualIds;

extern Model* modelMonsoon;
extern Model* modelMonsoonDeepStraitsSandsEast;

struct StraitsEastSandsVisualWidget : ModuleWidget {
    SandsVisualEditorV4*            visualEditor = nullptr;
    TabButtonGroup*                 tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr     = nullptr;
    int  selectedVoice = 0;
    bool initialized   = false;

    explicit StraitsEastSandsVisualWidget(StraitsEastSandsVisual* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/StraitsEastSandsVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        tabGroup = new TabButtonGroup(7, 2, mm2px(14.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos = mm2px(Vec(4.f, 22.f));
        addChild(tabGroup);

        // Handles are the L/O/R controls — no separate knobs
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(4.f, 32.f));
        visualEditor->box.size = mm2px(Vec(114.f, 175.f));
        addChild(visualEditor);

        // Spread sliders: 3 lanes for selected voice
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(20.f,  218.f)), mod, SPREAD_V0_R));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(60.f,  218.f)), mod, SPREAD_V0_M));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(100.f, 218.f)), mod, SPREAD_V0_O));
        addParam(createParamCentered<CKSS>(mm2px(Vec(56.f, 246.f)), mod, INTERP_TARGET));

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 7, 0);
    }

    ~StraitsEastSandsVisualWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }
    rack::Module* getDeepSandsEast() {
        if (!module) return nullptr;
        Module* curr = module->rightExpander.module;
        for (int d = 0; curr && d < 8; ++d) {
            if (curr->model == modelMonsoonDeepStraitsSandsEast) return curr;
            curr = curr->rightExpander.module;
        }
        return nullptr;
    }

    // ── L/O/R ↔ params helpers ───────────────────────────────────────────────
    void saveVoiceLOR(int v) {
        if (!module) return;
        for (int l = 0; l < 3; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            module->params[lorId(v, l, 0)].setValue((float)lane.length);
            module->params[lorId(v, l, 1)].setValue((float)lane.offset);
            module->params[lorId(v, l, 2)].setValue((float)lane.rotation);
        }
    }
    void loadVoiceLOR(int v) {
        if (!module) return;
        for (int l = 0; l < 3; ++l) {
            auto& lane = visualEditor->currentState.lanes[l];
            lane.length   = std::max(1, (int)std::round(module->params[lorId(v, l, 0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorId(v, l, 1)].getValue());
            lane.rotation = (int)std::round(module->params[lorId(v, l, 2)].getValue());
        }
    }

    void onVoiceTabChanged(int newVoice) {
        if (!paramMgr || !visualEditor) return;
        // Save current voice: probs + L/O/R
        paramMgr->syncEditorToPatternEngine(selectedVoice, visualEditor->currentState);
        saveVoiceLOR(selectedVoice);
        selectedVoice = newVoice;
        // Load new voice: probs + L/O/R
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

        // One-time init from saved params on patch load
        if (!initialized) {
            loadVoiceLOR(selectedVoice);
            initialized = true;
        }

        // Tab switching
        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        // Spread params (only show selected voice's spread on the 3 sliders)
        auto* mod = static_cast<StraitsEastSandsVisual*>(module);
        for (int v = 0; v < 7; ++v)
            for (int l = 0; l < 3; ++l)
                paramMgr->setSpread(v, l, mod->params[SPREAD_V0_R + v*3 + l].getValue());

        bool useMono = mod->params[INTERP_TARGET].getValue() > 0.5f;
        paramMgr->setInterpolationTarget(
            useMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // Continuously write current voice L/O/R from editor → params
        // (captures handle drags on audio thread's next read)
        saveVoiceLOR(selectedVoice);

        // Sync PatternEngine probs → editor display
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        // Per-lane playhead using our own L/O/R params
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

Model* modelStraitsEastSandsVisual =
    createModel<StraitsEastSandsVisual, StraitsEastSandsVisualWidget>(
        "StraitsEastSandsVisual");
