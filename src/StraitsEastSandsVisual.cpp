#include <rack.hpp>
#include "ui/SandsGrid.hpp"
#include "ui/StoreEditAction.hpp"
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "StraitsEastSandsVisual.hpp"
#include "StraitsSandsMacroVisual.hpp"  // complete type + StraitsMacroVisualIds for the spread-arc Macro-CV gate
#include "MonsoonSandsVisualExpander.hpp"  // complete mono type + SandsMonoVisualIds for the tab-1 mono mirror
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/OwnerCell.hpp"
#include "ui/DimmableTrimpot.hpp"
#include "ui/ModArcOverlay.hpp"
#include "ui/StoreBound.hpp"      // bindStoreKnob (de-param)
#include "ui/Controls.hpp"        // Tag_Grey_Trim_Bar
#include "dsp/SandsTopology.hpp"   // step 3c: East V1 write ownership via the resolver
#include <cassert>
#include <cmath>
#include <limits>
#include "ui/ConnectMark.hpp"
#include "ui/GoldPolyPort.hpp"
#include "dsp/managers/PolyVoiceSandsParameterManager.hpp"
#include "dsp/managers/SpreadManager.hpp"
#include "dsp/VoiceResolver.hpp"   //  activeVoiceCount + voice identity, single source of truth for the tab→voice mapping and uniform 16-voice addressing for prob-out
#include "dsp/LaneMapping.hpp"        //  ENGINE_LANE_TO_EDITOR / MONO_PARAM_TO_EDITOR — single source of truth for lane order

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsEastVisualIds;

extern Plugin* pluginInstance;

// A Trimpot that dims to partial alpha (still interactive) when its predicate says it's
// currently inactive — for East's per-lane base-spread / CV-depth / Macro-send knobs,
// which have no effect while Macro owns the lane but stay editable so the user can
// pre-configure East's values before claiming. Full alpha once East owns the lane.
// (Same nvgGlobalAlpha technique as Monsoon's TrialButton.)
// DimmableTrimpot moved to ui/DimmableTrimpot.hpp (shared with the Mono/Sands visual).

struct StraitsEastSandsVisualWidget;  // fwd

// Owner claim latch that dims + swallows input when inert (no Macro attached — there's
// nothing to claim ownership FROM, it's all East). Predicate set by the widget.
// (OwnerCell moved to ui/OwnerCell.hpp — shared with Mono.)
struct DimmableLatch : rack::componentlibrary::VCVLightLatch<rack::componentlibrary::SmallSimpleLight<rack::componentlibrary::WhiteLight>> {
    std::function<bool()> inertWhen;
    std::function<bool()> hideWhen;   // fully hidden (not drawn, no input) — e.g. mono tab
    bool inert() const { return inertWhen && inertWhen(); }
    bool hidden() const { return hideWhen && hideWhen(); }
    void onButton(const event::Button& e) override {
        if (hidden() || inert()) { e.consume(this); return; }
        VCVLightLatch::onButton(e);
    }
    void onDragStart(const event::DragStart& e) override {
        if (hidden() || inert()) return;
        VCVLightLatch::onDragStart(e);
    }
    void draw(const DrawArgs& args) override {
        if (hidden()) return;            // V1/mono tab: nothing to opt into — don't show
        bool dim = inert();
        if (dim) nvgGlobalAlpha(args.vg, 0.4f);
        VCVLightLatch::draw(args);
        if (dim) nvgGlobalAlpha(args.vg, 1.0f);
    }
};

