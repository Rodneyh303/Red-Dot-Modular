#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "ui/StoreEditAction.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/GoldPolyPort.hpp"
#include "ui/SvgPanelKit.hpp"
//#include "MonsoonStraitsSands.hpp"
#include "StraitsSandsMacroVisual.hpp"
#include "dsp/SandsTopology.hpp"           // step 4b: Macro lock predicate via the resolver
#include <cassert>
#include <cstring>   // memcmp — the prob-CV out write-elimination bit-compare (perf fix)
#include "MonsoonSandsVisualExpander.hpp"  // complete mono type + SandsMonoVisualIds for the tab-1 mono mirror
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
#include "ui/OwnerCell.hpp"       // DirCell
#include "ui/WarpCurveButton.hpp" // CC_PROMPT_04: the CRV-column popup trigger
#include "ui/WarpCurvePopup.hpp"  // CC_PROMPT_04 STEP 3: the curve editor popup
#include "dsp/engines/SequencerEngine.hpp"  // LaneDir
#include "dsp/managers/SandsParameterManager.hpp"
#include "dsp/VoiceResolver.hpp"   // activeVoiceCount + voice identity, single source of truth for the tab→voice mapping and uniform 16-voice addressing for prob-out
#include "dsp/LaneMapping.hpp"        // lane constants + strong types (identity — tables deleted Phase 3b)
#include "MonsoonProfile.hpp"   // CC_PROMPT_cpu_section_profile: Suspect-A counter + section timers (no-op flag-off)

using namespace rack;
using namespace redDot;
using namespace MonsoonIds;
using namespace StraitsMacroVisualIds;

