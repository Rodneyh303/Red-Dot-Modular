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
#include "MonsoonSandsVisualExpander.hpp"  // complete mono type + SandsMonoVisualIds for the tab-1 mono mirror
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/ModArcOverlay.hpp"
#include "ui/OwnerCell.hpp"       // DirCell
#include "dsp/engines/SequencerEngine.hpp"  // LaneDir
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
            const int engLane = dotModular::EDITOR_TO_ENGINE_LANE_QMIX[lane];
            const int bef0=before[0],bef1=before[1],bef2=before[2];
            const int aft0=after[0], aft1=after[1], aft2=after[2];
            auto* ed = visualEditor;
            redDot::applyAndPushStoreEdit<Monsoon>(m, "LOR edit",
                [engLane, lane, bef0,bef1,bef2, aft0,aft1,aft2, ed](Monsoon& mm, float dir) {
                    const bool redo = dir > 0.5f;
                    const int L = redo?aft0:bef0, O = redo?aft1:bef1, R = redo?aft2:bef2;
                    mm.setGlobalLor(engLane, 0, (float)L);
                    mm.setGlobalLor(engLane, 1, (float)O);
                    mm.setGlobalLor(engLane, 2, (float)R);
                    if (ed && lane >= 0 && lane < 6) {
                        ed->currentState.lanes[lane].length   = std::max(1, L);
                        ed->currentState.lanes[lane].offset   = O;
                        ed->currentState.lanes[lane].rotation = R;
                    }
                },
                0.f, 1.f);
        };

        Module* mod_ = module;
        auto themeOut = [mod_](redDot::GoldPolyPort* p) {
            p->lightTheme = [mod_]() { Monsoon* m = mod_ ? redDot::findMonsoonEitherSide(mod_) : nullptr;
                                       return m && m->lightTheme; };
        };

        // ── CRITICAL lane-order note ──────────────────────────────────────────
        // The panel ROWS are EDITOR order (top→bottom: MEL,OCT,QMIX,REST,ACC) and the
        // anchors are editor-lane indexed (param_*_<el>). But EVERY store accessor here
        // (getGlobalSpread/Atten/Tap, getMacroSend, cvId, PROB_OUT_REST) and the label
        // arrays are ENGINE order (REST,MEL,OCT,ACC,QMIX). So for each editor row `el`
        // we MUST convert el→engine lane via EDITOR_TO_ENGINE_LANE_QMIX and use THAT for
        // the store + label. Passing `el` straight through was the "top row is MELODY but
        // the knob is labelled/modulates REST" bug. Anchor name stays <el>.
        // Strong-typed conversion: editor row → engine lane (the single editor↔engine crossing).
        // EditorLane/EngineLane make a raw-int mixup a compile error; .v feeds the int-keyed store.
        auto EL2ENG = [](int el){ return dotModular::toEngine(dotModular::EditorLane(el)).v; };
        // Editor-ordered lane names for tooltips (top→bottom).
        static const char* EDN[dotModular::SandsGrid::POLY_LANES] = {"MEL","OCT","QMIX","REST","ACC"};

        // 5 poly probability CV outs — jack on editor row el drives engine lane's prob out.
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el)
            bindOutput<redDot::GoldPolyPort>(
                "output_prob_" + std::to_string(el),
                StraitsMacroVisualIds::PROB_OUT_REST + EL2ENG(el),
                std::function<void(redDot::GoldPolyPort*)>(themeOut));

        // ── Left section: 5 poly lanes × (4 CV jacks + 4 attens + 1 spread) ──
        // Anchor row = editor lane el; store/id = engine lane eng.
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            const int eng = EL2ENG(el);
            for (int c = 0; c < 4; ++c)
                bindInput<PJ301MPort>(
                    "input_cv_" + std::to_string(el) + "_" + std::to_string(c), cvId(eng,c));
            // STORE-BACKED (MVC step 1d): global attenuverters (engine-indexed store).
            for (int c = 0; c < 4; ++c) {
                static const char* CN[4] = {"Length","Offset","Rotation","Spread"};
                const std::string albl = std::string(EDN[el]) + " " + CN[c] + " CV depth";
                auto* k = redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    "param_atten_" + std::to_string(el) + "_" + std::to_string(c), storeResolver(),
                    -1.f, 1.f, 0.f, albl,
                    [eng, c](Monsoon& m)          { return m.getGlobalAtten(eng, c); },
                    [eng, c](Monsoon& m, float v) { m.setGlobalAtten(eng, c, v); });
                if (k) leftAttenuverters.push_back(k);
            }
        }

        // Per-lane global SPREAD trimpots — anchor param_spr_<el>, store engine lane.
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            const int eng = EL2ENG(el);
            auto* sp = redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                "param_spr_" + std::to_string(el), storeResolver(),
                -1.f, 1.f, 0.f, std::string(EDN[el]) + " spread",
                [eng](Monsoon& m)          { return m.getGlobalSpread(eng); },
                [eng](Monsoon& m, float v) { m.setGlobalSpread(eng, v); });
            if (sp) pendingSpreadArcs.push_back({sp, eng});   // arc reads engine-lane spread
        }

        // Macro→voice MIX-IN send 2×2 grids — anchor param_send_<el>_<item>, store engine lane.
        // STORE-BACKED: reads/writes editor.macroSend for the viewed voice (slot resolved LIVE).
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el)
            for (int item = 0; item < 4; ++item) {
                const int eng = EL2ENG(el);
                redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                    "param_send_" + std::to_string(el) + "_" + std::to_string(item),
                    storeResolver(), -1.f, 1.f, 0.f,
                    std::string(EDN[el]) + " mix-in " + std::to_string(item) + " (viewed voice)",
                    [this, eng, item](Monsoon& m) {
                        const int slot = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);
                        return m.getMacroSend(slot, eng, item);
                    },
                    [this, eng, item](Monsoon& m, float v) {
                        const int slot = dotModular::VoiceResolver::voiceSlot(viewVoice + 1);
                        m.setMacroSend(slot, eng, item, v);
                    });
            }
        // PRE/POST CV taps per lane — anchor param_taplor_/tapspr_<el>, store engine lane.
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            const int eng = EL2ENG(el);
            redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                "param_taplor_" + std::to_string(el), storeResolver(),
                0.f, 1.f, 1.f, std::string(EDN[el]) + " LOR send tap (PRE-POST)",
                [eng](Monsoon& m)          { return m.getGlobalTap(eng, 0); },
                [eng](Monsoon& m, float v) { m.setGlobalTap(eng, 0, v); });
            redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(this,
                "param_tapspr_" + std::to_string(el), storeResolver(),
                0.f, 1.f, 1.f, std::string(EDN[el]) + " spread send tap (PRE-POST)",
                [eng](Monsoon& m)          { return m.getGlobalTap(eng, 1); },
                [eng](Monsoon& m, float v) { m.setGlobalTap(eng, 1, v); });
        }

        // ── Direction cells (param_dir_<lane>) — per-lane direction toggle (Fwd/Rev/Pend/PingPong).
        // Macro is the global editor → DirCell sets the MONO direction (laneDirPending_).
        // Kit markers use EDITOR lane order (row 0..3 = MEL/OCT/REST/ACC), matching East.
        // Locked when no Monsoon or when Mono is present (Mono is the authority).
        static const NVGcolor editorDirCol[dotModular::SandsGrid::POLY_LANES] = {
            nvgRGB(0xd4,0xaf,0x37), nvgRGB(0xb8,0x86,0x0b),  // MEL gold, OCT dark gold
            nvgRGB(0x80,0x60,0xc0),  // QMIX purple
            nvgRGB(0x50,0x50,0x50), nvgRGB(0xff,0x95,0x00)   // REST grey, ACC orange
        };
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            // STORE-BACKED (MVC step 1: direction de-param). The DirCell reads/writes
            // editor.globalDir via get/setGlobalDir. globalDir is ENGINE-indexed
            // (REST/MEL/OCT/ACC/QMIX) — the manager reads getGlobalDir(engineLane) — but the
            // panel ROW is EDITOR order. So the cell on editor row `el` must target engine lane
            // eng = EDITOR_TO_ENGINE_LANE_QMIX[el]. (Was passing `el` straight in → the direction
            // cell on the MELODY row drove REST's direction.) Anchor stays param_dir_<el>.
            const int eng = EL2ENG(el);
            bindWidget<DirCell>(
                "param_dir_" + std::to_string(el),
                std::function<void(DirCell*)>([this, el, eng](DirCell* w) {
                    w->laneCol = editorDirCol[el];
                    const float stepW = (ED_W - 2.f*6.f) / 16.f;
                    w->box.size = mm2px(Vec(stepW, ED_LANE_H * 0.9f));
                    w->getStateFn = [this, eng]() -> int {
                        auto* mm = getMonsoon();
                        return mm ? (int)std::lround(mm->getGlobalDir(eng)) & 3 : 0;
                    };
                    w->setStateFn = [this, eng](int v) {
                        if (auto* mm = getMonsoon()) mm->setGlobalDir(eng, (float)(v & 3));
                    };
                    w->pushUndoFn = [this, eng](int oldV, int newV) {
                        auto* mm = getMonsoon(); if (!mm) return;
                        redDot::applyAndPushStoreEdit<Monsoon>(mm, "direction",
                            [eng](Monsoon& m, float val) { m.setGlobalDir(eng, val); },
                            (float)(oldV & 3), (float)(newV & 3));
                    };
                    w->lockWhen = [this]() { return !getMonsoon(); };
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
            for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l)
                in.monoV1Owner[l] = mon->getMonoOwner(l);
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
            const auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR_QMIX[l]];
            mm->setGlobalLor(l, 0, (float)lane.length);
            mm->setGlobalLor(l, 1, (float)lane.offset);
            mm->setGlobalLor(l, 2, (float)lane.rotation);
        }
    }
    void loadLOR() {
        if (!module || !visualEditor) return;
        auto* mm = getMonsoon(); if (!mm) return;
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {   // l = engine lane
            auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR_QMIX[l]];
            lane.length   = std::max(1,(int)std::round(mm->getGlobalLor(l, 0)));
            lane.offset   = (int)std::round(mm->getGlobalLor(l, 1));
            lane.rotation = (int)std::round(mm->getGlobalLor(l, 2));
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
        // Macro's own spread for this lane (base knob + send-tapped delta, clamped) — the
        // SAME expression the engine's MACRO_SOLE branch uses (MonsoonExpanderManager.cpp).
        const float sp = rack::math::clamp(mod->macroBase[engLane][3] + mod->macroSendDelta[engLane][3], -1.f, 1.f);
        // The pre-spread base draw buffer for this lane (mono strand vs poly bank).
        // The Change Alley pin remap now lives UPSTREAM in the slewed buffers
        // (pe.remapSlewedByPins, pre-spread), so reading this voice's OWN slewed buffer
        // already yields the pinned source's draw — Macro's own spread then applies on top.
        // Plain own-voice read; no srcRow indirection here.
        using PL = SequencerEngine;  // PL::PL_REST etc. (enum lives in SequencerEngine)
        // Read PUBLISHED snapshots (pubSlewed*) — NOT the live slewed* arrays the audio thread
        // is rewriting during recomputeEffective* (now ~116µs at r>0). Reading live arrays
        // mid-rewrite → torn read → flicker. (East already reads published values; this brings
        // Macro in line.)
        float base;
        if (mono) {
            base = (engLane == PL::PL_REST)   ? pe.pubSlewedRhythm[step & 0x0F]
                 : (engLane == PL::PL_MELODY) ? pe.pubSlewedMelody[step & 0x0F]
                 : (engLane == PL::PL_OCTAVE) ? pe.pubSlewedOctave[step & 0x0F]
                 : (engLane == PL::PL_ACCENT) ? pe.pubSlewedAccent[step & 0x0F]
                 :                              pe.pubSlewedQmix[step & 0x0F];
        } else {
            int v = rack::math::clamp(polyVoice, 0, 14);
            base = (engLane == PL::PL_REST)   ? pe.pubSlewedPolyRhythm[v][step & 0x0F]
                 : (engLane == PL::PL_MELODY) ? pe.pubSlewedPolyMelody[v][step & 0x0F]
                 : (engLane == PL::PL_OCTAVE) ? pe.pubSlewedPolyOctave[v][step & 0x0F]
                 : (engLane == PL::PL_ACCENT) ? pe.pubSlewedPolyAccent[v][step & 0x0F]
                 :                              pe.pubSlewedPolyQmix[v][step & 0x0F];
        }
        return redDot::SpreadInterp::apply(pe, engLane, step, base, sp);
    }

    void step() override {
        ModuleWidget::step();
        kitStep();   // kit: dev-mode live-reload poll (no-op unless enabled)
        if (!module || !paramMgr || !visualEditor) return;
        Monsoon* monsoon = getMonsoon();
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

        // Mix-in sends: STORE-BACKED via the StoreKnobs above (they read/write
        // getMacroSend/setMacroSend for the live view voice directly). The old per-voice
        // load/store sync dance and its clobber guard are gone -- no display proxy to sync.

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
            for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
                int el = dotModular::ENGINE_LANE_TO_EDITOR_QMIX[lane];
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
            for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
                const int engLane = dotModular::EDITOR_TO_ENGINE_LANE_QMIX[el];
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
        auto& eng = monsoon->engine;
        // Per-lane direction cue: use Macro's OWN macroLaneSign_ (always follows Macro's DirCell)
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
            int el = dotModular::ENGINE_LANE_TO_EDITOR_QMIX[l];
            int strand = dotModular::MONO_LANE_TO_STRAND[el];
            visualEditor->setLanePlayDir(el, eng.lastPlayDir * eng.macroLaneSign_[strand]);
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
            // engine lane (0=REST 1=MEL 2=OCT 3=ACC 4=QMIX) → editor lane.
            for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
                int ownLen = (int)std::lround(mod->macroBase[l][0] + mod->macroCVDelta[l][0]);
                int ownOff = (int)std::lround(mod->macroBase[l][1] + mod->macroCVDelta[l][1]);
                int ownRot = (int)std::lround(mod->macroBase[l][2] + mod->macroCVDelta[l][2]);
                ownLen = std::max(1, ownLen);
                int el = dotModular::ENGINE_LANE_TO_EDITOR_QMIX[l];
                visualEditor->currentState.lanes[el].setDisplayLOR(ownLen, ownOff, ownRot);
                // Use Macro's OWN macroLaneTick_ — advanced by the engine in advancePlayhead
                // using Macro's direction (macroLaneDir_), with full bounce support for
                // Pendulum/PingPong. Same pattern as East's laneTickV_.
                int strand = dotModular::MONO_LANE_TO_STRAND[el];
                // MVC: hide the playhead when stopped (match Mono's started ? tick : -1), so
                // rotation CV only moves the chevron, not the playhead, when the seq is stopped.
                int ph = (eng.stepIndex >= 0) ? eng.macroLaneTick_[strand] : -1;
                visualEditor->setLanePlayStep(el, calcPlayhead(ph, ownLen, ownOff, ownRot));
            }
        }
    }

    // Mix-in send group labels (NanoVG; panel carries no baked text). Geometry MUST
    // match the send grids in gen_macro_mono.py (gen_macro) EXACTLY — keep in lockstep:
    //   BLEND_TOP=85 BLEND_H=35 SEND_Y0=10 SEND_DY=9 SEND_DX=6 GROUP_W=ED_W/5 (5 lanes incl QMIX).
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        NVGcontext* vg = args.vg;

        // (P1/G1 no-hide: the old V1 atten-masking rectangle was removed — Macro's
        // left attenuverters are always visible, including on the V1 tab. They were
        // being painted over with the panel background here, which is why the V1
        // trimpots "disappeared" even though the widgets were visible.)

        // GEOMETRY IS OWNED BY THE GENERATOR. Every MIX-IN label position is derived from the
        // panel-kit anchors gen_macro_mono.py emits — ALL now EDITOR-lane indexed (Stage 3 unified
        // Macro on descriptive editor-order anchors, matching the widget binds): group header from
        // label_mixin_<el>, the four send items from param_send_<el>_<item>, the two taps from
        // param_taplor_<el>/param_tapspr_<el>. NOTHING is recomputed from GROUP_W/BLEND_* here, so
        // re-running the generator can never drift the labels off the boxes (the ED_W/4-vs-ED_W/5
        // bug that recurred 3×). The store accessors (getMacroSend/getGlobalTap) remain engine-
        // indexed and are keyed inside the bind closures — only the ANCHOR NAMES are editor order.
        const char* laneName[dotModular::SandsGrid::POLY_LANES] = { "MELODY", "OCTAVE", "QMIX", "REST", "ACCENT" };  // editor order
        const char* itemName[4] = { "LEN", "OFF", "ROT", "SPR" };
        static_assert(sizeof(laneName)/sizeof(laneName[0]) == dotModular::SandsGrid::POLY_LANES,
                      "MIX-IN lane-name table must be one per poly lane");

        bool isLight = false;
        if (auto* mon = getMonsoon()) isLight = mon->lightTheme;

        auto font = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!font) font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(vg, font->handle);

        NVGcolor head = isLight ? nvgRGB(40,44,52) : nvgRGB(210,214,222);
        NVGcolor item = isLight ? nvgRGB(150,120,20) : nvgRGB(190,160,60);

        // "MIX IN" header: anchor its baseline just above the first group's header anchor, so it
        // tracks the box row without a BLEND_TOP literal.
        auto anchorMM = [&](const std::string& name, bool& ok) -> Vec {
            if (NSVGshape* s = findNamed(name)) { ok = true; return centerOf(s); }   // px
            ok = false; return Vec(0, 0);
        };
        {
            bool ok0 = false; Vec g0 = anchorMM("label_mixin_0", ok0);
            nvgFontSize(vg, 8.0f);
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
            nvgFillColor(vg, head);
            const float baselineY = ok0 ? (g0.y - mm2px(5.5f)) : mm2px(83.5f);
            nvgText(vg, mm2px(ED_X), baselineY, "MIX IN", nullptr);
        }

        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
            // All MIX-IN anchors are EDITOR-lane indexed (Stage 3), so index every one by l.
            // Group header — from the editor-ordered label_mixin_<l> anchor.
            bool okH = false; Vec gH = anchorMM("label_mixin_" + std::to_string(l), okH);
            if (okH) {
                nvgFontSize(vg, 7.0f);
                nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
                nvgFillColor(vg, head);
                nvgText(vg, gH.x, gH.y, laneName[l], nullptr);
            }
            // Send-item labels — one under each param_send_<l>_<item> anchor.
            nvgFontSize(vg, 5.0f);
            nvgFillColor(vg, item);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            for (int it = 0; it < 4; ++it) {
                bool okS = false;
                Vec s = anchorMM("param_send_" + std::to_string(l) + "_" + std::to_string(it), okS);
                if (okS) nvgText(vg, s.x, s.y + mm2px(4.4f), itemName[it], nullptr);
            }
            // PRE/POST CV taps — LOR (param_taplor_<l>) + SPR (param_tapspr_<l>).
            {
                bool okL = false, okSp = false;
                Vec lTap = anchorMM("param_taplor_" + std::to_string(l), okL);
                Vec sTap = anchorMM("param_tapspr_" + std::to_string(l), okSp);
                if (okL)  nvgText(vg, lTap.x, lTap.y + mm2px(4.4f), "LOR", nullptr);
                if (okSp) nvgText(vg, sTap.x, sTap.y + mm2px(4.4f), "SPR", nullptr);
            }
        }
    }
};

