#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
//#include "MonsoonSandsExpander.hpp"
#include "MonsoonSandsVisualExpander.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "dsp/managers/MonoSandsParameterManager.hpp"
#include "dsp/managers/SpreadManager.hpp"

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
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    rack::app::SvgPanel* panelWidget = nullptr;
    int lastThemeLight = -1;

    explicit MonsoonSandsVisualExpanderWidget(MonsoonSandsVisualExpander* mod) {
        setModule(mod);
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/SandsMonoVisual_40HP.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/SandsMonoVisual_40HP_light.svg"));
        panelWidget = createPanel(asset::plugin(pluginInstance,
                            "res/panels/SandsMonoVisual_40HP.svg"));
        setPanel(panelWidget);

        redDot::addRedScrews(this);

        // ── Visual editor: right section, 6 lanes MONO ────────────────────
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::MONO);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ROW_BOT - ED_Y));
        addChild(visualEditor);

        // ── LOR controls: 3 CV jacks + 3 attens per lane (all 6 lanes) ────
        for (int lane = 0; lane < N_LANES; ++lane) {
            float y = rowY(lane);
            for (int p = 0; p < 3; ++p) {  // LEN/OFF/ROT
                addInput(createInputCentered<PJ301MPort>(
                    mm2px(Vec(JACK_X[p],  y)), mod, cvId(lane, p)));
                addParam(createParamCentered<Trimpot>(
                    mm2px(Vec(ATTEN_X[p], y)), mod, attenId(lane, p)));
            }
        }

        // ── Spread group: base trimpot + CV jack + atten, REST/MEL/OCT only ─
        // Aligned to the top 3 lanes (REST, MELODY, OCTAVE). LEG/ACC/VAR are
        // mono-only — no spread.
        for (int l = 0; l < N_SPREAD_LANES; ++l) {
            float y = rowY(l);
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(SPR_BASE_X, y)), mod, sprId(l)));
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(SPR_CV_X, y)), mod, sprCvId(l)));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(SPR_ATTEN_X, y)), mod, sprAttenId(l)));
        }

        paramMgr = new MonoSandsParameterManager();
    }

    ~MonsoonSandsVisualExpanderWidget() override { delete paramMgr; }

    Monsoon* getMonsoon() {
        return module ? findMonsoonEitherSide(module) : nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;

        Monsoon* monsoon = getMonsoon();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }

        int wantLight = monsoon->lightTheme ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            if (panelWidget) panelWidget->setBackground(wantLight ? panelSvgLight : panelSvgDark);
            if (visualEditor) visualEditor->setTheme(wantLight != 0);
        }

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
            }
            for (int l = 0; l < 3; ++l)   // spread: REST/MEL/OCT only
                mod->spreadEffective[l] = mod->params[sprId(l)].getValue();
            initialized = true;
        }

        // ── Editor → params (UI thread, own params) ───────────────────────
        for (int l = 0; l < 6; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            mod->params[lenId(l)].setValue((float)lane.length);
            mod->params[offId(l)].setValue((float)lane.offset);
            mod->params[rotId(l)].setValue((float)lane.rotation);
        }

        // ── Per-lane base spread (REST/MEL/OCT only — the spreadable lanes).
        // LEG/ACC/VAR are mono-only and have no spread. spreadEffective[] is
        // written by processDNA from sprId base + cv*atten (per-lane CV mod).
        for (int l = 0; l < 3; ++l)
            paramMgr->setLaneSpread(l, mod->spreadEffective[l]);

        paramMgr->setInterpolationTarget(SpreadManager::AVERAGE_POLY);

        // ── Sync PatternEngine → display (uses SpreadManager.getInterpolatedValue) ──
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState);

        // ── Per-lane playhead + CV-applied display window ─────────────────
        // The editor lanes' EDIT L/O/R are the user's canvas values (written to
        // params above). For DISPLAY, surface the engine's CV-APPLIED effective
        // L/O/R so the highlighted window + offset/rotation markers track LOR CV
        // modulation. processDNA writes these per-lane into engine.{strand}Len/Off/
        // Rot. Editor lane order = REST,MEL,OCT,LEG,ACC,VAR. Display-only: editing
        // still uses the edit values (editStartBar/editEndBar). With no CV the
        // effective values equal the params, so the window is unchanged.
        // Editor lane index → engine strand follows the readStrand() mapping in
        // MonsoonSandsManager (NOT the naive name order): lane 0..5 =
        // rhythm, variation, legato, accent, melody, octave. Must stay in sync
        // with readStrand() or the displayed window maps to the wrong lane.
        auto& eng = monsoon->engine;
        const int effLen[6] = { eng.rhythmLen, eng.variationLen, eng.legatoLen,
                                eng.accentLen, eng.melodyLen,    eng.octaveLen };
        const int effOff[6] = { eng.rhythmOff, eng.variationOff, eng.legatoOff,
                                eng.accentOff, eng.melodyOff,    eng.octaveOff };
        const int effRot[6] = { eng.rhythmRot, eng.variationRot, eng.legatoRot,
                                eng.accentRot, eng.melodyRot,    eng.octaveRot };
        int globalStep = monsoon->engine.stepIndex;
        for (int l = 0; l < 6; ++l) {
            visualEditor->currentState.lanes[l].setDisplayLOR(effLen[l], effOff[l], effRot[l]);
            visualEditor->setLanePlayStep(l,
                calcPlayhead(globalStep, effLen[l], effOff[l], effRot[l]));
        }
    }
};

Model* modelMonsoonSandsVisualExpander =
    createModel<MonsoonSandsVisualExpander, MonsoonSandsVisualExpanderWidget>(
        "MonsoonSandsVisualExpander");
