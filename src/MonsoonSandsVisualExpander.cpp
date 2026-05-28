#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonSandsExpander.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/MonoSandsParameterManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;

extern Model* modelMonsoon;
extern Model* modelMonsoonSandsExpander;

// Mono visual editor has NO spread — spread is a poly-only feature.
// This module has zero parameters of its own; it is purely a visual
// window onto the PatternEngine values already set by MonsoonSandsExpander.

struct MonsoonSandsVisualExpander : Module {
    MonsoonSandsVisualExpander() {
        config(0, 0, 0, 0);
    }
    void process(const ProcessArgs&) override {}
};

// ── Lane → DNA param base map ─────────────────────────────────────────────────
// SandsVisualEditorV4 lane order: REST=0 MELODY=1 OCTAVE=2 LEGATO=3 ACCENT=4 VARIATION=5
// MonsoonIds DNA param order:     R      M        O        L        A          V
// LEN param for lane l: dnaLenBase[l], OFF: +1, ROT: +2
static const int dnaLenBase[6] = {
    DNA_R_LEN_PARAM,   // REST
    DNA_M_LEN_PARAM,   // MELODY
    DNA_O_LEN_PARAM,   // OCTAVE
    DNA_L_LEN_PARAM,   // LEGATO
    DNA_A_LEN_PARAM,   // ACCENT
    DNA_V_LEN_PARAM,   // VARIATION
};

struct MonsoonSandsVisualExpanderWidget : ModuleWidget {
    SandsVisualEditorV4*   visualEditor = nullptr;
    MonoSandsParameterManager* paramMgr = nullptr;

    explicit MonsoonSandsVisualExpanderWidget(MonsoonSandsVisualExpander* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/SandsMonoVisual_24HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Visual editor: MONO mode, 6 lanes, full panel height minus header/footer
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

    // Walk rightExpander chain to find MonsoonSandsExpander (holds DNA L/O/R params)
    MonsoonSandsExpander* getSandsExpander() {
        if (!module) return nullptr;
        Module* curr = module->rightExpander.module;
        for (int depth = 0; curr && depth < 8; ++depth) {
            if (curr->model == modelMonsoonSandsExpander)
                return reinterpret_cast<MonsoonSandsExpander*>(curr);
            curr = curr->rightExpander.module;
        }
        return nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        // Wire engine lazily
        PatternEngine* pe = &monsoon->engine.pe;
        if (paramMgr->patternEngine != pe)
            paramMgr->patternEngine = pe;

        // Sync raw PatternEngine draws → editor (no spread for mono)
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // Per-lane playhead
        int globalStep = monsoon->engine.stepIndex;   // -1 when not running
        MonsoonSandsExpander* sands = getSandsExpander();

        for (int l = 0; l < 6; ++l) {
            if (globalStep < 0) {
                visualEditor->setLanePlayStep(l, -1);
                continue;
            }
            int len = 16, off = 0, rot = 0;
            if (sands) {
                len = readLenParam   (sands, dnaLenBase[l]);
                off = readOffRotParam(sands, dnaLenBase[l] + 1);
                rot = readOffRotParam(sands, dnaLenBase[l] + 2);
            }
            visualEditor->setLanePlayStep(l, calcPlayhead(globalStep, len, off, rot));
        }
    }
};

Model* modelMonsoonSandsVisualExpander =
    createModel<MonsoonSandsVisualExpander, MonsoonSandsVisualExpanderWidget>(
        "MonsoonSandsVisualExpander");