#ifdef MONSOON_PROFILE
// CC_PROMPT_cpu_section_profile — DIAGNOSTIC SCAFFOLDING (never ships; delete together
// with the MONSOON_PROF_* call sites in StraitsSandsMacroVisual::process). TSC-tick
// accumulators for the Macro module's process sections; the control-rate log (~50000
// samples) converts to ns/sample. Aggregate across Macro instances — read RELATIVE shares.
static uint64_t monsoonProfAcc_lookup = 0;   // Monsoon lookup (div /8) + dir-mod edge scan — per sample
static uint64_t monsoonProfAcc_sweep = 0;    // spread publish + gate + resolver + 7-lane x nCh prob-CV sweep — PRIME SUSPECT A
static uint64_t monsoonProfAcc_total = 0;    // WHOLE-process (CC_PROMPT_TILT_AND_RAIL_FEELTEST: restored safely — see the guard in process())
static uint64_t monsoonProfWindow = 0;       // samples since the last log
// The step cache's OWN effectiveProbability calls (engine thread) — the shared accessor
// counter also tallies the bars' UI-thread refresh, so this split pins which consumer
// a calls/sample number belongs to (the meter only sees the engine side).
static uint64_t monsoonProfSweepCalls = 0;
#endif

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
    SandsParameterManager*     paramMgr     = nullptr;
    TabButtonGroup*            tabGroup     = nullptr;
    int viewVoice = 0;   // which voice's resulting probabilities to DISPLAY (read-only)
    // (lastSendVoice removed: the per-voice send sync dance is gone -- StoreKnobs read/write
    //  the live view voice directly, so there is no proxy to re-sync on voice change.)
    bool                       initialized  = false;
    // Light/dark panel swap: kit's loadPanel() owns the live SvgPanel; we keep
    // both backgrounds and swap via the panel child (same pattern as East).
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    // Widget*, not ParamWidget*: ModArcOverlay needs the widget only for GEOMETRY
    // (box.size + attachOverKnob, both Widget-level) and takes its VALUE from the
    // getSetNorm lambda. Typing this ParamWidget* was the last thing coupling the arcs to
    // the param system. See MVC_UNIFICATION step 1d.
    std::vector<std::pair<rack::widget::Widget*, int>> pendingSpreadArcs;
    // The store lives on MONSOON, resolved lazily (it may be attached after this widget is
    // built, or detached later). One resolver shared by every store-backed control here.
    std::function<Monsoon*()> storeResolver() {
        auto* self = this;
        return [self]() -> Monsoon* {
            return self->module ? redDot::findMonsoonEitherSide(self->module) : nullptr; };
    }
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
            // Read the SET value from the STORE via lane, not from a param by id. Correct
            // both before step 1d (Macro's step() mirrors param -> store, so this is the
            // same number) and after it (the store becomes authoritative). This removes the
            // arcs' dependence on paramId -- what made them a step-1d blocker.
            arc->getSetNorm = [mm, lane]() -> float {
                Monsoon* mon = mm ? findMonsoonEitherSide(mm) : nullptr;
                if (!mon) return 0.5f;
                return rack::math::clamp((mon->getGlobalSpread(lane) + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->getModNorm = [mm, lane]() -> float {
                if (!mm || lane < 0 || lane >= dotModular::SandsGrid::POLY_LANES) return 0.5f;
                return rack::math::clamp((mm->spreadEffective[lane] + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->isActive = [mm, lane]() -> bool {
                if (!mm || lane < 0 || lane >= dotModular::SandsGrid::POLY_LANES) return false;
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
                            "res/panels/StraitsSandsMacroVisual_48HP.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsSandsMacroVisual_48HP_light.svg"));
        // Kit owns the SvgPanel (created + setPanel'd here); widgets bind to the
        // named shapes baked into the SVG by panel_src/gen_macro_mono.py, so the
        // gen script is the single source of widget geometry (no rowY/columns here).
        loadPanel(asset::plugin(pluginInstance,
                            "res/panels/StraitsSandsMacroVisual_48HP.svg"));

        redDot::addRedScrews(this);

        // Visual editor + view tabs are custom widgets (not kit shapes) — placed
        // manually. Everything else (jacks, attens, spreads, sends, prob-outs)
        // binds to named SVG shapes so geometry lives only in the gen script.
        tabGroup = new TabButtonGroup(16, 1, 2, mm2px(ED_W), mm2px(10.f));   // V1 mono + V2..V16 poly
        tabGroup->box.pos = mm2px(Vec(ED_X, ED_Y - 12.f));
        addChild(tabGroup);

        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->setLaneCount(dotModular::SandsGrid::POLY_LANES);   // 7 lanes — matches East exactly
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
        //visualEditor->laneEditBlockedFn = [](int /*editorLane*/) -> bool { return false; };
        // NOT be gated by who DRIVES the lane downstream. Previously this locked V1 LOR whenever MONO
        // owned the lane (owner(0,l)==MONO), which produced the asymmetry: on the Macro V1 tab with Mono
        // owning a lane, its SPREAD was editable but its LOR was blocked — and East+Macro (East owns V1)
        // did NOT block, so the two combos behaved inconsistently. Macro's own global LOR is now always
        // editable (like its spread, like V2+); ownership governs what reaches the ENGINE, not what Macro
        // may edit on its own panel. Off the V1 tab this was already false.
        visualEditor->laneEditBlockedFn = [](int /*editorLane*/) -> bool { return false; };
        // Ghost echo needs TRUE lock-mode state -- independent of the edit-permission above (which is
        // deliberately false on Macro). Macro edits under lock still show the ghost echo.
        visualEditor->lockActiveFn = [this]() -> bool { auto* m = getMonsoon(); return m && m->engine.locked; };
        // LOR drag undo: Macro edits GLOBAL LOR (setGlobalLor, indexed by ENGINE lane). Only
        // lanes 0..3 map to a global engine lane (VAR/LEG have no global LOR). Push a Rack
        // history action; refresh the editor cache too (store->editor seed is event-driven).
        visualEditor->onLorCommit = [this](int lane, const int before[3], const int after[3]) {
            auto* m = getMonsoon(); if (!m) return;
            if (lane < 0 || lane > 4) return;   // only MEL/OCT/REST/ACC/QMIX have global LOR (5 poly lanes)
            const int bef0=before[0],bef1=before[1],bef2=before[2];
            const int aft0=after[0], aft1=after[1], aft2=after[2];
            auto* ed = visualEditor;
            redDot::applyAndPushStoreEdit<Monsoon>(m, "LOR edit",
                [lane, bef0,bef1,bef2, aft0,aft1,aft2, ed](Monsoon& mm, bool redo) {
                    // Typed envelope: the undo/redo SELECTOR rides Rack history as a bool
                    // (was a 0.f/1.f float decoded with `dir > 0.5f` — envelope abuse).
                    const int L = redo?aft0:bef0, O = redo?aft1:bef1, R = redo?aft2:bef2;
                    mm.setGlobalLor(lane, 0, (float)L);
                    mm.setGlobalLor(lane, 1, (float)O);
                    mm.setGlobalLor(lane, 2, (float)R);
                    if (ed && lane >= 0 && lane < dotModular::SandsGrid::POLY_LANES) {
                        ed->currentState.lanes[lane].length   = std::max(1, L);
                        ed->currentState.lanes[lane].offset   = O;
                        ed->currentState.lanes[lane].rotation = R;
                    }
                },
                false, true);
        };

        Module* mod_ = module;
        auto themeOut = [mod_](redDot::GoldPolyPort* p) {
            p->lightTheme = [mod_]() { Monsoon* m = mod_ ? redDot::findMonsoonEitherSide(mod_) : nullptr;
                                       return m && m->lightTheme; };
        };

        // ── CRITICAL lane-order note ──────────────────────────────────────────
        // The panel ROWS, anchors and store accessors (getGlobalSpread/Atten/Tap,
        // getMacroSend, cvId, PROB_OUT_REST) all use the ONE unified lane order
        // (MEL,OCT,QMIX,REST,ACC top→bottom) — editor lane == engine lane (lane-index
        // unification), so el passes through everywhere with NO conversion. (Historically
        // the two orders differed and passing el straight through was the "top row is
        // MELODY but the knob is labelled/modulates REST" bug.) Anchor stays <el>.
        // Canonical lane names for tooltips (top→bottom).
        static const char* const* EDN = dotModular::SandsLaneNames::EDITOR;

        // 5 poly probability CV outs — jack on editor row el drives engine lane's prob out.
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el)
            bindOutput<redDot::GoldPolyPort>(
                "output_prob_" + std::to_string(el),
                StraitsMacroVisualIds::PROB_OUT_START + el,
                std::function<void(redDot::GoldPolyPort*)>(themeOut));

        // ── Left section: 5 poly lanes × (4 CV jacks + 4 attens + 1 spread) ──
        // Anchor row = editor lane el; store/id = the same lane (identity).
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            for (int c = 0; c < 4; ++c)
                bindInput<PJ301MPort>(
                    "input_cv_" + std::to_string(el) + "_" + std::to_string(c), cvId(el,c));
            // STORE-BACKED (MVC step 1d): global attenuverters (engine-indexed store).
            for (int c = 0; c < 4; ++c) {
                static const char* CN[4] = {"Length","Offset","Rotation","Spread"};
                const std::string albl = std::string(EDN[el]) + " " + CN[c] + " CV depth";
                auto* k = redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    "param_atten_" + std::to_string(el) + "_" + std::to_string(c), storeResolver(),
                    -1.f, 1.f, 0.f, albl,
                    [el, c](Monsoon& m)          { return m.getGlobalAtten(el, c); },
                    [el, c](Monsoon& m, float v) { m.setGlobalAtten(el, c, v); });
                if (k) leftAttenuverters.push_back(k);
            }
        }

        // Per-lane global SPREAD trimpots — anchor param_spr_<el>, store the same lane.
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            auto* sp = redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                "param_spr_" + std::to_string(el), storeResolver(),
                -1.f, 1.f, 0.f, std::string(EDN[el]) + " spread",
                [el](Monsoon& m)          { return m.getGlobalSpread(el); },
                [el](Monsoon& m, float v) { m.setGlobalSpread(el, v); });
            if (sp) pendingSpreadArcs.push_back({sp, el});   // arc reads the lane's spread
        }

        // ── CC_PROMPT_04 STEP 1: w-level knobs (param_wlevel_<el>) — Macro's
        //    Macro-delegated DEFAULT depth per lane (globalWarpW[lane]): the value every
        //    ceded (owner()==MACRO) voice on that lane tracks (WarpBank's delegated mix).
        //    Same delegation pattern as the spread base above (getGlobalSpread) — Macro
        //    authors the default, always editable (its globals exist regardless of East).
        //    The setter bumps Monsoon's pending warp-rebuild scope; process() performs
        //    rebuildWarpMirrorLaneDelegating (that lane across DELEGATING voices only) on
        //    the audio thread — prompt-03's "Macro curve/w edit" dirty scope.
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                "param_wlevel_" + std::to_string(el), storeResolver(),
                0.f, 1.f, 0.f, std::string(EDN[el]) + " warp level (delegated default)",
                [el](Monsoon& m)          { return m.getGlobalWarpW(el); },
                [el](Monsoon& m, float v) {
                    m.setGlobalWarpW(el, v);
                    m.requestWarpRebuildLaneDelegating(el);
                });
        }

        // Macro→voice MIX-IN send 2×2 grids — anchor param_send_<el>_<item>, store the same lane.
        // STORE-BACKED: reads/writes editor.macroSend for the viewed voice (slot resolved LIVE).
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el)
            for (int item = 0; item < 4; ++item) {
                redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    "param_send_" + std::to_string(el) + "_" + std::to_string(item),
                    storeResolver(), -1.f, 1.f, 0.f,
                    std::string(EDN[el]) + " mix-in " + std::to_string(item) + " (viewed voice)",
                    [this, el, item](Monsoon& m) {
                        const int slot = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);
                        return m.getMacroSend(slot, el, item);
                    },
                    [this, el, item](Monsoon& m, float v) {
                        const int slot = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);
                        m.setMacroSend(slot, el, item, v);
                    });
            }
        // PRE/POST CV taps per lane — anchor param_taplor_/tapspr_<el>, store the same lane.
        // P9b: PER-VOICE, following the send-knob pattern in the columns beside them — the
        // SAME trimpots, now authoring the VIEWED voice's own macroTap (continuous PRE↔POST
        // lerp, default 1.0 = POST) exactly like the sends resolve the tab's voice. No
        // controls added/changed; the Macro-global tap store (the delegation default,
        // editor.globalTap) is kept untouched for the delegation wiring.
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                "param_taplor_" + std::to_string(el), storeResolver(),
                0.f, 1.f, 1.f, std::string(EDN[el]) + " LOR send tap (PRE-POST, viewed voice)",
                [this, el](Monsoon& m) {
                    const int slot = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);
                    return m.getMacroTap(slot, el, 0);
                },
                [this, el](Monsoon& m, float v) {
                    const int slot = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);
                    m.setMacroTap(slot, el, 0, v);
                });
            redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                "param_tapspr_" + std::to_string(el), storeResolver(),
                0.f, 1.f, 1.f, std::string(EDN[el]) + " spread send tap (PRE-POST, viewed voice)",
                [this, el](Monsoon& m) {
                    const int slot = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);
                    return m.getMacroTap(slot, el, 1);
                },
                [this, el](Monsoon& m, float v) {
                    const int slot = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);
                    m.setMacroTap(slot, el, 1, v);
                });
        }

        // ── Direction cells (param_dir_<lane>) — per-lane direction toggle (Fwd/Rev/Pend/PingPong).
        // Macro is the global editor → DirCell sets the MONO direction (laneDirPending_).
        // Kit markers use EDITOR lane order (row 0..3 = MEL/OCT/REST/ACC), matching East.
        // Locked when no Monsoon or when Mono is present (Mono is the authority).
        static const NVGcolor editorDirCol[dotModular::SandsGrid::POLY_LANES] = {
            nvgRGB(0xd4,0xaf,0x37), nvgRGB(0xb8,0x86,0x0b),  // MEL gold, OCT dark gold
            nvgRGB(0x80,0x60,0xc0),  // QMIX purple
            nvgRGB(0x50,0x50,0x50), nvgRGB(0xff,0x95,0x00),  // REST grey, ACC orange
            nvgRGB(0x20,0x80,0x20), nvgRGB(0x80,0x40,0x20)   // VAR green, LEG brown
        };
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            // STORE-BACKED (MVC step 1: direction de-param). The DirCell reads/writes
            // editor.globalDir via get/setGlobalDir; the manager reads getGlobalDir(lane).
            // Editor row == engine lane (identity since lane-index unification), so the
            // cell on editor row el targets engine lane el directly. (Historically
            // the orders differed and passing el straight in made the MELODY-row cell
            // drive REST's direction.) Anchor stays param_dir_<el>.
            bindWidget<DirCell>(
                "param_dir_" + std::to_string(el),
                std::function<void(DirCell*)>([this, el](DirCell* w) {
                    w->laneCol = editorDirCol[el];
                    const float stepW = (ED_W - 2.f*6.f) / 16.f;
                    w->box.size = mm2px(Vec(stepW, ED_LANE_H * 0.9f));
                    w->getStateFn = [this, el]() -> int {
                        auto* mm = getMonsoon();
                        return mm ? (int)std::lround(mm->getGlobalDir(el)) & 3 : 0;
                    };
                    w->setStateFn = [this, el](int v) {
                        if (auto* mm = getMonsoon()) mm->setGlobalDir(el, (float)(v & 3));
                    };
                    w->pushUndoFn = [this, el](int oldV, int newV) {
                        auto* mm = getMonsoon(); if (!mm) return;
                        redDot::applyAndPushStoreEdit<Monsoon>(mm, "direction",
                            [el](Monsoon& m, float val) { m.setGlobalDir(el, val); },
                            (float)(oldV & 3), (float)(newV & 3));
                    };
                    w->lockWhen = [this]() { return !getMonsoon(); };
                })
            );
        }

        // ── CC_PROMPT_04 STEP 2: warp curve-editor trigger buttons (param_warpedit_<el>).
        //    Macro's button is ALWAYS available — it edits Macro's own delegated-default
        //    curve for the lane (the curve every ceded voice tracks) — locked only with no
        //    Monsoon attached (the store lives on Monsoon; the DirCell lock precedent).
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            bindWidget<dotModular::WarpCurveButton>(
                "param_warpedit_" + std::to_string(el),
                std::function<void(dotModular::WarpCurveButton*)>([this, el](dotModular::WarpCurveButton* w) {
                    w->laneCol = editorDirCol[el];
                    Vec ctr = w->box.pos.plus(w->box.size.div(2.f));
                    w->box.size = mm2px(Vec(6.5f, ED_LANE_H * 0.9f));   // the cell-column rhythm
                    w->box.pos  = ctr.minus(w->box.size.div(2.f));
                    w->lockWhen = [this]() { return !getMonsoon(); };
                    w->onOpen   = [this, el]() { openWarpPopup(el); };
                })
            );
        }

        // Direction gate-mod jacks (input_dir_mod_<lane>) — mono, gate cycles Fwd→Rev→Pend→PingPong.
        {
            Module* mod_ = module;
            auto themeIn = [mod_](redDot::GoldPolyPort* p) {
                p->lightTheme = [mod_]() { Monsoon* m = mod_ ? redDot::findMonsoonEitherSide(mod_) : nullptr;
                                          return m && m->lightTheme; };
            };
            for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane)
                bindInput<redDot::GoldPolyPort>("input_dir_mod_" + std::to_string(lane),
                    dirModId(lane), std::function<void(redDot::GoldPolyPort*)>(themeIn));
        }

        paramMgr = new SandsParameterManager(nullptr, nullptr, 7, 0, true);   // monoContext: V1 interpolates from the MONO draw
        flushSpreadArcs();   // attach spread mod-arcs on top of the trimpots

        // dot.modular connect mark (brand mark; greyed when no Monsoon attached).
        {
            connectMark = redDot::makeConnectMark(module, mm2px(rack::math::Vec(W_MM * 0.5f, 124.f)), mm2px(8.f));
            addChild(connectMark);
        }
    }

    ~StraitsSandsMacroVisualWidget() override {
        // The scrim + popup are SCENE children while open, and their lambdas
        // capture this widget — detach + delete them now (immediate, not the
        // deferred requestDelete path) so they can never outlive the host.
        closeWarpPopup(/*immediate=*/true);
        delete paramMgr;
    }

    void appendContextMenu(Menu* menu) override {
        ModuleWidget::appendContextMenu(menu);
        auto* mod = dynamic_cast<StraitsSandsMacroVisual*>(module);
        if (!mod) return;
        // Spread interpolation target moved to the Monsoon module context menu.
    }

    Monsoon* getMonsoon() {
        return module ? findMonsoonEitherSide(module) : nullptr;
    }

    // ── CC_PROMPT_04 STEP 3: the warp curve popup ──────────────────────────────────
    // Macro's popup edits globalWarpF[lane] — ITS OWN delegated-default curve (the curve
    // every ceded voice on that lane tracks; the button is always available). Closes on
    // tab switch or Monsoon detach. Placed over the LEFT cluster (3..83mm) so the lane's
    // bars + the w-level column stay visible; a module-sized scrim under it closes on any
    // click. Every edit path commits setGlobalWarpF + bumps requestWarpRebuildLane-
    // Delegating — the lane's bars across DELEGATING voices redistribute live (prompt-
    // 03's "Macro curve/w edit" dirty scope).
    // Z-ORDER (CC_PROMPT_04-FIX): the scrim + popup attach to APP->scene — the
    // createMenu/MenuOverlay host — NOT to this module widget. A module child draws
    // and receives events WITHIN the module's own layer: later-drawn module children,
    // adjacent modules and cables render on top of it (controls show through) and
    // higher layers intercept presses before the popup's knobs (can't grab / can't
    // drag). As the scene's LAST children they float above everything and own their
    // events; geometry is SCENE-space, converted from module-local via
    // getAbsoluteOffset (the box.pos chain through rack scroll + zoom).
    dotModular::WarpCurvePopup* warpPopup_ = nullptr;
    rack::widget::Widget*       warpScrim_  = nullptr;
    int warpPopupTab_ = -1;

    void openWarpPopup(int lane) {
        closeWarpPopup();
        Monsoon* m = getMonsoon();
        if (!m || lane < 0 || lane >= dotModular::SandsGrid::POLY_LANES) return;
        const rack::math::Vec sceneOrigin = getAbsoluteOffset(rack::math::Vec(0.f, 0.f));
        auto* scrim = new dotModular::WarpPopupScrim();
        scrim->box.pos  = sceneOrigin;       // this module's rect, in SCENE coordinates
        scrim->box.size = box.size;
        scrim->onClick  = [this]() { closeWarpPopup(); };
        APP->scene->addChild(scrim);         // BEFORE the popup → popup on top (draw + events)
        auto* pop = new dotModular::WarpCurvePopup();
        pop->resolveMon  = [this]() { return getMonsoon(); };
        pop->slot        = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);  // informational
        pop->lane        = lane;
        pop->macroGlobal = true;             // Macro authors the delegated default
        pop->laneName    = dotModular::SandsLaneNames::EDITOR[lane];
        pop->voiceName   = "MACRO";
        pop->isLight     = [this]() { auto* mm = getMonsoon(); return mm && mm->lightTheme; };
        pop->box.pos     = getAbsoluteOffset(mm2px(Vec(3.f, 16.f)));   // module-local → scene-space
        APP->scene->addChild(pop);
        pop->refresh();
        warpScrim_  = scrim;
        warpPopup_  = pop;
        warpPopupTab_ = tabGroup ? tabGroup->getSelectedTab() : -1;
    }
    // Detach from the SCENE — the owner while open, NOT this module widget. The
    // default path uses requestDelete (deferred to the scene's next step) because
    // closeWarpPopup runs inside event dispatch (the scrim's own onButton) and
    // inside step(): deleting a widget mid-dispatch invalidates the parent's child
    // iteration. immediate=true (host destruction) detaches + deletes NOW via the
    // widget's current parent.
    void closeWarpPopup(bool immediate = false) {
        if (warpScrim_) {
            if (immediate) {
                if (warpScrim_->parent) warpScrim_->parent->removeChild(warpScrim_);
                delete warpScrim_;
            } else {
                warpScrim_->requestDelete();
            }
            warpScrim_ = nullptr;
        }
        if (warpPopup_) {
            if (immediate) {
                if (warpPopup_->parent) warpPopup_->parent->removeChild(warpPopup_);
                delete warpPopup_;
            } else {
                warpPopup_->requestDelete();
            }
            warpPopup_ = nullptr;
        }
        warpPopupTab_ = -1;
    }

    // STEP 4b: ownership authority for the Macro widget's V1 view. Macro is present
    // (this widget IS Macro). SANDS CONSOLIDATION Step 7: monoV1Owner removed — V1
    // ownership now via eastV1Owner[] only. lockedOn(MACRO,0,l) == owner(0,l)!=MACRO,
    // i.e. "Mono owns it" — matching the old mv->ownerDispId(l) > 0.5 test.
    // NOTE: currently UNUSED — laneEditBlockedFn stopped gating V1 LOR on Mono ownership (Macro's own
    // global LOR is always editable, matching its spread). Kept because the tracked East-ownership
    // open question (SANDS_TOPOLOGY_RESOLVER_PLAN.md) may revive a per-lane V1 ownership predicate.
    //[[maybe_unused]] dotModular::SandsTopology buildV1Topo() {
    //[[maybe_unused]] 
    dotModular::SandsTopology buildV1Topo() {
        dotModular::SandsTopology::Inputs in;
        if (auto* mon = getMonsoon()) {
            mon->expanderManager.fillPresence(in, mon->engine.numPolyVoices);  // single authority
            // MVC step 1d: Mono's owner is STORE-BACKED (editor.monoOwner via getMonoOwner).
            // mon IS the Monsoon store owner; was mv->params[ownerDispId(l)].
            // SANDS CONSOLIDATION Step 7: monoV1Owner removed — V1 ownership via eastV1Owner only.
        }
        return dotModular::SandsTopology::build(in);
    }

    // LOR is STORE-BACKED (MVC step 1: LOR de-param). Macro's GLOBAL LOR lives in the store's
    // dedicated globalLor[12] array (lane*3 + c), NOT East's per-slot lorBase[336] -- Macro's
    // LOR is global, not per-voice. globalLor is the array the ENGINE already reads
    // (MonsoonSandsManager getGlobalLor) and that already persists (PersistenceManager
    // editorGlobalLor). The 12 globalDnaId params were a redundant mirror of it; removing them
    // leaves get/setGlobalLor as the single source. lane here is the ENGINE lane, matching
    // getGlobalLor's indexing.
    void saveLOR() {
        if (!module || !visualEditor) return;
        auto* mm = getMonsoon(); if (!mm) return;
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {   // l = engine lane
            const auto& lane = visualEditor->currentState.lanes[l];
            mm->setGlobalLor(l, 0, (float)lane.length);
            mm->setGlobalLor(l, 1, (float)lane.offset);
            mm->setGlobalLor(l, 2, (float)lane.rotation);
        }
    }
    void loadLOR() {
        if (!module || !visualEditor) return;
        auto* mm = getMonsoon(); if (!mm) return;
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {   // l = engine lane
            auto& lane = visualEditor->currentState.lanes[l];
            lane.length   = std::max(1,(int)std::round(mm->getGlobalLor(l, 0)));
            lane.offset   = (int)std::round(mm->getGlobalLor(l, 1));
            lane.rotation = (int)std::round(mm->getGlobalLor(l, 2));
        }
    }

    // (macroOwnProbability DELETED — CC_PROMPT_UI_TO_RESOLVER_SPEC step 2. The bars
    //  now call the ONE accessor directly — monsoon->engine.effectiveProbability(
    //  voice, lane, step, SequencerEngine::ProbLens::MacroOwn) — the same call the
    //  prob CV OUT makes through VoiceResolver::laneProbabilityAtStep. One producer,
    //  both consumers; the widget-level wrapper had nowhere left to add value.)

    void step() override {
        ModuleWidget::step();
        kitStep();   // kit: dev-mode live-reload poll (no-op unless enabled)
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
        // CC_PROMPT_04: close the curve popup when its bound target goes away — tab
        // switch or Monsoon detach.
        if (warpPopup_ && (!monsoon || warpPopupTab_ < 0 ||
                           (tabGroup && tabGroup->getSelectedTab() != warpPopupTab_)))
            closeWarpPopup();
        if (!monsoon) { if (visualEditor) visualEditor->clearPlaySteps(); return; }

        // MVC step 1: the dual-write GLOBAL mirror is fully RETIRED. Every global-slice
        // group -- LOR, attenuverters, direction -- is store-backed now (editor.globalLor /
        // globalAtten / globalDir), so the engine reads the store directly and there is no
        // params->store mirror left to run here.

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

        auto* mod = static_cast<StraitsSandsMacroVisual*>(module);

        if (!initialized) {
            loadLOR();
            // Direction is store-backed, authoritative, and PERSISTED (editorGlobalDir).
            // It must NOT be seeded from the engine here: on patch load the store already
            // holds the saved value, but engine.laneDirPending_ is still at its Forward
            // default (the store drives the engine, not the reverse), so seeding would
            // clobber the loaded direction with 0. (This is exactly the "widget must NOT
            // overwrite FROM the engine" rule noted below -- the old seed was safe only
            // because it wrote a re-derivable display proxy, not the authoritative store.)
            initialized = true;
        }

        // ── Direction sync: Step 4 (lane_direction_homes.md) — NO sync needed.
        //    dirDispId IS the home. The DirCell writes it, the manager reads it and pushes
        //    to laneDirPending_. The widget must NOT overwrite dirDispId FROM the engine.
        //    (Macro only owns lanes 0..3 when Mono delegates them — the manager handles
        //    ownership by reading from the correct expander.)

        // Spread: processDNA has already applied CV offset to SPREAD_REST/MEL/OCT params.
        // Read effective values here for SpreadManager display.
        paramMgr->setSpreadAll(0, mod->spreadEffective[0]);
        paramMgr->setSpreadAll(1, mod->spreadEffective[1]);
        paramMgr->setSpreadAll(2, mod->spreadEffective[2]);
        paramMgr->setSpreadAll(3, mod->spreadEffective[3]);
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
        const int  pv = dotModular::VoiceResolver::polyVoiceSlot(viewVoiceNum);  // voiceSlot for poly, -1 on mono

        // Mix-in sends: STORE-BACKED via the StoreKnobs above (they read/write
        // getMacroSend/setMacroSend for the live view voice directly). The old per-voice
        // load/store sync dance and its clobber guard are gone -- no display proxy to sync.

        saveLOR();
        // (syncPatternEngineToEditor DELETED — CC_PROMPT_UI_TO_RESOLVER_SPEC step 2: its
        //  probability writes were dead the same frame — the loop below overwrote all 7
        //  lanes from the ONE accessor — and its SpreadManager::getInterpolatedValue path
        //  was the last SpreadInterp-in-UI bar composition outside the accessor.)
        // ── The bars read the ONE accessor (CC_PROMPT_UI_TO_RESOLVER_SPEC step 2) ──
        // Every lane, every step, the DISPLAYED voice (pv on poly tabs, voiceSlot 0 on
        // the mono/V1 tab) — the SAME call the prob CV OUT makes through
        // VoiceResolver::laneProbabilityAtStep (MacroOwn lens: the published base draw +
        // Macro's OWN spread + the ONE warp composition; Macro never borrows East). One
        // producer, both consumers — bars and out can no longer diverge.
        {
            const int barVoice = onMonoTab ? dotModular::VoiceResolver::kMonoSlot : pv;
            for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
                for (int s = 0; s < SandsVisualEditorV4::STEP_COUNT; ++s)
                    visualEditor->currentState.lanes[lane].probabilities[s] =
                        monsoon->engine.effectiveProbability(barVoice, lane, s,
                                                             SequencerEngine::ProbLens::MacroOwn);
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
        auto& eng = monsoon->engine;
        // Per-lane direction cue: use Macro's OWN macroLaneSign_ (always follows Macro's DirCell)
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
            visualEditor->setLanePlayDir(l, eng.lastPlayDir * eng.macroLaneSign_[l]);
        }
        // TAB-1 MONO MIRROR: view tab 1 = voice 1 = the mono master strand when Sands
        // Mono is attached. Show mono's LORS read-only (consistent treatment with the
        // other voices' display), not Macro's global base. (Macro's left attenuverters
        // are hidden on tab 1 via gen panel / widget — Macro's global base doesn't reach
        // voice 1; only the mix-in sends could, under the deferred interp. Y.)
        // (tab1Mono removed — its only use, readOnly=tab1Mono, was replaced by the
        // per-lane laneEditBlockedFn set in the constructor.)
        // When V1 is editable (no Mono), Macro's global LOR knobs act as the V1 base.
        // The global base params are already wired to the engine for poly; for V1,
        // processDNA reads them via publishGlobal which writes all mono strands.
        // Macro V1 editing is locked PER-LANE via laneEditBlockedFn (set up in the
        // constructor): on the V1 tab with Mono attached, a lane is locked iff Mono owns
        // it; Macro-owned (delegated) lanes stay editable. This replaced the old blunt
        // readOnly = tab1Mono, which locked ALL of Macro's V1 whenever Mono was present
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
            // engine lane (0=REST 1=MEL 2=OCT 3=ACC 4=QMIX) → editor lane.
            for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
                int ownLen = (int)std::lround(mod->macroBase[l][0] + mod->macroCVDelta[l][0]);
                int ownOff = (int)std::lround(mod->macroBase[l][1] + mod->macroCVDelta[l][1]);
                int ownRot = (int)std::lround(mod->macroBase[l][2] + mod->macroCVDelta[l][2]);
                ownLen = std::max(1, ownLen);
                visualEditor->currentState.lanes[l].setDisplayLOR(ownLen, ownOff, ownRot);
                // Use Macro's OWN macroLaneTick_ — advanced by the engine in advancePlayhead
                // using Macro's direction (macroLaneDir_), with full bounce support for
                // Pendulum/PingPong. Same pattern as East's laneTickV_.
                // MVC: hide the playhead when stopped (match Mono's started ? tick : -1), so
                // rotation CV only moves the chevron, not the playhead, when the seq is stopped.
                int ph = (eng.stepIndex >= 0) ? eng.macroLaneTick_[l] : -1;
                visualEditor->setLanePlayStep(l, calcPlayhead(ph, ownLen, ownOff, ownRot));
            }
        }
    }

    // Mix-in SEND grid labels (NanoVG; panel carries no baked text). SANDS CONSOLIDATION Step 3:
    // 6 send knobs in a single horizontal row per lane, 9mm pitch, row-aligned to each LED lane.
    // Column headers (LEN/OFF/ROT/SPR/LOR/SPR) sit above lane 0. Geometry is owned by the generator.
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        NVGcontext* vg = args.vg;

        // (P1/G1 no-hide: the old V1 atten-masking rectangle was removed — Macro's left
        // attenuverters are always visible, including on the V1 tab.)

        bool isLight = false;
        if (auto* mon = getMonsoon()) isLight = mon->lightTheme;

        auto font = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!font) font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(vg, font->handle);

        NVGcolor head = isLight ? nvgRGB(40,44,52) : nvgRGB(210,214,222);

        auto anchorMM = [&](const std::string& name, bool& ok) -> Vec {
            if (NSVGshape* s = findNamed(name)) { ok = true; return centerOf(s); }   // px
            ok = false; return Vec(0, 0);
        };

        // Column X positions from lane-0 anchors: items 0..3 = LEN/OFF/ROT/SPR sends,
        // col 4 = LOR tap (param_taplor_0), col 5 = SPR tap (param_tapspr_0).
        float colX[6] = {};
        bool okAny = false;
        for (int it = 0; it < 4; ++it) {
            bool ok = false; Vec s = anchorMM("param_send_0_" + std::to_string(it), ok);
            if (ok) { colX[it] = s.x; okAny = true; }
        }
        { bool ok = false; Vec l = anchorMM("param_taplor_0", ok);  if (ok) { colX[4] = l.x;  okAny = true; } }
        { bool ok = false; Vec sp = anchorMM("param_tapspr_0", ok); if (ok) { colX[5] = sp.x; okAny = true; } }
        if (!okAny) return;

        // Column headers, sitting above lane 0 at the send columns' X.
        const char* colName[6] = { "LEN", "OFF", "ROT", "SPR", "LOR", "SPR" };
        nvgFontSize(vg, 6.0f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, head);
        const float headerY = mm2px(ED_Y - 3.0f);   // 11mm — above lane 0 (14), below the tab band
        for (int c = 0; c < 6; ++c)
            nvgText(vg, colX[c], headerY, colName[c], nullptr);
        // CC_PROMPT_04: the warp curve-editor ("CRV") column header, at the button column's X.
        { bool ok = false; Vec cw = anchorMM("param_warpedit_0", ok);
          if (ok) nvgText(vg, cw.x, headerY, "CRV", nullptr); }
    }
};

