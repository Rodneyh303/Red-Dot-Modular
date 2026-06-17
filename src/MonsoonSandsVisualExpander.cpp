#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
//#include "MonsoonSandsExpander.hpp"
#include "MonsoonSandsVisualExpander.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
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
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    // Spread mod-arcs (bipolar -1..1), same as Macro: set = SPR param,
    // effective = mod->spreadEffective[lane] (CV-modulated). Normalised (v+1)/2.
    std::vector<std::pair<rack::ParamWidget*, int>> pendingSpreadArcs;
    void flushSpreadArcs(MonsoonSandsVisualExpander* mod) {
        for (auto& pr : pendingSpreadArcs) {
            auto* knob = pr.first; int lane = pr.second;
            if (!knob) continue;
            auto* arc = new redDot::ModArcOverlay();
            // Pad the overlay box so the arc stays inside it (no redraw trail —
            // bug #2). Knob occupies [pad, box-pad]; radial centre = box/2.
            const float arcPad = mm2px(2.2f);
            arc->box.pos  = knob->box.pos.minus(rack::math::Vec(arcPad, arcPad));
            arc->box.size = knob->box.size.plus(rack::math::Vec(arcPad*2.f, arcPad*2.f));
            arc->pad = arcPad;
            arc->radius   = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            MonsoonSandsVisualExpander* mm = mod;
            int pid = knob->paramId;
            arc->getSetNorm = [mm, pid]() -> float {
                if (!mm) return 0.5f;
                auto* pq = mm->paramQuantities[pid];
                return pq ? (float)pq->getScaledValue() : 0.5f;
            };
            arc->getModNorm = [mm, lane]() -> float {
                if (!mm || lane < 0 || lane >= 6) return 0.5f;
                return rack::math::clamp((mm->spreadEffective[lane] + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->isActive = [mm, lane, pid]() -> bool {
                if (!mm || lane < 0 || lane >= 6) return false;
                Monsoon* mon = findMonsoonEitherSide(mm);
                if (!mon || !mon->modVizMono) return false;
                return std::fabs(mm->spreadEffective[lane] - mm->params[pid].getValue()) > 1e-4f;
            };
            addChild(arc);
        }
        pendingSpreadArcs.clear();
    }

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
            auto* sp = createParamCentered<Trimpot>(mm2px(Vec(SPR_BASE_X, y)), mod, sprId(l));
            addParam(sp);
            pendingSpreadArcs.push_back({sp, l});
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(SPR_CV_X, y)), mod, sprCvId(l)));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(SPR_ATTEN_X, y)), mod, sprAttenId(l)));
        }

        paramMgr = new MonoSandsParameterManager();
        flushSpreadArcs(mod);

        // dot.modular connect mark (brand mark; greyed when no Monsoon attached).
        {
            auto* w = new redDot::ConnectMark();
            w->box.size = mm2px(rack::math::Vec(8.f, 8.f));
            w->box.pos  = mm2px(rack::math::Vec(W_MM * 0.5f, 124.f)).minus(w->box.size.div(2));
            w->connected  = [this]() { return module && redDot::findMonsoonEitherSide(module) != nullptr; };
            w->lightTheme = [this]() { Monsoon* mm = module ? redDot::findMonsoonEitherSide(module) : nullptr; return mm && mm->lightTheme; };
            connectMark = w;
            addChild(w);
        }
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
        // Editor lane index → engine strand via the single source of truth
        // (dotModular::MONO_LANE_TO_STRAND in dsp/LaneMapping.hpp) + the engine's
        // indexable strand accessors. No hardcoded permutation here, so this can
        // never drift from readStrand() again.
        auto& eng = monsoon->engine;
        int effLen[6], effOff[6], effRot[6];
        for (int l = 0; l < 6; ++l) {
            int strand = dotModular::MONO_LANE_TO_STRAND[l];
            effLen[l] = eng.strandLen(strand);
            effOff[l] = eng.strandOff(strand);
            effRot[l] = eng.strandRot(strand);
        }
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
