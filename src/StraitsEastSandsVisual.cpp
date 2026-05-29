#include <rack.hpp>
#include "Monsoon.hpp"
#include "StraitsEastSandsVisual.hpp"
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "managers/PolyVoiceSandsParameterManager.hpp"
#include "managers/SpreadManager.hpp"

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsEastVisualIds;

extern Plugin* pluginInstance;

// ── Context menu items ─────────────────────────────────────────────────────
struct EastInterpItem : MenuItem {
    StraitsEastSandsVisual* mod;
    void onAction(const event::Action&) override { mod->interpUseMono = !mod->interpUseMono; }
    void step() override {
        rightText = mod->interpUseMono ? "Mono Draw ✓" : "Avg Poly ✓";
        MenuItem::step();
    }
};
struct EastLorVoiceItem : MenuItem {
    StraitsEastSandsVisual* mod; int v;
    void onAction(const event::Action&) override { mod->cvLorVoiceMask    ^= (1<<v); }
    void step() override { rightText=(mod->cvLorVoiceMask    &(1<<v))?"✓":""; MenuItem::step(); }
};
struct EastSprVoiceItem : MenuItem {
    StraitsEastSandsVisual* mod; int v;
    void onAction(const event::Action&) override { mod->cvSpreadVoiceMask ^= (1<<v); }
    void step() override { rightText=(mod->cvSpreadVoiceMask &(1<<v))?"✓":""; MenuItem::step(); }
};

// ── Widget ─────────────────────────────────────────────────────────────────
struct StraitsEastSandsVisualWidget : ModuleWidget {
    SandsVisualEditorV4*            visualEditor = nullptr;
    TabButtonGroup*                 tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr     = nullptr;
    int  selectedVoice = 0;
    bool initialized   = false;

    // Row y-positions (mm): 9 rows evenly spaced y=14..122.5
    static constexpr float ROW_TOP = 14.f;
    static constexpr float ROW_BOT = 122.5f;
    static float rowY(int r) {
        return ROW_TOP + (r + 0.5f) * (ROW_BOT - ROW_TOP) / 9.f;
    }

    // Column x-positions (mm): interchange-style outer=jacks, inner=attens
    static constexpr float COL_JACK_LOR = 10.f;   // outer left  — LOR jacks
    static constexpr float COL_ATTEN_LOR = 22.f;  // inner left  — LOR attens
    static constexpr float COL_ATTEN_SPR = 34.f;  // inner right — Spread attens
    static constexpr float COL_JACK_SPR  = 46.f;  // outer right — Spread jacks

    // Editor: starts after left section gap
    static constexpr float ED_X    = 54.f;
    static constexpr float ED_W    = W_MM - ED_X - 4.f;  // ~124.9mm
    static constexpr float TAB_Y   = 8.f;
    static constexpr float ED_Y    = 18.f;
    static constexpr float ED_H    = ROW_BOT - ED_Y;     // ~104.5mm