// (pubSlewedLane_ DELETED with the old change-fingerprint gate — CC_PROMPT_UI_TO_RESOLVER_SPEC
//  step 3: the step cache reads the ONE accessor directly; nothing walks the raw
//  pubSlewed twins here any more.)

// ── Module process(): 3 poly probability CV outs (ch1 reserved, voices ch2+) ──
void StraitsSandsMacroVisual::process(const ProcessArgs&) {
    using namespace StraitsMacroVisualIds;
#ifdef MONSOON_PROFILE
    // WHOLE-PROCESS avgNs, RESTORED SAFELY (CC_PROMPT_TILT_AND_RAIL_FEELTEST — Rodney noted
    // it went missing): rdtsc bracket around ALL of process(), RAII so the no-Monsoon early
    // return below is covered too; logged at the existing ~50k-sample window as process=.
    // This is the MonsoonProfile.hpp SAFE FORM (now() = one rdtsc, chrono ONLY inside the
    // window's calibrateNsPerTick) — NEVER the per-sample std::chrono pair that caused the
    // 124%. Flag-off builds compile this guard to NOTHING (zero cost, byte-identical).
    struct MonsoonProfTotal {
        uint64_t t0 = monsoonProfile::now();
        ~MonsoonProfTotal() { monsoonProfAcc_total += monsoonProfile::now() - t0; }
    } monsoonProfTotal;
#endif
    MONSOON_PROF_BEGIN(lookup);   // lookup: Monsoon chain walk (div /8) + dir-mod edge scan
    // PERF (Rodney audit item 3): findMonsoonEitherSide walks the expander chain and ran
    // every sample. Topology is control-rate, so cache it and refresh on a divider.
    if (monLookupDiv.process()) cachedMon_ = redDot::findMonsoonEitherSide(this);
    Monsoon* mon = cachedMon_;
    if (!mon) {
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
            auto& out = outputs[PROB_OUT_START + l];
            out.setChannels(1);
            // WRITE-ELIMINATION (perf fix, same discipline as the sweep below): setVoltage
            // only on BIT-difference. The port's own voltages[] is the last-written cache
            // (Rack holds output values once set), so an identical write was a no-op store;
            // comparing the LIVE port (not a shadow cache) makes this immune to
            // setChannels' shrink-zeroing and cable events — a skipped write is provably
            // one that would not have changed a bit.
            const float zero = 0.f;
            if (std::memcmp(&out.voltages[0], &zero, sizeof(float)) != 0)
                out.setVoltage(0.f);
        }
        return;
    }
    // ── Gate edge detection for dir_mod inputs ────────────────────────────
    // Mono jacks (1 channel). Rising edge cycles Fwd→Rev→Pend→PingPong→Fwd.
    // Cycles the dirDispId display proxy param; the widget's step() syncs to engine.
    {
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            // dirModId(el) is the jack on editor row el; globalDir is lane-indexed and
            // editor row == engine lane (identity), so eng = el directly. (Historically
            // the orders differed and cycling el directly advanced REST's direction.)
            auto& in = inputs[dirModId(el)];
            if (!in.isConnected()) continue;
            bool high = in.getVoltage(0) > 1.f;
            if (high && !dirModPrev[el]) {
                if (auto* mm = redDot::findMonsoonEitherSide(this)) {
                    int cur = (int)std::lround(mm->getGlobalDir(el));
                    mm->setGlobalDir(el, (float)((cur + 1) % 4));
                }
            }
            dirModPrev[el] = high;
        }
    }
    MONSOON_PROF_END(lookup);
    MONSOON_PROF_BEGIN(sweep);   // sweep: the per-lane step detect + the STEP-cached resolve + the 7-lane prob-CV out writes
    const float scaleV = (mon->probOutScale == 0) ? 1.f : (mon->probOutScale == 1) ? 5.f : 10.f;
    auto& eng = mon->engine;
    const int nV = eng.numPolyVoices;
    const int nCh = 1 + nV;
    // Channel count is set EVERY sample (polyphony takes effect at once), all 7 lanes.
    for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l)
        outputs[PROB_OUT_START + l].setChannels(nCh < 1 ? 1 : nCh);
    // ── THE STEP CACHE (CC_PROMPT_UI_TO_RESOLVER_SPEC step 3) ─────────────────────
    // THE CONTROL-RATE THROTTLE WORKAROUND IS WHAT THIS REPLACES: the CPU fix throttled
    // the recompute to control rate and fingerprinted every input (a shadow sig) to skip
    // idle ticks — a STOPGAP whose own comments said the proper fix is this migration. The resolve
    // (the ONE accessor: polySlewedPub + spread mix2 + WarpBank::warpedValue → invCDF)
    // is a pure function of the engine stores, and the OUT is LATCHED per step by
    // design — so the resolve runs ONLY when a lane's Macro-lens step changes (or the
    // voice count does), into this cache; the ports hold the last written bits between
    // steps (Rack's port-hold + the write-elimination below). Per-sample work is now:
    // 7 playhead computes (a multiply-add + mask each) + cache compares. The heavy
    // resolve is STEP-rate only (8-16 Hz × 7×(1+N) calls ≈ 0.02%); the control-rate
    // throttle,
    // the change-fingerprint gate and the S&H latch state are DELETED (the latch is
    // inherent: the cache only moves on a step/polyphony change — probOutSampleHold is
    // inert on THIS module's outs now; the value between steps is held by design).
    // macroOwnSpread is published by the MANAGER (publishGlobal's resolver output),
    // not here.
    // PERF (CPU regression fix): the per-lane step-detect + resolve + out-walk below is
    // throttled to control rate (/32). 70dd837 removed the old /32 throttle and relied on the
    // step-cache's per-sample gate (calcPlayhead×7 + compares every sample); that per-sample
    // scaffolding — not the resolve — was the regression (Macro-only 3% vs East 1%). The out is
    // latched per step BY DESIGN, so running the step-detect at control rate instead of per
    // sample is behaviour-identical (a musical step lasts orders of magnitude longer than one
    // control cycle): the step change is caught at the next throttle tick, and between ticks the
    // ports hold their last-written voltages (Rack port-hold + the outFresh write-elimination).
    if (probOutDiv.process())
    for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {   // 7 poly lanes (MEL/OCT/QMIX/REST/ACC/VAR/LEG)
        // Macro's OWN global LOR step for this lane (from macroBase+CVDelta — identical
        // to Macro's editor playhead, independent of East/ownership). Same step for
        // every voice (Macro's view is global); each voice contributes its own draw.
        const int ownLen = std::max(1, (int)std::lround(macroBase[l][0] + macroCVDelta[l][0]));
        const int ownOff = (int)std::lround(macroBase[l][1] + macroCVDelta[l][1]);
        const int ownRot = (int)std::lround(macroBase[l][2] + macroCVDelta[l][2]);
        const int step = calcPlayhead(eng.stepIndex, ownLen, ownOff, ownRot) & 0x0F;
        StepProbRow& row = stepProb_[l];
        if (!row.valid || step != row.step || nV != row.nVoices) {
            row.valid = true;
            row.step = step;
            row.nVoices = nV;
            row.outFresh = false;   // new values: the out walk must run once
            // Uniform addressing: VCV channel ch carries voice ch+1; ch0 → voice 1
            // (mono). Every channel is Macro-OWN at Macro's global step — the resolver
            // routes to the ONE accessor (effectiveProbability, MacroOwn lens) — the
            // SAME source the bars read. East's spread on an East-owned lane cannot
            // reach this out (one-way borrowing); only the step latch + scale differ
            // from the continuous bars, by design.
            dotModular::VoiceResolver resolver(eng);
            for (int ch = 0; ch < nCh; ++ch)
                row.val[ch] = resolver.laneProbabilityAtStep(ch + 1, l, step);
#ifdef MONSOON_PROFILE
            monsoonProfSweepCalls += (uint64_t)nCh;   // this lane's step-rate resolve count
#endif
        }
        // The OUT reads the STEP-CACHED value. WRITE-ELIMINATION (the perf fix, KEPT —
        // now ROW-scoped, the FINISH2 CPU fix): setVoltage ONLY on bit-difference, and
        // the per-channel bit-diff WALK itself runs only when the row (or the scale, or
        // the channel count) changed since the last walk. Steady state per sample is
        // one lane compare instead of (1+N) clamp+memcmp iterations — at 16 channels
        // that walk was ~112 iterations/sample, the dominant per-sample cost on this
        // module's meter. A walk that DOES run keeps the LIVE-port compare (not a
        // shadow cache), so it re-heals any external port mutation: setChannels'
        // shrink-zeroing rides the nCh change, which forces the walk; cable events
        // leave held voltages intact. Steady state: zero walks, zero writes.
        auto& out = outputs[PROB_OUT_START + l];
        if (!row.outFresh || scaleV != row.outScale || nCh != row.outCh) {
            for (int ch = 0; ch < nCh; ++ch) {
                const float v = rack::math::clamp(row.val[ch], 0.f, 1.f) * scaleV;
                if (std::memcmp(&out.voltages[ch], &v, sizeof(float)) != 0)
                    out.setVoltage(v, ch);
            }
            row.outFresh = true;
            row.outScale = scaleV;
            row.outCh = nCh;
        }
    }
    MONSOON_PROF_END(sweep);
