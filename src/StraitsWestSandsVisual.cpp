#include <rack.hpp>
#include "Monsoon.hpp"
#include "StraitsWestSandsVisual.hpp"
#include "StraitsEastSandsVisual.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "dsp/managers/PolyVoiceSandsParameterManager.hpp"
#include "dsp/managers/SpreadManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsWestVisualIds;

extern Plugin* pluginInstance;

struct WestInterpItem : MenuItem {
    StraitsWestSandsVisual* mod;
    void onAction(const event::Action&) override { mod->interpUseMono = !mod->interpUseMono; }
    void step() override {
        rightText = mod->interpUseMono ? "Mono Draw ✓" : "Avg Poly ✓";
        MenuItem::step();
    }
};

struct StraitsWestSandsVisualWidget : ModuleWidget {
    SandsVisualEditorV4*            visualEditor = nullptr;
    TabButtonGroup*                 tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr     = nullptr;
    int  selectedVoice = 0;
    bool initialized   = false;

    using E = StraitsEastVisualIds;   // alias for layout constants

    explicit StraitsWestSandsVisualWidget(StraitsWestSandsVisual* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/StraitsWestSandsVisual_36HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

        // Voice tabs (voices 9-16)
        tabGroup = new TabButtonGroup(8, 9, mm2px(E::ED_Y-2.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos    = mm2px(Vec(E::ED_X, 8.f));
        tabGroup->box.size.x = mm2px(E::ED_W);
        addChild(tabGroup);

        // Visual editor — same width/position as East
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(E::ED_X, E::ED_Y));
        visualEditor->box.size = mm2px(Vec(E::ED_W, E::ROW_BOT - E::ED_Y));
        addChild(visualEditor);

        // West has no CV jacks — left section shows "via East" info only

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 8, 7);
    }

    ~StraitsWestSandsVisualWidget() override { delete paramMgr; }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* mod = dynamic_cast<StraitsWestSandsVisual*>(module);
        if (!mod) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Spread interpolation"));
        auto* ii = createMenuItem<WestInterpItem>("Interpolation target");
        ii->mod=mod; menu->addChild(ii);
    }

    void saveVoiceSpread(int lv) {
        if (!module) return;
        module->params[restInterpId(lv)  ].setValue(module->params[SPREAD_R].getValue());
        module->params[melodyInterpId(lv)].setValue(module->params[SPREAD_M].getValue());
        module->params[octaveInterpId(lv)].setValue(module->params[SPREAD_O].getValue());
    }
    void loadVoiceSpread(int lv) {
        if (!module) return;
        module->params[SPREAD_R].setValue(module->params[restInterpId(lv)  ].getValue());
        module->params[SPREAD_M].setValue(module->params[melodyInterpId(lv)].getValue());
        module->params[SPREAD_O].setValue(module->params[octaveInterpId(lv)].getValue());
    }
    void saveVoiceLOR(int lv) {
        if (!module || !visualEditor) return;
        for (int l=0; l<3; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            module->params[lorId(lv,l,0)].setValue((float)lane.length);
            module->params[lorId(lv,l,1)].setValue((float)lane.offset);
            module->params[lorId(lv,l,2)].setValue((float)lane.rotation);
        }
    }
    void loadVoiceLOR(int lv) {
        if (!module || !visualEditor) return;
        for (int l=0; l<3; ++l) {
            auto& lane = visualEditor->currentState.lanes[l];
            lane.length   = std::max(1,(int)std::round(module->params[lorId(lv,l,0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorId(lv,l,1)].getValue());
            lane.rotation = (int)std::round(module->params[lorId(lv,l,2)].getValue());
        }
    }
    void onVoiceTabChanged(int nv) {
        if (!paramMgr || !visualEditor) return;
        paramMgr->syncEditorToPatternEngine(selectedVoice, visualEditor->currentState);
        saveVoiceLOR(selectedVoice);
        saveVoiceSpread(selectedVoice);
        selectedVoice = nv;
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
        loadVoiceLOR(selectedVoice);
        loadVoiceSpread(selectedVoice);
    }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        auto* mod = static_cast<StraitsWestSandsVisual*>(module);
        PatternEngine*   pe = &monsoon->engine.pe;
        SequencerEngine* se = &monsoon->engine;
        if (paramMgr->patternEngine != pe) {
            paramMgr->patternEngine             = pe;
            paramMgr->sequencerEngine           = se;
            paramMgr->spreadMgr.patternEngine   = pe;
            paramMgr->spreadMgr.sequencerEngine = se;
        }

        if (!initialized) {
            loadVoiceLOR(selectedVoice);
            loadVoiceSpread(selectedVoice);
            initialized = true;
        }

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        saveVoiceSpread(selectedVoice);

        auto& smgr = paramMgr->spreadMgr;
        smgr.setSpread(selectedVoice, 0, mod->params[SPREAD_R].getValue());
        smgr.setSpread(selectedVoice, 1, mod->params[SPREAD_M].getValue());
        smgr.setSpread(selectedVoice, 2, mod->params[SPREAD_O].getValue());
        smgr.setInterpolationTarget(
            mod->interpUseMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // CV applied at control rate in Monsoon::process() via East's jacks.

        saveVoiceLOR(selectedVoice);
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        int gs = monsoon->engine.stepIndex;
        for (int l=0; l<3; ++l)
            visualEditor->setLanePlayStep(l,
                calcPlayhead(gs,
                    readLenParam   (mod, lorId(selectedVoice,l,0)),
                    readOffRotParam(mod, lorId(selectedVoice,l,1)),
                    readOffRotParam(mod, lorId(selectedVoice,l,2))));
    }
};

Model* modelStraitsWestSandsVisual =
    createModel<StraitsWestSandsVisual,StraitsWestSandsVisualWidget>(
        "StraitsWestSandsVisual");