struct StraitsEastSandsVisualWidget : ModuleWidget,
    dotModular::Compose<StraitsEastSandsVisualWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    SandsVisualEditorV4*            visualEditor = nullptr;
    TabButtonGroup*                 tabGroup     = nullptr;
    PolyVoiceSandsParameterManager* paramMgr     = nullptr;
    // (blend controls now dim/disable themselves via DimmableTrimpot/DimmableLatch
    //  predicates — no central visibility list needed.)
    int  selectedVoice = 0;
    // V1 (East-alone) editor is seeded from Monsoon's eastV1Lor/Spread stores on the first
    // frame of each V1 entry, then mirrors live edits back. false = seed pending.
    bool v1Loaded_ = false;
    // East spread mod-arcs. Compared in the INTERP domain (0..1) to sidestep the
    // pre-existing display-trimpot bipolar (-1..1) vs interp (0..1) mismatch: set
    // = the viewed voice's interp param (pre-CV), effective = the published
    // polySpreadEffective[viewedVoice][lane] (post per-voice/lane CV + combineSpread).
    std::vector<std::pair<rack::widget::Widget*, int>> pendingSpreadArcs;
    void flushSpreadArcs() {
        auto* mod = dynamic_cast<StraitsEastSandsVisual*>(module);
        for (auto& pr : pendingSpreadArcs) {
            auto* knob = pr.first; int lane = pr.second;
            if (!knob) continue;
            auto* arc = new redDot::ModArcOverlay();
            arc->radius   = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            arc->attachOverKnob(knob, mm2px(2.5f));
            arc->getSetNorm = [this, lane]() -> float {
                // MVC step 1d: SET = the store-backed knob's value = editor.spread[currentSlot()].
                // currentSlot() = 0 (V1) or polySlot, so one read covers both. (v+1)/2 normalise.
                Monsoon* mm = getMonsoon(); if (!mm) return 0.5f;
                float v = mm->getSpread(currentSlot(), lane);
                return rack::math::clamp((v + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->getModNorm = [mod, this, lane]() -> float {
                if (!mod) return 0.5f;
                int v = polyVoice();
                if (v < 0) {
                    // V1 / mono tab: MOD = the EFFECTIVE V1 spread on this lane, matching
                    // the manager's sprForLane: delegated → Macro base+CVdelta; owned →
                    // East knob + East V1 spread CV + Macro send blend. lane = spread index
                    // 0=REST,1=MEL,2=OCT,3=ACC; CV jack cvId(lane,3).
                    if (lane < 0 || lane >= 4) return 0.5f;
                    Monsoon* mon = findMonsoonEitherSide(mod);
                    auto* macroVis = mon ? mon->expanderManager.cachedMacroSandsVisual : nullptr;
                    bool delegated = macroVis && !eastOwnsLane(lane);   // MVC step 1d: store-backed
                    float sp;
                    if (delegated) {
                        sp = macroVis->macroBase[lane][3] + macroVis->macroCVDelta[lane][3];
                    } else {
                        int pid = (lane==0) ? (int)SPREAD_R : (lane==1) ? (int)SPREAD_M
                                : (lane==2) ? (int)SPREAD_O : (int)SPREAD_A;
                        sp = mod->params[pid].getValue();   // bipolar -1..1
                        if (mod->inputs[cvId(lane,3)].isConnected()) {
                            float att = (redDot::findMonsoonEitherSide(mod) ? redDot::findMonsoonEitherSide(mod)->getMacroAtten(dotModular::VoiceResolver::kMonoSlot, lane*4 + 3) : 0.f);
                            float cv  = mod->inputs[cvId(lane,3)].getVoltage(0) / 10.f;
                            sp += cv * att * 2.f;
                        }
                        if (macroVis) {
                            float send = (redDot::findMonsoonEitherSide(macroVis) ? redDot::findMonsoonEitherSide(macroVis)->getMacroSend(dotModular::VoiceResolver::kMonoSlot, lane, 3) : 0.f);
                            sp += macroVis->macroSendDelta[lane][3] * send;
                        }
                    }
                    return rack::math::clamp((rack::math::clamp(sp,-1.f,1.f) + 1.f) * 0.5f, 0.f, 1.f);
                }
                if (v >= 15) return 0.5f;
                // polySpreadEffective is bipolar -1..1 → map to 0..1.
                return rack::math::clamp((mod->polySpreadEffective[v][lane] + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->isActive = [mod, this, lane]() -> bool {
                if (!mod) return false;
                Monsoon* mon = findMonsoonEitherSide(mod);
                if (!mon || !mon->modVizEast) return false;
                int v = polyVoice();
                if (v < 0) {
                    // V1 / mono tab: active when REAL modulation enters V1's spread on this
                    // lane — East's own V1 spread CV, OR Macro modulation: delegated lane
                    // with Macro spread CV live, OR owned lane with a non-zero send AND
                    // Macro spread CV live (matches poly macroBlend; static blend excluded
                    // to avoid the manual-turn red-residue race).
                    if (lane < 0 || lane >= 4) return false;
                    if (mod->inputs[cvId(lane,3)].isConnected()) return true;
                    auto* macroVis = mon->expanderManager.cachedMacroSandsVisual;
                    // MVC step 1d: delegated ⟺ !eastOwnsLane (store-backed). Helper covers
                    // delegated (CV live) + owned (mono-slot send + CV live). sendSlot = kMonoSlot.
                    bool delegated = macroVis && !eastOwnsLane(lane);
                    return StraitsMacroVisualIds::macroSpreadModulatesLane(
                        macroVis, lane, delegated, dotModular::VoiceResolver::kMonoSlot);
                }
                if (v >= 15) return false;
                // Gate on a REAL modulation source (not a transient set-vs-effective
                // delta, which races during a manual knob turn — the control-rate
                // polySpreadEffective lags the live param for a frame and drew a red
                // residue arc; same desync class as the Monsoon big-5 fix). The spread
                // is genuinely modulated when its per-lane spread CV jack is connected,
                // or when Macro is blending into an East-owned lane.
                // SPR CV jack is col 3 in the lane's own row (mono-style 4+4+1 layout)
                bool cvConnected = mod->inputs[cvId(lane, 3)].isConnected();
                bool macroBlend = false;
                if (auto* macroVis = mon->expanderManager.cachedMacroSandsVisual) {
                    // East-owned lane receiving Macro's per-voice SEND. Helper consolidates
                    // the "non-zero send AND Macro spread CV live" test (static blend
                    // excluded — avoids the manual-turn red-residue race). Pass
                    // delegated=false so this stays the OWNED-blend case exactly as before;
                    // sendSlot = v (poly voice). (Delegated poly lanes show via getModNorm/
                    // polySpreadEffective, unchanged.)
                    // NOTE (step 5b): left on the direct persistent read deliberately.
                    // Migrating to buildTopo() here would switch this current-tab decision
                    // from persistent ownerId to the live-overlaid ownerDispId — a subtle
                    // BEHAVIOUR change (persistent-until-tab-exit vs live), not a pure
                    // refactor. Deferred until the live-vs-persistent semantics for the
                    // current tab are decided (same question as the deferred edit-lock one).
                    bool eastOwns = (redDot::findMonsoonEitherSide(mod) ? redDot::findMonsoonEitherSide(mod)->getMacroOwn(v, lane) > 0.5f : false);
                    if (eastOwns)
                        macroBlend = StraitsMacroVisualIds::macroSpreadModulatesLane(
                            macroVis, lane, /*delegated=*/false, /*sendSlot=*/v);
                }
                return cvConnected || macroBlend;
            };
            addChild(arc);
        }
        pendingSpreadArcs.clear();
    }
    bool initialized   = false;
    // Theme follow-Monsoon: cache both panel SVGs + the panel widget so step()
    // can swap when the connected host's lightTheme changes.
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;  // -1 = unset, forces first apply

    explicit StraitsEastSandsVisualWidget(StraitsEastSandsVisual* mod) {
        setModule(mod);
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_48HP.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_48HP_light.svg"));
        loadPanel(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_48HP.svg"));

        redDot::addRedScrews(this);

        // Voice tabs: V1 = mono master strand (index 0, mirrors Sands Mono), V2..V16 =
        // the 15 poly voices (indices 1..15 → poly bank slots 0..14). 16 total, two rows.
        tabGroup = new TabButtonGroup(16, 1, 2,
                                      mm2px(ED_W), mm2px(10.f));
        tabGroup->box.pos = mm2px(Vec(ED_X, ED_Y - 12.f));
        addChild(tabGroup);

        // Visual editor
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        // EAST_EXTRA_LANES.md stage 1: show all six lanes (VARIATION/LEGATO added), on the same
        // 14..98 band as Mono. Mode stays POLY (all mode==POLY logic unchanged); only the lane
        // count is overridden. Lanes 4/5 are LOCKED below — display-only, nothing reads them.
        visualEditor->setLaneCount(dotModular::SandsGrid::EAST_LANES);   // 6
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ED_H));
        // Lanes fill the box evenly (no internal padding) so the live lanes line
        // up with the painted recess lanes and the kit-bound jacks/prob-outs,
        // which all divide ED_H by laneCount. MONO/POLY label suppressed (would
        // land on lane 0); lane labels stay (panel doesn't draw them).
        visualEditor->layout.topPadding = 0.f;
        visualEditor->layout.botPadding = 0.f;
        visualEditor->showControlBar    = false;
        // P4 (G5): a lane is locked (inoperable, tracks Macro) when it's delegated to
        // Macro on a poly voice, OR when this is the V1 tab and Mono owns V1 (East
        // mirrors Mono, inoperable). editorLane → engine lane for the ownership check.
        visualEditor->laneEditBlockedFn = [this](int editorLane) -> bool {
            if (tab1MonoMirror()) return true;           // V1 owned by Mono → all lanes locked on East
            // VARIATION (4) / LEGATO (5): the usual V1 pattern. MONO owns these strands, so on the
            // V1 tab they are LOCKED and merely MIRROR mono's values — even when no Sands Mono is
            // attached (East is the V1 editor for the four poly lanes only; VAR/LEG stay mono's).
            // On poly tabs they are editable (stage 1b banks). They are never Macro-delegated:
            // an owned lane drives all voices identically, annihilating per-voice divergence.
            // Modulation still reaches mono's VAR/LEG through mono's own path — locking the East
            // display does not gate the strand.
            // Editable on poly tabs, and on V1 when East IS the V1 editor (no Sands Mono).
            // Locked only when Mono owns V1 — which tab1MonoMirror() already caught above.
            if (editorLane >= dotModular::SandsGrid::POLY_LANES) return onMonoTab() && !v1Editable();
            if (editorLane < 0) return false;
            int engLane = dotModular::EDITOR_TO_ENGINE_LANE[editorLane];
            // STEP 4c: a lane delegated to Macro is inoperable on East (V1 + poly tabs).
            // Shared resolver-backed helper: owner(currentVoice, lane) == MACRO.
            return laneOwnedByMacroTopo(engLane);
        };
        // Ghost echo needs TRUE lock-mode state (engine.locked), not the edit-permission above.
        visualEditor->lockActiveFn = [this]() -> bool { auto* m = getMonsoon(); return m && m->engine.locked; };
        // Right-click on a lane row opens the ownership context menu.
        visualEditor->onLaneRightClick = [this](int lane, rack::math::Vec pos) -> bool {
            if (!macroAttached()) return false;  // no menu when Macro absent
            if (onMonoTab()) return false;        // ownership is per poly voice, not mono
            openLaneOwnershipMenu(lane, pos);
            return true;
        };
        // LOR drag undo: push a Rack history action against the store for a completed drag.
        // slot = currentSlot() (V1 mono or poly voice), bank = lorBank(lane). Captures the
        // resolved slot at commit time. Refreshes the editor's cached currentState too (the
        // store->editor seed is event-driven, not per-frame). visualEditor capture is safe for
        // a store-edit action (widget persists across it).
        visualEditor->onLorCommit = [this](int lane, const int before[3], const int after[3]) {
            auto* m = getMonsoon(); if (!m) return;
            const int slot = currentSlot();
            const int bank = lorBank(lane);
            const int bef0=before[0],bef1=before[1],bef2=before[2];
            const int aft0=after[0], aft1=after[1], aft2=after[2];
            auto* ed = visualEditor;
            redDot::applyAndPushStoreEdit<Monsoon>(m, "LOR edit",
                [slot, bank, lane, bef0,bef1,bef2, aft0,aft1,aft2, ed](Monsoon& mm, float dir) {
                    const bool redo = dir > 0.5f;
                    const int L = redo?aft0:bef0, O = redo?aft1:bef1, R = redo?aft2:bef2;
                    mm.setLorBase(slot, bank, 0, (float)L);
                    mm.setLorBase(slot, bank, 1, (float)O);
                    mm.setLorBase(slot, bank, 2, (float)R);
                    if (ed && lane >= 0 && lane < 6) {
                        ed->currentState.lanes[lane].length   = std::max(1, L);
                        ed->currentState.lanes[lane].offset   = O;
                        ed->currentState.lanes[lane].rotation = R;
                    }
                },
                0.f, 1.f);
        };
        addChild(visualEditor);

        // 4 poly probability CV outs — bound via SVG panel kit (output_prob_<lane>).
        // NOTE: output IDs are in ENGINE order (REST=0, MEL=1, OCT=2, ACC=3) but panel
        // rows are in EDITOR order (MEL=0, OCT=1, REST=2, ACC=3). Convert via
        // EDITOR_TO_ENGINE_LANE so the jack at the MEL row drives PROB_OUT_MEL, etc.
        {
            Module* mod = module;
            auto themeOut = [mod](redDot::GoldPolyPort* p) {
                p->lightTheme = [mod]() { Monsoon* m = mod ? redDot::findMonsoonEitherSide(mod) : nullptr;
                                          return m && m->lightTheme; };
            };
            for (int l = 0; l < 4; ++l)
                bindOutput<redDot::GoldPolyPort>("output_prob_" + std::to_string(l),
                    StraitsEastVisualIds::PROB_OUT_REST + dotModular::EDITOR_TO_ENGINE_LANE[l],
                    std::function<void(redDot::GoldPolyPort*)>(themeOut));
        }
        // Direction gate-mod jacks (input_dir_mod_<lane>) — poly, gate cycles Fwd→Rev→Pend→PingPong.
        // Delegation gate-mod jacks (input_deleg_mod_<lane>) — poly, gate flips local/delegated.
        // All 6 lanes (MEL/OCT/REST/ACC/VAR/LEG).
        {
            Module* mod = module;
            auto themeIn = [mod](redDot::GoldPolyPort* p) {
                p->lightTheme = [mod]() { Monsoon* m = mod ? redDot::findMonsoonEitherSide(mod) : nullptr;
                                          return m && m->lightTheme; };
            };
            for (int lane = 0; lane < 6; ++lane) {
                bindInput<redDot::GoldPolyPort>("input_dir_mod_" + std::to_string(lane),
                    dirModId(lane), std::function<void(redDot::GoldPolyPort*)>(themeIn));
                bindInput<redDot::GoldPolyPort>("input_deleg_mod_" + std::to_string(lane),
                    delegModId(lane), std::function<void(redDot::GoldPolyPort*)>(themeIn));
            }
        }

        // ── Controls bound by id from the SVG kit (#components in
        //    gen_east_clean.py). Marker index == enum value:
        //      input_<n>  n = cvId(lane,c) = 0 + lane*4 + c  (CV jacks, 0..15, 4 per lane)
        //      param_<n>  n = attenDispId(lane,c) = 4 + lane*4 + c (attens, 4..19)
        //      param_<n>  n = SPREAD_R/M/O/A = 0/1/2/3              (spread trimpots)
        for (int r = 0; r < N_ROWS; ++r) {
            Module* mod = module;
            auto themeCfg = [mod](redDot::GoldPolyPort* p) {
                p->lightTheme = [mod]() { Monsoon* m = mod ? redDot::findMonsoonEitherSide(mod) : nullptr;
                                          return m && m->lightTheme; };
            };
            for (int c = 0; c < 4; ++c)
                bindInput<redDot::GoldPolyPort>("input_" + std::to_string(cvId(r,c)), cvId(r,c),
                    std::function<void(redDot::GoldPolyPort*)>(themeCfg));
            // CV-depth attenuverters: STORE-BACKED (MVC step 1d). East's own controls, always
            // live. Live slot resolution: currentSlot() (0=V1, 1..15=poly) per-call, so a tab
            // switch re-targets the same knob (no proxy + flush). Was the attenDispId param.
            static const char* LN[4] = {"MEL","OCT","REST","ACC"};
            static const char* CN[4] = {"Len","Off","Rot","Spr"};
            for (int c = 0; c < 4; ++c) {
                const int aLane = r, aCol = c;
                redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    "param_" + std::to_string(attenDispId(r,c)),
                    [this](){ return getMonsoon(); },
                    -1.f, 1.f, 0.f, std::string(LN[r])+" "+CN[c]+" depth",
                    [this, aLane, aCol](Monsoon& m)          { return m.getMacroAtten(currentSlot(), aLane*4 + aCol); },
                    [this, aLane, aCol](Monsoon& m, float v) { m.setMacroAtten(currentSlot(), aLane*4 + aCol, v); });
            }
        }

        // VARIATION / LEGATO poly CV (LEN/OFF/ROT only — no SPR). Same bind pattern as
        // the 4 lanes: gold poly jacks (theme-follow) + always-live depth attenuverters.
        // ch0 = mono/V1 mix-in, ch1+ = poly voices (applied in the expander manager).
        {
            Module* mod = module;
            auto themeCfg = [mod](redDot::GoldPolyPort* p) {
                p->lightTheme = [mod]() { Monsoon* m = mod ? redDot::findMonsoonEitherSide(mod) : nullptr;
                                          return m && m->lightTheme; };
            };
            for (int lane = 0; lane < 2; ++lane)
                for (int c = 0; c < 3; ++c)
                    bindInput<redDot::GoldPolyPort>("input_" + std::to_string(varlegCvId(lane,c)),
                        varlegCvId(lane,c), std::function<void(redDot::GoldPolyPort*)>(themeCfg));
            // VAR/LEG CV-depth: STORE-BACKED (MVC step 1d). editor.varlegAtten[currentSlot(), lane, col]
            // (V1 → slot 0; poly → polySlot). Live slot resolution per tab. Was varlegAttDispId param.
            static const char* vlN[2] = {"VAR","LEG"};
            static const char* vlC[3] = {"Len","Off","Rot"};
            for (int lane = 0; lane < 2; ++lane)
                for (int c = 0; c < 3; ++c) {
                    const int vlLane = lane, vlCol = c;
                    redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                        "param_" + std::to_string(varlegAttDispId(lane,c)),
                        [this](){ return getMonsoon(); },
                        -1.f, 1.f, 0.f, std::string(vlN[lane])+" "+vlC[c]+" depth",
                        [this, vlLane, vlCol](Monsoon& m)          { return m.getVarlegAtten(currentSlot(), vlLane, vlCol); },
                        [this, vlLane, vlCol](Monsoon& m, float v) { m.setVarlegAtten(currentSlot(), vlLane, vlCol, v); });
                }
        }
        // Spread base: STORE-BACKED (MVC step 1d). editor.spread[currentSlot(), lane] (V1 → slot 0
        // = Mono's spread, locked; poly → polySlot). The per-frame push syncs editor.spread →
        // SpreadManager (the engine's poly spread source). lockWhen/displayValueFn carry over.
        static const int sprPid[4] = { SPREAD_R, SPREAD_M, SPREAD_O, SPREAD_A };
        static const char* sprN[4] = {"REST","MEL","OCT","ACC"};
        for (int lane = 0; lane < 4; ++lane) {
            auto* k = redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                "param_" + std::to_string(sprPid[lane]),
                [this](){ return getMonsoon(); },
                -1.f, 1.f, 0.f, std::string(sprN[lane]) + " spread",
                [this, lane](Monsoon& m)          { return m.getSpread(currentSlot(), lane); },
                [this, lane](Monsoon& m, float v) { m.setSpread(currentSlot(), lane, v); });
            if (k) {
                k->lockWhen = [this, lane]() { return laneOwnedByMacroTopo(lane) || tab1MonoMirror(); };
                k->displayValueFn = [this, lane]() { return spreadDisplayValue(lane); };
                pendingSpreadArcs.push_back({k, lane});
            }
        }

        // Macro/East blend controls (bound to the display proxies; copied to/from
        // the per-voice MACRO params on voice switch + each frame). Owner = a
        // latching on/off button (off=Macro owns base, on=East owns). With NO Macro
        // attached, ownership is meaningless (it's all East) — the owner button is
        // inert + dimmed and the sends are dimmed. With Macro attached, sends dim per
        // lane when Macro owns it. (Base-spread / CV-depth are East's own controls and
        // stay live solo — see laneOwnedByMacro above.)
        // Per-lane ownership cell (Option C, treatment A): a lane-step block right
        // of the editor — FILLED = global/Macro owns, OUTLINE = East/per-voice owns.
        // Click toggles. Inert+dimmed with no Macro; hidden on the V1 mono tab.
        // laneColEng indexed by ENGINE lane: 0 REST,1 MEL,2 OCT,3 ACC.
        static const NVGcolor laneColEng[4] = {
            nvgRGB(0x50,0x50,0x50), nvgRGB(0xd4,0xaf,0x37),
            nvgRGB(0xb8,0x86,0x0b), nvgRGB(0xff,0x95,0x00)
        };
        for (int lane = 0; lane < 4; ++lane) {
            // STORE-BACKED (MVC step 1d): OwnerCell reads/writes editor.macroOwn via
            // get/setMacroOwn (poly) or get/setMonoMacroOwn (V1). V1+Mono (tab1MonoMirror) is
            // locked and DISPLAYS Mono's owner (getMonoOwner, engine→editor lane conversion).
            // Live slot resolution: onMonoTab()/polyVoice() per tab. Was ownerDispId param.
            bindWidget<OwnerCell>(
                "param_owner_" + std::to_string(lane),
                [this, lane](OwnerCell* w) {
                    w->laneCol = laneColEng[lane];
                    Vec ctr = w->box.pos.plus(w->box.size.div(2.f));
                    const float stepW = (ED_W - 2.f*6.f) / 16.f;
                    w->box.size = mm2px(Vec(stepW, ED_LANE_H * 0.9f));
                    w->box.pos  = ctr.minus(w->box.size.div(2.f));
                    const int ocLane = lane;
                    w->getOwnsFn = [this, ocLane]() {
                        Monsoon* m = getMonsoon(); if (!m) return true;
                        if (tab1MonoMirror()) return m->getMonoOwner(dotModular::ENGINE_LANE_TO_EDITOR[ocLane]);
                        if (onMonoTab()) return m->getMonoMacroOwn(ocLane) > 0.5f;
                        int pv = polyVoice();
                        return (pv >= 0 && pv < 15) ? (m->getMacroOwn(pv, ocLane) > 0.5f) : true;
                    };
                    w->setOwnsFn = [this, ocLane](bool b) {
                        Monsoon* m = getMonsoon(); if (!m) return;
                        if (onMonoTab()) { m->setMonoMacroOwn(ocLane, b); return; }
                        int pv = polyVoice();
                        if (pv >= 0 && pv < 15) m->setMacroOwn(pv, ocLane, b ? 1.f : 0.f);
                    };
                    w->pushUndoFn = [this, ocLane](bool oldB, bool newB) {
                        Monsoon* m = getMonsoon(); if (!m) return;
                        const bool mono = onMonoTab();
                        const int  pv   = mono ? -1 : polyVoice();
                        if (!mono && (pv < 0 || pv >= 15)) return;
                        redDot::applyAndPushStoreEdit<Monsoon>(m, "lane owner",
                            [ocLane, mono, pv](Monsoon& mm, float val) {
                                if (mono) mm.setMonoMacroOwn(ocLane, val > 0.5f);
                                else      mm.setMacroOwn(pv, ocLane, val);
                            },
                            oldB ? 1.f : 0.f, newB ? 1.f : 0.f);
                    };
                    // Locked when no Macro (nothing to delegate to) OR V1+Mono (Mono owns V1).
                    w->lockWhen = [this](){ return !macroAttached() || tab1MonoMirror(); };
                }
            );
        }

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 15, 0);

        // VAR/LEG delegation cells (editor lanes 4/5): same lane-end OwnerCell, but the
        // delegation target is MONO (always present), so FILLED = follow mono, OUTLINE =
        // Local East. Locked on the V1/mono tab (V1 is mono — nothing to delegate). Anchored
        // to param_owner_4/5 in the SRC column, one LANE_H below the poly lanes.
        static const NVGcolor varlegCol[2] = {
            nvgRGB(0xff,0x6b,0x6b),   // VARIATION (matches editor colors.variation)
            nvgRGB(0x26,0xa6,0x9a)    // LEGATO    (matches editor colors.legato)
        };
        for (int lane = 0; lane < 2; ++lane) {
            // STORE-BACKED (MVC step 1d): OwnerCell reads/writes editor.varlegDeleg via
            // get/setVarlegDeleg(polyVoice, lane) — POLY-ONLY (no mono slot; V1 follows mono,
            // locked). Live slot resolution per tab. Was varlegDelegDispId param.
            bindWidget<OwnerCell>(
                "param_owner_" + std::to_string(4 + lane),
                [this, lane](OwnerCell* w) {
                    w->laneCol = varlegCol[lane];
                    Vec ctr = w->box.pos.plus(w->box.size.div(2.f));
                    const float stepW = (ED_W - 2.f*6.f) / 16.f;
                    w->box.size = mm2px(Vec(stepW, ED_LANE_H * 0.9f));
                    w->box.pos  = ctr.minus(w->box.size.div(2.f));
                    const int vlLane = lane;
                    w->getOwnsFn = [this, vlLane]() {
                        if (onMonoTab()) return false;   // V1 follows mono (no mono slot)
                        Monsoon* m = getMonsoon(); int pv = polyVoice();
                        return (m && pv >= 0 && pv < 15) ? (m->getVarlegDeleg(pv, vlLane) > 0.5f) : false;
                    };
                    w->setOwnsFn = [this, vlLane](bool b) {
                        if (onMonoTab()) return;   // locked on V1
                        Monsoon* m = getMonsoon(); int pv = polyVoice();
                        if (m && pv >= 0 && pv < 15) m->setVarlegDeleg(pv, vlLane, b ? 1.f : 0.f);
                    };
                    w->pushUndoFn = [this, vlLane](bool oldB, bool newB) {
                        if (onMonoTab()) return;   // locked on V1
                        Monsoon* m = getMonsoon(); int pv = polyVoice();
                        if (!m || pv < 0 || pv >= 15) return;
                        redDot::applyAndPushStoreEdit<Monsoon>(m, "varleg deleg",
                            [vlLane, pv](Monsoon& mm, float val) { mm.setVarlegDeleg(pv, vlLane, val); },
                            oldB ? 1.f : 0.f, newB ? 1.f : 0.f);
                    };
                    w->lockWhen = [this](){ return onMonoTab(); };
                }
            );
        }
        // Direction cells (param_dir_<lane>) — per-lane direction toggle (Fwd/Rev/Pend/PingPong).
        // Locked when the lane is delegated (not locally owned): direction follows the delegated
        // owner and can't be overridden. Lanes 0..3 (poly): locked when Macro owns. Lanes 4..5
        // (VAR/LEG): locked on the mono tab (V1 follows mono's direction).
        static const NVGcolor dirCol[6] = {
            nvgRGB(0xd4,0xaf,0x37), nvgRGB(0xb8,0x86,0x0b),  // MEL gold, OCT dark gold
            nvgRGB(0x50,0x50,0x50), nvgRGB(0xff,0x95,0x00),  // REST grey, ACC orange
            nvgRGB(0xff,0x6b,0x6b), nvgRGB(0x26,0xa6,0x9a)   // VAR red, LEG teal
        };
        for (int lane = 0; lane < 6; ++lane) {
            // STORE-BACKED (MVC step 1d): DirCell reads/writes editor.laneDir via get/setLaneDir
            // (poly tab) or get/setMonoLaneDir (V1 tab) — the same store syncDirBank wrote, now
            // read live. Live slot resolution: onMonoTab()/polyVoice() evaluated per-call so a tab
            // switch re-targets the same cell (no proxy + flush). Was the dirDispId param (removed).
            bindWidget<DirCell>(
                "param_dir_" + std::to_string(lane),
                [this, lane](DirCell* w) {
                    w->laneCol = dirCol[lane];
                    Vec ctr = w->box.pos.plus(w->box.size.div(2.f));
                    const float stepW = (ED_W - 2.f*6.f) / 16.f;
                    w->box.size = mm2px(Vec(stepW, ED_LANE_H * 0.9f));
                    w->box.pos  = ctr.minus(w->box.size.div(2.f));
                    const int dcLane = lane;
                    w->getStateFn = [this, dcLane]() {
                        Monsoon* m = getMonsoon(); if (!m) return 0;
                        if (onMonoTab()) return (int)std::lround(m->getMonoLaneDir(dcLane));
                        int pv = polyVoice();
                        return (pv >= 0 && pv < 15) ? (int)std::lround(m->getLaneDir(pv, dcLane)) : 0;
                    };
                    w->setStateFn = [this, dcLane](int v) {
                        Monsoon* m = getMonsoon(); if (!m) return;
                        if (onMonoTab()) { m->setMonoLaneDir(dcLane, (float)v); return; }
                        int pv = polyVoice();
                        if (pv >= 0 && pv < 15) m->setLaneDir(pv, dcLane, (float)v);
                    };
                    // Undo hook: route a direction cycle through Rack history (Ctrl+Z). Captures
                    // the resolved store target at click time (mono vs poly, which voice/lane).
                    w->pushUndoFn = [this, dcLane](int oldV, int newV) {
                        Monsoon* m = getMonsoon(); if (!m) return;
                        const bool mono = onMonoTab();
                        const int  pv   = mono ? -1 : polyVoice();
                        if (!mono && (pv < 0 || pv >= 15)) return;
                        redDot::applyAndPushStoreEdit<Monsoon>(m, "direction",
                            [dcLane, mono, pv](Monsoon& mm, float val) {
                                if (mono) mm.setMonoLaneDir(dcLane, val);
                                else      mm.setLaneDir(pv, dcLane, val);
                            },
                            (float)oldV, (float)newV);
                    };
                    // Lanes 0..3 (MEL/OCT/REST/ACC): locked when Macro owns the lane
                    // (delegated) OR on the V1/mono tab with Mono attached (tab1MonoMirror).
                    // NOTE: laneOwnedByMacroTopo takes an ENGINE lane (REST=0,MEL=1,OCT=2,ACC=3),
                    // but `lane` here is an EDITOR lane (MEL=0,OCT=1,REST=2,ACC=3). Convert via
                    // EDITOR_TO_ENGINE_LANE to avoid the MEL↔OCT↔REST circular permutation.
                    // Lanes 4..5 (VAR/LEG): on mono tab, locked when tab1MonoMirror (V1 follows
                    // Mono). On poly tabs, locked when the lane is delegated to mono (follows
                    // mono's direction) — checked via varlegDelegDispId (0 = delegated).
                    if (lane < 4)
                        w->lockWhen = [this, lane]() {
                            int engLane = dotModular::EDITOR_TO_ENGINE_LANE[lane];
                            return laneOwnedByMacroTopo(engLane) || tab1MonoMirror();
                        };
                    else
                        w->lockWhen = [this, lane]() {
                            if (tab1MonoMirror()) return true;
                            if (!onMonoTab() && selectedVoice >= 1) {
                                int vl = lane - 4;   // 0=VAR, 1=LEG
                                Monsoon* m = getMonsoon(); return m && (m->getVarlegDeleg(polyVoice(), vl) < 0.5f);
                            }
                            return false;
                        };
                }
            );
        }
        flushSpreadArcs();

        // dot.modular connect mark (brand mark; greyed when no Monsoon attached).
        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            addChild(connectMark);
        }
    }

    ~StraitsEastSandsVisualWidget() override { delete paramMgr; }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* mod = dynamic_cast<StraitsEastSandsVisual*>(module);
        if (!mod) return;
        // Probability-out config lives on Monsoon (single source of truth).
    }

    // Unified per-slot store <-> editor for LOR + spread. slot = voiceSlot (0 = V1/mono,
    // 1..15 = V2..V16). V1 and every poly voice run THIS code — the only difference is the slot
    // number. Owner/atten/send keep their own load/save (saveVoiceMacro); the mono-tab master
    // display is mirrorMonoExtraLanes, an engine->display path, not a store load (Stage 3).
    void saveSlot(int slot) {   // editor -> store
        if (!module || !visualEditor) return;
        auto* mm = getMonsoon(); if (!mm) return;
        for (int el=0; el<dotModular::SandsGrid::EAST_LANES; ++el) {
            const auto& lane = visualEditor->currentState.lanes[el];
            const int bank = lorBank(el);
            mm->setLorBase(slot, bank, 0, (float)lane.length);
            mm->setLorBase(slot, bank, 1, (float)lane.offset);
            mm->setLorBase(slot, bank, 2, (float)lane.rotation);
        }
        // (spread proxy→editor.spread flush DELETED — MVC step 1d: store-backed knob writes editor.spread directly.)
    }
    void loadSlot(int slot) {   // store -> editor
        if (!module || !visualEditor) return;
        auto* mm = getMonsoon(); if (!mm) return;
        for (int el=0; el<dotModular::SandsGrid::EAST_LANES; ++el) {
            auto& lane = visualEditor->currentState.lanes[el];
            const int bank = lorBank(el);
            lane.length   = std::max(1,(int)std::round(mm->getLorBase(slot, bank, 0)));
            lane.offset   = (int)std::round(mm->getLorBase(slot, bank, 1));
            lane.rotation = (int)std::round(mm->getLorBase(slot, bank, 2));
        }
        // (spread store→proxy load DELETED — MVC step 1d: store-backed knob reads editor.spread directly.)
    }
    // (saveVoiceMacro/loadVoiceMacro DELETED — MVC step 1d: all four groups (owner, varlegDeleg,
    //  atten, varlegAtten) are store-backed, so the per-voice proxy↔bank flush apparatus is dead.)
    // Iterate EDITOR lanes 0..5 directly — currentState.lanes[] is editor-indexed, and VAR(4)/LEG(5)
    // have no PolyLane id, so the old engine-lane loop could not reach them. lorIdEditor() maps.
    // On the V1/mono tab, editor lanes 4/5 DISPLAY mono's VARIATION/LEGATO (they are mono strands;
    // East never owns them). The strands were renumbered so editor lane == strand index
    // (MEL 0, OCT 1, REST 2, ACC 3, VAR 4, LEG 5), hence lorStore_[0][el] is mono's own LOR column.
    // Read-only: laneEditBlockedFn() locks these lanes whenever onMonoTab().
    void mirrorMonoExtraLanes() {
        Monsoon* m = getMonsoon();
        if (!m || !visualEditor || !onMonoTab()) return;
        for (int el = dotModular::SandsGrid::POLY_LANES; el < dotModular::SandsGrid::EAST_LANES; ++el) {
            const int strand = el;   // identity, by the renumber above
            int mLen = std::max(1, m->engine.lor(strand, SequencerEngine::LOR_LEN));
            int mOff = m->engine.lor(strand, SequencerEngine::LOR_OFF);
            int mRot = m->engine.lor(strand, SequencerEngine::LOR_ROT);
            auto& lane = visualEditor->currentState.lanes[el];
            lane.setDisplayLOR(mLen, mOff, mRot);
            for (int st = 0; st < SandsVisualEditorV4::STEP_COUNT; ++st)
                lane.probabilities[st] = m->engine.pe.finalRandomByStrand(strand, st);
            int ph580 = (m->engine.stepIndex >= 0) ? m->engine.laneTick_[strand] : -1;
            visualEditor->setLanePlayStep(el, calcPlayhead(ph580, mLen, mOff, mRot));
        }
    }

    void onVoiceTabChanged(int nv) {
        if (!paramMgr || !visualEditor) return;
        // Save the OUTGOING voice's edits.
        // Poly tabs always save. Mono tab saves when it is editable (no Mono attached).
        if (selectedVoice >= 1) {
            paramMgr->syncEditorToPatternEngine(polyVoice(), visualEditor->currentState);
            saveSlot(currentSlot());       // LOR + spread (poly slot; V1 handled in step)
        }
        // V1-editable: nothing to save here — East writes the engine MONO strands
        // directly each frame in step() (V1's true home), so there is no per-voice
        // bank to persist. (The old saveVoiceLOR(0) wrote voice-2's poly bank — wrong.)
        // BUT V1's per-lane OWNERSHIP does persist (monoOwnerId, the spare slot): push the
        // live owner-cell proxy into it when leaving the V1 tab.
        if (selectedVoice == 0) {
            // (V1 owner proxy→setMonoMacroOwn flush DELETED — MVC step 1d: store-backed OwnerCell writes it directly.)
        }
        selectedVoice = nv;
        // Load the INCOMING voice.
        if (selectedVoice >= 1) {
            // (Removed: paramMgr->syncPatternEngineToEditor(...) — master's cleanup/dead-poly-sync
            //  deleted the voice-indexed overload; its job is done by step()'s resolver fill on
            //  the next frame, which covers all four poly lanes. loadSlot carries LOR + spread.)
            loadSlot(currentSlot());       // LOR + spread (poly slot)
            // (DirCell poly display-seed DELETED — MVC step 1d: the store-backed DirCell reads
            // getLaneDir(polyVoice()) live, so a tab switch updates the cell on the next draw.)
        }
        // V1-editable: the editor is refreshed from the engine mono strands by the
        // v1Editable() display branch in step(); no explicit load needed. Owner cell
        // proxy is restored from the persistent mono owner store.
        if (selectedVoice == 0) {
            // Seed editor lanes 4/5 from mono's VARIATION/LEGATO strands BEFORE step()'s v1Editable
            // write runs, otherwise the outgoing poly voice's VAR/LEG would be pushed into mono.
            mirrorMonoExtraLanes();
            // Force the v1Editable seed on the next frame: the editor currently still holds the
            // OUTGOING poly voice's LOR/spread, so we must restore V1's stores rather than mirror
            // that stale data into them.
            v1Loaded_ = false;
            // (V1 owner setMonoMacroOwn→proxy load DELETED — MVC step 1d: store-backed OwnerCell reads it directly.)
            // V1 is mono → VAR/LEG always follow mono; show the cells filled (and they lock).
            // (V1 varlegDeleg proxy mirror DELETED — MVC step 1d: store-backed OwnerCell shows follow-mono on V1.)
            // (V1 DirCell display-seed DELETED — MVC step 1d: the store-backed DirCell reads
            // getMonoLaneDir live on the V1 tab.)
        }
    }

    Monsoon* getMonsoon() const {
        return module ? findMonsoonEitherSide(module) : nullptr;
    }

    // STEP 4c: full ownership authority for East. Populates V1 + all poly owners from the
    // PERSISTENT storage params (monoOwnerId / ownerId), engine-ordered → converted to
    // editor lane so topo speaks editor lane (decision 1). eastPresent=true (this IS East).
    dotModular::SandsTopology buildTopo() const {
        dotModular::SandsTopology::Inputs in;
        if (auto* m = getMonsoon()) {
            // Presence from the single authority (the expander-scan cache), NOT self-assertion.
            // A widget Monsoon hasn't cached is not topologically present — same rule everywhere.
            m->expanderManager.fillPresence(in, m->engine.numPolyVoices);
        }
        if (module) {
            Monsoon* mmT = getMonsoon();
            for (int el = 0; el < 4; ++el) {
                int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
                in.eastV1Owner[el] = mmT ? (mmT->getMonoMacroOwn(eng) > 0.5f) : false;
                for (int pv = 0; pv < 15; ++pv)
                    in.eastPolyOwner[pv][el] = mmT ? (mmT->getMacroOwn(pv, eng) > 0.5f) : false;
            }
            // The CURRENT tab's owner cells live in the display proxy (ownerDispId) and are
            // only flushed to the persistent slot on tab-exit — so for the current voice,
            // read the LIVE proxy, else buildTopo would lag live edits (and the 4c cross-
            // check would fire spuriously). ownerDispId is engine-ordered.
            const int cv = currentVoice() - 1;   // 0 = V1/mono
            for (int el = 0; el < 4; ++el) {
                int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
                bool live = eastOwnsLane(eng);   // MVC step 1d: store-backed (was ownerDispId proxy)
                if (cv == 0)                       in.eastV1Owner[el] = live;
                else if (cv >= 1 && cv <= 15)      in.eastPolyOwner[cv - 1][el] = live;
            }
        }
        return dotModular::SandsTopology::build(in);
    }
    // Macro visual attached on the chain?
    bool macroAttached() {
        Monsoon* m = getMonsoon();
        return m && m->expanderManager.cachedMacroSandsVisual != nullptr;
    }
    // For East's OWN controls (base-spread, CV-depth): inert only when Macro is present
    // AND owns the lane (East base bypassed). Fully usable solo.
    // Topology-backed lane ownership for the CURRENT tab, used by laneEditBlockedFn and the
    // spread-arc lockWhen lambdas so all East lock predicates read one authority.
    // engLane in; converts to editor lane for the resolver.
    bool laneOwnedByMacroTopo(int engLane) {
        const int el = dotModular::ENGINE_LANE_TO_EDITOR[engLane];
        return buildTopo().owner(currentVoice() - 1, el) == dotModular::SandsTopology::Role::MACRO;
    }
    // DISPLAY value for a spread knob (engine lane). When the current voice's lane is ceded
    // to Macro, return Macro's BASE spread (macroBase[lane][3]) so the knob DISPLAYS it —
    // without writing SPREAD_* (the store). Else NaN = show the knob's own stored value.
    // Base only (no CV/modulation — that's a separate display concern). Same -1..1 units.
    float spreadDisplayValue(int engLane) {
        if (!laneOwnedByMacroTopo(engLane)) return std::numeric_limits<float>::quiet_NaN();
        auto* mon = getMonsoon();
        auto* macroVis = mon ? mon->expanderManager.cachedMacroSandsVisual : nullptr;
        if (!macroVis) return std::numeric_limits<float>::quiet_NaN();
        return macroVis->macroBase[engLane][3];
    }
    // MVC step 1d: the current tab's East-owns status for an ENGINE lane (0..3), mirroring the
    // store-backed OwnerCell's getOwnsFn. V1+Mono → Mono's owner (getMonoOwner, eng→editor);
    // V1 (East) → getMonoMacroOwn; poly → getMacroOwn(polyVoice). Used by the spread-arc isActive,
    // buildTopo's live-voice read, the lane-ownership menu, and the owner light.
    bool eastOwnsLane(int engLane) const {
        Monsoon* m = getMonsoon(); if (!m) return true;
        if (tab1MonoMirror()) return m->getMonoOwner(dotModular::ENGINE_LANE_TO_EDITOR[engLane]);
        if (onMonoTab()) return m->getMonoMacroOwn(engLane) > 0.5f;
        int pv = polyVoice();
        return (pv >= 0 && pv < 15) ? (m->getMacroOwn(pv, engLane) > 0.5f) : true;
    }
    // The voice NUMBER (1..16) for the selected tab: tab 0 = V1 (mono), tab v = V(v+1).
    // All mono/poly identity + bank mapping flows through VoiceResolver so there's one
    // source of truth (the resolver), not hand-rolled selectedVoice arithmetic scattered
    // here. The resolver methods are static/constexpr — no engine ref, no per-call cost.
    int currentVoice() const { return selectedVoice + 1; }
    // Unified store slot for the selected tab: 0 = V1/mono, 1..15 = V2..V16. Composes
    // currentVoice() with voiceSlot so V1 and poly index the same lorBase/spread store.
    int currentSlot() const { return dotModular::VoiceResolver::voiceSlot(currentVoice()); }

    // Voice 1 / tab 1 with Sands Mono attached: the lane base belongs to Mono — East's
    // base controls lock + mirror mono (display-only). Independent of Macro.
    bool tab1MonoMirror() const {
        Monsoon* m = getMonsoon();
        return onMonoTab() && m && m->expanderManager.cachedSandsVisualExpander != nullptr;
    }
    // Mono tab? = the selected voice is the mono master strand (resolver owns this).
    bool onMonoTab() const { return dotModular::VoiceResolver::isMono(currentVoice()); }
    // V1 editable: on mono tab AND Sands Mono is NOT attached. East acts as the
    // mono-lane editor for V1 in this case (combinations 3, 7: East without Mono).
    bool v1Editable() const {
        Monsoon* m = getMonsoon();
        if (!m) return false;
        // Only edit/write V1 if Monsoon's AUTHORITATIVE expander scan recognises THIS module as its
        // East visual. Otherwise Monsoon's topology classifies e.g. MACRO_SOLE (eastPresent=false)
        // and its own MACRO path writes the mono strand — while this widget, asserting eastPresent=
        // true from self-knowledge, would ALSO write it: two producers, one strand, per block (the
        // persistent StrandLedger MACRO-then-EAST conflict). Deferring to the authoritative cache
        // makes the two views agree by construction — exactly one writer.
        if (m->expanderManager.cachedEastSandsVisual != module) return false;
        return onMonoTab() && !(m->expanderManager.cachedSandsVisualExpander != nullptr);
    }
    // Poly bank index (0..14) for the selected tab; -1 on the mono tab (resolver-mapped,
    // == the old selectedVoice-1). Use only when !onMonoTab().
    int  polyVoice() const { return dotModular::VoiceResolver::polyBankIndex(currentVoice()); }

    void openLaneOwnershipMenu(int lane, rack::math::Vec editorLocalPos) {
        if (!module) return;
        const int voice = polyVoice();   // current poly bank index (0-based)
        const bool macroOwns = !eastOwnsLane(lane);   // MVC step 1d: store-backed
        static const char* laneNames[4] = { "MELODY", "OCTAVE", "REST", "ACCENT" };
        const char* ln = (lane >= 0 && lane < 4) ? laneNames[lane] : "?";

        Menu* menu = createMenu();
        menu->addChild(createMenuLabel(
            std::string("Lane: ") + ln + "  (V" + std::to_string(voice + 2) + ")"));
        menu->addChild(new MenuSeparator);

        // Toggle ownership for this voice+lane
        struct OwnerItem : MenuItem {
            StraitsEastSandsVisualWidget* widget;
            int lane, voice;
            bool setToEast;   // true = set East owns; false = set Macro owns
            void onAction(const event::Action& e) override {
                // MVC step 1d: write the store directly (was proxy param + saveVoiceMacro flush,
                // both deleted). voice = polyVoice() (0..14); lane = engine lane.
                float val = setToEast ? 1.f : 0.f;
                if (auto* m = widget->getMonsoon()) m->setMacroOwn(voice, lane, val);
            }
        };

        auto* eastItem = new OwnerItem;
        eastItem->text = "East owns this lane";
        eastItem->rightText = (!macroOwns) ? "✓" : "";
        eastItem->widget = this; eastItem->lane = lane; eastItem->voice = voice;
        eastItem->setToEast = true;
        menu->addChild(eastItem);

        auto* macroItem = new OwnerItem;
        macroItem->text = "Macro owns this lane";
        macroItem->rightText = macroOwns ? "✓" : "";
        macroItem->widget = this; macroItem->lane = lane; macroItem->voice = voice;
        macroItem->setToEast = false;
        menu->addChild(macroItem);

        menu->addChild(new MenuSeparator);

        // Set all voices for this lane
        struct AllVoicesItem : MenuItem {
            StraitsEastSandsVisualWidget* widget;
            int lane;
            bool setToEast;
            void onAction(const event::Action& e) override {
                float val = setToEast ? 1.f : 0.f;
                // Set display proxy and persist to all 15 poly voice slots
                // (owner proxy write DELETED — MVC step 1d: setMacroOwn below is the home.)
                if (auto* mmA = redDot::findMonsoonEitherSide(widget->module))
                    for (int v = 0; v < 15; ++v) mmA->setMacroOwn(v, lane, val);
            }
        };

        auto* allEast = new AllVoicesItem;
        allEast->text = "East owns — all voices";
        allEast->widget = this; allEast->lane = lane; allEast->setToEast = true;
        menu->addChild(allEast);

        auto* allMacro = new AllVoicesItem;
        allMacro->text = "Macro owns — all voices";
        allMacro->widget = this; allMacro->lane = lane; allMacro->setToEast = false;
        menu->addChild(allMacro);
    }

    // ── Apply gate-mod edges queued by the audio thread ─────────────────────────
    // The audio thread only counts edges; interpreting them lives here because this is
    // where the display-proxy <-> per-voice-store contract is owned, and where the selected
    // tab is known. Runs early in step() so a proxy write lands before the direction push
    // and any tab-exit flush later in the same frame.
    //
    // Channel n targets exactly what tab n shows (ch0 = V1/mono, ch n = poly bank n-1), so
    // "is this target on screen?" is just `selectedVoice == ch`. That matters because the
    // proxy holds ONLY the selected target's value: write it for anything else and the sync
    // smears that value onto whichever voice is open. So each edge writes the per-target
    // truth (engine for direction, the per-voice owner store for delegation) and touches the
    // proxy only when its target is displayed — for that one the proxy IS the live value.
    uint8_t dirModSeen[6][16]   = {};
    uint8_t delegModSeen[6][16] = {};
    bool    gateModSeenInit     = false;

    void applyGateMods() {
        auto* mod = static_cast<StraitsEastSandsVisual*>(module);
        Monsoon* m = getMonsoon();
        if (!mod || !m) return;
        SequencerEngine* se = &m->engine;
        // First pass after a (re)build: adopt the current counts rather than treating the
        // backlog as fresh edges, which would fire a spurious burst of cycles.
        if (!gateModSeenInit) {
            for (int lane = 0; lane < 6; ++lane)
                for (int ch = 0; ch < 16; ++ch) {
                    dirModSeen[lane][ch]   = mod->dirModEdges[lane][ch];
                    delegModSeen[lane][ch] = mod->delegModEdges[lane][ch];
                }
            gateModSeenInit = true;
            return;
        }
        for (int lane = 0; lane < 6; ++lane) {
            const int strand = dotModular::MONO_LANE_TO_STRAND[lane];
            // ── Direction: cycle Fwd→Rev→Pend→PingPong→Fwd, once per queued edge ──
            // A 1-channel cable BROADCASTS to every target (VCV poly norm): a mono gate means
            // "apply this everywhere", not "apply it to V1" — which is all ch0 would mean under
            // the channel==tab convention. Poly cables stay per-channel.
            const bool dBcast = (mod->dirModChans[lane] == 1);
            for (int ch = 0; ch < 16; ++ch) {
                const int src = dBcast ? 0 : ch;                  // which counter to read
                uint8_t n = mod->dirModEdges[lane][src];
                uint8_t d = (uint8_t)(n - dirModSeen[lane][src]);  // unsigned diff: wrap-safe
                if (!d) continue;
                if (!dBcast || ch == 15) dirModSeen[lane][src] = n;   // broadcast: consume once, after all targets
                // Read the CURRENT value from the engine (the per-target truth), never from
                // the shared proxy — the proxy only ever holds the displayed target's value.
                int nxt;
                if (ch == 0) {
                    // V1/mono: write through the SAME authority the manager reads — Mono's
                    // cell, Macro's cell, or East's own V1 slot. Writing East's slot
                    // unconditionally was the bug: with Mono or Macro attached the manager
                    // pushes THEIR cell at control rate and never reads East's slot, so the
                    // mod silently did nothing on V1 (and lanes 0..3 were the visible case,
                    // because Macro owns exactly those).
                    auto auth = m->expanderManager.monoDirAuthority(strand);
                    if (!auth.valid()) continue;   // nobody owns this lane — nothing to cycle
                    // MVC step 1d: all authorities are field-backed now. macroGlobal → Macro's
                    // globalDir; else the mono-lane dir (Mono/East V1). Param-backed branch is dead.
                    int cur;
                    if (auth.macroGlobal)      cur = (int)std::lround(math::clamp(m->getGlobalDir(auth.eastMonoLane), 0.f, 3.f));
                    else if (auth.isField())   cur = (int)std::lround(math::clamp(m->getMonoLaneDir(auth.eastMonoLane), 0.f, 3.f));
                    else                       cur = (int)std::lround(math::clamp(auth.mod->params[auth.paramId].getValue(), 0.f, 3.f));
                    nxt = (cur + d) % 4;
                    if (auth.macroGlobal)      m->setGlobalDir(auth.eastMonoLane, (float)nxt);
                    else if (auth.isField())   m->setMonoLaneDir(auth.eastMonoLane, (float)nxt);
                    else                       auth.mod->params[auth.paramId].setValue((float)nxt);
                } else {
                    const int pv = ch - 1;
                    nxt = (((int)se->laneDirVPending_[pv][strand]) + d) % 4;
                    se->laneDirVPending_[pv][strand] = (SequencerEngine::LaneDir)nxt;
                }
                // Poly voices: persist to the target's BANK slot — syncDirBank() only ever
                // sees the proxy, i.e. the displayed voice, so a mod aimed at any other voice
                // would never reach the bank. ch0 is not here: its store was already written
                // through monoDirAuthority above (which IS East's monoDirId when East owns V1,
                // and Mono's/Macro's cell otherwise — writing monoDirId unconditionally would
                // just poke a store nobody reads).
                if (ch != 0) { if (auto* m = findMonsoonEitherSide(mod)) m->setLaneDir(ch - 1, lane, (float)nxt); }
                // (dirDispId proxy write DELETED — MVC step 1d: the store-backed DirCell reads
                // getLaneDir live; the store write above is what the cell now displays.)
            }
            // ── Delegation: flip local/delegated in the voice's OWN owner store ──
            // lanes 0..3 -> ownerId(pv, engineLane) (V1 uses its own monoOwnerId slot);
            // lanes 4..5 -> varlegDelegId(pv, VAR|LEG), which is poly-only: mono's
            // VARIATION/LEGATO are mono strands East never owns, so ch0 is a no-op there.
            const int eng = (lane < 4) ? dotModular::EDITOR_TO_ENGINE_LANE[lane] : -1;
            const bool gBcast = (mod->delegModChans[lane] == 1);
            for (int ch = 0; ch < 16; ++ch) {
                const int src = gBcast ? 0 : ch;
                uint8_t n = mod->delegModEdges[lane][src];
                uint8_t d = (uint8_t)(n - delegModSeen[lane][src]);
                if (!d) continue;
                if (!gBcast || ch == 15) delegModSeen[lane][src] = n;
                if (!(d & 1)) continue;          // an even number of flips is a no-op
                if (lane >= 4 && ch == 0) continue;   // mono has no VAR/LEG delegation
                if (lane < 4) {
                    // owner delegation (MACRO_OWN) migrated to Monsoon::editor.macroOwn
                    if (auto* mm = findMonsoonEitherSide(mod)) {
                        const float cur = (ch == 0) ? mm->getMonoMacroOwn(eng) : mm->getMacroOwn(ch - 1, eng);
                        const float nv = (cur > 0.5f) ? 0.f : 1.f;
                        if (ch == 0) mm->setMonoMacroOwn(eng, nv); else mm->setMacroOwn(ch - 1, eng, nv);
                        // (owner proxy write DELETED — MVC step 1d: setMonoMacroOwn/setMacroOwn above is the home.)
                    }
                } else {
                    // VAR/LEG delegation migrated to Monsoon::editor.varlegDeleg (fields)
                    if (auto* mm = findMonsoonEitherSide(mod)) {
                        const int vlLane = lane - 4;
                        const float nv = (mm->getVarlegDeleg(ch - 1, vlLane) > 0.5f) ? 0.f : 1.f;
                        mm->setVarlegDeleg(ch - 1, vlLane, nv);
                        // (varlegDeleg proxy write DELETED — MVC step 1d: setVarlegDeleg above is the home.)
                    }
                }
            }
        }
    }

    // ── Step 2 (plans/lane_direction_homes.md): keep East's direction BANK correct ──────
    // DirCell is a ParamWidget bound to the shared proxy (a ParamWidget binds one paramId at
    // construction, so a per-voice control needs proxy + tab copy) and so cannot know which
    // voice it is editing. Persist any proxy change into the CURRENT voice's bank slot the
    // frame it happens — the continuous equivalent of the tab-exit flush. Writes only on
    // (syncDirBank + lastDirDisp + dirDispInit DELETED — MVC step 1d: the DirCell writes the
    // store directly via get/setLaneDir, so the proxy→bank flush + clobber-guard are dead.)

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }
        applyGateMods();

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

        // Blend controls dim themselves (self-contained, via DimmableTrimpot/
        // DimmableLatch predicates): the owner button + Macro-sends go dim+inert with
        // no Macro attached or (sends) when Macro owns the lane; base-spread / CV-depth
        // dim only when Macro owns the lane (they're East's own, live solo). No
        // per-frame visibility work needed here.

        // (mod unused after de-param — store-backed widgets read Monsoon directly via getMonsoon().)

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
        // Active tabs = mono (always, index 0) + the active poly voices. So tab i is
        // enabled for i <= numPolyVoices (i=0 mono; i=1..numPolyVoices poly).
        if (tabGroup) tabGroup->setActiveCount(dotModular::VoiceResolver(*se).activeVoiceCount());

        if (!initialized) {
            if (selectedVoice >= 1) {
                loadSlot(currentSlot());   // LOR + spread
            }
            initialized = true;
        }

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        // Per-frame write-back of display proxies → the selected voice's params, so edits
        // take effect immediately. Poly tabs only — the mono tab (index 0) is display-only
        // (its base lives on Sands Mono); writing it back would corrupt poly slot 0.
        if (selectedVoice >= 1) {
            // (per-frame saveVoiceMacro DELETED — MVC step 1d: store-backed knobs/cells write directly.)

            // DISPLAY/STORE/ENGINE separation (see DISPLAY_STORE_ENGINE_SEPARATION.md):
            //  - STORE:   SPREAD_* param → *InterpId + smgr. Always East's real value now —
            //             the knob no longer force-overwrites the param (a ceded lane's knob
            //             DISPLAYS Macro's base via DimmableTrimpot.displayValueFn instead).
            //  - ENGINE:  combineSpread arbitrates at playback (ceded → Macro base+CV, owned →
            //             East store), independent of this store. So writing East's value here
            //             for a ceded lane is harmless — the engine plays Macro regardless.
            //  - Result: cede shows/plays Macro, store keeps East → reclaim reverts. No guards.
            // MVC step 1d: sync editor.spread[polySlot] → SpreadManager (the engine's poly spread
            // source). Was params[SPREAD_*]; the store-backed knob writes editor.spread directly.
            auto& smgr = paramMgr->spreadMgr;
            const int ps = currentSlot();
            for (int lane = 0; lane < 4; ++lane)
                smgr.setSpread(polyVoice(), lane, monsoon->getSpread(ps, lane));
        }
        // ── Direction sync: push the DirCell display proxy values into the engine's
        //    laneDirPending_ (mono tab) or laneDirVPending_ (poly tabs). The DirCell
        //    writes the display proxy (dirDispId); the engine reads laneDirPending_ /
        //    laneDirVPending_. This per-frame sync makes a click on the DirCell take
        //    effect immediately, exactly like saveVoiceMacro syncs owner cells.
        //
        //    ONLY laneDirPending_ / laneDirVPending_ is pushed here. The engine's
        //    advancePlayhead derives laneSign_ from laneDir_ for Forward/Reverse, and
        //    manages the sign internally for Pendulum/PingPong bounces (flipping
        //    laneSignPending_ at the LOR endpoint). If we also pushed laneSignPending_
        //    every frame, we would overwrite the bounce-induced sign flip with
        //    laneDirSign(Pendulum) = +1, undoing the bounce at the next promotion.
        if (onMonoTab() && !tab1MonoMirror()) {
            // V1 editable (no Mono attached): East IS the mono editor.
            // Step 4: NO sync needed. The DirCell writes dirDispId; syncDirBank()
            // persists it to monoDirId(lane); the manager reads monoDirId and pushes
            // to laneDirPending_. The widget must NOT overwrite dirDispId FROM engine.
        } else if (onMonoTab() && tab1MonoMirror()) {
            // Mono attached: Mono is the mono-direction authority. MVC step 1d: the store-backed
            // DirCell reads getMonoLaneDir live (the slot Mono writes), so no per-frame proxy
            // sync from the engine is needed.
        } else if (selectedVoice >= 1) {
            // Step 3 (plans/lane_direction_homes.md): the poly push is GONE. East's direction
            // BANK is the home now and MonsoonExpanderManager::sync() pushes it into the engine
            // module-side, exactly as it already does for LOR and delegation. syncDirBank()
            // persists a DirCell edit into this voice's bank slot; the engine is derived.
            //
            // Deleting this is the point of the step: an unconditional per-frame push made the
            // display proxy an AUTHORITY, which is what smeared one voice's direction onto
            // another and made gate-mods fight the frame loop. A view must not write its model
            // on a timer.
            // (Mono still pushes above until step 4 moves it to the Mono expander.)
        }
        // (Spread target mode is now pulled from the engine by SpreadManager —
        // Monsoon::process mirrors the menu setting onto engine.pe each frame. No
        // per-widget push needed.)

        // CV applied at control rate in Monsoon::process() — base + scaled offset, no mutation here.

        if (selectedVoice >= 1) {
            // Per-frame LOR + spread push so grid edits and spread-knob turns take effect
            // LIVE (spread esp. ACCENT previously only mutated on tab change). One call now.
            // (Removed: paramMgr->syncPatternEngineToEditor(...) — master's cleanup/dead-poly-sync
            //  deleted the voice-indexed overload; it wrote REST/MEL/OCT from PRE-spread interp
            //  values, missing ACCENT, and every lane is overwritten by the resolver fill
            //  immediately below in this same block anyway.)
            saveSlot(currentSlot());
            // The editor's drag only edits the LOR WINDOW (length/offset/rotation), never
            // individual step probabilities — those are display-only. So show the
            // SPREAD-APPLIED probabilities (polyRhythmRandom etc., what actually plays)
            // for EVERY lane, not just Macro-ceded ones. Previously East-owned lanes kept
            // the raw drawn pattern, so moving spread changed the audio but NOT the
            // visible bars. Reading the resolver (post-spread) makes spread visible and
            // also fixes the blank-lane case under Macro ownership.
            dotModular::VoiceResolver resolver(monsoon->engine);
            const int vnum = currentVoice();
            for (int lane = 0; lane < 4; ++lane) {
                int el = dotModular::ENGINE_LANE_TO_EDITOR[lane];
                for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s)
                    visualEditor->currentState.lanes[el].probabilities[s] =
                        resolver.laneProbabilityAtStep(vnum, lane, s);
            }
        }

        // Surface the engine's CV-APPLIED L/O/R to the display window so the
        // highlighted range + offset/rotation markers track L/O/R CV modulation.
        // engine.polyLen/Off/Rot[voice][lane] (lane 0/1/2 = REST/MEL/OCT) hold the
        // post-CV values. With no CV these equal the edit values. Editing/drag use
        // the EDIT values, so this is display-only.
        auto& eng = monsoon->engine;
        // Per-lane direction cue: mono tab uses laneSign_, poly tabs use the per-voice
        // sign directly (absolute direction, not relative to mono).
        if (onMonoTab())
            for (int l = 0; l < 6; ++l) visualEditor->setLanePlayDir(l, eng.lastPlayDir * eng.laneSign_[l]);
        else {
            int pv = polyVoice();
            for (int l = 0; l < 6; ++l) {
                int strand = dotModular::MONO_LANE_TO_STRAND[l];
                visualEditor->setLanePlayDir(l, eng.lastPlayDir * eng.polyLaneSign(pv, strand));
            }
        }
        // TAB-1 MONO MIRROR: when Sands Mono is attached, voice 1 / tab 1 follows the
        // mono master strand — its LORS base belongs to Mono, not East. Show mono's
        // values read-only (consistent with the other lanes' display), and lock the
        // editor so the base can't be edited here (edit it on Sands Mono). The base-
        // spread knob locks via tab1MonoMirror() (see laneOwnedByMacro/lock predicates).
        // (Per-voice modulation folding onto voice 1 — interp. Y — is the deferred
        //  follow-up; this stage is the display/lock mirror only.)
        auto* monoVis = monsoon->expanderManager.cachedSandsVisualExpander;
        bool tab1Mono = onMonoTab() && (monoVis != nullptr);
        // readOnly: only when Mono is attached (it owns V1). When V1 is editable
        // (no Mono), the editor is live and the user edits V1's lanes directly here.
        visualEditor->readOnly = tab1Mono;
        if (tab1Mono) {
            // Show Mono's base LOR for all 4 poly lanes (Mono params are editor-ordered:
            // MEL=0 OCT=1 REST=2 ACC=3 → editor lane == param index). V1 base belongs to
            // Mono and is inoperable on East (locked by laneEditBlockedFn / readOnly). The mod
            // arriving at East is shown by the V1 mod arcs (P6), not folded into this base.
            // Show the V1 LOR for all 4 poly lanes. Read the engine MONO STRAND (which
            // the manager has already written with Mono's base + East's V1 CV + Macro CV)
            // so East's V1 LOR display REFLECTS the incoming modulation — matching Mono's
            // CV-applied display. (Previously read Mono's static base params, so LOR mod
            // arriving via East showed on Mono but not here.) editor lane → engine strand.
            for (int l=0; l<4; ++l) {
                int strand  = dotModular::MONO_LANE_TO_STRAND[l];   // editor lane → engine strand
                int mLen = monsoon->engine.strandLen(strand);
                int mOff = monsoon->engine.strandOff(strand);
                int mRot = monsoon->engine.strandRot(strand);
                visualEditor->currentState.lanes[l].setDisplayLOR(std::max(1,mLen), mOff, mRot);
                int strand_m = dotModular::MONO_LANE_TO_STRAND[l];
                int ph1130 = (eng.stepIndex >= 0) ? eng.laneTick_[strand_m] : -1;
                visualEditor->setLanePlayStep(l, calcPlayhead(ph1130, std::max(1,mLen), mOff, mRot));
            }
            // Spread (combo 7): East's V1 spread knobs FOLLOW Mono's spread (inoperable,
            // locked). Mono's spread base is now STORE-BACKED (editor.spread[kMonoSlot,l],
            // MVC step 1d) — was params[sprId(l)], now read the store. l is engine/spread
            // order (REST=0,MEL=1,OCT=2,ACC=3); East's SPREAD_R/M/O/A match, so copy directly.
            // This makes the knob track Mono and gives the V1 spread arc a real base to deflect from.
            // (V1 SPREAD_*←getSpread mirror DELETED — MVC step 1d: store-backed knob reads editor.spread[0] directly.)
            // CV-DEPTH attenuators on V1: these are East's OWN modulation controls (the
            // user patches CV into East + sets depth to modulate V1). saveVoiceMacro only
            // ever writes POLY slots, so the mono slot (kMonoSlot=0) — which the V1 CV
            // mix-in in readStrand reads — stayed 0, making East V1 CV inaudible/invisible.
            // (V1 atten proxy→mono-slot mirror DELETED — MVC step 1d: store-backed knob writes macroAtten[0] directly.)
            // (V1 varleg atten proxy→mono-slot mirror DELETED — MVC step 1d: store-backed knob writes varlegAtten[0] directly.)
            // P8: East's owner cells on V1 are locked (East can't delegate V1) but should
            // SHOW the real V1 ownership, which Mono decides. Mono's owner is STORE-BACKED
            // (editor.monoOwner via getMonoOwner); mirror it into East's ownerDispId so the cell
            // draws filled/outline correctly. East ownerDispId is engine-lane indexed; Mono's
            // store is editor-lane indexed (el).
            // (V1+Mono owner mirror DELETED — MVC step 1d: the store-backed OwnerCell's getOwnsFn
            //  reads getMonoOwner directly for the V1+Mono case, so no proxy mirror is needed.)
            // Probabilities: show the SPREAD-APPLIED V1 values (what plays) for all 4
            // lanes, so Mono's spread/CV AND East's V1 spread CV (both folded into the
            // engine mono strand by the manager) are visible — matching Mono's display.
            // Use finalRandomByStrand PER STEP (the resolver's masterLaneProbability only
            // returns the current playhead step, which would flatten all 16 bars). Bars
            // are display-only on V1 (editor readOnly), so overwriting each frame is safe.
            {
                // editor lane → engine poly lane → mono strand index.
                for (int el = 0; el < 4; ++el) {
                    int engLane = dotModular::EDITOR_TO_ENGINE_LANE[el];   // 0..3 = REST/MEL/OCT/ACC
                    int strand  = (engLane == 0) ? dotModular::STRAND_RHYTHM
                                : (engLane == 1) ? dotModular::STRAND_MELODY
                                : (engLane == 2) ? dotModular::STRAND_OCTAVE
                                :                  dotModular::STRAND_ACCENT;
                    for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s)
                        visualEditor->currentState.lanes[el].probabilities[s] =
                            monsoon->engine.pe.finalRandomByStrand(strand, s);
                }
            }
            mirrorMonoExtraLanes();   // lanes 4/5: mono's VARIATION/LEGATO, read-only
        } else if (v1Editable()) {
            // ── V1 (East-alone) persistence ──────────────────────────────────────────────
            // V1 has no per-voice bank (poly voices use lorIdEditor/interp params). Its
            // editable base LOR + spread live only in the editor/display, which
            // loadVoice*(polyVoice) overwrites on a tab switch — so without this, returning
            // to V1 would let the last-visited poly voice's data get written into V1's mono
            // strands (the every-frame writes below), losing V1. Seed the editor/display from
            // the dedicated Monsoon stores on the first frame of each V1 entry (v1Loaded_
            // latched in onVoiceTabChanged / initial), then mirror live edits back each frame
            // so the stores — and thus save/load and the next round-trip — stay current.
            if (getMonsoon()) {
                if (!v1Loaded_) {
                    // Seed editor/display from the unified store slot 0 — the SAME loadSlot()
                    // every poly voice uses, just slot 0. Safe even on a fresh patch: lorBase[0]
                    // is identity (len=16) and spread[0] is 0 (the editor's own defaults), so the
                    // seed is a no-op until V1 has been edited. (Owner/atten aren't in loadSlot;
                    // V1's atten displays are restored from the mono-slot store just below.)
                    loadSlot(dotModular::VoiceResolver::kMonoSlot);
                    // (V1 atten display-seed DELETED — MVC step 1d: store-backed knob reads macroAtten[0] directly.)
                    // (V1 varleg atten display-seed DELETED — MVC step 1d: store-backed knob reads varlegAtten[0] directly.)
                    v1Loaded_ = true;
                } else {
                    saveSlot(dotModular::VoiceResolver::kMonoSlot);   // mirror V1 edits → store slot 0
                }
            }
            // ─────────────────────────────────────────────────────────────────────────────
            // (no per-frame mirror here: East OWNS lanes 4/5 on V1 in this branch and writes them
            //  to the mono strands below. They are seeded once on tab entry — see onVoiceTabChanged.)
            // V1 editable (no Mono, combo 3/7-without-Mono): East IS the V1 editor.
            // Spread-follow is now handled by the knob's displayValueFn (see the SPREAD_*
            // binds + spreadDisplayValue): on a ceded V1 lane the knob DISPLAYS Macro's base
            // spread while SPREAD_* (the store) stays East's value — so reclaim reverts even
            // after Macro's global spread is moved. (On the V1 tab currentVoice()-1==0, so
            // spreadDisplayValue/laneOwnedByMacroTopo already evaluate owner(0,lane) = V1.)
            // The old force of SPREAD_* to macroBase here was the clobber that lost V1's
            // stored spread when Macro's knob moved during a cede — removed.

            // (V1 atten proxy→mono-slot mirror DELETED — MVC step 1d: store-backed knob writes macroAtten[0] directly.)
            // (V1 varleg atten proxy→mono-slot mirror DELETED — MVC step 1d: store-backed knob writes varlegAtten[0] directly.)
            // V1 LOR is now derived in the MANAGER (MonsoonSandsManager hasEastV1 block),
            // reading the Model (lorBase[kMonoSlot]) — same as poly. The direct mono-strand
            // write that used to live here was moved there in Stage 3b. The atten/varleg-depth
            // display->store mirror above stays (View->Model); the manager reads that store.
            // Display: reflect the engine's current mono strand LOR back to the editor.
            // Six lanes now — MONO_LANE_TO_STRAND is [6] and VAR/LEG are East-owned on V1 here.
            for (int el = 0; el < dotModular::SandsGrid::EAST_LANES; ++el) {
                int strand = dotModular::MONO_LANE_TO_STRAND[el];
                int cvLen = eng.strandLenRef(strand);
                int cvOff = eng.strandOffRef(strand);
                int cvRot = eng.strandRotRef(strand);
                visualEditor->currentState.lanes[el].setDisplayLOR(cvLen, cvOff, cvRot);
                int ph1222 = (eng.stepIndex >= 0) ? eng.laneTick_[dotModular::MONO_LANE_TO_STRAND[el]] : -1;
                visualEditor->setLanePlayStep(el, calcPlayhead(ph1222, cvLen, cvOff, cvRot));
            }
            // Probabilities: V1 (mono) probabilities are display-only (drag edits the LOR
            // window only), so show the SPREAD-APPLIED values the sequencer plays. Read
            // PER-STEP from finalRandomByStrand — NOT resolver.laneProbabilityAtStep, which
            // for mono returns masterLaneProbability (the CURRENT step only), making every
            // bar identical (regression). editor lane → engine strand via MONO_LANE_TO_STRAND.
            // All SIX lanes: lanes 4/5 (VAR/LEG) were previously skipped (loop ran el<4), so on
            // first load V1's VAR/LEG cells showed a flat/default array until a poly-voice visit
            // populated the shared array — the "flat probability until V2 round-trip" bug. VAR/LEG
            // share the mono array (§4d), so this reads the same finalRandomByStrand the mono tab does.
            {
                auto& peRef = monsoon->engine.pe;
                for (int el = 0; el < dotModular::SandsGrid::EAST_LANES; ++el) {
                    int strand = dotModular::MONO_LANE_TO_STRAND[el];
                    for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s)
                        visualEditor->currentState.lanes[el].probabilities[s] =
                            peRef.finalRandomByStrand(strand, s);
                }
            }
        } else if (selectedVoice >= 1) {
            const int pv = polyVoice();
            // l = engine lane (0=REST 1=MEL 2=OCT 3=ACC) → editor lane
            for (int l=0; l<4; ++l) {
                int cvLen = eng.polyLenE(pv, l);
                int cvOff = eng.polyOffE(pv, l);
                int cvRot = eng.polyRotE(pv, l);
                int el = dotModular::ENGINE_LANE_TO_EDITOR[l];
                visualEditor->currentState.lanes[el].setDisplayLOR(cvLen, cvOff, cvRot);
                // Use the PER-VOICE tick (laneTickV_), not mono's laneTick_ — otherwise
                // the poly playhead always follows mono's direction, ignoring the DirCell.
                int ph1253 = (eng.stepIndex >= 0) ? eng.laneTickV_[pv][dotModular::MONO_LANE_TO_STRAND[el]] : -1;
                visualEditor->setLanePlayStep(el, calcPlayhead(ph1253, cvLen, cvOff, cvRot));
            }
            // VARIATION (4) / LEGATO (5): show the window the ENGINE actually reads for this voice
            // — mono's when the lane DELEGATES (default), the voice's own when Local East — plus the
            // moving playhead and mono's SHARED VAR/LEG probability array (per §4d the array is
            // global; only the reading window is per-voice). strand == editor lane (VAR 4, LEG 5).
            // Use polyLOR (editor-order, masks &7) NOT polyLenE (engine-order, masks &3 → would
            // alias VAR→REST). eng.lor(el) / finalRandomByStrand(el) are mono's, as in mirrorMono.
            for (int el = dotModular::SandsGrid::POLY_LANES; el < dotModular::SandsGrid::EAST_LANES; ++el) {
                const bool localEast =
                    (getMonsoon() && getMonsoon()->getVarlegDeleg(pv, el - dotModular::SandsGrid::POLY_LANES) > 0.5f);
                int cvLen, cvOff, cvRot;
                if (localEast) {   // voice's own window (what the engine reads under Local East)
                    cvLen = std::max(1, eng.polyLOR(pv, el, SequencerEngine::LOR_LEN));
                    cvOff = eng.polyLOR(pv, el, SequencerEngine::LOR_OFF);
                    cvRot = eng.polyLOR(pv, el, SequencerEngine::LOR_ROT);
                } else {           // delegated → mono's window (what the engine reads under delegate)
                    cvLen = std::max(1, eng.lor(el, SequencerEngine::LOR_LEN));
                    cvOff = eng.lor(el, SequencerEngine::LOR_OFF);
                    cvRot = eng.lor(el, SequencerEngine::LOR_ROT);
                }
                visualEditor->currentState.lanes[el].setDisplayLOR(cvLen, cvOff, cvRot);
                int pvTick = eng.laneTickV_[pv][dotModular::MONO_LANE_TO_STRAND[el]];
                int ph1276 = (eng.stepIndex >= 0) ? pvTick : -1;
                visualEditor->setLanePlayStep(el, calcPlayhead(ph1276, cvLen, cvOff, cvRot));
                for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s)
                    visualEditor->currentState.lanes[el].probabilities[s] = eng.pe.finalRandomByStrand(el, s);
            }
        }
    }

    // (The old BASE blend-group draw() override was removed: per-lane ownership
    //  now lives in the OwnerCell widgets at the SRC column, which draw themselves.
    //  There is no below-lanes BASE band any more, so no custom NanoVG painting is
    //  needed here — ModuleWidget::draw handles the panel + child widgets.)
};

