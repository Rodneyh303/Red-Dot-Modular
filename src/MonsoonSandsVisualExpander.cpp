#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonSandsExpander.hpp"
#include "MonsoonSandsVisualExpander.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/MonoSandsParameterManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace SandsMonoVisualIds;

extern Model* modelMonsoon;
extern Model* modelMonsoonSandsExpander;

// ── Widget ────────────────────────────────────────────────────────────────────
struct MonsoonSandsVisualExpanderWidget : ModuleWidget {
    SandsVisualEditorV4*       visualEditor = nullptr;
    MonoSandsParameterManager* paramMgr     = nullptr;
    bool                       initialized  = false;

    explicit MonsoonSandsVisualExpanderWidget(MonsoonSandsVisualExpander* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/SandsMonoVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Visual editor: full panel (header to footer), MONO 6 lanes.
        // Handles ARE the L/O/R controls — no separate knobs needed.
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::MONO);
        visualEditor->box.pos  = mm2px(Vec(2.f, 16.f));
        visualEditor->box.size = mm2px(Vec(117.92f, 355.f));
        addChild(visualEditor);

        paramMgr = new MonoSandsParameterManager();
    }

    ~MonsoonSandsVisualExpanderWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        PatternEngine* pe = &monsoon->engine.pe;
        if (paramMgr->patternEngine != pe)
            paramMgr->patternEngine = pe;

        // ── One-time initialisation from saved params ─────────────────────
        // On patch load the module's params are restored from JSON before
        // step() runs. We copy them into the editor state once so the
        // handles show the correct saved positions immediately.
        if (!initialized && module) {
            for (int l = 0; l < 6; ++l) {
                visualEditor->currentState.lanes[l].length   = (int)std::round(module->params[lenId(l)].getValue());
                visualEditor->currentState.lanes[l].offset   = (int)std::round(module->params[offId(l)].getValue());
                visualEditor->currentState.lanes[l].rotation = (int)std::round(module->params[rotId(l)].getValue());
            }
            initialized = true;
        }

        // ── Editor → params (UI thread writes OUR OWN params) ────────────
        // The audio thread reads these via cachedSandsVisualExpander.
        // Writing our own ParamQuantity from the UI thread is thread-safe.
        for (int l = 0; l < 6; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            module->params[lenId(l)].setValue((float)lane.length);
            module->params[offId(l)].setValue((float)lane.offset);
            module->params[rotId(l)].setValue((float)lane.rotation);
        }

        // ── Sync PatternEngine draws → display (no spread for mono) ──────
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // ── Per-lane playhead using our own L/O/R ─────────────────────────
        int globalStep = monsoon->engine.stepIndex;
        for (int l = 0; l < 6; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            visualEditor->setLanePlayStep(l,
                calcPlayhead(globalStep, lane.length, lane.offset, lane.rotation));
        }
    }
};

Model* modelMonsoonSandsVisualExpander =
    createModel<MonsoonSandsVisualExpander, MonsoonSandsVisualExpanderWidget>(
        "MonsoonSandsVisualExpander");
