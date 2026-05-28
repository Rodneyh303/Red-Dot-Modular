#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "managers/PolyVoiceSandsParameterManager.hpp"

using namespace rack;
using namespace redDot;

extern Model* modelMonsoon;

// ── Param layout ──────────────────────────────────────────────────────────────
// 7 voices × 3 lanes = 21 spread params + 1 target toggle
namespace StraitsEastVisualIds {
    enum ParamId {
        SPREAD_V0_R = 0, SPREAD_V0_M, SPREAD_V0_O,   // Voice 2
        SPREAD_V1_R,     SPREAD_V1_M, SPREAD_V1_O,   // Voice 3
        SPREAD_V2_R,     SPREAD_V2_M, SPREAD_V2_O,   // Voice 4
        SPREAD_V3_R,     SPREAD_V3_M, SPREAD_V3_O,   // Voice 5
        SPREAD_V4_R,     SPREAD_V4_M, SPREAD_V4_O,   // Voice 6
        SPREAD_V5_R,     SPREAD_V5_M, SPREAD_V5_O,   // Voice 7
        SPREAD_V6_R,     SPREAD_V6_M, SPREAD_V6_O,   // Voice 8
        INTERP_TARGET,
        NUM_PARAMS
    };
    enum InputId  { NUM_INPUTS  = 0 };
    enum OutputId { NUM_OUTPUTS = 0 };
    enum LightId  { NUM_LIGHTS  = 0 };
}

// ── Module ────────────────────────────────────────────────────────────────────
struct StraitsEastSandsVisual : Module {
    StraitsEastSandsVisual() {
        using namespace StraitsEastVisualIds;
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        for (int v = 0; v < 7; ++v) {
            for (int l = 0; l < 3; ++l) {
                const char* laneNames[3] = {"REST", "MELODY", "OCTAVE"};
                configParam(SPREAD_V0_R + v*3 + l, 0.f, 1.f, 0.f,
                    string::f("Voice %d Spread %s", v+2, laneNames[l]));
            }
        }
        configSwitch(INTERP_TARGET, 0.f, 1.f, 0.f, "Interp Target",
                     {"Avg Poly", "Mono Draw"});
    }
    void process(const ProcessArgs&) override {}
};

// ── Widget ────────────────────────────────────────────────────────────────────
struct StraitsEastSandsVisualWidget : ModuleWidget {
    SandsVisualEditorV4* visualEditor = nullptr;
    TabButtonGroup* tabGroup  = nullptr;
    PolyVoiceSandsParameterManager* paramMgr = nullptr;

    int  selectedVoice = 0;

    explicit StraitsEastSandsVisualWidget(StraitsEastSandsVisual* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/StraitsEastSandsVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Voice tabs: V2–V8, Y=22mm, tab width 14mm with 1mm gap ──────────
        tabGroup = new TabButtonGroup(7, /*startVoiceNum=*/2,
                                      mm2px(14.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos = mm2px(Vec(4.f, 22.f));
        addChild(tabGroup);

        // ── Visual Editor: 3 lanes, POLY mode ───────────────────────────────
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(4.f, 32.f));
        visualEditor->box.size = mm2px(Vec(114.f, 120.f));
        addChild(visualEditor);

        // ── Spread sliders (for currently selected voice, 3 lanes) ───────────
        using P = StraitsEastVisualIds;
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(20.f,  168.f)), mod, P::SPREAD_V0_R));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(60.f,  168.f)), mod, P::SPREAD_V0_M));
        addParam(createParamCentered<VCVSlider>(mm2px(Vec(100.f, 168.f)), mod, P::SPREAD_V0_O));

        // ── Interp target toggle ─────────────────────────────────────────────
        addParam(createParamCentered<CKSS>(mm2px(Vec(56.f, 199.f)), mod, P::INTERP_TARGET));

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 7, 0);
    }

    ~StraitsEastSandsVisualWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        if (!module) return nullptr;
        return dynamic_cast<Monsoon*>(module->rightExpander.module);
    }

    void onVoiceTabChanged(int newVoice) {
        if (!paramMgr || !visualEditor) return;
        // Save current voice edits back to PatternEngine
        paramMgr->syncEditorToPatternEngine(selectedVoice, visualEditor->currentState);
        selectedVoice = newVoice;
        // Load new voice state
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        PatternEngine*    pe = &monsoon->engine.pe;
        SequencerEngine*  se = &monsoon->engine;

        // Wire engines lazily
        if (paramMgr->patternEngine != pe) {
            paramMgr->patternEngine   = pe;
            paramMgr->sequencerEngine = se;
            paramMgr->spreadMgr.patternEngine   = pe;
            paramMgr->spreadMgr.sequencerEngine = se;
        }

        // Handle tab switching
        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) {
            onVoiceTabChanged(newSel);
        }

        // Update all 21 spread params
        auto* mod = static_cast<StraitsEastSandsVisual*>(module);
        using P = StraitsEastVisualIds;
        for (int v = 0; v < 7; ++v)
            for (int l = 0; l < 3; ++l)
                paramMgr->setSpread(v, l, mod->params[P::SPREAD_V0_R + v*3 + l].getValue());

        // Interp target
        bool useMono = mod->params[P::INTERP_TARGET].getValue() > 0.5f;
        paramMgr->setInterpolationTarget(
            useMono ? SpreadManager::MONO_DRAW
                    : SpreadManager::AVERAGE_POLY);

        // Sync selected voice → editor (shows interpolated values)
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
    }
};

Model* modelStraitsEastSandsVisual =
    createModel<StraitsEastSandsVisual, StraitsEastSandsVisualWidget>(
        "StraitsEastSandsVisual");
