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

// ── Lane → DNA param base map ─────────────────────────────────────────────────
// Visual editor lane order: REST=0  MELODY=1  OCTAVE=2  LEGATO=3  ACCENT=4  VARIATION=5
// MonsoonIds DNA param base: R       M         O         L         A          V
static const int dnaLenBase[6] = {
    DNA_R_LEN_PARAM,   // REST
    DNA_M_LEN_PARAM,   // MELODY
    DNA_O_LEN_PARAM,   // OCTAVE
    DNA_L_LEN_PARAM,   // LEGATO
    DNA_A_LEN_PARAM,   // ACCENT
    DNA_V_LEN_PARAM,   // VARIATION
};

// Lane display names (for configParam labels)
static const char* laneName[6] = {
    "REST", "MELODY", "OCTAVE", "LEGATO", "ACCENT", "VARIATION"
};

// ── Param layout ──────────────────────────────────────────────────────────────
// 6 lanes × 3 params (LEN, OFF, ROT) = 18 params total.
// No spread — spread is a poly-only feature.
namespace SandsMonoVisualIds {
    enum ParamId {
        // REST
        LEN_REST = 0, OFF_REST, ROT_REST,
        // MELODY
        LEN_MELODY, OFF_MELODY, ROT_MELODY,
        // OCTAVE
        LEN_OCTAVE, OFF_OCTAVE, ROT_OCTAVE,
        // LEGATO
        LEN_LEGATO, OFF_LEGATO, ROT_LEGATO,
        // ACCENT
        LEN_ACCENT, OFF_ACCENT, ROT_ACCENT,
        // VARIATION
        LEN_VARIATION, OFF_VARIATION, ROT_VARIATION,
        NUM_PARAMS
    };
    // Convenience: base param ID for lane l is LEN_REST + l*3
    inline int lenId(int l) { return LEN_REST + l * 3; }
    inline int offId(int l) { return LEN_REST + l * 3 + 1; }
    inline int rotId(int l) { return LEN_REST + l * 3 + 2; }
}

// ── Module ────────────────────────────────────────────────────────────────────
struct MonsoonSandsVisualExpander : Module {
    MonsoonSandsVisualExpander() {
        using namespace SandsMonoVisualIds;
        config(NUM_PARAMS, 0, 0, 0);
        for (int l = 0; l < 6; ++l) {
            configParam(lenId(l), 1.f, 16.f, 16.f,
                string::f("%s Length", laneName[l]));
            configParam(offId(l), 0.f, 15.f,  0.f,
                string::f("%s Offset", laneName[l]));
            configParam(rotId(l), 0.f, 15.f,  0.f,
                string::f("%s Rotation", laneName[l]));
        }
    }
    void process(const ProcessArgs&) override {}
};

// ── Widget ────────────────────────────────────────────────────────────────────
struct MonsoonSandsVisualExpanderWidget : ModuleWidget {
    SandsVisualEditorV4*       visualEditor = nullptr;
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

        // ── Visual editor: MONO mode, 6 lanes ────────────────────────────────
        // Panel zone: X=2mm, Y=18mm, W=117.92mm, H=195mm
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::MONO);
        visualEditor->box.pos  = mm2px(Vec(2.f, 18.f));
        visualEditor->box.size = mm2px(Vec(117.92f, 195.f));
        addChild(visualEditor);

        // ── L/O/R knobs: 6 columns (one per lane), 3 rows (L / O / R) ───────
        // Column centres (mm): spread across 121.92mm panel
        // Margins 5mm each side → 111.92mm usable → 6 cols at ~18.65mm spacing
        const float colX[6] = { 12.f, 30.f, 48.f, 66.f, 84.f, 102.f };
        const float rowY[3]  = { 225.f, 248.f, 271.f };  // LEN / OFF / ROT

        using P = SandsMonoVisualIds;
        for (int l = 0; l < 6; ++l) {
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(colX[l], rowY[0])), mod, P::lenId(l)));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(colX[l], rowY[1])), mod, P::offId(l)));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(colX[l], rowY[2])), mod, P::rotId(l)));
        }

        paramMgr = new MonoSandsParameterManager();
    }

    ~MonsoonSandsVisualExpanderWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    MonsoonSandsExpander* getSandsExpander() {
        if (!module) return nullptr;
        Module* curr = module->rightExpander.module;
        for (int d = 0; curr && d < 8; ++d) {
            if (curr->model == modelMonsoonSandsExpander)
                return reinterpret_cast<MonsoonSandsExpander*>(curr);
            curr = curr->rightExpander.module;
        }
        return nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        PatternEngine* pe = &monsoon->engine.pe;
        if (paramMgr->patternEngine != pe)
            paramMgr->patternEngine = pe;

        // ── Write L/O/R params to the DNA expander (or Monsoon directly) ────
        // This keeps Monsoon's existing DNA-reading logic working unchanged.
        auto* mod  = static_cast<MonsoonSandsVisualExpander*>(module);
        MonsoonSandsExpander* sands = getSandsExpander();

        using P = SandsMonoVisualIds;
        for (int l = 0; l < 6; ++l) {
            float len = mod->params[P::lenId(l)].getValue();
            float off = mod->params[P::offId(l)].getValue();
            float rot = mod->params[P::rotId(l)].getValue();

            if (sands) {
                // Write to existing DNA expander so Monsoon picks them up
                sands->params[dnaLenBase[l]    ].setValue(len);
                sands->params[dnaLenBase[l] + 1].setValue(off);
                sands->params[dnaLenBase[l] + 2].setValue(rot);
            } else {
                // No DNA expander — write directly to Monsoon's param array
                monsoon->params[dnaLenBase[l]    ].setValue(len);
                monsoon->params[dnaLenBase[l] + 1].setValue(off);
                monsoon->params[dnaLenBase[l] + 2].setValue(rot);
            }
        }

        // ── Sync raw PatternEngine draws → editor (no spread for mono) ───────
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // ── Per-lane playhead using our own L/O/R params ─────────────────────
        int globalStep = monsoon->engine.stepIndex;
        for (int l = 0; l < 6; ++l) {
            int len = readLenParam   (mod, P::lenId(l));
            int off = readOffRotParam(mod, P::offId(l));
            int rot = readOffRotParam(mod, P::rotId(l));
            visualEditor->setLanePlayStep(l, calcPlayhead(globalStep, len, off, rot));
        }
    }
};

Model* modelMonsoonSandsVisualExpander =
    createModel<MonsoonSandsVisualExpander, MonsoonSandsVisualExpanderWidget>(
        "MonsoonSandsVisualExpander");