    explicit StraitsEastSandsVisualWidget(StraitsEastSandsVisual* mod) {
        setModule(mod);
        setPanel(APP->window->loadSvg(
            asset::plugin(pluginInstance,
                "res/panels/StraitsEastSandsVisual_36HP.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH, RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

        // Voice tabs (voices 2-8)
        tabGroup = new TabButtonGroup(7, 2, mm2px(TAB_Y+7.f), mm2px(8.f), mm2px(1.f));
        tabGroup->box.pos = mm2px(Vec(ED_X, TAB_Y));
        tabGroup->box.size.x = mm2px(ED_W);
        addChild(tabGroup);

        // Visual editor
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ED_H));
        addChild(visualEditor);

        // ── 9 LOR jacks (outer left) + 9 LOR attenuverters (inner left) ──────
        // ── 9 Spread attenuverters (inner right) + 9 Spread jacks (outer right)
        for (int r = 0; r < 9; ++r) {
            float y = rowY(r);
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(COL_JACK_LOR,  y)), mod, CV_LOR_0+r));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(COL_ATTEN_LOR, y)), mod, ATTEN_LOR_0+r));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(COL_ATTEN_SPR, y)), mod, ATTEN_SPR_0+r));
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(COL_JACK_SPR,  y)), mod, CV_SPR_0+r));
        }

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 7, 0);
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

        static const char* vn[7]={"V2","V3","V4","V5","V6","V7","V8"};

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("LOR CV — voice opt-out"));
        for (int v=0; v<7; ++v) {
            auto* vi = createMenuItem<EastLorVoiceItem>(vn[v]);
            vi->mod=mod; vi->v=v; menu->addChild(vi);
        }

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Spread CV — voice opt-out"));
        for (int v=0; v<7; ++v) {
            auto* si = createMenuItem<EastSprVoiceItem>(vn[v]);
            si->mod=mod; si->v=v; menu->addChild(si);
        }
    }

    // ── Spread voice memory ────────────────────────────────────────────────
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

    // ── LOR voice memory ───────────────────────────────────────────────────
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
        return module ? findMonsoon(module->rightExpander.module) : nullptr;
    }

    void step() override {
        ModuleWidget::step();
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) return;

        auto* mod = static_cast<StraitsEastSandsVisual*>(module);
        PatternEngine*   pe = &monsoon->engine.pe;
        SequencerEngine* se = &monsoon->engine;
        if (paramMgr->patternEngine != pe) {
            paramMgr->patternEngine             = pe;
            paramMgr->sequencerEngine           = se;
            paramMgr->spreadMgr.patternEngine   = pe;
            paramMgr->spreadMgr.sequencerEngine = se;
        }

        if (!initialized) {
            loadVoiceLOR(selectedVoice);
            loadVoiceSpread(selectedVoice);
            initialized = true;
        }

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        // ── Write display trimpots → selected voice INTERP params ─────────────
        saveVoiceSpread(selectedVoice);

        // ── SpreadManager for editor display ──────────────────────────────────
        auto& smgr = paramMgr->spreadMgr;
        smgr.setSpread(selectedVoice, 0, mod->params[SPREAD_R].getValue());
        smgr.setSpread(selectedVoice, 1, mod->params[SPREAD_M].getValue());
        smgr.setSpread(selectedVoice, 2, mod->params[SPREAD_O].getValue());
        smgr.setInterpolationTarget(
            mod->interpUseMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // ── Apply LOR CV (poly, channels 0-6 for East voices 2-8) ─────────────
        for (int row = 0; row < 9; ++row) {
            auto& inp = mod->inputs[CV_LOR_0 + row];
            if (!inp.isConnected()) continue;
            float atten = mod->params[ATTEN_LOR_0 + row].getValue();
            for (int v = 0; v < 7; ++v) {
                if (!(mod->cvLorVoiceMask & (1<<v))) continue;
                float cv = inp.getVoltage(v) / 10.f * atten;
                int pid  = rowLorId(v, row);
                float lo = (row%3 == 0) ? 1.f : 0.f;
                float hi = (row%3 == 0) ? 16.f : 15.f;
                mod->params[pid].setValue(clamp(mod->params[pid].getValue()+cv*hi, lo, hi));
            }
        }

        // ── Apply Spread CV (poly, channels 0-6, grouped by lane) ────────────
        for (int row = 0; row < 9; ++row) {
            auto& inp = mod->inputs[CV_SPR_0 + row];
            if (!inp.isConnected()) continue;
            float atten = mod->params[ATTEN_SPR_0 + row].getValue();
            for (int v = 0; v < 7; ++v) {
                if (!(mod->cvSpreadVoiceMask & (1<<v))) continue;
                float cv  = inp.getVoltage(v) / 10.f * atten;
                int   pid = rowInterpId(v, row);
                mod->params[pid].setValue(clamp(mod->params[pid].getValue()+cv, 0.f, 1.f));
            }
        }

        saveVoiceLOR(selectedVoice);
        paramMgr->syncPatternEngineToEditor(selectedVoice, visualEditor->currentState);

        int gs = monsoon->engine.stepIndex;
        for (int l=0; l<3; ++l) {
            visualEditor->setLanePlayStep(l,
                calcPlayhead(gs,
                    readLenParam   (mod, lorId(selectedVoice,l,0)),
                    readOffRotParam(mod, lorId(selectedVoice,l,1)),
                    readOffRotParam(mod, lorId(selectedVoice,l,2))));
        }
    }
};

Model* modelStraitsEastSandsVisual =
    createModel<StraitsEastSandsVisual,StraitsEastSandsVisualWidget>(
        "StraitsEastSandsVisual");
