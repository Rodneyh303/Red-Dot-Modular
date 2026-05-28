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

// Lane → DNA param base (for playhead calculation using our own params)
// Visual editor order: REST=0 MELODY=1 OCTAVE=2 LEGATO=3 ACCENT=4 VARIATION=5
static const int dnaLenBase[6] = {
    DNA_R_LEN_PARAM,   // REST
    DNA_M_LEN_PARAM,   // MELODY
    DNA_O_LEN_PARAM,   // OCTAVE
    DNA_L_LEN_PARAM,   // LEGATO
    DNA_A_LEN_PARAM,   // ACCENT
    DNA_V_LEN_PARAM,   // VARIATION
};

// ── Widget ────────────────────────────────────────────────────────────────────
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

        // Visual editor: MONO mode, 6 lanes
        // Zone: X=2mm, Y=18mm, W=117.92mm, H=195mm
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::MONO);
        visualEditor->box.pos  = mm2px(Vec(2.f, 18.f));
        visualEditor->box.size = mm2px(Vec(117.92f, 195.f));
        addChild(visualEditor);

        // L/O/R knobs: 6 columns × 3 rows
        // Col centres (mm): 12 30 48 66 84 102
        // Row centres (mm): 225=LEN  248=OFF  271=ROT
        const float colX[6] = { 12.f, 30.f, 48.f, 66.f, 84.f, 102.f };
        const float rowY[3]  = { 225.f, 248.f, 271.f };

        for (int l = 0; l < 6; ++l) {
            addParam(createParamCentered<Trimpot>(mm2px(Vec(colX[l], rowY[0])), mod, lenId(l)));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(colX[l], rowY[1])), mod, offId(l)));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(colX[l], rowY[2])), mod, rotId(l)));
        }

        paramMgr = new MonoSandsParameterManager();
    }

    ~MonsoonSandsVisualExpanderWidget() override { delete paramMgr; }

    // Walk rightExpander chain to find Monsoon
    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        // Wire PatternEngine lazily
        PatternEngine* pe = &monsoon->engine.pe;
        if (paramMgr->patternEngine != pe)
            paramMgr->patternEngine = pe;

        // Sync raw PatternEngine draws → editor.
        // No spread (mono), no write-back. Monsoon reads our params
        // directly via cachedSandsVisualExpander on the audio thread.
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // Per-lane playhead: read our OWN params (we are the source of truth)
        auto* mod = static_cast<MonsoonSandsVisualExpander*>(module);
        int globalStep = monsoon->engine.stepIndex;
        for (int l = 0; l < 6; ++l) {
            visualEditor->setLanePlayStep(l,
                calcPlayhead(globalStep,
                             readLenParam   (mod, lenId(l)),
                             readOffRotParam(mod, offId(l)),
                             readOffRotParam(mod, rotId(l))));
        }
    }
};

Model* modelMonsoonSandsVisualExpander =
    createModel<MonsoonSandsVisualExpander, MonsoonSandsVisualExpanderWidget>(
        "MonsoonSandsVisualExpander");