#ifdef MONSOON_PROFILE
    // CC_PROMPT_cpu_section_profile — section log (every ~50000 samples, ~1 s @ 48k).
    // Suspect-A (the old 124%) RESOLVED by CC_PROMPT_UI_TO_RESOLVER_SPEC: the
    // effectiveProbability resolve is STEP-cached above. Expected reads:
    //   engine step-cache ≈ 0.00x calls/sample at idle; when playing ≈
    //   7×(1+N)/steps-per-second ÷ 48000 (a fraction of the old control-rate-throttle number);
    //   total= also counts the bars' UI-thread refresh (7 lanes × 16 steps per UI
    //   frame — NOT on the audio meter; step-cache= is the meter's number).
    if (++monsoonProfWindow >= 50000) {
        monsoonProfWindow = 0;
        const double npt = monsoonProfile::calibrateNsPerTick(monsoonProfile::now());
        // (process= below reads the accumulator BEFORE this call's RAII guard folds its
        // ticks in — one call of 50000 lags, a 0.002% bookkeeping detail, not a lie.)
        static uint64_t prevCalls = 0, prevSweep = 0;
        const uint64_t calls =
            monsoonProfile::effProbCalls.load(std::memory_order_relaxed);
        const uint64_t swp = monsoonProfSweepCalls;
        const double cps  = double(calls - prevCalls) / 50000.0;  // total calls/sample this window
        const double scps = double(swp - prevSweep) / 50000.0;    // engine-thread (step cache) share
        prevCalls = calls;
        prevSweep = swp;
        const double perStep = 7.0 * (1.0 + (double)nV);          // POLY_LANES x (1+N) per STEP
        if (npt > 0.0)
            INFO("[MONSOON PROFILE] Macro ns/sample: lookup=%.0f sweep=%.0f sum=%.0f process=%.0f "
                 "| effProbability calls/sample=%.3f (engine step-cache=%.3f, UI bars=%.3f) "
                 "(step-rate 7x(1+N)=%.1f per step, N=%d)",
                 monsoonProfAcc_lookup * npt / 50000.0, monsoonProfAcc_sweep * npt / 50000.0,
                 (monsoonProfAcc_lookup + monsoonProfAcc_sweep) * npt / 50000.0,
                 monsoonProfAcc_total * npt / 50000.0,
                 cps, scps, cps - scps, perStep, nV);
        else
            INFO("[MONSOON PROFILE] Macro: effProbability calls/sample=%.3f (engine step-cache=%.3f, "
                 "UI bars=%.3f) (step-rate 7x(1+N)=%.1f per step, N=%d); TSC calibrating",
                 cps, scps, cps - scps, perStep, nV);
        monsoonProfAcc_lookup = monsoonProfAcc_sweep = monsoonProfAcc_total = 0;
    }
#endif
}

Model* modelStraitsSandsMacroVisual =
    createModel<StraitsSandsMacroVisual, StraitsSandsMacroVisualWidget>(
        "StraitsSandsMacroVisual");
