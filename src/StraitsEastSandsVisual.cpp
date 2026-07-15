#include <rack.hpp>
#include "ui/SandsGrid.hpp"
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
            arc->radius   = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            arc->attachOverKnob(knob, mm2px(2.5f));
            auto interpParamId = [this, lane]() -> int {
                int v = polyVoice();
                if (v < 0) v = 0;   // mono tab: arc is inactive anyway; keep id in range
                return (lane==0) ? restInterpId(v)
                     : (lane==1) ? melodyInterpId(v)
                     : (lane==2) ? octaveInterpId(v)
                     :             accentInterpId(v);   // lane 3 = ACCENT (was falling through to octave)
            };
            arc->getSetNorm = [mod, this, interpParamId, lane]() -> float {
                if (!mod) return 0.5f;
                if (polyVoice() < 0) {
                    // V1 (combo 7): SET = East's spread knob, which now mirrors Mono's
                    // spread. SPREAD_R/M/O/A = lane 0/1/2/3 (engine order).
                    if (lane < 0 || lane >= 4) return 0.5f;
                    int pid = (lane==0) ? (int)SPREAD_R : (lane==1) ? (int)SPREAD_M
                            : (lane==2) ? (int)SPREAD_O : (int)SPREAD_A;
                    auto* pq = mod->paramQuantities[pid];
                    return pq ? (float)pq->getScaledValue() : 0.5f;
                }
                auto* pq = mod->paramQuantities[interpParamId()];
                return pq ? (float)pq->getScaledValue() : 0.5f;
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
                    bool delegated = macroVis && !(mod->params[ownerDispId(lane)].getValue() > 0.5f);
                    float sp;
                    if (delegated) {
                        sp = macroVis->macroBase[lane][3] + macroVis->macroCVDelta[lane][3];
                    } else {
                        int pid = (lane==0) ? (int)SPREAD_R : (lane==1) ? (int)SPREAD_M
                                : (lane==2) ? (int)SPREAD_O : (int)SPREAD_A;
                        sp = mod->params[pid].getValue();   // bipolar -1..1
                        if (mod->inputs[cvId(lane,3)].isConnected()) {
                            float att = mod->params[attenId(dotModular::VoiceResolver::kMonoSlot, lane, 3)].getValue();
                            float cv  = mod->inputs[cvId(lane,3)].getVoltage(0) / 10.f;
                            sp += cv * att * 2.f;
                        }
                        if (macroVis) {
                            float send = macroVis->params[StraitsMacroVisualIds::sendId(
                                dotModular::VoiceResolver::kMonoSlot, lane, 3)].getValue();
                            sp += macroVis->macroSendDelta[lane][3] * send;
                        }
                    }
                    return rack::math::clamp((rack::math::clamp(sp,-1.f,1.f) + 1.f) * 0.5f, 0.f, 1.f);
                }
                if (v >= 15) return 0.5f;
                // polySpreadEffective is bipolar -1..1 → map to 0..1.
                return rack::math::clamp((mod->polySpreadEffective[v][lane] + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->isActive = [mod, this, lane, interpParamId]() -> bool {
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
                    // East's ownerDispId is engine-ordered → no conversion. delegated =
                    // Macro owns this V1 lane. Helper covers delegated (CV live) + owned
                    // (mono-slot send + CV live). sendSlot = kMonoSlot (V1).
                    bool delegated = macroVis && !(mod->params[ownerDispId(lane)].getValue() > 0.5f);
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
                    bool eastOwns = mod->params[ownerId(v, lane)].getValue() > 0.5f;
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
        visualEditor->laneLockedFn = [this](int editorLane) -> bool {
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
        // Right-click on a lane row opens the ownership context menu.
        visualEditor->onLaneRightClick = [this](int lane, rack::math::Vec pos) -> bool {
            if (!macroAttached()) return false;  // no menu when Macro absent
            if (onMonoTab()) return false;        // ownership is per poly voice, not mono
            openLaneOwnershipMenu(lane, pos);
            return true;
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
            // CV-depth attenuverters: East's own controls, always live.
            for (int c = 0; c < 4; ++c)
                bindParam<DimmableTrimpot>("param_" + std::to_string(attenDispId(r,c)), attenDispId(r,c));
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
            for (int lane = 0; lane < 2; ++lane)
                for (int c = 0; c < 3; ++c)
                    bindParam<DimmableTrimpot>("param_" + std::to_string(varlegAttDispId(lane,c)),
                        varlegAttDispId(lane,c));
        }
        bindParam<DimmableTrimpot>("param_" + std::to_string((int)SPREAD_R), SPREAD_R,
            std::function<void(DimmableTrimpot*)>([this](DimmableTrimpot* k){ k->dimWhen = [](){ return false; }; k->lockWhen = [this](){ return laneOwnedByMacroTopo(0) || tab1MonoMirror(); }; k->displayValueFn = [this](){ return spreadDisplayValue(0); }; pendingSpreadArcs.push_back({k, 0}); }));
        bindParam<DimmableTrimpot>("param_" + std::to_string((int)SPREAD_M), SPREAD_M,
            std::function<void(DimmableTrimpot*)>([this](DimmableTrimpot* k){ k->dimWhen = [](){ return false; }; k->lockWhen = [this](){ return laneOwnedByMacroTopo(1) || tab1MonoMirror(); }; k->displayValueFn = [this](){ return spreadDisplayValue(1); }; pendingSpreadArcs.push_back({k, 1}); }));
        bindParam<DimmableTrimpot>("param_" + std::to_string((int)SPREAD_O), SPREAD_O,
            std::function<void(DimmableTrimpot*)>([this](DimmableTrimpot* k){ k->dimWhen = [](){ return false; }; k->lockWhen = [this](){ return laneOwnedByMacroTopo(2) || tab1MonoMirror(); }; k->displayValueFn = [this](){ return spreadDisplayValue(2); }; pendingSpreadArcs.push_back({k, 2}); }));
        bindParam<DimmableTrimpot>("param_" + std::to_string((int)SPREAD_A), SPREAD_A,
            std::function<void(DimmableTrimpot*)>([this](DimmableTrimpot* k){ k->dimWhen = [](){ return false; }; k->lockWhen = [this](){ return laneOwnedByMacroTopo(3) || tab1MonoMirror(); }; k->displayValueFn = [this](){ return spreadDisplayValue(3); }; pendingSpreadArcs.push_back({k, 3}); }));

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
            bindParam<OwnerCell>(
                "param_owner_" + std::to_string(lane),
                ownerDispId(lane),
                [this, lane](OwnerCell* w) {
                    w->laneCol = laneColEng[lane];
                    Vec ctr = w->box.pos.plus(w->box.size.div(2.f));
                    // Match the editor's lane-step cell: one step wide × ~90% lane tall.
                    const float stepW = (ED_W - 2.f*6.f) / 16.f;   // editor padding=6, 16 steps
                    w->box.size = mm2px(Vec(stepW, ED_LANE_H * 0.9f));
                    w->box.pos  = ctr.minus(w->box.size.div(2.f));
                    // Owner cell is locked (inoperable) when:
                    //   • no Macro attached → nothing to delegate to (condition 2), OR
                    //   • Mono is present AND this is the V1 tab → Mono owns V1, so East
                    //     can't cede it (G4). Use tab1MonoMirror() (= V1 tab AND Mono
                    //     attached), NOT onMonoTab() alone: with East+Macro and NO Mono,
                    //     V1 IS delegatable to Macro like any other lane, so it must stay
                    //     operable on the V1 tab. (Previously onMonoTab() locked V1 even
                    //     without Mono, wrongly blocking East+Macro V1 delegation.)
                    w->lockWhen = [this](){ return !macroAttached() || tab1MonoMirror(); };
                    // P1 (G1 no-hide): the owner cell is never hidden — not even on the
                    // V1/mono tab. It stays visible and is *locked* where appropriate
                    // (P2/P8). hideWhen is left unset → always shown.
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
            bindParam<OwnerCell>(
                "param_owner_" + std::to_string(4 + lane),
                varlegDelegDispId(lane),
                [this, lane](OwnerCell* w) {
                    w->laneCol = varlegCol[lane];
                    Vec ctr = w->box.pos.plus(w->box.size.div(2.f));
                    const float stepW = (ED_W - 2.f*6.f) / 16.f;
                    w->box.size = mm2px(Vec(stepW, ED_LANE_H * 0.9f));
                    w->box.pos  = ctr.minus(w->box.size.div(2.f));
                    // V1 is mono → always follows; lock the cell on the mono tab. Off the V1
                    // tab the cell is always operable (mono is always a valid delegation target).
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
            bindParam<DirCell>(
                "param_dir_" + std::to_string(lane),
                dirDispId(lane),
                [this, lane](DirCell* w) {
                    w->laneCol = dirCol[lane];
                    Vec ctr = w->box.pos.plus(w->box.size.div(2.f));
                    const float stepW = (ED_W - 2.f*6.f) / 16.f;
                    w->box.size = mm2px(Vec(stepW, ED_LANE_H * 0.9f));
                    w->box.pos  = ctr.minus(w->box.size.div(2.f));
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
                                return module->params[varlegDelegDispId(vl)].getValue() < 0.5f;
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

    void saveVoiceSpread(int v) {
        if (!module) return;
        // The SPREAD_* trimpot is never force-overwritten anymore (a ceded lane's knob
        // DISPLAYS Macro's base via DimmableTrimpot.displayValueFn without touching the
        // param), so it always holds East's real value and saves cleanly. Store stays
        // pristine → cede→reclaim reverts. (See DISPLAY_STORE_ENGINE_SEPARATION.md.)
        module->params[restInterpId(v)  ].setValue(module->params[SPREAD_R].getValue());
        module->params[melodyInterpId(v)].setValue(module->params[SPREAD_M].getValue());
        module->params[octaveInterpId(v)].setValue(module->params[SPREAD_O].getValue());
        module->params[accentInterpId(v)].setValue(module->params[SPREAD_A].getValue());
    }
    void loadVoiceSpread(int v) {
        if (!module) return;
        module->params[SPREAD_R].setValue(module->params[restInterpId(v)  ].getValue());
        module->params[SPREAD_M].setValue(module->params[melodyInterpId(v)].getValue());
        module->params[SPREAD_O].setValue(module->params[octaveInterpId(v)].getValue());
        module->params[SPREAD_A].setValue(module->params[accentInterpId(v)].getValue());
    }
    // Owner display proxy ↔ per-voice MACRO_OWN; CV-depth attenuverters disp↔per-voice.
    // (Macro mix-in send sync relocated to Macro under the control inversion.)
    void saveVoiceMacro(int v) {   // v = 0-based poly bank (ownerId is poly-indexed)
        if (!module) return;
        // atten bank is 16-wide voice-slot indexed; derive via the resolver (poly bank v →
        // voice v+2 → slot), anchored to the asserted slot/bank invariant.
        const int slot = dotModular::VoiceResolver::voiceSlot(v + dotModular::VoiceResolver::kFirstPoly);
        for (int lane=0; lane<4; ++lane)
            module->params[ownerId(v,lane)].setValue(module->params[ownerDispId(lane)].getValue());
        // VAR/LEG delegation: display proxy → this voice's per-voice store. lane 0=VAR, 1=LEG.
        for (int lane=0; lane<2; ++lane)
            module->params[varlegDelegId(v,lane)].setValue(module->params[varlegDelegDispId(lane)].getValue());
        // CV-depth attenuverters: display proxy → this voice's per-voice store.
        for (int lane=0; lane<4; ++lane)
            for (int c=0; c<4; ++c)
                module->params[attenId(slot,lane,c)].setValue(module->params[attenDispId(lane,c)].getValue());
        // VAR/LEG CV-depth: display proxy → this voice's per-voice store. lane 0=VAR,1=LEG; col 0..2.
        for (int lane=0; lane<2; ++lane)
            for (int c=0; c<3; ++c)
                module->params[varlegAttId(slot,lane,c)].setValue(module->params[varlegAttDispId(lane,c)].getValue());
    }
    void loadVoiceMacro(int v) {   // v = 0-based poly bank
        if (!module) return;
        const int slot = dotModular::VoiceResolver::voiceSlot(v + dotModular::VoiceResolver::kFirstPoly);
        for (int lane=0; lane<4; ++lane)
            module->params[ownerDispId(lane)].setValue(module->params[ownerId(v,lane)].getValue());
        // VAR/LEG delegation: this voice's per-voice store → display proxy. lane 0=VAR, 1=LEG.
        for (int lane=0; lane<2; ++lane)
            module->params[varlegDelegDispId(lane)].setValue(module->params[varlegDelegId(v,lane)].getValue());
        // CV-depth attenuverters: this voice's per-voice store → display proxy.
        for (int lane=0; lane<4; ++lane)
            for (int c=0; c<4; ++c)
                module->params[attenDispId(lane,c)].setValue(module->params[attenId(slot,lane,c)].getValue());
        // VAR/LEG CV-depth: this voice's per-voice store → display proxy. lane 0=VAR,1=LEG; col 0..2.
        for (int lane=0; lane<2; ++lane)
            for (int c=0; c<3; ++c)
                module->params[varlegAttDispId(lane,c)].setValue(module->params[varlegAttId(slot,lane,c)].getValue());
    }
    // Iterate EDITOR lanes 0..5 directly — currentState.lanes[] is editor-indexed, and VAR(4)/LEG(5)
    // have no PolyLane id, so the old engine-lane loop could not reach them. lorIdEditor() maps.
    // On the V1/mono tab, editor lanes 4/5 DISPLAY mono's VARIATION/LEGATO (they are mono strands;
    // East never owns them). The strands were renumbered so editor lane == strand index
    // (MEL 0, OCT 1, REST 2, ACC 3, VAR 4, LEG 5), hence lorStore_[0][el] is mono's own LOR column.
    // Read-only: laneLockedFn() locks these lanes whenever onMonoTab().
    void mirrorMonoExtraLanes() {
        Monsoon* m = getMonsoon();
        if (!m || !visualEditor || !onMonoTab()) return;
        const int gs = m->engine.stepIndex;
        for (int el = dotModular::SandsGrid::POLY_LANES; el < dotModular::SandsGrid::EAST_LANES; ++el) {
            const int strand = el;   // identity, by the renumber above
            int mLen = std::max(1, m->engine.lor(strand, SequencerEngine::LOR_LEN));
            int mOff = m->engine.lor(strand, SequencerEngine::LOR_OFF);
            int mRot = m->engine.lor(strand, SequencerEngine::LOR_ROT);
            auto& lane = visualEditor->currentState.lanes[el];
            lane.setDisplayLOR(mLen, mOff, mRot);
            for (int st = 0; st < SandsVisualEditorV4::STEP_COUNT; ++st)
                lane.probabilities[st] = m->engine.pe.finalRandomByStrand(strand, st);
            visualEditor->setLanePlayStep(el, calcPlayhead(m->engine.laneTick_[strand], mLen, mOff, mRot));
        }
    }

    void saveVoiceLOR(int v) {
        if (!module || !visualEditor) return;   // poly banks only; V1 has none (see onVoiceTabChanged)
        for (int el=0; el<dotModular::SandsGrid::EAST_LANES; ++el) {
            const auto& lane = visualEditor->currentState.lanes[el];
            module->params[lorIdEditor(v,el,0)].setValue((float)lane.length);
            module->params[lorIdEditor(v,el,1)].setValue((float)lane.offset);
            module->params[lorIdEditor(v,el,2)].setValue((float)lane.rotation);
        }
    }
    void loadVoiceLOR(int v) {
        if (!module || !visualEditor) return;
        for (int el=0; el<dotModular::SandsGrid::EAST_LANES; ++el) {
            auto& lane = visualEditor->currentState.lanes[el];
            lane.length   = std::max(1,(int)std::round(module->params[lorIdEditor(v,el,0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorIdEditor(v,el,1)].getValue());
            lane.rotation = (int)std::round(module->params[lorIdEditor(v,el,2)].getValue());
        }
    }
    void onVoiceTabChanged(int nv) {
        if (!paramMgr || !visualEditor) return;
        // Save the OUTGOING voice's edits.
        // Poly tabs always save. Mono tab saves when it is editable (no Mono attached).
        if (selectedVoice >= 1) {
            paramMgr->syncEditorToPatternEngine(polyVoice(), visualEditor->currentState);
            saveVoiceLOR(polyVoice());
            saveVoiceSpread(polyVoice());
            saveVoiceMacro(polyVoice());
        }
        // V1-editable: nothing to save here — East writes the engine MONO strands
        // directly each frame in step() (V1's true home), so there is no per-voice
        // bank to persist. (The old saveVoiceLOR(0) wrote voice-2's poly bank — wrong.)
        // BUT V1's per-lane OWNERSHIP does persist (monoOwnerId, the spare slot): push the
        // live owner-cell proxy into it when leaving the V1 tab.
        if (selectedVoice == 0) {
            for (int lane = 0; lane < 4; ++lane)
                module->params[monoOwnerId(lane)].setValue(module->params[ownerDispId(lane)].getValue());
        }
        selectedVoice = nv;
        // Load the INCOMING voice.
        if (selectedVoice >= 1) {
            paramMgr->syncPatternEngineToEditor(polyVoice(), visualEditor->currentState);
            loadVoiceLOR(polyVoice());
            loadVoiceSpread(polyVoice());
            loadVoiceMacro(polyVoice());
            // Sync DirCell display proxy from the engine's per-voice direction so the
            // DirCell shows the incoming voice's actual state (not the outgoing voice's).
            if (auto* m = getMonsoon()) {
                int pv = polyVoice();
                for (int lane = 0; lane < 6; ++lane) {
                    int strand = dotModular::MONO_LANE_TO_STRAND[lane];
                    module->params[dirDispId(lane)].setValue((float)m->engine.laneDirVPending_[pv][strand]);
                }
            }
        }
        // V1-editable: the editor is refreshed from the engine mono strands by the
        // v1Editable() display branch in step(); no explicit load needed. Owner cell
        // proxy is restored from the persistent mono owner store.
        if (selectedVoice == 0) {
            // Seed editor lanes 4/5 from mono's VARIATION/LEGATO strands BEFORE step()'s v1Editable
            // write runs, otherwise the outgoing poly voice's VAR/LEG would be pushed into mono.
            mirrorMonoExtraLanes();
            for (int lane = 0; lane < 4; ++lane)
                module->params[ownerDispId(lane)].setValue(module->params[monoOwnerId(lane)].getValue());
            // V1 is mono → VAR/LEG always follow mono; show the cells filled (and they lock).
            for (int lane = 0; lane < 2; ++lane)
                module->params[varlegDelegDispId(lane)].setValue(0.f);
            // Sync DirCell display proxy from the engine's mono direction.
            if (auto* m = getMonsoon()) {
                for (int lane = 0; lane < 6; ++lane) {
                    int strand = dotModular::MONO_LANE_TO_STRAND[lane];
                    module->params[dirDispId(lane)].setValue((float)m->engine.laneDirPending_[strand]);
                }
            }
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
            for (int el = 0; el < 4; ++el) {
                int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
                in.eastV1Owner[el] = module->params[StraitsEastVisualIds::monoOwnerId(eng)].getValue() > 0.5f;
                for (int pv = 0; pv < 15; ++pv)
                    in.eastPolyOwner[pv][el] = module->params[StraitsEastVisualIds::ownerId(pv, eng)].getValue() > 0.5f;
            }
            // The CURRENT tab's owner cells live in the display proxy (ownerDispId) and are
            // only flushed to the persistent slot on tab-exit — so for the current voice,
            // read the LIVE proxy, else buildTopo would lag live edits (and the 4c cross-
            // check would fire spuriously). ownerDispId is engine-ordered.
            const int cv = currentVoice() - 1;   // 0 = V1/mono
            for (int el = 0; el < 4; ++el) {
                int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
                bool live = module->params[StraitsEastVisualIds::ownerDispId(eng)].getValue() > 0.5f;
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
    // Topology-backed lane ownership for the CURRENT tab, used by laneLockedFn and the
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
    // The voice NUMBER (1..16) for the selected tab: tab 0 = V1 (mono), tab v = V(v+1).
    // All mono/poly identity + bank mapping flows through VoiceResolver so there's one
    // source of truth (the resolver), not hand-rolled selectedVoice arithmetic scattered
    // here. The resolver methods are static/constexpr — no engine ref, no per-call cost.
    int currentVoice() const { return selectedVoice + 1; }

    // Voice 1 / tab 1 with Sands Mono attached: the lane base belongs to Mono — East's
    // base controls lock + mirror mono (display-only). Independent of Macro.
    bool tab1MonoMirror() {
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
        const bool macroOwns = !( module->params[ownerDispId(lane)].getValue() > 0.5f );
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
                float val = setToEast ? 1.f : 0.f;
                widget->module->params[StraitsEastVisualIds::ownerDispId(lane)].setValue(val);
                widget->saveVoiceMacro(voice);  // persist to per-voice bank
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
                widget->module->params[StraitsEastVisualIds::ownerDispId(lane)].setValue(val);
                for (int v = 0; v < 15; ++v) {
                    widget->module->params[StraitsEastVisualIds::ownerId(v, lane)].setValue(val);
                }
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
            for (int ch = 0; ch < 16; ++ch) {
                uint8_t n = mod->dirModEdges[lane][ch];
                uint8_t d = (uint8_t)(n - dirModSeen[lane][ch]);   // unsigned diff: wrap-safe
                if (!d) continue;
                dirModSeen[lane][ch] = n;
                // Read the CURRENT value from the engine (the per-target truth), never from
                // the shared proxy — the proxy only ever holds the displayed target's value.
                int nxt;
                if (ch == 0) {
                    nxt = (((int)se->laneDirPending_[strand]) + d) % 4;
                    se->laneDirPending_[strand] = (SequencerEngine::LaneDir)nxt;
                } else {
                    const int pv = ch - 1;
                    nxt = (((int)se->laneDirVPending_[pv][strand]) + d) % 4;
                    se->laneDirVPending_[pv][strand] = (SequencerEngine::LaneDir)nxt;
                }
                // Step 2: persist to the target's BANK slot as well. syncDirBank() only ever
                // sees the proxy, i.e. the displayed voice — a mod aimed at any other voice
                // would never reach the bank otherwise. Indexed by channel, so no tab needed:
                // this is the write that becomes the ONLY one at step 3.
                mod->params[(ch == 0) ? monoDirId(lane) : dirId(ch - 1, lane)].setValue((float)nxt);
                if (selectedVoice == ch) mod->params[dirDispId(lane)].setValue((float)nxt);
            }
            // ── Delegation: flip local/delegated in the voice's OWN owner store ──
            // lanes 0..3 -> ownerId(pv, engineLane) (V1 uses its own monoOwnerId slot);
            // lanes 4..5 -> varlegDelegId(pv, VAR|LEG), which is poly-only: mono's
            // VARIATION/LEGATO are mono strands East never owns, so ch0 is a no-op there.
            const int eng = (lane < 4) ? dotModular::EDITOR_TO_ENGINE_LANE[lane] : -1;
            for (int ch = 0; ch < 16; ++ch) {
                uint8_t n = mod->delegModEdges[lane][ch];
                uint8_t d = (uint8_t)(n - delegModSeen[lane][ch]);
                if (!d) continue;
                delegModSeen[lane][ch] = n;
                if (!(d & 1)) continue;          // an even number of flips is a no-op
                if (lane >= 4 && ch == 0) continue;   // mono has no VAR/LEG delegation
                int storeId;
                if (lane < 4) storeId = (ch == 0) ? monoOwnerId(eng) : ownerId(ch - 1, eng);
                else          storeId = varlegDelegId(ch - 1, lane - 4);
                const float nv = (mod->params[storeId].getValue() > 0.5f) ? 0.f : 1.f;
                mod->params[storeId].setValue(nv);
                if (selectedVoice == ch) {
                    const int dispId = (lane < 4) ? ownerDispId(eng) : varlegDelegDispId(lane - 4);
                    mod->params[dispId].setValue(nv);
                }
            }
        }
    }

    // ── Step 2 (plans/lane_direction_homes.md): keep East's direction BANK correct ──────
    // DirCell is a ParamWidget bound to the shared proxy (a ParamWidget binds one paramId at
    // construction, so a per-voice control needs proxy + tab copy) and so cannot know which
    // voice it is editing. Persist any proxy change into the CURRENT voice's bank slot the
    // frame it happens — the continuous equivalent of the tab-exit flush. Writes only on
    // change, so it never fights the existing pushes. Inert until step 3 reads the bank.
    float lastDirDisp[6] = {};
    bool  dirDispInit = false;

    void syncDirBank() {
        auto* mod = static_cast<StraitsEastSandsVisual*>(module);
        if (!mod) return;
        const int cv = currentVoice();                       // 1 = V1/mono, 2..16 = poly
        const bool mono = dotModular::VoiceResolver::isMono(cv);
        const int  pv   = dotModular::VoiceResolver::polyBankIndex(cv);
        for (int lane = 0; lane < 6; ++lane) {
            const float v = mod->params[dirDispId(lane)].getValue();
            if (dirDispInit && v == lastDirDisp[lane]) continue;   // no edit → don't write
            lastDirDisp[lane] = v;
            if (mono)                        mod->params[monoDirId(lane)].setValue(v);
            else if (pv >= 0 && pv < 15)     mod->params[dirId(pv, lane)].setValue(v);
        }
        dirDispInit = true;
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }
        applyGateMods();
        syncDirBank();

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
        // Active tabs = mono (always, index 0) + the active poly voices. So tab i is
        // enabled for i <= numPolyVoices (i=0 mono; i=1..numPolyVoices poly).
        if (tabGroup) tabGroup->setActiveCount(dotModular::VoiceResolver(*se).activeVoiceCount());

        if (!initialized) {
            if (selectedVoice >= 1) {
                loadVoiceLOR(polyVoice());
                loadVoiceSpread(polyVoice());
            }
            initialized = true;
        }

        int newSel = tabGroup->getSelectedTab();
        if (newSel != selectedVoice) onVoiceTabChanged(newSel);

        // Per-frame write-back of display proxies → the selected voice's params, so edits
        // take effect immediately. Poly tabs only — the mono tab (index 0) is display-only
        // (its base lives on Sands Mono); writing it back would corrupt poly slot 0.
        if (selectedVoice >= 1) {
            saveVoiceSpread(polyVoice());
            saveVoiceMacro(polyVoice());

            // DISPLAY/STORE/ENGINE separation (see DISPLAY_STORE_ENGINE_SEPARATION.md):
            //  - STORE:   SPREAD_* param → *InterpId + smgr. Always East's real value now —
            //             the knob no longer force-overwrites the param (a ceded lane's knob
            //             DISPLAYS Macro's base via DimmableTrimpot.displayValueFn instead).
            //  - ENGINE:  combineSpread arbitrates at playback (ceded → Macro base+CV, owned →
            //             East store), independent of this store. So writing East's value here
            //             for a ceded lane is harmless — the engine plays Macro regardless.
            //  - Result: cede shows/plays Macro, store keeps East → reclaim reverts. No guards.
            auto& smgr = paramMgr->spreadMgr;
            smgr.setSpread(polyVoice(), 0, mod->params[SPREAD_R].getValue());
            smgr.setSpread(polyVoice(), 1, mod->params[SPREAD_M].getValue());
            smgr.setSpread(polyVoice(), 2, mod->params[SPREAD_O].getValue());
            smgr.setSpread(polyVoice(), 3, mod->params[SPREAD_A].getValue());
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
            // V1 editable (no Mono attached): East IS the mono editor → push to engine.
            // Macro (if present) also pushes — both are editable, last writer wins.
            for (int lane = 0; lane < 6; ++lane) {
                int strand = dotModular::MONO_LANE_TO_STRAND[lane];
                auto d = (SequencerEngine::LaneDir)(int)std::round(mod->params[dirDispId(lane)].getValue());
                se->laneDirPending_[strand] = d;
            }
        } else if (onMonoTab() && tab1MonoMirror()) {
            // Mono is attached: Mono is the authority for mono direction. Sync the
            // display proxy FROM the engine so the DirCell shows what Mono set (read-only).
            for (int lane = 0; lane < 6; ++lane) {
                int strand = dotModular::MONO_LANE_TO_STRAND[lane];
                mod->params[dirDispId(lane)].setValue((float)se->laneDirPending_[strand]);
            }
        } else if (selectedVoice >= 1) {
            // Poly tab: direction applies to this voice's lanes (laneDirV_).
            int pv = polyVoice();
            for (int lane = 0; lane < 6; ++lane) {
                int strand = dotModular::MONO_LANE_TO_STRAND[lane];
                auto d = (SequencerEngine::LaneDir)(int)std::round(mod->params[dirDispId(lane)].getValue());
                se->laneDirVPending_[pv][strand] = d;
            }
        }
        // (Spread target mode is now pulled from the engine by SpreadManager —
        // Monsoon::process mirrors the menu setting onto engine.pe each frame. No
        // per-widget push needed.)

        // CV applied at control rate in Monsoon::process() — base + scaled offset, no mutation here.

        if (selectedVoice >= 1) {
            saveVoiceLOR(polyVoice());
            // Per-frame: push the spread knobs (SPREAD_R/M/O/A) into this voice's interp
            // params so turning a spread knob takes effect LIVE — previously this only
            // happened on tab change (saveVoiceSpread in onVoiceTabChanged), so a spread
            // turn (esp. ACCENT) didn't mutate until you switched tabs. Now accent spread
            // modulates immediately and its mod arc reads a live value.
            saveVoiceSpread(polyVoice());
            paramMgr->syncPatternEngineToEditor(polyVoice(), visualEditor->currentState);
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
        int gs = monsoon->engine.stepIndex;
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
            // Mono and is inoperable on East (locked by laneLockedFn / readOnly). The mod
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
                visualEditor->setLanePlayStep(l, calcPlayhead(eng.laneTick_[strand_m], std::max(1,mLen), mOff, mRot));
            }
            // Spread (combo 7): East's V1 spread knobs FOLLOW Mono's spread (inoperable,
            // locked). Mono's sprId is engine/spread order (REST=0,MEL=1,OCT=2); East's
            // SPREAD_R/M/O are also engine order, so copy directly. This makes the knob
            // track Mono and gives the V1 spread arc a real base to deflect from.
            module->params[SPREAD_R].setValue(monoVis->params[SandsMonoVisualIds::sprId(0)].getValue());
            module->params[SPREAD_M].setValue(monoVis->params[SandsMonoVisualIds::sprId(1)].getValue());
            module->params[SPREAD_O].setValue(monoVis->params[SandsMonoVisualIds::sprId(2)].getValue());
            module->params[SPREAD_A].setValue(monoVis->params[SandsMonoVisualIds::sprId(3)].getValue());  // accent (was missing)
            // CV-DEPTH attenuators on V1: these are East's OWN modulation controls (the
            // user patches CV into East + sets depth to modulate V1). saveVoiceMacro only
            // ever writes POLY slots, so the mono slot (kMonoSlot=0) — which the V1 CV
            // mix-in in readStrand reads — stayed 0, making East V1 CV inaudible/invisible.
            // Mirror the display-proxy attens into the mono slot every frame so V1 CV has
            // its depth. (Owner/base stay Mono's; only the CV depth is East's here.)
            for (int lane=0; lane<4; ++lane)
                for (int c=0; c<4; ++c)
                    module->params[attenId(dotModular::VoiceResolver::kMonoSlot, lane, c)]
                        .setValue(module->params[attenDispId(lane,c)].getValue());
            // VAR/LEG CV-depth: same mono-slot mirror so V1's ch1 mix-in has its depth.
            for (int lane=0; lane<2; ++lane)
                for (int c=0; c<3; ++c)
                    module->params[varlegAttId(dotModular::VoiceResolver::kMonoSlot, lane, c)]
                        .setValue(module->params[varlegAttDispId(lane,c)].getValue());
            // P8: East's owner cells on V1 are locked (East can't delegate V1) but should
            // SHOW the real V1 ownership, which Mono decides. Mirror Mono's owner param
            // into East's ownerDispId so the cell draws filled/outline correctly. East
            // ownerDispId is engine-lane indexed; Mono's is editor-lane indexed.
            for (int eng=0; eng<4; ++eng) {
                int el = dotModular::ENGINE_LANE_TO_EDITOR[eng];
                module->params[ownerDispId(eng)].setValue(
                    monoVis->params[SandsMonoVisualIds::ownerDispId(el)].getValue());
            }
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

            // CV depth for V1 lives in the mono slot (kMonoSlot); saveVoiceMacro only
            // writes poly slots, so mirror the atten display proxies into the mono slot
            // each frame (engine-lane indexed) — otherwise V1 CV depth would be 0.
            for (int lane=0; lane<4; ++lane)
                for (int c=0; c<4; ++c)
                    module->params[attenId(dotModular::VoiceResolver::kMonoSlot, lane, c)]
                        .setValue(module->params[attenDispId(lane,c)].getValue());
            // VAR/LEG CV-depth: same mono-slot mirror so V1's ch1 mix-in has its depth.
            for (int lane=0; lane<2; ++lane)
                for (int c=0; c<3; ++c)
                    module->params[varlegAttId(dotModular::VoiceResolver::kMonoSlot, lane, c)]
                        .setValue(module->params[varlegAttDispId(lane,c)].getValue());
            // Write the editor's lane LOR straight into the engine MONO strands (the
            // V1 home), via MONO_LANE_TO_STRAND, for all 4 poly lanes (MEL/OCT/REST/
            // ACC). The manager skips its no-Mono reset when East drives V1 (see
            // MonsoonSandsManager hasEastV1). VAR/LEG are mono-only — not edited here.
            //   editor lane el (0=MEL 1=OCT 2=REST 3=ACC) → engine strand.
            // V1 also responds to East's CV: channel 0 of each lane's CV jack (= voice 1),
            // using getPolyVoltage(0) so a MONO cable broadcasts to V1 too (Rack convention).
            // CV is additive on top of the editor base, at control rate (no audio-rate CV).
            // Depth = the mono-slot attenuator (mirrored from the display proxy below).
            // STEP 3c: per-consumer topology for the V1 write ownership. East's ownerDispId
            // is ENGINE-ordered, so populate eastV1Owner[editorLane] by reading
            // ownerDispId(EDITOR_TO_ENGINE_LANE[editorLane]) — the conversion is baked here
            // so topo speaks editor lane (design decision 1). No Mono in this branch
            // (v1Editable), so monoPresent=false → owner() takes the East-V1 path.
            // Presence from the single authority. This branch runs only under v1Editable(), which
            // now requires cachedEastSandsVisual==module and no Mono visual — so fillPresence yields
            // eastPresent=true, monoPresent=false here by construction. Sourcing from the authority
            // (not re-asserting) means if that ever stops holding, the topology reflects reality
            // instead of a stale hardcode.
            dotModular::SandsTopology::Inputs v1In;
            if (auto* mm = getMonsoon())
                mm->expanderManager.fillPresence(v1In, mm->engine.numPolyVoices);
            for (int el = 0; el < 4; ++el) {
                int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
                v1In.eastV1Owner[el] = module->params[StraitsEastVisualIds::ownerDispId(eng)].getValue() > 0.5f;
            }
            const dotModular::SandsTopology v1Topo = dotModular::SandsTopology::build(v1In);
            for (int el = 0; el < 4; ++el) {
                auto& lane = visualEditor->currentState.lanes[el];
                int strand  = dotModular::MONO_LANE_TO_STRAND[el];
                int engLane = dotModular::EDITOR_TO_ENGINE_LANE[el];   // East CV jack / macroBase use engine lane
                // STEP 3c: this block runs only when East is the V1 editor (no Mono,
                // v1Editable), so V1's owner is EAST or (delegated) MACRO.
                // STEP 3c: delegated ⟺ v1Topo.owner(0, el) == MACRO (East is V1 editor here).
                const bool delegated = (v1Topo.owner(0, el) == dotModular::SandsTopology::Role::MACRO);
                if (delegated) {
                    auto* macroVis = getMonsoon()->expanderManager.cachedMacroSandsVisual;
                    if (macroVis) {
                        eng.setStrand(StrandWriter::EAST, strand,
                            (int)std::round(macroVis->macroBase[engLane][0] + macroVis->macroCVDelta[engLane][0]),
                            (int)std::round(macroVis->macroBase[engLane][1] + macroVis->macroCVDelta[engLane][1]),
                            (int)std::round(macroVis->macroBase[engLane][2] + macroVis->macroCVDelta[engLane][2]));
                    }
                } else {
                    float len = (float)std::max(1, lane.length);
                    float off = (float)(((lane.offset % 16) + 16) % 16);
                    float rot = (float)(((lane.rotation % 16) + 16) % 16);
                    // Macro send blend on East-owned V1 lanes (mirrors poly combineLOR):
                    // blend = macroSendDelta[lane][item] * mono-slot send. macroVis may be
                    // absent (East alone) → blend 0. Send slot for V1 is kMonoSlot.
                    auto* macroVis = getMonsoon()->expanderManager.cachedMacroSandsVisual;
                    auto sendBlend = [&](int item)->float {
                        if (!macroVis) return 0.f;
                        float send = macroVis->params[StraitsMacroVisualIds::sendId(
                            dotModular::VoiceResolver::kMonoSlot, engLane, item)].getValue();
                        return macroVis->macroSendDelta[engLane][item] * send;
                    };
                    auto addCV = [&](float base, int item, float lo, float hi)->float {
                        if (module->inputs[cvId(engLane,item)].isConnected()) {
                            float att = module->params[attenId(dotModular::VoiceResolver::kMonoSlot, engLane, item)].getValue();
                            float cv  = module->inputs[cvId(engLane,item)].getPolyVoltage(0) / 10.f;  // ch0 = V1
                            base += cv * att * (hi - lo);
                        }
                        return rack::math::clamp(base + sendBlend(item), lo, hi);
                    };
                    eng.setStrand(StrandWriter::EAST, strand,
                        (int)std::round(addCV(len, 0, 1.f, 16.f)),
                        (int)std::round(addCV(off, 1, 0.f, 15.f)),
                        (int)std::round(addCV(rot, 2, 0.f, 15.f)));
                }
            }

            // VARIATION (editor 4) / LEGATO (5): East is the V1 editor here (v1Editable), so it owns
            // these mono strands too — same as lanes 0..3. No delegation branch: Macro can never own
            // them (an owned lane drives every voice identically, annihilating per-voice divergence).
            // strand == editor lane by the deliberate strand renumber (VAR 4, LEG 5).
            for (int el = dotModular::SandsGrid::POLY_LANES; el < dotModular::SandsGrid::EAST_LANES; ++el) {
                const auto& lane = visualEditor->currentState.lanes[el];
                const int vl = el - dotModular::SandsGrid::POLY_LANES;   // 0=VAR, 1=LEG
                // V1 mono mix-in: ch0 of this lane's VAR/LEG CV jack + the mono-slot depth
                // (mirrored above), additive on the editor base — same ch1→mono convention as
                // lanes 0..3. No Macro blend (VAR/LEG are never Macro-owned).
                auto addCV = [&](float base, int item, float lo, float hi)->float {
                    if (module->inputs[varlegCvId(vl,item)].isConnected()) {
                        float att = module->params[varlegAttId(dotModular::VoiceResolver::kMonoSlot, vl, item)].getValue();
                        float cv  = module->inputs[varlegCvId(vl,item)].getPolyVoltage(0) / 10.f;  // ch0 = V1
                        base += cv * att * (hi - lo);
                    }
                    return rack::math::clamp(base, lo, hi);
                };
                eng.setStrand(StrandWriter::EAST, /*strand=*/el,
                              (int)std::round(addCV((float)std::max(1, lane.length), 0, 1.f, 16.f)),
                              (int)std::round(addCV((float)(((lane.offset   % 16) + 16) % 16), 1, 0.f, 15.f)),
                              (int)std::round(addCV((float)(((lane.rotation % 16) + 16) % 16), 2, 0.f, 15.f)));
            }
            // Display: reflect the engine's current mono strand LOR back to the editor.
            // Six lanes now — MONO_LANE_TO_STRAND is [6] and VAR/LEG are East-owned on V1 here.
            for (int el = 0; el < dotModular::SandsGrid::EAST_LANES; ++el) {
                int strand = dotModular::MONO_LANE_TO_STRAND[el];
                int cvLen = eng.strandLenRef(strand);
                int cvOff = eng.strandOffRef(strand);
                int cvRot = eng.strandRotRef(strand);
                visualEditor->currentState.lanes[el].setDisplayLOR(cvLen, cvOff, cvRot);
                visualEditor->setLanePlayStep(el, calcPlayhead(eng.laneTick_[dotModular::MONO_LANE_TO_STRAND[el]], cvLen, cvOff, cvRot));
            }
            // Probabilities: V1 (mono) probabilities are display-only (drag edits the LOR
            // window only), so show the SPREAD-APPLIED values the sequencer plays. Read
            // PER-STEP from finalRandomByStrand — NOT resolver.laneProbabilityAtStep, which
            // for mono returns masterLaneProbability (the CURRENT step only), making every
            // bar identical (regression). editor lane → engine strand via MONO_LANE_TO_STRAND.
            {
                auto& peRef = monsoon->engine.pe;
                for (int el = 0; el < 4; ++el) {
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
                visualEditor->setLanePlayStep(el, calcPlayhead(eng.laneTickV_[pv][dotModular::MONO_LANE_TO_STRAND[el]], cvLen, cvOff, cvRot));
            }
            // VARIATION (4) / LEGATO (5): show the window the ENGINE actually reads for this voice
            // — mono's when the lane DELEGATES (default), the voice's own when Local East — plus the
            // moving playhead and mono's SHARED VAR/LEG probability array (per §4d the array is
            // global; only the reading window is per-voice). strand == editor lane (VAR 4, LEG 5).
            // Use polyLOR (editor-order, masks &7) NOT polyLenE (engine-order, masks &3 → would
            // alias VAR→REST). eng.lor(el) / finalRandomByStrand(el) are mono's, as in mirrorMono.
            for (int el = dotModular::SandsGrid::POLY_LANES; el < dotModular::SandsGrid::EAST_LANES; ++el) {
                const bool localEast =
                    module->params[varlegDelegId(pv, el - dotModular::SandsGrid::POLY_LANES)].getValue() > 0.5f;
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
                visualEditor->setLanePlayStep(el, calcPlayhead(pvTick, cvLen, cvOff, cvRot));
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
        lights[ownerLightId(lane)].setBrightness(
            params[ownerDispId(lane)].getValue() > 0.5f ? 1.f : 0.f);

    Monsoon* mon = redDot::findMonsoonEitherSide(this);
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
    if (gateModDiv.process()) {
        for (int lane = 0; lane < 6; ++lane) {
            auto& din = inputs[dirModId(lane)];
            if (din.isConnected()) {
                int nch = std::min(din.getChannels(), 16);
                for (int ch = 0; ch < nch; ++ch) {
                    bool high = din.getVoltage(ch) > 1.f;
                    if (high && !dirModPrev[lane][ch]) ++dirModEdges[lane][ch];
                    dirModPrev[lane][ch] = high;
                }
            }
            auto& gin = inputs[delegModId(lane)];
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
    dotModular::VoiceResolver resolver(eng);
    for (int l = 0; l < 4; ++l) {
        auto& out = outputs[PROB_OUT_REST + l];
        out.setChannels(nCh < 1 ? 1 : nCh);
        // Uniform addressing: VCV channel ch (0-based) carries voice number ch+1.
        //   ch 0 → voice 1 (mono master strand) — previously hardcoded 0V; now its real draw.
        //   ch v → voice v+1 (poly) for v in 1..nV.
        // The resolver maps voice 1 → master accessors, voices 2..16 → poly bank (voice-2),
        // so this single loop replaces the old hand-split (0V stub + per-poly indexing).
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
