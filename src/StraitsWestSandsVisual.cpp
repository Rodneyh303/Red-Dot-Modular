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

struct InterpTargetItemW : MenuItem {
    StraitsWestSandsVisual* mod;
    void onAction(const event::Action&) override { mod->interpUseMono = !mod->interpUseMono; }
    void step() override {
        rightText = mod->interpUseMono ? "Mono Draw ✓" : "Avg Poly ✓";
        MenuItem::step();
    }
};
struct VoiceMaskItemW : MenuItem {
    StraitsWestSandsVisual* mod; int voiceIdx;
    void onAction(const event::Action&) override { mod->cvVoiceMask ^= (1 << voiceIdx); }
    void step() override { rightText = (mod->cvVoiceMask & (1 << voiceIdx)) ? "✓" : ""; MenuItem::step(); }
};
struct LaneMaskItemW : MenuItem {
    StraitsWestSandsVisual* mod; int laneIdx;
    void onAction(const event::Action&) override { mod->cvLaneMask ^= (1 << laneIdx); }
    void step() override { rightText = (mod->cvLaneMask & (1 << laneIdx)) ? "✓" : ""; MenuItem::step(); }
};

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

        // Voice tabs (voices 9-16), starting label = 9
        tabGroup = new TabButtonGroup(8, 9, mm2px(13.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos = mm2px(Vec(2.f, 14.f));
        addChild(tabGroup);

        // Visual editor
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(2.f, 24.f));
        visualEditor->box.size = mm2px(Vec(117.92f, 58.f));
        addChild(visualEditor);

        // Spread trimpots (y=88mm): selected voice, remembered per voice
        addParam(createParamCentered<Trimpot>(mm2px(Vec(20.f,  88.f)), mod, SPREAD_R));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(60.f,  88.f)), mod, SPREAD_M));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(100.f, 88.f)), mod, SPREAD_O));

        // CV inputs (y=108mm): LEN / OFF / ROT poly jacks + depth trimpot
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.f,  108.f)), mod, CV_LEN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(50.f,  108.f)), mod, CV_OFF_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(82.f,  108.f)), mod, CV_ROT_INPUT));
        addParam(createParamCentered<Trimpot>(   mm2px(Vec(108.f, 108.f)), mod, CV_DEPTH_PARAM));

        // West voices 9-16 → local 0-7 → global 7-14
        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 8, 7);
    }

    ~StraitsWestSandsVisualWidget() override { delete paramMgr; }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* mod = dynamic_cast<StraitsWestSandsVisual*>(module);
        if (!mod) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Spread interpolation target"));
        auto* ii = createMenuItem<InterpTargetItemW>("Interp target");
        ii->mod = mod; menu->addChild(ii);

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("CV modulation — voices"));
        static const char* vn[8] = {"V9","V10","V11","V12","V13","V14","V15","V16"};
        for (int v = 0; v < 8; ++v) {
            auto* vi = createMenuItem<VoiceMaskItemW>(vn[v]);
            vi->mod = mod; vi->voiceIdx = v; menu->addChild(vi);
        }

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("CV modulation — lanes"));
        static const char* ln[3] = {"REST","MELODY","OCTAVE"};
        for (int l = 0; l < 3; ++l) {
            auto* li = createMenuItem<LaneMaskItemW>(ln[l]);
            li->mod = mod; li->laneIdx = l; menu->addChild(li);
        }
    }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    // ── Spread helpers ────────────────────────────────────────────────────
    void saveVoiceSpread(int v) {
        if (!module) return;
        module->params[interpId(v, 0)].setValue(module->params[SPREAD_R].getValue());
        module->params[interpId(v, 1)].setValue(module->params[SPREAD_M].getValue());
        module->params[interpId(v, 2)].setValue(module->params[SPREAD_O].getValue());
    }
    void loadVoiceSpread(int v) {
        if (!module) return;
        module->params[SPREAD_R].setValue(module->params[interpId(v, 0)].getValue());
        module->params[SPREAD_M].setValue(module->params[interpId(v, 1)].getValue());
        module->params[SPREAD_O].setValue(module->params[interpId(v, 2)].getValue());
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
        saveVoiceSpread(selectedVoice);
        selectedVoice = newVoice;
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
        loadVoiceLOR(selectedVoice);
        loadVoiceSpread(selectedVoice);
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
            paramMgr->patternEngine            = pe;
            paramMgr->sequencerEngine          = se;
            paramMgr->spreadMgr.patternEngine  = pe;
            paramMgr->spreadMgr.sequencerEngine= se;
        }

        if (!initialized) {
            loadVoiceLOR(selectedVoice);
            loadVoiceSpread(selectedVoice);
            initialized = true;
        }

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        // Write trimpot → INTERP params (Monsoon reads directly, zero-delay)
        saveVoiceSpread(selectedVoice);

        // Update SpreadManager for editor display
        auto& smgr = paramMgr->spreadMgr;
        smgr.setSpread(selectedVoice, 0, mod->params[SPREAD_R].getValue());
        smgr.setSpread(selectedVoice, 1, mod->params[SPREAD_M].getValue());
        smgr.setSpread(selectedVoice, 2, mod->params[SPREAD_O].getValue());
        smgr.setInterpolationTarget(
            mod->interpUseMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // Apply poly CV to L/O/R
        float depth = mod->params[CV_DEPTH_PARAM].getValue();
        if (depth != 0.f) {
            auto applyCV = [&](int inputId, int paramOffset) {
                auto& inp = mod->inputs[inputId];
                if (!inp.isConnected()) return;
                for (int v = 0; v < 8; ++v) {
                    if (!(mod->cvVoiceMask & (1 << v))) continue;
                    float cv = inp.getVoltage(v + 7) / 10.f * depth; // channels 8-15 for voices 9-16
                    for (int l = 0; l < 3; ++l) {
                        if (!(mod->cvLaneMask & (1 << l))) continue;
                        int pid = lorId(v, l, paramOffset);
                        float cur = mod->params[pid].getValue();
                        float nudge = cv * 15.f;
                        if (paramOffset == 0)
                            mod->params[pid].setValue(clamp(cur + nudge, 1.f, 16.f));
                        else
                            mod->params[pid].setValue(clamp(cur + nudge, 0.f, 15.f));
                    }
                }
            };
            applyCV(CV_LEN_INPUT, 0);
            applyCV(CV_OFF_INPUT, 1);
            applyCV(CV_ROT_INPUT, 2);
        }

        saveVoiceLOR(selectedVoice);
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        int gs = monsoon->engine.stepIndex;
        for (int l = 0; l < 3; ++l) {
            visualEditor->setLanePlayStep(l,
                calcPlayhead(gs,
                    readLenParam   (mod, lorId(selectedVoice, l, 0)),
                    readOffRotParam(mod, lorId(selectedVoice, l, 1)),
                    readOffRotParam(mod, lorId(selectedVoice, l, 2))));
        }
    }
};

Model* modelStraitsWestSandsVisual =
    createModel<StraitsWestSandsVisual, StraitsWestSandsVisualWidget>(
        "StraitsWestSandsVisual");
