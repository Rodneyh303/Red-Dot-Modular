#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/GoldPolyPort.hpp"
#include "ui/SvgPanelKit.hpp"
//#include "MonsoonStraitsSands.hpp"
#include "StraitsSandsMacroVisual.hpp"
#include "dsp/SandsTopology.hpp"           // step 4b: Macro lock predicate via the resolver
#include <cassert>
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
        // Lanes fill the box evenly (no padding) → align with painted lanes +
        // kit-bound jacks/prob-outs. MONO/POLY label suppressed; lane labels stay.
        visualEditor->layout.topPadding = 0.f;
        visualEditor->layout.botPadding = 0.f;
        visualEditor->showControlBar    = false;
        addChild(visualEditor);

        // DRIVES the lane downstream (Mono or East). Ownership governs what reaches the ENGINE, not what
        // Macro may edit on its own panel. Previously this locked V1 lanes owner(0,l)==MONO, which (with
        // Gate A) was half the asymmetry: Mono owning a V1 lane blocked Macro's LOR there while spread
        // stayed editable, and East+Mono+Macro behaved differently again.
        //visualEditor->laneLockedFn = [](int /*editorLane*/) -> bool { return false; };
        // NOT be gated by who DRIVES the lane downstream. Previously this locked V1 LOR whenever MONO
        // owned the lane (owner(0,l)==MONO), which produced the asymmetry: on the Macro V1 tab with Mono
        // owning a lane, its SPREAD was editable but its LOR was blocked — and East+Macro (East owns V1)
        // did NOT block, so the two combos behaved inconsistently. Macro's own global LOR is now always
        // editable (like its spread, like V2+); ownership governs what reaches the ENGINE, not what Macro
        // may edit on its own panel. Off the V1 tab this was already false.
        visualEditor->laneLockedFn = [](int /*editorLane*/) -> bool { return false; };

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
            // P9b: the two PRE/POST send taps per lane live in the send groups below the
            // lanes (3rd row) — bound by name there, not here. (see send-group binds.)
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
        // P9b: the two PRE/POST CV taps per lane (3rd row of each send group) —
        // param_taplor_{lane} → tapLorId, param_tapspr_{lane} → tapSprId.
        for (int lane = 0; lane < 4; ++lane) {
            bindParam<Trimpot>("param_taplor_" + std::to_string(lane), tapLorId(lane));
            bindParam<Trimpot>("param_tapspr_" + std::to_string(lane), tapSprId(lane));
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

    // STEP 4b: ownership authority for the Macro widget's V1 view. Macro is present
    // (this widget IS Macro); monoV1Owner[] read from Mono's editor-ordered ownerDispId
    // (the same source the old predicate read). lockedOn(MACRO,0,l) == owner(0,l)!=MACRO,
    // i.e. "Mono owns it" — matching the old mv->ownerDispId(l) > 0.5 test.
    // NOTE: currently UNUSED — laneLockedFn stopped gating V1 LOR on Mono ownership (Macro's own
    // global LOR is always editable, matching its spread). Kept because the tracked East-ownership
    // open question (SANDS_TOPOLOGY_RESOLVER_PLAN.md) may revive a per-lane V1 ownership predicate.
    //[[maybe_unused]] dotModular::SandsTopology buildV1Topo() {
    //[[maybe_unused]] 
    dotModular::SandsTopology buildV1Topo() {
        dotModular::SandsTopology::Inputs in;
        if (auto* mon = getMonsoon()) {
            mon->expanderManager.fillPresence(in, mon->engine.numPolyVoices);  // single authority
            if (auto* mv = mon->expanderManager.cachedSandsVisualExpander) {
                for (int l = 0; l < 4; ++l)
                    in.monoV1Owner[l] = mv->params[SandsMonoVisualIds::ownerDispId(l)].getValue() > 0.5f;
            }
        }
        return dotModular::SandsTopology::build(in);
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

    // Macro's OWN probability for a lane/step — the shared base draw with MACRO's own
    // spread applied (NOT East's). Mirrors how Macro's LOR display uses macroBase instead
    // of reading East's LOR: Macro shows its own modulation on the shared draw, never the
    // East-spread-modulated shared final (rhythmRandom[] etc.). This enforces the one-way
    // borrowing rule (East may borrow Macro; Macro never borrows East). See
    // docs/design/DISPLAY_STORE_ENGINE_SEPARATION.md. engLane = PL_REST/MEL/OCT/ACC (0..3).
    float macroOwnProbability(int engLane, int step, bool mono, int polyVoice) {
        auto* mod = dynamic_cast<StraitsSandsMacroVisual*>(module);
        Monsoon* mon = getMonsoon();
        if (!mod || !mon) return 0.5f;
        auto& pe = mon->engine.pe;
        const redDot::SpreadInterp::Target smode = pe.spreadInterpMono
            ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
        const int nPoly = mon->engine.numPolyVoices;
        // Macro's own spread for this lane (base knob + send-tapped delta, clamped) — the
        // SAME expression the engine's MACRO_SOLE branch uses (MonsoonExpanderManager.cpp).
        const float sp = rack::math::clamp(mod->macroBase[engLane][3] + mod->macroSendDelta[engLane][3], -1.f, 1.f);
        // The pre-spread base draw buffer for this lane (mono strand vs poly bank).
        using PL = SequencerEngine;  // PL::PL_REST etc. (enum lives in SequencerEngine)
        float base;
        if (mono) {
            base = (engLane == PL::PL_REST)   ? pe.slewedRhythm[step & 0x0F]
                 : (engLane == PL::PL_MELODY) ? pe.slewedMelody[step & 0x0F]
                 : (engLane == PL::PL_OCTAVE) ? pe.slewedOctave[step & 0x0F]
                 :                              pe.slewedAccent[step & 0x0F];
        } else {
            int v = rack::math::clamp(polyVoice, 0, 14);
            base = (engLane == PL::PL_REST)   ? pe.slewedPolyRhythm[v][step & 0x0F]
                 : (engLane == PL::PL_MELODY) ? pe.slewedPolyMelody[v][step & 0x0F]
                 : (engLane == PL::PL_OCTAVE) ? pe.slewedPolyOctave[v][step & 0x0F]
                 :                              pe.slewedPolyAccent[v][step & 0x0F];
        }
        return redDot::SpreadInterp::apply(pe, smode, engLane, step, nPoly, base, sp);
    }

    void step() override {
        ModuleWidget::step();
        kitStep();   // kit: dev-mode live-reload poll (no-op unless enabled)
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }

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
        // Macro's OWN global base (V1 + V2..V16 LOR/spread knobs) is always editable — it edits
        // Macro's own params, which exist regardless of any expander. East only supplies the poly
        // voice COUNT (tabGroup->setActiveCount below limits selectable tabs to it, so without East the
        // only tab is V1). So we no longer early-return / set inert when East is absent — that used to
        // kill ALL of Macro's editing, producing the asymmetry where Mono+Macro/no-East blocked Macro
        // LOR while spread (a separate control) stayed editable. inert stays false; editing is live.
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
        // (Spread target mode pulled from the engine by SpreadManager — no push needed.)

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
        // (v1Editable removed — its only use, hiding Macro attens on V1, was dropped in P1.)
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
        // POLY TAB: show MACRO's OWN probability (shared base draw + Macro's own spread),
        // NOT resolver.laneProbabilityAtStep (which returns the shared final that East's spread
        // writes on East-owned lanes → leak). One-way borrowing: Macro never borrows East.
        // lane = engine PL lane; el = editor lane it displays into; pv = poly bank index.
        if (!onMonoTab) {
            for (int lane = 0; lane < 4; ++lane) {
                int el = dotModular::ENGINE_LANE_TO_EDITOR[lane];
                for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s)
                    visualEditor->currentState.lanes[el].probabilities[s] =
                        macroOwnProbability(lane, s, /*mono=*/false, /*polyVoice=*/pv);
            }
        } else {
            // MONO TAB (V1): neither sync nor the resolver-overwrite above ran (both
            // gated !onMonoTab), so V1's probability bars were NEVER populated from the
            // engine — they showed stale currentState. That's why standalone-Macro spread
            // looked dead: with numPolyVoices=0 the ONLY tab is V1. Read V1 per-step from
            // the engine MONO final arrays (finalRandomByStrand — what Mono/East/standalone-
            // Macro spread write), editor lane → engine strand via MONO_LANE_TO_STRAND.
            // MONO TAB (V1): show MACRO's OWN probability — the shared base draw with Macro's
            // OWN spread — NOT the shared finalRandomByStrand (which East's spread writes on an
            // East-owned lane, leaking East's modulation into Macro's display). One-way
            // borrowing: Macro never borrows East. el = EDITOR lane; macroOwnProbability wants
            // the ENGINE PL lane → EDITOR_TO_ENGINE_LANE[el].
            for (int el = 0; el < 4; ++el) {
                const int engLane = dotModular::EDITOR_TO_ENGINE_LANE[el];
                for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s)
                    visualEditor->currentState.lanes[el].probabilities[s] =
                        macroOwnProbability(engLane, s, /*mono=*/true, /*polyVoice=*/0);
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
        // (tab1Mono removed — its only use, readOnly=tab1Mono, was replaced by the
        // per-lane laneLockedFn set in the constructor.)
        // When V1 is editable (no Mono), Macro's global LOR knobs act as the V1 base.
        // The global base params are already wired to the engine for poly; for V1,
        // processDNA reads them via publishGlobal which writes all mono strands.
        // Macro V1 editing is locked PER-LANE via laneLockedFn (set up in the
        // constructor): on the V1 tab with Mono attached, a lane is locked iff Mono owns
        // it; Macro-owned (delegated) lanes stay editable. This replaced the old blunt
        // `readOnly = tab1Mono`, which locked ALL of Macro's V1 whenever Mono was present
        // (so Macro-owned lanes couldn't be edited in Mono+Macro). readOnly stays false.
        visualEditor->readOnly = false;
        // P1 (G1 no-hide): Macro's left attenuverters are ALWAYS visible — they were
        // previously hidden on the V1 tab when Mono was attached. They stay shown now;
        // whether Macro displays its own base vs mirrors Mono on V1, and any locking,
        // is handled by the delegation P-items (P4/P5/P6), not by hiding.
        for (rack::Widget* w : leftAttenuverters) if (w) w->visible = true;
        {
            // Macro's V1 (and poly) LOR display ALWAYS shows MACRO's OWN global base +
            // CV delta — the panel represents the Macro module's own state regardless of
            // who owns the lane downstream (same principle as the poly display). Previously
            // the tab1Mono branch showed MONO's LOR for Mono-owned lanes (and only 3 lanes,
            // missing accent), so Macro's V1 page reflected Mono instead of Macro. l =
            // engine lane (0=REST 1=MEL 2=OCT 3=ACC) → editor lane.
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

        // (P1/G1 no-hide: the old V1 atten-masking rectangle was removed — Macro's
        // left attenuverters are always visible, including on the V1 tab. They were
        // being painted over with the panel background here, which is why the V1
        // trimpots "disappeared" even though the widgets were visible.)

        const float BLEND_TOP=72.f, SEND_Y0=12.f, SEND_DY=11.f, SEND_DX=6.f, BGAP=2.5f;
        const float GROUP_W = ED_W/4.f;
        // Labels in DISPLAY order (matching gen_macro_mono.py DISPLAY_ORDER = editor
        // order MEL/OCT/REST/ACC). The SVG already places the send groups left-to-right
        // in this order; the labels must match. (Previously laneName was indexed by
        // physical position in ENGINE order, so e.g. the MEL group was mislabelled
        // "REST" — the off-by-mapping the user saw: "REST" group drove melody.)
        const char* laneName[4] = { "MELODY", "OCTAVE", "REST", "ACCENT" };  // editor/display order
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
