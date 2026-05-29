#include <rack.hpp>
#include "Monsoon.hpp"
#include "StraitsWestSandsVisual.hpp"
#include "StraitsEastSandsVisual.hpp"
#include "MonsoonExpanderManager.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/PolyVoiceSandsParameterManager.hpp"
#include "managers/SpreadManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsWestVisualIds;
using namespace StraitsEastVisualIds;  // for InputId, ATTEN_* enums

extern Plugin* pluginInstance;

struct WestInterpItem : MenuItem {
    StraitsWestSandsVisual* mod;
    void onAction(const event::Action&) override { mod->interpUseMono = !mod->interpUseMono; }
    void step() override {
        rightText = mod->interpUseMono ? "Mono Draw ✓" : "Avg Poly ✓";
        MenuItem::step();
    }
};
struct WestLorVoiceItem : MenuItem {
    StraitsWestSandsVisual* mod; int v;
    void onAction(const event::Action&) override { mod->cvLorVoiceMask    ^= (1<<v); }
    void step() override { rightText=(mod->cvLorVoiceMask    &(1<<v))?"✓":""; MenuItem::step(); }
};
struct WestSprVoiceItem : MenuItem {
    StraitsWestSandsVisual* mod; int v;
    void onAction(const event::Action&) override { mod->cvSpreadVoiceMask ^= (1<<v); }
    void step() override { rightText=(mod->cvSpreadVoiceMask &(1<<v))?"✓":""; MenuItem::step(); }
};

struct StraitsWestSandsVisualWidget : ModuleWidget {
    SandsVisualEditorV4*            visualEditor = nullptr;
    TabButtonGroup*                 tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr     = nullptr;
    int  selectedVoice = 0;
    bool initialized   = false;

    static constexpr float ED_X  = 54.f;
    static constexpr float ED_W  = W_MM - ED_X - 4.f;
    static constexpr float TAB_Y = 8.f;
    static constexpr float ED_Y  = 18.f;
    static constexpr float ED_H  = 104.5f;

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
        tabGroup = new TabButtonGroup(8, 9, mm2px(TAB_Y+7.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos  = mm2px(Vec(ED_X, TAB_Y));
        tabGroup->box.size.x = mm2px(ED_W);
        addChild(tabGroup);

        // Visual editor — same size/position as East
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ED_H));
        addChild(visualEditor);

        // West has no input jacks — the left section shows
        // "connected to East" info labels only (drawn by panel SVG)
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

        static const char* vn[8]={"V9","V10","V11","V12","V13","V14","V15","V16"};
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("LOR CV — voice opt-out (via East)"));
        for (int v=0; v<8; ++v) {
            auto* vi=createMenuItem<WestLorVoiceItem>(vn[v]);
            vi->mod=mod; vi->v=v; menu->addChild(vi);
        }
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Spread CV — voice opt-out (via East)"));
        for (int v=0; v<8; ++v) {
            auto* si=createMenuItem<WestSprVoiceItem>(vn[v]);
            si->mod=mod; si->v=v; menu->addChild(si);
        }
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

    // Get cached East visual expander (West's CV source)
    StraitsEastSandsVisual* getEastVisual() {
        if (!module) return nullptr;
        auto* monsoon = getMonsoon();
        if (!monsoon) return nullptr;
        return monsoon->expanderManager.cachedEastSandsVisual;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        auto* mod  = static_cast<StraitsWestSandsVisual*>(module);
        auto* east = getEastVisual();   // null if East not connected

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

        // ── Apply LOR CV via East's jacks (channels 7-14 for West voices) ─────
        if (east) {
            for (int row = 0; row < 9; ++row) {
                auto& inp = east->inputs[CV_LOR_0 + row];
                if (!inp.isConnected()) continue;
                // East uses channels 0-6, West uses 7-14 from same cable
                float atten = east->params[ATTEN_LOR_0 + row].getValue();
                for (int lv = 0; lv < 8; ++lv) {
                    if (!(mod->cvLorVoiceMask & (1<<lv))) continue;
                    float cv  = inp.getVoltage(lv + 7) / 10.f * atten;
                    int   pid = rowLorId(lv, row);
                    float lo  = (row%3 == 0) ? 1.f : 0.f;
                    float hi  = (row%3 == 0) ? 16.f : 15.f;
                    mod->params[pid].setValue(clamp(mod->params[pid].getValue()+cv*hi, lo, hi));
                }
            }

            // ── Apply Spread CV via East's jacks (channels 7-14) ──────────────
            for (int row = 0; row < 9; ++row) {
                auto& inp = east->inputs[CV_SPR_0 + row];
                if (!inp.isConnected()) continue;
                float atten = east->params[ATTEN_SPR_0 + row].getValue();
                for (int lv = 0; lv < 8; ++lv) {
                    if (!(mod->cvSpreadVoiceMask & (1<<lv))) continue;
                    float cv  = inp.getVoltage(lv + 7) / 10.f * atten;
                    int   pid = rowInterpId(lv, row);
                    mod->params[pid].setValue(clamp(mod->params[pid].getValue()+cv, 0.f, 1.f));
                }
            }
        }

        saveVoiceLOR(selectedVoice);
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        int gs = monsoon->engine.stepIndex;
        for (int l=0; l<3; ++l) {
            visualEditor->setLanePlayStep(l,
                calcPlayhead(gs,
                    readLenParam   (mod, lorId(selectedVoice,l,0)),
                    readOffRotParam(mod, lorId(selectedVoice,l,1)),
                    readOffRotParam(mod, lorId(selectedVoice,l,2))));
        }
    }
};

Model* modelStraitsWestSandsVisual =
    createModel<StraitsWestSandsVisual,StraitsWestSandsVisualWidget>(
        "StraitsWestSandsVisual");
