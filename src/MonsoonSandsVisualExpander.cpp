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
                "res/panels/SandsMonoVisual_34HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Visual editor: right section, 6 lanes MONO ────────────────────
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::MONO);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ROW_BOT - ED_Y));
        addChild(visualEditor);

        // ── Left section: 8 cols × 6 rows ────────────────────────────────
        // Cols 0-3: mono CV jacks (LEN/OFF/ROT/SPR)
        // Cols 4-7: corresponding attenuverters
        for (int lane = 0; lane < N_LANES; ++lane) {
            float y = rowY(lane);
            for (int p = 0; p < 4; ++p) {
                addInput(createInputCentered<PJ301MPort>(
                    mm2px(Vec(JACK_X[p],  y)), mod, cvId(lane, p)));
                addParam(createParamCentered<Trimpot>(
                    mm2px(Vec(ATTEN_X[p], y)), mod, attenId(lane, p)));
            }
        }

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
        if (!initialized && module) {
            for (int l = 0; l < 6; ++l) {
                visualEditor->currentState.lanes[l].length   = (int)std::round(module->params[lenId(l)].getValue());
                visualEditor->currentState.lanes[l].offset   = (int)std::round(module->params[offId(l)].getValue());
                visualEditor->currentState.lanes[l].rotation = (int)std::round(module->params[rotId(l)].getValue());
            }
            initialized = true;
        }

        // ── Editor → params (UI thread writes OUR OWN params) ────────────
        for (int l = 0; l < 6; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            module->params[lenId(l)].setValue((float)lane.length);
            module->params[offId(l)].setValue((float)lane.offset);
            module->params[rotId(l)].setValue((float)lane.rotation);
            // Spread display trimpot (sprId) written from drag — no separate display
        }

        // CV applied at control rate in Monsoon::process() — base + cv*atten*scale.
        // Spread (SPR jack, param 3) base lives in sprId(lane) param.
        // Effective spread = clamp(base + cv*atten, 0, 1) read in processDNA.

        // ── Sync PatternEngine draws → display ─────────────────────────────
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
