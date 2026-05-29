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

// ── Context menu items ────────────────────────────────────────────────────────
struct InterpTargetItem : MenuItem {
    StraitsEastSandsVisual* mod;
    void onAction(const event::Action&) override {
        mod->interpUseMono = !mod->interpUseMono;
    }
    void step() override {
        rightText = mod->interpUseMono ? "Mono Draw ✓" : "Avg Poly ✓";
        MenuItem::step();
    }
};

struct VoiceMaskItem : MenuItem {
    StraitsEastSandsVisual* mod;
    int voiceIdx; // 0-based local index
    void onAction(const event::Action&) override {
        mod->cvVoiceMask ^= (1 << voiceIdx);
    }
    void step() override {
        rightText = (mod->cvVoiceMask & (1 << voiceIdx)) ? "✓" : "";
        MenuItem::step();
    }
};

struct LaneMaskItem : MenuItem {
    StraitsEastSandsVisual* mod;
    int laneIdx; // 0=REST 1=MELODY 2=OCTAVE
    void onAction(const event::Action&) override {
        mod->cvLaneMask ^= (1 << laneIdx);
    }
    void step() override {
        rightText = (mod->cvLaneMask & (1 << laneIdx)) ? "✓" : "";
        MenuItem::step();
    }
};

// ── Widget ────────────────────────────────────────────────────────────────────
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

        // Voice tabs (voices 2-8)
        tabGroup = new TabButtonGroup(7, 2, mm2px(14.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos = mm2px(Vec(4.f, 14.f));
        addChild(tabGroup);

        // Visual editor — taller now controls zone is slimmer
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(2.f, 24.f));
        visualEditor->box.size = mm2px(Vec(117.92f, 58.f));  // 58mm = to y=82mm
        addChild(visualEditor);

        // ── Spread Trimpots (y=88mm): show selected voice, remembered per voice ─
        addParam(createParamCentered<Trimpot>(mm2px(Vec(20.f,  88.f)), mod, SPREAD_R));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(60.f,  88.f)), mod, SPREAD_M));
        addParam(createParamCentered<Trimpot>(mm2px(Vec(100.f, 88.f)), mod, SPREAD_O));

        // ── CV inputs: 3 poly jacks + 1 depth trimpot ────────────────────────
        // LEN=x18  OFF=x50  ROT=x82  DEPTH=x108   y=106mm
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.f,  106.f)), mod, CV_LEN_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(50.f,  106.f)), mod, CV_OFF_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(82.f,  106.f)), mod, CV_ROT_INPUT));
        addParam(createParamCentered<Trimpot>(   mm2px(Vec(108.f, 106.f)), mod, CV_DEPTH_PARAM));

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 7, 0);
    }

    ~StraitsEastSandsVisualWidget() override { delete paramMgr; }

    // ── Context menu ──────────────────────────────────────────────────────────
    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* mod = dynamic_cast<StraitsEastSandsVisual*>(module);
        if (!mod) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Spread interpolation target"));
        auto* ii = createMenuItem<InterpTargetItem>("Interp target");
        ii->mod = mod; menu->addChild(ii);

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("CV modulation — voices"));
        static const char* vnames[7] = {"V2","V3","V4","V5","V6","V7","V8"};
        for (int v = 0; v < 7; ++v) {
            auto* vi = createMenuItem<VoiceMaskItem>(vnames[v]);
            vi->mod = mod; vi->voiceIdx = v; menu->addChild(vi);
        }

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("CV modulation — lanes"));
        static const char* lnames[3] = {"REST","MELODY","OCTAVE"};
        for (int l = 0; l < 3; ++l) {
            auto* li = createMenuItem<LaneMaskItem>(lnames[l]);
            li->mod = mod; li->laneIdx = l; menu->addChild(li);
        }
    }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    // ── Spread helpers ────────────────────────────────────────────────────────
    // INTERP params are what Monsoon reads directly via cached pointer.
    // The 3 display trimpots (SPREAD_R/M/O) always show selected voice;
    // on tab switch we save the outgoing voice and load the incoming one.
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
        paramMgr->syncEditorToPatternEngine(selectedVoice, visualEditor->currentState);
        saveVoiceLOR(selectedVoice);
        saveVoiceSpread(selectedVoice);       // ← remember outgoing voice's spread
        selectedVoice = newVoice;
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
        loadVoiceLOR(selectedVoice);
        loadVoiceSpread(selectedVoice);       // ← recall incoming voice's spread
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        auto* mod = static_cast<StraitsEastSandsVisual*>(module);

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
            loadVoiceSpread(selectedVoice);   // ← restore spread on patch load
            initialized = true;
        }

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        // ── Write trimpot values to selected voice's INTERP params ────────────
        // Monsoon reads params[POLY_REST_INTERP_1 + v] etc. directly from
        // the cached visual expander pointer — zero-delay, audio thread safe.
        saveVoiceSpread(selectedVoice);

        // ── Update SpreadManager for editor display ───────────────────────────
        auto& smgr = paramMgr->spreadMgr;
        smgr.setSpread(selectedVoice, 0, module->params[SPREAD_R].getValue());
        smgr.setSpread(selectedVoice, 1, module->params[SPREAD_M].getValue());
        smgr.setSpread(selectedVoice, 2, module->params[SPREAD_O].getValue());
        smgr.setInterpolationTarget(
            mod->interpUseMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // ── Apply poly CV to L/O/R ────────────────────────────────────────────
        // Runs on the UI thread — writes our own params (thread-safe)
        float depth = mod->params[CV_DEPTH_PARAM].getValue();
        if (depth != 0.f) {
            auto applyCV = [&](int inputId, int paramOffset) {
                auto& inp = mod->inputs[inputId];
                if (!inp.isConnected()) return;
                for (int v = 0; v < 7; ++v) {
                    if (!(mod->cvVoiceMask & (1 << v))) continue;
                    float cv = inp.getVoltage(v) / 10.f * depth;  // normalise to ±1
                    for (int l = 0; l < 3; ++l) {
                        if (!(mod->cvLaneMask & (1 << l))) continue;
                        int pid = lorId(v, l, paramOffset);
                        float cur = mod->params[pid].getValue();
                        float range = (paramOffset == 0) ? 15.f : 15.f; // LEN/OFF/ROT all 0-15 or 1-16
                        float nudge = cv * range;
                        if (paramOffset == 0) // LEN: clamp 1-16
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

Model* modelStraitsEastSandsVisual =
    createModel<StraitsEastSandsVisual, StraitsEastSandsVisualWidget>(
        "StraitsEastSandsVisual");
