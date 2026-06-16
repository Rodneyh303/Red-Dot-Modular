#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
//#include "MonsoonStraitsSands.hpp"
#include "StraitsSandsMacroVisual.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
#include "dsp/managers/PolySandsParameterManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsMacroVisualIds;

extern Plugin* pluginInstance;

struct MacroInterpItem : MenuItem {
    StraitsSandsMacroVisual* mod;

    // Spread mod-arcs (bipolar -1..1). Queued during construction, attached after
    // all controls (z-order). Effective spread = mod->spreadEffective[lane] (the
    // CV-modulated value); set = the SPREAD_* param. Both normalised (v+1)/2.
    std::vector<std::pair<rack::ParamWidget*, int>> pendingSpreadArcs;
    void flushSpreadArcs() {
        for (auto& pr : pendingSpreadArcs) {
            auto* knob = pr.first; int lane = pr.second;
            if (!knob) continue;
            auto* arc = new redDot::ModArcOverlay();
            arc->box.pos  = knob->box.pos;
            arc->box.size = knob->box.size;
            arc->radius   = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            StraitsSandsMacroVisual* mm = mod;
            int pid = knob->paramId;
            arc->getSetNorm = [mm, pid]() -> float {
                if (!mm) return 0.5f;
                auto* pq = mm->paramQuantities[pid];
                return pq ? (float)pq->getScaledValue() : 0.5f;   // bipolar → 0..1
            };
            arc->getModNorm = [mm, lane]() -> float {
                if (!mm || lane < 0 || lane >= 3) return 0.5f;
                return rack::math::clamp((mm->spreadEffective[lane] + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->isActive = [mm, lane, pid]() -> bool {
                if (!mm || lane < 0 || lane >= 3) return false;
                float setV = mm->params[pid].getValue();           // -1..1
                return std::fabs(mm->spreadEffective[lane] - setV) > 1e-4f;
            };
            addChild(arc);
        }
        pendingSpreadArcs.clear();
    }
    // void onAction(const event::Action&) override { mod->interpUseMono = !mod->interpUseMono; }
    // void step() override {
    //     rightText = mod->interpUseMono ? "Mono Draw ✓" : "Avg Poly ✓";
    //     MenuItem::step();
    // }
};

struct StraitsSandsMacroVisualWidget : ModuleWidget {
    SandsVisualEditorV4*       visualEditor = nullptr;
    PolySandsParameterManager* paramMgr     = nullptr;
    TabButtonGroup*            tabGroup     = nullptr;
    int viewVoice = 0;   // which voice's resulting probabilities to DISPLAY (read-only)
    bool                       initialized  = false;
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    rack::app::SvgPanel* panelWidget = nullptr;
    int lastThemeLight = -1;

    explicit StraitsSandsMacroVisualWidget(StraitsSandsMacroVisual* mod) {
        setModule(mod);
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsSandsMacroVisual_40HP.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsSandsMacroVisual_40HP_light.svg"));
        panelWidget = createPanel(asset::plugin(pluginInstance,
                            "res/panels/StraitsSandsMacroVisual_40HP.svg"));
        setPanel(panelWidget);

        redDot::addRedScrews(this);

        // Visual editor — right section, 3 lanes (REST/MEL/OCT), global
        // Voice VIEW tabs (voices 2-16). Macro has no per-voice editing — these
        // let the user flip through voices to SEE each one's resulting (spread/
        // blend-applied) probabilities. Read-only: changing tab only changes
        // which voice is displayed, nothing is saved per voice.
        tabGroup = new TabButtonGroup(15, 2, 2, mm2px(ED_W), mm2px(10.f));
        tabGroup->box.pos = mm2px(Vec(ED_X, ED_Y - 12.f));
        addChild(tabGroup);

        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ED_H));
        addChild(visualEditor);

        // ── Left section: 4 cols × 6 rows ─────────────────────────────────
        // j1, j2 = mono CV jacks   a1, a2 = attenuverters
        // Row r, col c:  lane=r/2, param=(r%2)*2+c
        for (int r = 0; r < N_ROWS; ++r) {
            float y = rowY(r);
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_J1, y)), mod, cvId(r,0)));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_J2, y)), mod, cvId(r,1)));
            addParam(createParamCentered<Trimpot>(   mm2px(Vec(COL_A1, y)), mod, attenId(r,0)));
            addParam(createParamCentered<Trimpot>(   mm2px(Vec(COL_A2, y)), mod, attenId(r,1)));
        }

        // ── Per-lane global SPREAD trimpots (REST / MELODY / OCTAVE) ─────────
        // Mirrors the East visual: one trimpot per lane, vertically centred on
        // each lane's two-row band, in a column between the attenuverters and
        // the visual editor. The module already has these params (SPREAD_REST/
        // MELODY/OCTAVE); they were just never placed on the panel.
        for (int lane = 0; lane < 3; ++lane) {
            float y = 0.5f * (rowY(lane*2) + rowY(lane*2+1));  // centre of lane band
            int pid = (lane==0) ? SPREAD_REST : (lane==1) ? SPREAD_MELODY : SPREAD_OCTAVE;
            auto* sp = createParamCentered<Trimpot>(mm2px(Vec(SPREAD_X, y)), mod, pid);
            addParam(sp);
           // pendingSpreadArcs.push_back({sp, lane});
        }

        paramMgr = new PolySandsParameterManager(nullptr, nullptr, nullptr, 7);
        //flushSpreadArcs();   // attach spread mod-arcs on top of the trimpots
    }

    ~StraitsSandsMacroVisualWidget() override { delete paramMgr; }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* mod = dynamic_cast<StraitsSandsMacroVisual*>(module);
        if (!mod) return;
        // Spread interpolation target moved to the Monsoon module context menu.
    }

    Monsoon* getMonsoon() {
        return module ? findMonsoonEitherSide(module) : nullptr;
    }

    void saveLOR() {
        if (!module || !visualEditor) return;
        for (int l = 0; l < 3; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            module->params[lorId(l,0)].setValue((float)lane.length);
            module->params[lorId(l,1)].setValue((float)lane.offset);
            module->params[lorId(l,2)].setValue((float)lane.rotation);
        }
    }
    void loadLOR() {
        if (!module || !visualEditor) return;
        for (int l = 0; l < 3; ++l) {
            auto& lane = visualEditor->currentState.lanes[l];
            lane.length   = std::max(1,(int)std::round(module->params[lorId(l,0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorId(l,1)].getValue());
            lane.rotation = (int)std::round(module->params[lorId(l,2)].getValue());
        }
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

        PatternEngine*   pe = &monsoon->engine.pe;
        SequencerEngine* se = &monsoon->engine;

        // INERT until the Straits East CV expander is attached (defines poly voice
        // count; without it there are no poly lanes to show). Show the hint, skip
        // all data work.
        // INERT unless poly data exists (expander + >=1 poly voice; matches the
        // engine's polyBaseActive). See the East visual note re: >=1 vs >=2.
        if (monsoon->expanderManager.cachedPolyVoiceExpander == nullptr
            || monsoon->engine.numPolyVoices < 1) {
            visualEditor->inert = true;
            visualEditor->inertMessage = "Attach Straits East expander";
            visualEditor->clearPlaySteps();
            return;
        }
        visualEditor->inert = false;

        if (paramMgr->patternEngine != pe) {
            paramMgr->patternEngine             = pe;
            paramMgr->sequencerEngine           = se;
            paramMgr->spreadMgr.patternEngine   = pe;
            paramMgr->spreadMgr.sequencerEngine = se;
        }

        if (!initialized) { loadLOR(); initialized = true; }

        auto* mod = static_cast<StraitsSandsMacroVisual*>(module);

        // Spread: processDNA has already applied CV offset to SPREAD_REST/MEL/OCT params.
        // Read effective values here for SpreadManager display.
        paramMgr->spreadMgr.setSpread(0, mod->spreadEffective[0]);
        paramMgr->spreadMgr.setSpread(1, mod->spreadEffective[1]);
        paramMgr->spreadMgr.setSpread(2, mod->spreadEffective[2]);
        paramMgr->spreadMgr.setInterpolationTarget(
            monsoon->spreadInterpMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // CV applied at control rate in Monsoon::process() — base + cv*atten*scale.

        // Which voice to DISPLAY (read-only view lens). Clamp to active count.
        if (tabGroup) {
            tabGroup->setActiveCount(monsoon->engine.numPolyVoices);
            viewVoice = std::min(tabGroup->getSelectedTab(),
                                 std::max(0, monsoon->engine.numPolyVoices - 1));
        }

        saveLOR();
        paramMgr->syncPatternEngineToEditor(visualEditor->currentState, viewVoice);

        // Surface CV-APPLIED global L/O/R to the display window (Macro applies the
        // same lane L/O/R to every voice — voice 0 is representative). Display-only.
        int gs = monsoon->engine.stepIndex;
        auto& eng = monsoon->engine;
        for (int l = 0; l < 3; ++l) {
            int cvLen = eng.polyLen[0][l];
            int cvOff = eng.polyOff[0][l];
            int cvRot = eng.polyRot[0][l];
            visualEditor->currentState.lanes[l].setDisplayLOR(cvLen, cvOff, cvRot);
            visualEditor->setLanePlayStep(l, calcPlayhead(gs, cvLen, cvOff, cvRot));
        }
    }
};

Model* modelStraitsSandsMacroVisual =
    createModel<StraitsSandsMacroVisual, StraitsSandsMacroVisualWidget>(
        "StraitsSandsMacroVisual");
