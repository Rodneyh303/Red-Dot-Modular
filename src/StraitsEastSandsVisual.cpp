#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "StraitsEastSandsVisual.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ModArcOverlay.hpp"
#include "dsp/managers/PolyVoiceSandsParameterManager.hpp"
#include "dsp/managers/SpreadManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsEastVisualIds;

extern Plugin* pluginInstance;

struct StraitsEastSandsVisualWidget : ModuleWidget,
    dotModular::Compose<StraitsEastSandsVisualWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    SandsVisualEditorV4*            visualEditor = nullptr;
    TabButtonGroup*                 tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr     = nullptr;
    std::vector<rack::Widget*> blendControls;   // owner/send controls; greyed when no Macro
    int  selectedVoice = 0;
    // East spread mod-arcs. Compared in the INTERP domain (0..1) to sidestep the
    // pre-existing display-trimpot bipolar (-1..1) vs interp (0..1) mismatch: set
    // = the viewed voice's interp param (pre-CV), effective = the published
    // polySpreadEffective[viewedVoice][lane] (post per-voice/lane CV + combineSpread).
    std::vector<std::pair<rack::ParamWidget*, int>> pendingSpreadArcs;
    void flushSpreadArcs() {
        auto* mod = dynamic_cast<StraitsEastSandsVisual*>(module);
        for (auto& pr : pendingSpreadArcs) {
            auto* knob = pr.first; int lane = pr.second;
            if (!knob) continue;
            auto* arc = new redDot::ModArcOverlay();
            arc->box.pos  = knob->box.pos;
            arc->box.size = knob->box.size;
            arc->radius   = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            auto interpParamId = [this, lane]() -> int {
                int v = selectedVoice;
                return (lane==0) ? restInterpId(v) : (lane==1) ? melodyInterpId(v) : octaveInterpId(v);
            };
            arc->getSetNorm = [mod, interpParamId]() -> float {
                if (!mod) return 0.f;
                return rack::math::clamp(mod->params[interpParamId()].getValue(), 0.f, 1.f);
            };
            arc->getModNorm = [mod, this, lane]() -> float {
                if (!mod) return 0.f;
                int v = selectedVoice;
                if (v < 0 || v >= 15) return 0.f;
                return rack::math::clamp(mod->polySpreadEffective[v][lane], 0.f, 1.f);
            };
            arc->isActive = [mod, this, lane, interpParamId]() -> bool {
                if (!mod) return false;
                Monsoon* mon = findMonsoonEitherSide(mod);
                if (!mon || !mon->modVizEast) return false;
                int v = selectedVoice;
                if (v < 0 || v >= 15) return false;
                return std::fabs(mod->polySpreadEffective[v][lane] - mod->params[interpParamId()].getValue()) > 1e-4f;
            };
            addChild(arc);
        }
        pendingSpreadArcs.clear();
    }
    bool initialized   = false;
    // Theme follow-Monsoon: cache both panel SVGs + the panel widget so step()
    // can swap when the connected host's lightTheme changes.
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;  // -1 = unset, forces first apply

    explicit StraitsEastSandsVisualWidget(StraitsEastSandsVisual* mod) {
        setModule(mod);
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_40HP.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_40HP_light.svg"));
        loadPanel(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_40HP.svg"));

        redDot::addRedScrews(this);

        // Voice tabs (voices 2-16, i.e. 15 voices) — two rows to stay legible.
        // Row band sits just above the editor; uses the editor width.
        tabGroup = new TabButtonGroup(15, 2, 2,
                                      mm2px(ED_W), mm2px(10.f));
        tabGroup->box.pos = mm2px(Vec(ED_X, ED_Y - 12.f));
        addChild(tabGroup);

        // Visual editor
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ED_H));
        addChild(visualEditor);

        // ── Controls bound by id from the SVG kit (#components in
        //    gen_east_clean.py). Marker index == enum value:
        //      input_<n>  n = cvId(r,c)   = 0 + r*2 + c   (CV jacks, 0..11)
        //      param_<n>  n = attenId(r,c)= 3 + r*2 + c   (attenuverters, 3..14)
        //      param_<n>  n = SPREAD_R/M/O = 0/1/2         (selected-voice spread)
        for (int r = 0; r < N_ROWS; ++r) {
            bindInput<PJ301MPort>("input_" + std::to_string(cvId(r,0)), cvId(r,0));
            bindInput<PJ301MPort>("input_" + std::to_string(cvId(r,1)), cvId(r,1));
            bindParam<Trimpot>   ("param_" + std::to_string(attenId(r,0)), attenId(r,0));
            bindParam<Trimpot>   ("param_" + std::to_string(attenId(r,1)), attenId(r,1));
        }
        bindParam<Trimpot>("param_" + std::to_string((int)SPREAD_R), SPREAD_R,
            std::function<void(Trimpot*)>([this](Trimpot* k){ pendingSpreadArcs.push_back({k, 0}); }));
        bindParam<Trimpot>("param_" + std::to_string((int)SPREAD_M), SPREAD_M,
            std::function<void(Trimpot*)>([this](Trimpot* k){ pendingSpreadArcs.push_back({k, 1}); }));
        bindParam<Trimpot>("param_" + std::to_string((int)SPREAD_O), SPREAD_O,
            std::function<void(Trimpot*)>([this](Trimpot* k){ pendingSpreadArcs.push_back({k, 2}); }));

        // Macro/East blend controls (bound to the display proxies; copied to/from
        // the per-voice MACRO params on voice switch + each frame). Owner = a
        // latching on/off button (off=Macro owns base, on=East owns). Sends =
        // attenuverter trimpots. Captured so step() can grey/hide them when no
        // Macro visual is attached (they have no effect then — the equation's
        // macro terms are zero — so this is feedback, not function).
        for (int lane = 0; lane < 3; ++lane) {
            bindParam<VCVLatch>("param_owner_" + std::to_string(lane), ownerDispId(lane),
                std::function<void(VCVLatch*)>([this](VCVLatch* w){ blendControls.push_back(w); }));
            for (int item = 0; item < 4; ++item)
                bindParam<Trimpot>("param_send_" + std::to_string(lane) + "_" + std::to_string(item),
                    sendDispId(lane, item),
                    std::function<void(Trimpot*)>([this](Trimpot* w){ blendControls.push_back(w); }));
        }

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 15, 0);
        flushSpreadArcs();
    }

    ~StraitsEastSandsVisualWidget() override { delete paramMgr; }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* mod = dynamic_cast<StraitsEastSandsVisual*>(module);
        if (!mod) return;
        // Spread interpolation target moved to the Monsoon module context menu
        // (single source of truth — was duplicated here and on Macro).
    }

    void saveVoiceSpread(int v) {
        if (!module) return;
        module->params[restInterpId(v)  ].setValue(module->params[SPREAD_R].getValue());
        module->params[melodyInterpId(v)].setValue(module->params[SPREAD_M].getValue());
        module->params[octaveInterpId(v)].setValue(module->params[SPREAD_O].getValue());
    }
    void loadVoiceSpread(int v) {
        if (!module) return;
        module->params[SPREAD_R].setValue(module->params[restInterpId(v)  ].getValue());
        module->params[SPREAD_M].setValue(module->params[melodyInterpId(v)].getValue());
        module->params[SPREAD_O].setValue(module->params[octaveInterpId(v)].getValue());
    }
    // Owner/send display proxies ↔ per-voice MACRO_OWN/SEND params.
    void saveVoiceMacro(int v) {
        if (!module) return;
        for (int lane=0; lane<3; ++lane) {
            module->params[ownerId(v,lane)].setValue(module->params[ownerDispId(lane)].getValue());
            for (int item=0; item<4; ++item)
                module->params[sendId(v,lane,item)].setValue(module->params[sendDispId(lane,item)].getValue());
        }
    }
    void loadVoiceMacro(int v) {
        if (!module) return;
        for (int lane=0; lane<3; ++lane) {
            module->params[ownerDispId(lane)].setValue(module->params[ownerId(v,lane)].getValue());
            for (int item=0; item<4; ++item)
                module->params[sendDispId(lane,item)].setValue(module->params[sendId(v,lane,item)].getValue());
        }
    }
    void saveVoiceLOR(int v) {
        if (!module || !visualEditor) return;
        for (int l=0; l<3; ++l) {
            const auto& lane = visualEditor->currentState.lanes[l];
            module->params[lorId(v,l,0)].setValue((float)lane.length);
            module->params[lorId(v,l,1)].setValue((float)lane.offset);
            module->params[lorId(v,l,2)].setValue((float)lane.rotation);
        }
    }
    void loadVoiceLOR(int v) {
        if (!module || !visualEditor) return;
        for (int l=0; l<3; ++l) {
            auto& lane = visualEditor->currentState.lanes[l];
            lane.length   = std::max(1,(int)std::round(module->params[lorId(v,l,0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorId(v,l,1)].getValue());
            lane.rotation = (int)std::round(module->params[lorId(v,l,2)].getValue());
        }
    }
    void onVoiceTabChanged(int nv) {
        if (!paramMgr || !visualEditor) return;
        paramMgr->syncEditorToPatternEngine(selectedVoice, visualEditor->currentState);
        saveVoiceLOR(selectedVoice);
        saveVoiceSpread(selectedVoice);
        saveVoiceMacro(selectedVoice);
        selectedVoice = nv;
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
        loadVoiceLOR(selectedVoice);
        loadVoiceSpread(selectedVoice);
        loadVoiceMacro(selectedVoice);
    }

    Monsoon* getMonsoon() {
        return module ? findMonsoonEitherSide(module) : nullptr;
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }

        // Follow the connected Monsoon's theme: swap panel SVG + editor colours
        // when it changes (and on first run). One toggle on Monsoon themes the
        // whole connected suite.
        int wantLight = monsoon->lightTheme ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
            if (visualEditor) visualEditor->setTheme(wantLight != 0);
        }

        // Macro/East blend controls only do anything with a Macro visual attached
        // (the equation's macro terms are zero otherwise). Hide them when absent
        // so the panel doesn't imply controls that have no effect.
        bool macroPresent = (monsoon->expanderManager.cachedMacroSandsVisual != nullptr);
        for (Widget* w : blendControls) if (w) w->visible = macroPresent;

        auto* mod = static_cast<StraitsEastSandsVisual*>(module);

        // INERT until the Straits East CV expander is attached: it defines the
        // poly voice count, so without it there are no poly lanes to show. Show
        // the hint and skip all data work (no frozen bars).
        // INERT unless poly data actually exists: needs the Straits East CV
        // expander AND at least one poly voice (matches engine polyBaseActive =
        // cachedPolyVoiceExpander && numPolyVoices>=1). Without that there are no
        // poly lanes to show. (If you later want a lone single voice to also read
        // as inert because spread is degenerate, change >=1 to >=2 here and in
        // the Macro visual — left at >=1 to match the engine's poly gate.)
        if (monsoon->expanderManager.cachedPolyVoiceExpander == nullptr
            || monsoon->engine.numPolyVoices < 1) {
            visualEditor->inert = true;
            visualEditor->inertMessage = "Attach Straits East expander";
            visualEditor->clearPlaySteps();
            return;
        }
        visualEditor->inert = false;
        PatternEngine*   pe = &monsoon->engine.pe;
        SequencerEngine* se = &monsoon->engine;
        if (paramMgr->patternEngine != pe) {
            paramMgr->patternEngine             = pe;
            paramMgr->sequencerEngine           = se;
            paramMgr->spreadMgr.patternEngine   = pe;
            paramMgr->spreadMgr.sequencerEngine = se;
        }

        // Grey out voice tabs beyond the active poly count (numPolyVoices).
        if (tabGroup) tabGroup->setActiveCount(se->numPolyVoices);

        if (!initialized) {
            loadVoiceLOR(selectedVoice);
            loadVoiceSpread(selectedVoice);
            initialized = true;
        }

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        // Write display trimpots → selected voice INTERP params
        saveVoiceSpread(selectedVoice);
        // Write owner/send display proxies → selected voice's per-voice MACRO
        // params each frame, so the blend equation sees edits immediately (not
        // only on voice switch).
        saveVoiceMacro(selectedVoice);

        // SpreadManager for editor display
        auto& smgr = paramMgr->spreadMgr;
        smgr.setSpread(selectedVoice, 0, mod->params[SPREAD_R].getValue());
        smgr.setSpread(selectedVoice, 1, mod->params[SPREAD_M].getValue());
        smgr.setSpread(selectedVoice, 2, mod->params[SPREAD_O].getValue());
        smgr.setInterpolationTarget(
            monsoon->spreadInterpMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // CV applied at control rate in Monsoon::process() — base + scaled offset, no mutation here.

        saveVoiceLOR(selectedVoice);
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        // Surface the engine's CV-APPLIED L/O/R to the display window so the
        // highlighted range + offset/rotation markers track L/O/R CV modulation.
        // engine.polyLen/Off/Rot[voice][lane] (lane 0/1/2 = REST/MEL/OCT) hold the
        // post-CV values. With no CV these equal the edit values. Editing/drag use
        // the EDIT values, so this is display-only.
        int gs = monsoon->engine.stepIndex;
        auto& eng = monsoon->engine;
        const int vi = selectedVoice;
        for (int l=0; l<3; ++l) {
            int cvLen = eng.polyLen[vi][l];
            int cvOff = eng.polyOff[vi][l];
            int cvRot = eng.polyRot[vi][l];
            visualEditor->currentState.lanes[l].setDisplayLOR(cvLen, cvOff, cvRot);
            visualEditor->setLanePlayStep(l, calcPlayhead(gs, cvLen, cvOff, cvRot));
        }
    }
};

Model* modelStraitsEastSandsVisual =
    createModel<StraitsEastSandsVisual,StraitsEastSandsVisualWidget>(
        "StraitsEastSandsVisual");