// ── Module process(): light latches + 3 poly probability CV outs (audio rate) ──
void StraitsEastSandsVisual::process(const ProcessArgs&) {
    using namespace StraitsEastVisualIds;
    for (int lane = 0; lane < 4; ++lane)
        // MVC step 1d: module process has no tab context; light the V1 owner (the OwnerCell
        // widget draws the live per-voice state via getOwnsFn). cachedMon_ is the store owner.
        lights[ownerLightId(lane)].setBrightness((cachedMon_ && (cachedMon_->getMonoMacroOwn(lane) > 0.5f)) ? 1.f : 0.f);

    // PERF: findMonsoonEitherSide walks the expander chain. Topology only changes at
    // control rate, so refresh the cached pointer on the same /8 divider as the gate scan
    // rather than every sample. (Rodney's audit, item 3.)
    if (gateModDiv.process()) { cachedMon_ = redDot::findMonsoonEitherSide(this); gateModScan_ = true; }
    Monsoon* mon = cachedMon_;
    if (!mon) {
        for (int l = 0; l < 4; ++l) { outputs[PROB_OUT_REST + l].setChannels(1);
                                      outputs[PROB_OUT_REST + l].setVoltage(0.f); }
        return;
    }
    // ── Gate-mod edge detection (audio thread) ────────────────────────────
    // The only audio-rate job here: spot rising edges and bump a counter. Every decision
    // about what an edge MEANS (which voice, per-voice store vs display proxy, whether that
    // target is on screen) belongs to the widget's step(), which already owns that sync and
    // is the only place that knows the selected tab — so nothing reads the tab at audio rate.
    // Scanned on a divider: gates are milliseconds (hundreds of samples), so /8 catches every
    // edge at an eighth of the cost. dir/deleg share the scan since they share the cadence.
    if (gateModScan_) {
        gateModScan_ = false;
        for (int lane = 0; lane < 6; ++lane) {
            auto& din = inputs[dirModId(lane)];
            dirModChans[lane] = din.isConnected() ? (uint8_t)din.getChannels() : 0;
            if (din.isConnected()) {
                int nch = std::min(din.getChannels(), 16);
                for (int ch = 0; ch < nch; ++ch) {
                    bool high = din.getVoltage(ch) > 1.f;
                    if (high && !dirModPrev[lane][ch]) ++dirModEdges[lane][ch];
                    dirModPrev[lane][ch] = high;
                }
            }
            auto& gin = inputs[delegModId(lane)];
            delegModChans[lane] = gin.isConnected() ? (uint8_t)gin.getChannels() : 0;
            if (gin.isConnected()) {
                int nch = std::min(gin.getChannels(), 16);
                for (int ch = 0; ch < nch; ++ch) {
                    bool high = gin.getVoltage(ch) > 1.f;
                    if (high && !delegModPrev[lane][ch]) ++delegModEdges[lane][ch];
                    delegModPrev[lane][ch] = high;
                }
            }
        }
    }
    const float scaleV = (mon->probOutScale == 0) ? 1.f : (mon->probOutScale == 1) ? 5.f : 10.f;
    const bool sh = mon->probOutSampleHold;
    auto& eng = mon->engine;
    const int nV = eng.numPolyVoices;                  // 0..15 poly voices
    const int nCh = 1 + nV;                            // mono on ch1 + poly on ch2..1+nV
    // PERF (Rodney's audit, item 2 -- the heaviest per-sample work in the expanders):
    // recomputing every lane x voice probability EVERY SAMPLE is pointless. laneStep() +
    // laneProbability() x 4 lanes x up to 16 voices = up to 64 resolver calls per sample.
    // Probabilities change at STEP rate, so /16 is far finer than the signal warrants and
    // still gives ~3 kHz CV updates at 48 kHz.
    //
    // The output VOLTAGE is not gated -- Rack ports hold their last value, so channels are
    // still set every sample and the CV is continuous; only the COMPUTATION is throttled.
    // Channel count is likewise set every sample so polyphony changes take effect at once.
    const bool recompute = probOutDiv.process();
    dotModular::VoiceResolver resolver(eng);
    for (int l = 0; l < 4; ++l) {
        auto& out = outputs[PROB_OUT_REST + l];
        out.setChannels(nCh < 1 ? 1 : nCh);
        // Uniform addressing: VCV channel ch (0-based) carries voice number ch+1.
        //   ch 0 → voice 1 (mono master strand) — previously hardcoded 0V; now its real draw.
        //   ch v → voice v+1 (poly) for v in 1..nV.
        // The resolver maps voice 1 → master accessors, voices 2..16 → poly bank (voice-2),
        // so this single loop replaces the old hand-split (0V stub + per-poly indexing).
        if (!recompute) continue;                      // ports hold their last voltage
        for (int ch = 0; ch < nCh; ++ch) {
            const int voice = ch + 1;                  // 1..16
            int step = resolver.laneStep(voice, l);
            float raw = resolver.laneProbability(voice, l);
            float val;
            if (sh) {
                if (step != probLastStep[l][ch]) { probHeld[l][ch] = raw; probLastStep[l][ch] = step; }
                val = probHeld[l][ch];
            } else val = raw;
            out.setVoltage(rack::math::clamp(val, 0.f, 1.f) * scaleV, ch);
        }
    }
}

Model* modelStraitsEastSandsVisual =
    createModel<StraitsEastSandsVisual,StraitsEastSandsVisualWidget>(
        "StraitsEastSandsVisual");
