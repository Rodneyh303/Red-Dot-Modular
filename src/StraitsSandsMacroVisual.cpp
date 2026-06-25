#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/GoldPolyPort.hpp"
#include "ui/SvgPanelKit.hpp"
//#include "MonsoonStraitsSands.hpp"
#include "StraitsSandsMacroVisual.hpp"
#include "MonsoonSandsVisualExpander.hpp"  // complete mono type + SandsMonoVisualIds for the tab-1 mono mirror
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
#include "dsp/managers/PolySandsParameterManager.hpp"
#include "dsp/VoiceResolver.hpp"   // activeVoiceCount + voice identity, single source of truth for the tab→voice mapping and uniform 16-voice addressing for prob-out
#include "dsp/LaneMapping.hpp"        // ENGINE_LANE_TO_EDITOR / MONO_PARAM_TO_EDITOR — single source of truth for lane order

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
   

struct StraitsSandsMacroVisualWidget : ModuleWidget,
    dotModular::Compose<StraitsSandsMacroVisualWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    SandsVisualEditorV4*       visualEditor = nullptr;
    PolySandsParameterManager* paramMgr     = nullptr;
    TabButtonGroup*            tabGroup     = nullptr;
    int viewVoice = 0;   // which voice's resulting probabilities to DISPLAY (read-only)
    int lastSendVoice = -1;  // detect view-voice change to sync the mix-in send proxies
    bool                       initialized  = false;
    // Light/dark panel swap: kit's loadPanel() owns the live SvgPanel; we keep
    // both backgrounds and swap via the panel child (same pattern as East).
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
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
        // Kit owns the SvgPanel (created + setPanel'd here); widgets bind to the
        // named shapes baked into the SVG by panel_src/gen_macro_mono.py, so the
        // gen script is the single source of widget geometry (no rowY/columns here).
        loadPanel(asset::plugin(pluginInstance,
                            "res/panels/StraitsSandsMacroVisual_40HP.svg"));

        redDot::addRedScrews(this);

        // Visual editor + view tabs are custom widgets (not kit shapes) — placed
        // manually. Everything else (jacks, attens, spreads, sends, prob-outs)
        // binds to named SVG shapes so geometry lives only in the gen script.
        tabGroup = new TabButtonGroup(16, 1, 2, mm2px(ED_W), mm2px(10.f));   // V1 mono + V2..V16 poly
        tabGroup->box.pos = mm2px(Vec(ED_X, ED_Y - 12.f));
        addChild(tabGroup);

        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ED_H));
        addChild(visualEditor);

        Module* mod_ = module;
        auto themeOut = [mod_](redDot::GoldPolyPort* p) {
            p->lightTheme = [mod_]() { Monsoon* m = mod_ ? redDot::findMonsoonEitherSide(mod_) : nullptr;
                                       return m && m->lightTheme; };
        };

        // 4 poly probability CV outs (output_PROB_OUT_REST..+3), aligned to lane rows.
        for (int l = 0; l < 4; ++l)
            bindOutput<redDot::GoldPolyPort>(
                "output_" + std::to_string(StraitsMacroVisualIds::PROB_OUT_REST + l),
                StraitsMacroVisualIds::PROB_OUT_REST + l,
                std::function<void(redDot::GoldPolyPort*)>(themeOut));

        // ── Left section: 4 lanes × (4 CV jacks + 4 attens + 1 spread) ──
        // input_{cvId(lane,c)}  param_{attenId(lane,c)}  param_{SPREAD_REST+lane}
        for (int lane = 0; lane < 4; ++lane) {
            for (int c = 0; c < 4; ++c)
                bindInput<PJ301MPort>("input_" + std::to_string(cvId(lane,c)), cvId(lane,c));
            for (int c = 0; c < 4; ++c)
                bindParam<Trimpot>("param_" + std::to_string(attenId(lane,c)), attenId(lane,c),
                    std::function<void(Trimpot*)>([this](Trimpot* a){ leftAttenuverters.push_back(a); }));
        }

        // Per-lane global SPREAD trimpots (param_SPREAD_REST..+3 = lanes 0..3).
        static const int spreadPid[4] = { SPREAD_REST, SPREAD_MELODY, SPREAD_OCTAVE, SPREAD_ACCENT };
        for (int lane = 0; lane < 4; ++lane) {
            int pid = spreadPid[lane];
            bindParam<Trimpot>("param_" + std::to_string(pid), pid,
                std::function<void(Trimpot*)>([this, lane](Trimpot* sp){ pendingSpreadArcs.push_back({sp, lane}); }));
        }

        // Macro→voice MIX-IN send 2×2 grids — bound to param_send_{lane}_{item}
        // markers (gen_macro_mono.py), wired to the sendDispId display proxies.
        for (int lane = 0; lane < 4; ++lane)
            for (int item = 0; item < 4; ++item)
                bindParam<Trimpot>("param_send_" + std::to_string(lane) + "_" + std::to_string(item),
                                   sendDispId(lane, item));

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
        for (int l = 0; l < 4; ++l) {   // l = engine lane (lorId) → editor lane
            const auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR[l]];
            module->params[lorId(l,0)].setValue((float)lane.length);
            module->params[lorId(l,1)].setValue((float)lane.offset);
            module->params[lorId(l,2)].setValue((float)lane.rotation);
        }
    }
    void loadLOR() {
        if (!module || !visualEditor) return;
        for (int l = 0; l < 4; ++l) {   // l = engine lane (lorId) → editor lane
            auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR[l]];
            lane.length   = std::max(1,(int)std::round(module->params[lorId(l,0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorId(l,1)].getValue());
            lane.rotation = (int)std::round(module->params[lorId(l,2)].getValue());
        }
    }

    void step() override {
        ModuleWidget::step();
        kitStep();   // kit: dev-mode live-reload poll (no-op unless enabled)
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }
        auto* monoVis = monsoon->expanderManager.cachedSandsVisualExpander;  // null = no Mono attached

        int wantLight = monsoon->lightTheme ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            // Kit's loadPanel() added the SvgPanel as a child; swap its background.
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
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
        paramMgr->spreadMgr.setSpread(3, mod->spreadEffective[3]);
        paramMgr->spreadMgr.setInterpolationTarget(
            monsoon->spreadInterpMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);

        // CV applied at control rate in Monsoon::process() — base + cv*atten*scale.

        // Which voice to DISPLAY (read-only view lens). Tab 0 = mono (V1, always
        // active); tabs 1..15 = poly voices V2..V16. Active = mono + active poly count.
        if (tabGroup) {
            tabGroup->setActiveCount(dotModular::VoiceResolver(monsoon->engine).activeVoiceCount());
            viewVoice = std::min(tabGroup->getSelectedTab(),
                                 monsoon->engine.numPolyVoices);   // 0..numPolyVoices
        }
        // Voice NUMBER (1..16) for the displayed tab: tab 0 = V1 (mono), tab v = V(v+1).
        // Mono/poly identity + bank mapping via VoiceResolver — one source of truth, not
        // local viewVoice arithmetic (static/constexpr, no engine ref).
        const int  viewVoiceNum = viewVoice + 1;
        const bool onMonoTab = dotModular::VoiceResolver::isMono(viewVoiceNum);
        const bool v1Editable = onMonoTab && (monoVis == nullptr);  // edit V1 when no Mono attached
        const int  pv = dotModular::VoiceResolver::polyBankIndex(viewVoiceNum);  // -1 on mono

        // Mix-in send display proxies ↔ per-voice store, N→N: tab v (0-based) edits voice
        // slot v (slot 0 = voice 1 / mono, slot 1 = poly voice 2, …). This is uniform across
        // ALL tabs including mono — the mono tab's sends fold onto voice 1 via Interp Y, which
        // reads slot 0. (Previously this used pv=polyBankIndex, so the v2 tab wrote slot 0 and
        // its CV leaked onto mono — the N→N off-by-one.)
        {
            const int slot = dotModular::VoiceResolver::voiceSlot(viewVoiceNum);  // V→16-wide slot (slot0=mono)
            if (viewVoice != lastSendVoice) {
                for (int lane=0; lane<4; ++lane)
                    for (int item=0; item<4; ++item)
                        module->params[sendDispId(lane,item)].setValue(
                            module->params[sendId(slot,lane,item)].getValue());
                lastSendVoice = viewVoice;
            } else {
                for (int lane=0; lane<4; ++lane)
                    for (int item=0; item<4; ++item)
                        module->params[sendId(slot,lane,item)].setValue(
                            module->params[sendDispId(lane,item)].getValue());
            }
        }

        saveLOR();
        if (!onMonoTab)
            paramMgr->syncPatternEngineToEditor(visualEditor->currentState, pv);
        // Bug fix (same as East): the sync reads slewedPoly* which isn't populated for a voice
        // when Macro owns the lane → blank lanes. Macro's editor is read-only (display only),
        // so overwrite ALL displayed lanes from the resolver (polyRhythmRandom — the final
        // output the sequencer plays, populated regardless of owner; the prob-outs use it too).
        if (!onMonoTab) {
            dotModular::VoiceResolver resolver(monsoon->engine);
            for (int lane = 0; lane < 4; ++lane) {
                int el = dotModular::ENGINE_LANE_TO_EDITOR[lane];
                for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s)
                    visualEditor->currentState.lanes[el].probabilities[s] =
                        resolver.laneProbabilityAtStep(viewVoiceNum, lane, s);
            }
        }

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
        bool tab1Mono = onMonoTab && (monoVis != nullptr);
        // When V1 is editable (no Mono), Macro's global LOR knobs act as the V1 base.
        // The global base params are already wired to the engine for poly; for V1,
        // processDNA reads them via publishGlobal which writes all mono strands.
        visualEditor->readOnly = tab1Mono;   // Macro is already view-only, but mark it explicitly
        // Macro's global-base CV-depth attenuverters don't reach voice 1 (mono provides
        // the base) — hide them on tab 1 when mono attached. Only the mix-in sends (below
        // the lanes) could affect voice 1, and only under the deferred interp. Y.
        // Attens hidden when Mono is attached on V1 (Mono owns the base).
        // Shown for poly tabs and when V1 is editable without Mono.
        for (rack::Widget* w : leftAttenuverters) if (w) w->visible = !tab1Mono || v1Editable;
        if (tab1Mono) {
            // l = mono param bank (0=REST 1=MEL 2=OCT) → editor lane
            for (int l = 0; l < 3; ++l) {
                int mLen = (int)std::lround(monoVis->params[SandsMonoVisualIds::lenId(l)].getValue());
                int mOff = (int)std::lround(monoVis->params[SandsMonoVisualIds::offId(l)].getValue());
                int mRot = (int)std::lround(monoVis->params[SandsMonoVisualIds::rotId(l)].getValue());
                mLen = std::max(1, mLen);
                int el = dotModular::MONO_PARAM_TO_EDITOR[l];
                visualEditor->currentState.lanes[el].setDisplayLOR(mLen, mOff, mRot);
                visualEditor->setLanePlayStep(el, calcPlayhead(gs, mLen, mOff, mRot));
            }
        } else {
            // l = engine lane (0=REST 1=MEL 2=OCT 3=ACC) → editor lane
            for (int l = 0; l < 4; ++l) {
                int ownLen = (int)std::lround(mod->macroBase[l][0] + mod->macroCVDelta[l][0]);
                int ownOff = (int)std::lround(mod->macroBase[l][1] + mod->macroCVDelta[l][1]);
                int ownRot = (int)std::lround(mod->macroBase[l][2] + mod->macroCVDelta[l][2]);
                ownLen = std::max(1, ownLen);
                int el = dotModular::ENGINE_LANE_TO_EDITOR[l];
                visualEditor->currentState.lanes[el].setDisplayLOR(ownLen, ownOff, ownRot);
                visualEditor->setLanePlayStep(el, calcPlayhead(gs, ownLen, ownOff, ownRot));
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
            bool monoTab = mon && dotModular::VoiceResolver::isMono(viewVoice + 1) &&
                           (mon->expanderManager.cachedSandsVisualExpander != nullptr);
            if (monoTab) {
                NVGcolor bg = isLight ? nvgRGB(0xe8,0xe8,0xea) : nvgRGB(0x16,0x18,0x1c);
                float x0 = mm2px(COL_A1) - mm2px(4.f);
                float x1 = mm2px(COL_A4) + mm2px(4.f);
                float y0 = mm2px(rowY(0)) - mm2px(4.f);
                float y1 = mm2px(rowY(N_ROWS - 1)) + mm2px(4.f);
                nvgBeginPath(vg);
                nvgRect(vg, x0, y0, x1 - x0, y1 - y0);
                nvgFillColor(vg, bg);
                nvgFill(vg);
            }
        }

        const float BLEND_TOP=72.f, SEND_Y0=12.f, SEND_DY=11.f, SEND_DX=6.f, BGAP=2.5f;
        const float GROUP_W = ED_W/4.f;
        const char* laneName[4] = { "REST", "MELODY", "OCTAVE", "ACCENT" };
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

        for (int l = 0; l < 4; ++l) {
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
        for (int l = 0; l < 4; ++l) { outputs[PROB_OUT_REST + l].setChannels(1);
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
    for (int l = 0; l < 4; ++l) {
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
