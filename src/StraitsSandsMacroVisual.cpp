#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
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

// struct MacroInterpItem : MenuItem {
    

   
//     // void onAction(const event::Action&) override { mod->interpUseMono = !mod->interpUseMono; }
//     // void step() override {
//     //     rightText = mod->interpUseMono ? "Mono Draw ✓" : "Avg Poly ✓";
//     //     MenuItem::step();
//     // }
// };
//StraitsSandsMacroVisual* mod;

 // Spread mod-arcs (bipolar -1..1). Queued during construction, attached after
    // all controls (z-order). Effective spread = mod->spreadEffective[lane] (the
    // CV-modulated value); set = the SPREAD_* param. Both normalised (v+1)/2.
   

struct StraitsSandsMacroVisualWidget : ModuleWidget {
    SandsVisualEditorV4*       visualEditor = nullptr;
    PolySandsParameterManager* paramMgr     = nullptr;
    TabButtonGroup*            tabGroup     = nullptr;
    int viewVoice = 0;   // which voice's resulting probabilities to DISPLAY (read-only)
    bool                       initialized  = false;
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    rack::app::SvgPanel* panelWidget = nullptr;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

 std::vector<std::pair<rack::ParamWidget*, int>> pendingSpreadArcs;
    void flushSpreadArcs() {
        auto* mod = dynamic_cast<StraitsSandsMacroVisual*>(module);
        for (auto& pr : pendingSpreadArcs) {
            auto* knob = pr.first; int lane = pr.second;
            if (!knob) continue;
            auto* arc = new redDot::ModArcOverlay();
            arc->radius   = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            arc->attachOverKnob(knob, mm2px(2.5f));
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
                Monsoon* mon = findMonsoonEitherSide(mm);
                if (!mon || !mon->modVizMacro) return false;
                float setV = mm->params[pid].getValue();           // -1..1
                return std::fabs(mm->spreadEffective[lane] - setV) > 1e-4f;
            };
            addChild(arc);
        }
        pendingSpreadArcs.clear();
    }

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

        // 3 poly probability CV outs, one per lane (aligned to editor lane centers).
        for (int l = 0; l < 3; ++l) {
            float y = ED_Y + (l + 0.5f) * ED_LANE_H;
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(PROB_OUT_X, y)), module, StraitsMacroVisualIds::PROB_OUT_REST + l));
        }

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
            pendingSpreadArcs.push_back({sp, lane});
        }

        paramMgr = new PolySandsParameterManager(nullptr, nullptr, nullptr, 7);
        flushSpreadArcs();   // attach spread mod-arcs on top of the trimpots

        // dot.modular connect mark (brand mark; greyed when no Monsoon attached).
        {
            connectMark = redDot::makeConnectMark(module, mm2px(rack::math::Vec(W_MM * 0.5f, 124.f)), mm2px(8.f));
            addChild(connectMark);
        }
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

        // Surface Macro's OWN CV-applied L/O/R to the display window. Previously
        // this read the engine output (eng.polyLen[0] etc.), but when East owns a
        // lane the engine value is East's, so the Macro panel showed East's LOR —
        // a category error (the panel should represent the MACRO module's own
        // state, regardless of who owns the lane downstream). processDNA publishes
        // Macro's own base + CV-only delta per lane/item (0=LEN 1=OFF 2=ROT), which
        // is exactly Macro's own CV-applied value independent of ownership.
        // Display-only. (Lane base rings already come from Macro's own params via
        // loadLOR; this overlay now matches them.)
        int gs = monsoon->engine.stepIndex;
        visualEditor->setPlayDir(monsoon->engine.lastPlayDir);   // direction cue (Mode E reverse)
        for (int l = 0; l < 3; ++l) {
            int ownLen = (int)std::lround(mod->macroBase[l][0] + mod->macroCVDelta[l][0]);
            int ownOff = (int)std::lround(mod->macroBase[l][1] + mod->macroCVDelta[l][1]);
            int ownRot = (int)std::lround(mod->macroBase[l][2] + mod->macroCVDelta[l][2]);
            ownLen = std::max(1, ownLen);
            visualEditor->currentState.lanes[l].setDisplayLOR(ownLen, ownOff, ownRot);
            visualEditor->setLanePlayStep(l, calcPlayhead(gs, ownLen, ownOff, ownRot));
        }
    }
};

// ── Module process(): 3 poly probability CV outs (ch1 reserved, voices ch2+) ──
void StraitsSandsMacroVisual::process(const ProcessArgs&) {
    using namespace StraitsMacroVisualIds;
    Monsoon* mon = redDot::findMonsoonEitherSide(this);
    if (!mon) {
        for (int l = 0; l < 3; ++l) { outputs[PROB_OUT_REST + l].setChannels(1);
                                      outputs[PROB_OUT_REST + l].setVoltage(0.f); }
        return;
    }
    const float scaleV = (mon->probOutScale == 0) ? 1.f : (mon->probOutScale == 1) ? 5.f : 10.f;
    const bool sh = mon->probOutSampleHold;
    auto& eng = mon->engine;
    const int nV = eng.numPolyVoices;
    const int nCh = 1 + nV;
    const int gs = eng.stepIndex;
    for (int l = 0; l < 3; ++l) {
        auto& out = outputs[PROB_OUT_REST + l];
        out.setChannels(nCh < 1 ? 1 : nCh);
        out.setVoltage(0.f, 0);              // ch1 reserved (future mono tab)
        // Macro's OWN global LOR step for this lane (from macroBase+CVDelta — identical
        // to Macro's editor playhead, independent of East/ownership). Same step for
        // every voice (Macro's view is global); each voice contributes its own draw.
        int ownLen = std::max(1, (int)std::lround(macroBase[l][0] + macroCVDelta[l][0]));
        int ownOff = (int)std::lround(macroBase[l][1] + macroCVDelta[l][1]);
        int ownRot = (int)std::lround(macroBase[l][2] + macroCVDelta[l][2]);
        int step = calcPlayhead(gs, ownLen, ownOff, ownRot) & 0x0F;
        for (int v = 0; v < nV; ++v) {
            int ch = v + 1;
            float raw = eng.polyLaneProbabilityAtStep(l, v, step);
            float val;
            if (sh) {
                if (step != probLastStep[l][ch]) { probHeld[l][ch] = raw; probLastStep[l][ch] = step; }
                val = probHeld[l][ch];
            } else val = raw;
            out.setVoltage(rack::math::clamp(val, 0.f, 1.f) * scaleV, ch);
        }
    }
}

Model* modelStraitsSandsMacroVisual =
    createModel<StraitsSandsMacroVisual, StraitsSandsMacroVisualWidget>(
        "StraitsSandsMacroVisual");
