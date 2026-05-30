#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonSandsExpander.hpp"
#include "MonsoonSandsVisualExpander.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/MonoSandsParameterManager.hpp"
#include "managers/SpreadManager.hpp"

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

        auto* mod = static_cast<MonsoonSandsVisualExpander*>(module);

        PatternEngine* pe = &monsoon->engine.pe;
        if (paramMgr->patternEngine != pe)
            paramMgr->setPatternEngine(pe);   // updates both patternEngine + spreadMgr.patternEngine

        // ── One-time initialisation from saved params ─────────────────────
        if (!initialized) {
            for (int l = 0; l < 6; ++l) {
                visualEditor->currentState.lanes[l].length   = (int)std::round(mod->params[lenId(l)].getValue());
                visualEditor->currentState.lanes[l].offset   = (int)std::round(mod->params[offId(l)].getValue());
                visualEditor->currentState.lanes[l].rotation = (int)std::round(mod->params[rotId(l)].getValue());
                mod->spreadEffective[l] = mod->params[sprId(l)].getValue();
            }
            initialized = true;
        }

        // ── Editor → params (UI thread, own params) ───────────────────────
        for (int l = 0; l < 6; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            mod->params[lenId(l)].setValue((float)lane.length);
            mod->params[offId(l)].setValue((float)lane.offset);
            mod->params[rotId(l)].setValue((float)lane.rotation);
        }

        // ── SpreadManager: feed effective spread (CV-modified at control rate) ──
        // spreadEffective[] is written by processDNA from sprId base + cv*atten.
        // No CV connected → spreadEffective[l] == sprId(l) base param (initialised above).
        for (int l = 0; l < 6; ++l)
            paramMgr->spreadMgr.setSpread(0, l, mod->spreadEffective[l]);

        paramMgr->spreadMgr.setInterpolationTarget(SpreadManager::AVERAGE_POLY);

        // ── Sync PatternEngine → display (uses SpreadManager.getInterpolatedValue) ──
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // ── Per-lane playhead ─────────────────────────────────────────────
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
