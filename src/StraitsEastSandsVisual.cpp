#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "StraitsEastSandsVisual.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "dsp/managers/PolyVoiceSandsParameterManager.hpp"
#include "dsp/managers/SpreadManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsEastVisualIds;

extern Plugin* pluginInstance;

struct EastInterpItem : MenuItem {
    StraitsEastSandsVisual* mod;
    void onAction(const event::Action&) override { mod->interpUseMono = !mod->interpUseMono; }
    void step() override {
        rightText = mod->interpUseMono ? "Mono Draw ✓" : "Avg Poly ✓";
        MenuItem::step();
    }
};

struct StraitsEastSandsVisualWidget : ModuleWidget {
    SandsVisualEditorV4*            visualEditor = nullptr;
    TabButtonGroup*                 tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr     = nullptr;
    int  selectedVoice = 0;
    bool initialized   = false;
    // Theme follow-Monsoon: cache both panel SVGs + the panel widget so step()
    // can swap when the connected host's lightTheme changes.
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    rack::app::SvgPanel* panelWidget = nullptr;
    int lastThemeLight = -1;  // -1 = unset, forces first apply

    explicit StraitsEastSandsVisualWidget(StraitsEastSandsVisual* mod) {
        setModule(mod);
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_40HP.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_40HP_light.svg"));
        panelWidget = createPanel(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_40HP.svg"));
        setPanel(panelWidget);

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

        // ── 4 cols × 6 rows: jack1, jack2, atten1, atten2 ────────────────
        for (int r = 0; r < N_ROWS; ++r) {
            float y = rowY(r);
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_J1, y)), mod, cvId(r,0)));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_J2, y)), mod, cvId(r,1)));
            addParam(createParamCentered<Trimpot>(   mm2px(Vec(COL_A1, y)), mod, attenId(r,0)));
            addParam(createParamCentered<Trimpot>(   mm2px(Vec(COL_A2, y)), mod, attenId(r,1)));
        }

        // ── Per-lane SPREAD trimpots (selected voice): REST / MELODY / OCTAVE
        // Placed in a column to the right of the atten columns, one per lane,
        // vertically centred on each lane's two-row band.
        {
            float sx = SPREAD_X;
            for (int lane = 0; lane < 3; ++lane) {
                float y = 0.5f * (rowY(lane*2) + rowY(lane*2+1)); // centre of lane band
                int pid = (lane==0)?SPREAD_R : (lane==1)?SPREAD_M : SPREAD_O;
                addParam(createParamCentered<Trimpot>(mm2px(Vec(sx, y)), mod, pid));
            }
        }

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 15, 0);
    }

    ~StraitsEastSandsVisualWidget() override { delete paramMgr; }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* mod = dynamic_cast<StraitsEastSandsVisual*>(module);
        if (!mod) return;
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Spread interpolation"));
        auto* ii = createMenuItem<EastInterpItem>("Interpolation target");
        ii->mod = mod; menu->addChild(ii);
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
        selectedVoice = nv;
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);
        loadVoiceLOR(selectedVoice);
        loadVoiceSpread(selectedVoice);
    }

    Monsoon* getMonsoon() {
        return module ? findMonsoonEitherSide(module) : nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }

        // Follow the connected Monsoon's theme: swap panel SVG + editor colours
        // when it changes (and on first run). One toggle on Monsoon themes the
        // whole connected suite.
        int wantLight = monsoon->lightTheme ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            if (panelWidget) {
                panelWidget->setBackground(wantLight ? panelSvgLight : panelSvgDark);
            }
            if (visualEditor) visualEditor->setTheme(wantLight != 0);
        }

        auto* mod = static_cast<StraitsEastSandsVisual*>(module);
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

        // SpreadManager for editor display
        auto& smgr = paramMgr->spreadMgr;
        smgr.setSpread(selectedVoice, 0, mod->params[SPREAD_R].getValue());
        smgr.setSpread(selectedVoice, 1, mod->params[SPREAD_M].getValue());
        smgr.setSpread(selectedVoice, 2, mod->params[SPREAD_O].getValue());
        smgr.setInterpolationTarget(
            mod->interpUseMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // CV applied at control rate in Monsoon::process() — base + scaled offset, no mutation here.

        saveVoiceLOR(selectedVoice);
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        // Surface the engine's CV-APPLIED L/O/R to the editor's display window so
        // the highlighted range + offset/rotation markers visually track L/O/R CV
        // modulation. processDNA has already written these for selectedVoice into
        // engine.polyLen/Off/Rot[voice][lane] (lane 0/1/2 = REST/MEL/OCT). When no
        // CV is patched these equal the edit values, so the window is unchanged.
        // Editing/drag still acts on the edit values (editStartBar/editEndBar), so
        // this is display-only and non-destructive.
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
