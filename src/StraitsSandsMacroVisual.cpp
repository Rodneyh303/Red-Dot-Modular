#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/GoldPolyPort.hpp"
//#include "MonsoonStraitsSands.hpp"
#include "StraitsSandsMacroVisual.hpp"
#include "MonsoonSandsVisualExpander.hpp"  // complete mono type + SandsMonoVisualIds for the tab-1 mono mirror
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
#include "dsp/managers/PolySandsParameterManager.hpp"
#include "dsp/VoiceResolver.hpp"   // uniform 16-voice addressing for prob-out

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
    int lastSendVoice = -1;  // detect view-voice change to sync the mix-in send proxies
    bool                       initialized  = false;
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    rack::app::SvgPanel* panelWidget = nullptr;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

 std::vector<std::pair<rack::ParamWidget*, int>> pendingSpreadArcs;
 std::vector<rack::Widget*> leftAttenuverters;  // 12 CV-depth knobs; hidden on tab-1 when mono attached
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
                // Gate on the spread CV jack actually being connected — NOT a
                // set-vs-effective delta, which races during a manual knob turn
                // (control-rate spreadEffective lags the live param → red residue arc;
                // same desync as the Monsoon big-5 fix).
                return mm->inputs[macroCvId(lane, 3)].isConnected();
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
        tabGroup = new TabButtonGroup(16, 1, 2, mm2px(ED_W), mm2px(10.f));   // V1 mono + V2..V16 poly
        tabGroup->box.pos = mm2px(Vec(ED_X, ED_Y - 12.f));
        addChild(tabGroup);

        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ED_H));
        addChild(visualEditor);

        // 3 poly probability CV outs, one per lane (aligned to editor lane centers).
        for (int l = 0; l < 3; ++l) {
            float y = ED_Y + (l + 0.5f) * ED_LANE_H;
            auto* p = createOutputCentered<redDot::GoldPolyPort>(
                mm2px(Vec(PROB_OUT_X, y)), module, StraitsMacroVisualIds::PROB_OUT_REST + l);
            Module* mod = module;
            p->lightTheme = [mod]() { Monsoon* m = mod ? redDot::findMonsoonEitherSide(mod) : nullptr;
                                      return m && m->lightTheme; };
            addOutput(p);
        }

        // ── Left section: 4 cols × 6 rows ─────────────────────────────────
        // j1, j2 = mono CV jacks   a1, a2 = attenuverters
        // Row r, col c:  lane=r/2, param=(r%2)*2+c
        for (int r = 0; r < N_ROWS; ++r) {
            float y = rowY(r);
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_J1, y)), mod, cvId(r,0)));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL_J2, y)), mod, cvId(r,1)));
            auto* a1 = createParamCentered<Trimpot>(   mm2px(Vec(COL_A1, y)), mod, attenId(r,0));
            auto* a2 = createParamCentered<Trimpot>(   mm2px(Vec(COL_A2, y)), mod, attenId(r,1));
            addParam(a1); addParam(a2);
            leftAttenuverters.push_back(a1); leftAttenuverters.push_back(a2);
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

        // Macro→voice MIX-IN send 2×2 grids (relocated from East). Coordinates MUST
        // match panel_src/gen_macro_mono.py: BLEND_TOP=72 SEND_Y0=12 SEND_DY=11 SEND_DX=7,
        // groups at ED_X + lane*ED_W/3. Bound to the sendDispId display proxies (synced
        // to/from the per-voice store on view-voice change).
        {
            const float BLEND_TOP=72.f, SEND_Y0=12.f, SEND_DY=11.f, SEND_DX=7.f, BGAP=3.5f;
            const float GROUP_W = ED_W/3.f;
            for (int lane=0; lane<3; ++lane) {
                float gx=ED_X+lane*GROUP_W+BGAP*0.5f, gw=GROUP_W-BGAP, gcx=gx+gw*0.5f;
                for (int item=0; item<4; ++item) {
                    float cxs=gcx+((item%2)==0?-SEND_DX:SEND_DX);
                    float cys=BLEND_TOP+SEND_Y0+(item/2)*SEND_DY;
                    addParam(createParamCentered<Trimpot>(mm2px(Vec(cxs,cys)), mod, sendDispId(lane,item)));
                }
            }
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

        // Which voice to DISPLAY (read-only view lens). Tab 0 = mono (V1, always
        // active); tabs 1..15 = poly voices V2..V16. Active = mono + active poly count.
        if (tabGroup) {
            tabGroup->setActiveCount(monsoon->engine.numPolyVoices + 1);
            viewVoice = std::min(tabGroup->getSelectedTab(),
                                 monsoon->engine.numPolyVoices);   // 0..numPolyVoices
        }
        const bool onMonoTab = (viewVoice == 0);
        const int  pv = viewVoice - 1;   // poly bank index; valid only when viewVoice >= 1

        // Mix-in send display proxies ↔ per-voice store — poly tabs only (the mono tab's
        // sends would be voice-0's slice, surfaced under interp. Y; not edited here).
        if (!onMonoTab) {
            if (viewVoice != lastSendVoice) {
                for (int lane=0; lane<3; ++lane)
                    for (int item=0; item<4; ++item)
                        module->params[sendDispId(lane,item)].setValue(
                            module->params[sendId(pv,lane,item)].getValue());
                lastSendVoice = viewVoice;
            } else {
                for (int lane=0; lane<3; ++lane)
                    for (int item=0; item<4; ++item)
                        module->params[sendId(pv,lane,item)].setValue(
                            module->params[sendDispId(lane,item)].getValue());
            }
        }

        saveLOR();
        if (!onMonoTab)
            paramMgr->syncPatternEngineToEditor(visualEditor->currentState, pv);

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
        // TAB-1 MONO MIRROR: view tab 1 = voice 1 = the mono master strand when Sands
        // Mono is attached. Show mono's LORS read-only (consistent treatment with the
        // other voices' display), not Macro's global base. (Macro's left attenuverters
        // are hidden on tab 1 via gen panel / widget — Macro's global base doesn't reach
        // voice 1; only the mix-in sends could, under the deferred interp. Y.)
        auto* monoVis = monsoon->expanderManager.cachedSandsVisualExpander;
        bool tab1Mono = onMonoTab && (monoVis != nullptr);
        visualEditor->readOnly = tab1Mono;   // Macro is already view-only, but mark it explicitly
        // Macro's global-base CV-depth attenuverters don't reach voice 1 (mono provides
        // the base) — hide them on tab 1 when mono attached. Only the mix-in sends (below
        // the lanes) could affect voice 1, and only under the deferred interp. Y.
        for (rack::Widget* w : leftAttenuverters) if (w) w->visible = !tab1Mono;
        if (tab1Mono) {
            for (int l = 0; l < 3; ++l) {
                int mLen = (int)std::lround(monoVis->params[SandsMonoVisualIds::lenId(l)].getValue());
                int mOff = (int)std::lround(monoVis->params[SandsMonoVisualIds::offId(l)].getValue());
                int mRot = (int)std::lround(monoVis->params[SandsMonoVisualIds::rotId(l)].getValue());
                mLen = std::max(1, mLen);
                visualEditor->currentState.lanes[l].setDisplayLOR(mLen, mOff, mRot);
                visualEditor->setLanePlayStep(l, calcPlayhead(gs, mLen, mOff, mRot));
            }
        } else {
            for (int l = 0; l < 3; ++l) {
                int ownLen = (int)std::lround(mod->macroBase[l][0] + mod->macroCVDelta[l][0]);
                int ownOff = (int)std::lround(mod->macroBase[l][1] + mod->macroCVDelta[l][1]);
                int ownRot = (int)std::lround(mod->macroBase[l][2] + mod->macroCVDelta[l][2]);
                ownLen = std::max(1, ownLen);
                visualEditor->currentState.lanes[l].setDisplayLOR(ownLen, ownOff, ownRot);
                visualEditor->setLanePlayStep(l, calcPlayhead(gs, ownLen, ownOff, ownRot));
            }
        }
    }

    // Mix-in send group labels (NanoVG; panel carries no baked text). Geometry MUST
    // match the send grids in gen_macro_mono.py and the widget knob placement above:
    // BLEND_TOP=72 SEND_Y0=12 SEND_DY=11 SEND_DX=7, groups at ED_X + lane*ED_W/3.
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        NVGcontext* vg = args.vg;

        // V1 / mono tab: Macro's global-base CV-depth attenuverters don't reach voice 1
        // (mono provides the base). The knob widgets hide themselves (visible=false), but
        // the panel bakes their gold rings — mask the two attenuverter columns with the
        // panel background so V1 reads clean.
        {
            auto* mon = getMonsoon();
            bool isLight = mon && mon->lightTheme;
            bool monoTab = mon && (viewVoice == 0) &&
                           (mon->expanderManager.cachedSandsVisualExpander != nullptr);
            if (monoTab) {
                NVGcolor bg = isLight ? nvgRGB(0xe8,0xe8,0xea) : nvgRGB(0x16,0x18,0x1c);
                float x0 = mm2px(COL_A1) - mm2px(4.f);
                float x1 = mm2px(COL_A2) + mm2px(4.f);
                float y0 = mm2px(rowY(0)) - mm2px(4.f);
                float y1 = mm2px(rowY(N_ROWS - 1)) + mm2px(4.f);
                nvgBeginPath(vg);
                nvgRect(vg, x0, y0, x1 - x0, y1 - y0);
                nvgFillColor(vg, bg);
                nvgFill(vg);
            }
        }

        const float BLEND_TOP=72.f, SEND_Y0=12.f, SEND_DY=11.f, SEND_DX=7.f, BGAP=3.5f;
        const float GROUP_W = ED_W/3.f;
        const char* laneName[3] = { "REST", "MELODY", "OCTAVE" };
        const char* itemName[4] = { "LEN", "OFF", "ROT", "SPR" };

        bool isLight = false;
        if (auto* mon = getMonsoon()) isLight = mon->lightTheme;

        auto font = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!font) font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(vg, font->handle);

        NVGcolor head = isLight ? nvgRGB(40,44,52) : nvgRGB(210,214,222);
        NVGcolor item = isLight ? nvgRGB(150,120,20) : nvgRGB(190,160,60);

        nvgFontSize(vg, 8.0f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        nvgFillColor(vg, head);
        nvgText(vg, mm2px(ED_X), mm2px(BLEND_TOP - 3.5f), "MIX IN", nullptr);

        for (int l = 0; l < 3; ++l) {
            float gx = ED_X + l*GROUP_W + BGAP*0.5f;
            float gw = GROUP_W - BGAP;
            float gcx = gx + gw*0.5f;
            nvgFontSize(vg, 7.0f);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, head);
            nvgText(vg, mm2px(gcx), mm2px(BLEND_TOP + 4.0f), laneName[l], nullptr);
            nvgFontSize(vg, 5.0f);
            nvgFillColor(vg, item);
            for (int it = 0; it < 4; ++it) {
                float cxs = gcx + ((it % 2)==0 ? -SEND_DX : SEND_DX);
                float cys = BLEND_TOP + SEND_Y0 + (it / 2)*SEND_DY;
                nvgText(vg, mm2px(cxs), mm2px(cys + 4.4f), itemName[it], nullptr);
            }
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
    dotModular::VoiceResolver resolver(eng);
    for (int l = 0; l < 3; ++l) {
        auto& out = outputs[PROB_OUT_REST + l];
        out.setChannels(nCh < 1 ? 1 : nCh);
        // Macro's OWN global LOR step for this lane (from macroBase+CVDelta — identical
        // to Macro's editor playhead, independent of East/ownership). Same step for
        // every voice (Macro's view is global); each voice contributes its own draw.
        int ownLen = std::max(1, (int)std::lround(macroBase[l][0] + macroCVDelta[l][0]));
        int ownOff = (int)std::lround(macroBase[l][1] + macroCVDelta[l][1]);
        int ownRot = (int)std::lround(macroBase[l][2] + macroCVDelta[l][2]);
        int step = calcPlayhead(gs, ownLen, ownOff, ownRot) & 0x0F;
        // Uniform addressing: VCV channel ch carries voice ch+1. ch0 → voice 1 (mono;
        // the resolver ignores the explicit step and returns the master draw) — was a 0V
        // stub. ch v → voice v+1 (poly), sampled at Macro's global step.
        for (int ch = 0; ch < nCh; ++ch) {
            const int voice = ch + 1;                 // 1..16
            float raw = resolver.laneProbabilityAtStep(voice, l, step);
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