// ── Module process(): 3 poly probability CV outs (ch1 reserved, voices ch2+) ──
void StraitsSandsMacroVisual::process(const ProcessArgs&) {
    using namespace StraitsMacroVisualIds;
    // PERF (Rodney audit item 3): findMonsoonEitherSide walks the expander chain and ran
    // every sample. Topology is control-rate, so cache it and refresh on a divider.
    if (monLookupDiv.process()) cachedMon_ = redDot::findMonsoonEitherSide(this);
    Monsoon* mon = cachedMon_;
    if (!mon) {
        for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
            outputs[PROB_OUT_REST + l].setChannels(1);
            outputs[PROB_OUT_REST + l].setVoltage(0.f);
        }
        return;
    }
    // ── Gate edge detection for dir_mod inputs ────────────────────────────
    // Mono jacks (1 channel). Rising edge cycles Fwd→Rev→Pend→PingPong→Fwd.
    // Cycles the dirDispId display proxy param; the widget's step() syncs to engine.
    {
        for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
            // dirModId(el) is the jack on editor row el; globalDir is ENGINE-indexed, so
            // cycle engine lane eng = EDITOR_TO_ENGINE_LANE_QMIX[el]. (Was cycling el directly →
            // the MELODY-row gate advanced REST's direction.)
            const int eng = dotModular::EDITOR_TO_ENGINE_LANE_QMIX[el];
            auto& in = inputs[dirModId(el)];
            if (!in.isConnected()) continue;
            bool high = in.getVoltage(0) > 1.f;
            if (high && !dirModPrev[el]) {
                if (auto* mm = redDot::findMonsoonEitherSide(this)) {
                    int cur = (int)std::lround(mm->getGlobalDir(eng));
                    mm->setGlobalDir(eng, (float)((cur + 1) % 4));
                }
            }
            dirModPrev[el] = high;
        }
    }
    const float scaleV = (mon->probOutScale == 0) ? 1.f : (mon->probOutScale == 1) ? 5.f : 10.f;
    const bool sh = mon->probOutSampleHold;
    auto& eng = mon->engine;
    const int nV = eng.numPolyVoices;
    const int nCh = 1 + nV;
    const int gs = eng.stepIndex;
    dotModular::VoiceResolver resolver(eng);
    for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {   // 5 poly lanes incl QMIX
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
