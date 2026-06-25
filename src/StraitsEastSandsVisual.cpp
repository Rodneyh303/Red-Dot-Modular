#include <rack.hpp>
#include "Monsoon.hpp"
#include "ui/RedScrew.hpp"
#include "StraitsEastSandsVisual.hpp"
#include "StraitsSandsMacroVisual.hpp"  // complete type + StraitsMacroVisualIds for the spread-arc Macro-CV gate
#include "MonsoonSandsVisualExpander.hpp"  // complete mono type + SandsMonoVisualIds for the tab-1 mono mirror
#include "ui/SandsVisualEditorV4.hpp"
#include "ui/TabButton.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ModArcOverlay.hpp"
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
struct DimmableTrimpot : rack::componentlibrary::Trimpot {
    std::function<bool()> dimWhen;    // draw at reduced alpha
    std::function<bool()> lockWhen;   // swallow input (inoperable) — for inherited/locked state
    bool locked() const { return lockWhen && lockWhen(); }
    void onButton(const event::Button& e) override {
        if (locked()) { e.consume(this); return; }
        rack::componentlibrary::Trimpot::onButton(e);
    }
    void onDragStart(const event::DragStart& e) override {
        if (locked()) return;
        rack::componentlibrary::Trimpot::onDragStart(e);
    }
    void draw(const DrawArgs& args) override {
        bool dim = (dimWhen && dimWhen()) || locked();
        if (dim) nvgGlobalAlpha(args.vg, 0.4f);
        rack::componentlibrary::Trimpot::draw(args);
        if (dim) nvgGlobalAlpha(args.vg, 1.0f);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        bool dim = (dimWhen && dimWhen()) || locked();
        if (dim) nvgGlobalAlpha(args.vg, 0.4f);
        rack::componentlibrary::Trimpot::drawLayer(args, layer);
        if (dim) nvgGlobalAlpha(args.vg, 1.0f);
    }
};

struct StraitsEastSandsVisualWidget;  // fwd

// Owner claim latch that dims + swallows input when inert (no Macro attached — there's
// nothing to claim ownership FROM, it's all East). Predicate set by the widget.
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
                return (lane==0) ? restInterpId(v) : (lane==1) ? melodyInterpId(v) : octaveInterpId(v);
            };
            arc->getSetNorm = [mod, interpParamId]() -> float {
                if (!mod) return 0.5f;
                // Interp/spread params are bipolar -1..1; map to 0..1 (centre=0.5).
                // getScaledValue() does this correctly over the param's range.
                auto* pq = mod->paramQuantities[interpParamId()];
                return pq ? (float)pq->getScaledValue() : 0.5f;
            };
            arc->getModNorm = [mod, this, lane]() -> float {
                if (!mod) return 0.5f;
                int v = polyVoice();   // poly bank index (-1 on mono tab → guarded by v<0 below)
                if (v < 0 || v >= 15) return 0.5f;
                // polySpreadEffective is bipolar -1..1 → map to 0..1.
                return rack::math::clamp((mod->polySpreadEffective[v][lane] + 1.f) * 0.5f, 0.f, 1.f);
            };
            arc->isActive = [mod, this, lane, interpParamId]() -> bool {
                if (!mod) return false;
                Monsoon* mon = findMonsoonEitherSide(mod);
                if (!mon || !mon->modVizEast) return false;
                int v = polyVoice();   // poly bank index (-1 on mono tab → guarded by v<0 below)
                if (v < 0 || v >= 15) return false;
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
                    // The Macro blend only DYNAMICALLY modulates spread when Macro's own
                    // spread CV jack is connected (else macroCVDelta[lane][3] is 0 — a
                    // static, zero contribution). Gating on send≠0 alone left the arc
                    // "active" with a static blend, so a manual spread turn raced the
                    // live param vs the control-rate effective → red flash. Require:
                    // East owns the lane, send is non-zero, AND Macro spread CV is live.
                    bool eastOwns = mod->params[ownerId(v, lane)].getValue() > 0.5f;
                    float send = macroVis->params[StraitsMacroVisualIds::sendId(v, lane, 3)].getValue();
                    bool macroSprCv = macroVis->inputs[StraitsMacroVisualIds::macroCvId(lane, 3)].isConnected();
                    macroBlend = eastOwns && std::fabs(send) > 1e-4f && macroSprCv;
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
                            "res/panels/StraitsEastSandsVisual_40HP.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_40HP_light.svg"));
        loadPanel(asset::plugin(pluginInstance,
                            "res/panels/StraitsEastSandsVisual_40HP.svg"));

        redDot::addRedScrews(this);

        // Voice tabs: V1 = mono master strand (index 0, mirrors Sands Mono), V2..V16 =
        // the 15 poly voices (indices 1..15 → poly bank slots 0..14). 16 total, two rows.
        tabGroup = new TabButtonGroup(16, 1, 2,
                                      mm2px(ED_W), mm2px(10.f));
        tabGroup->box.pos = mm2px(Vec(ED_X, ED_Y - 12.f));
        addChild(tabGroup);

        // Visual editor
        visualEditor = new SandsVisualEditorV4(SandsVisualEditorV4::POLY);
        visualEditor->box.pos  = mm2px(Vec(ED_X, ED_Y));
        visualEditor->box.size = mm2px(Vec(ED_W, ED_H));
        // Lanes fill the box evenly (no internal padding) so the live lanes line
        // up with the painted recess lanes and the kit-bound jacks/prob-outs,
        // which all divide ED_H by laneCount. MONO/POLY label suppressed (would
        // land on lane 0); lane labels stay (panel doesn't draw them).
        visualEditor->layout.topPadding = 0.f;
        visualEditor->layout.botPadding = 0.f;
        visualEditor->showControlBar    = false;
        // Right-click on a lane row opens the ownership context menu.
        visualEditor->onLaneRightClick = [this](int lane, rack::math::Vec pos) -> bool {
            if (!macroAttached()) return false;  // no menu when Macro absent
            if (onMonoTab()) return false;        // ownership is per poly voice, not mono
            openLaneOwnershipMenu(lane, pos);
            return true;
        };
        addChild(visualEditor);

        // 4 poly probability CV outs, one per lane row (aligned to editor lane centers).
        for (int l = 0; l < 4; ++l) {
            float y = ED_Y + (l + 0.5f) * ED_LANE_H;
            auto* p = createOutputCentered<redDot::GoldPolyPort>(
                mm2px(Vec(PROB_OUT_X, y)), module, StraitsEastVisualIds::PROB_OUT_REST + l);
            Module* mod = module;
            p->lightTheme = [mod]() { Monsoon* m = mod ? redDot::findMonsoonEitherSide(mod) : nullptr;
                                      return m && m->lightTheme; };
            addOutput(p);
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
        bindParam<DimmableTrimpot>("param_" + std::to_string((int)SPREAD_R), SPREAD_R,
            std::function<void(DimmableTrimpot*)>([this](DimmableTrimpot* k){ k->dimWhen = [this](){ return laneOwnedByMacro(0) || tab1MonoMirror(); }; k->lockWhen = [this](){ return laneOwnedByMacro(0) || tab1MonoMirror(); }; pendingSpreadArcs.push_back({k, 0}); }));
        bindParam<DimmableTrimpot>("param_" + std::to_string((int)SPREAD_M), SPREAD_M,
            std::function<void(DimmableTrimpot*)>([this](DimmableTrimpot* k){ k->dimWhen = [this](){ return laneOwnedByMacro(1) || tab1MonoMirror(); }; k->lockWhen = [this](){ return laneOwnedByMacro(1) || tab1MonoMirror(); }; pendingSpreadArcs.push_back({k, 1}); }));
        bindParam<DimmableTrimpot>("param_" + std::to_string((int)SPREAD_O), SPREAD_O,
            std::function<void(DimmableTrimpot*)>([this](DimmableTrimpot* k){ k->dimWhen = [this](){ return laneOwnedByMacro(2) || tab1MonoMirror(); }; k->lockWhen = [this](){ return laneOwnedByMacro(2) || tab1MonoMirror(); }; pendingSpreadArcs.push_back({k, 2}); }));
        bindParam<DimmableTrimpot>("param_" + std::to_string((int)SPREAD_A), SPREAD_A,
            std::function<void(DimmableTrimpot*)>([this](DimmableTrimpot* k){ k->dimWhen = [this](){ return laneOwnedByMacro(3) || tab1MonoMirror(); }; k->lockWhen = [this](){ return laneOwnedByMacro(3) || tab1MonoMirror(); }; pendingSpreadArcs.push_back({k, 3}); }));

        // Macro/East blend controls (bound to the display proxies; copied to/from
        // the per-voice MACRO params on voice switch + each frame). Owner = a
        // latching on/off button (off=Macro owns base, on=East owns). With NO Macro
        // attached, ownership is meaningless (it's all East) — the owner button is
        // inert + dimmed and the sends are dimmed. With Macro attached, sends dim per
        // lane when Macro owns it. (Base-spread / CV-depth are East's own controls and
        // stay live solo — see laneOwnedByMacro above.)
        for (int lane = 0; lane < 4; ++lane) {
            bindLightParam<DimmableLatch>(
                "param_owner_" + std::to_string(lane),
                ownerDispId(lane),
                ownerLightId(lane),
                [this](DimmableLatch* w) {
                    w->momentary = false;
                    w->latch = true;
                    w->inertWhen = [this](){ return !macroAttached(); };
                    w->hideWhen  = [this](){ return tab1MonoMirror(); };   // V1: nothing to opt into
                }
            );
            // (Macro mix-in send trims relocated to Macro's panel under the control
            //  inversion — East's panel no longer binds param_send_* markers.)
        }

        paramMgr = new PolyVoiceSandsParameterManager(nullptr, nullptr, 15, 0);
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
        // CV-depth attenuverters: display proxy → this voice's per-voice store.
        for (int lane=0; lane<4; ++lane)
            for (int c=0; c<4; ++c)
                module->params[attenId(slot,lane,c)].setValue(module->params[attenDispId(lane,c)].getValue());
    }
    void loadVoiceMacro(int v) {   // v = 0-based poly bank
        if (!module) return;
        const int slot = dotModular::VoiceResolver::voiceSlot(v + dotModular::VoiceResolver::kFirstPoly);
        for (int lane=0; lane<4; ++lane)
            module->params[ownerDispId(lane)].setValue(module->params[ownerId(v,lane)].getValue());
        // CV-depth attenuverters: this voice's per-voice store → display proxy.
        for (int lane=0; lane<4; ++lane)
            for (int c=0; c<4; ++c)
                module->params[attenDispId(lane,c)].setValue(module->params[attenId(slot,lane,c)].getValue());
    }
    void saveVoiceLOR(int v) {
        if (!module || !visualEditor) return;
        for (int l=0; l<4; ++l) {   // l = engine lane; read from mapped editor lane
            const auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR[l]];
            module->params[lorId(v,l,0)].setValue((float)lane.length);
            module->params[lorId(v,l,1)].setValue((float)lane.offset);
            module->params[lorId(v,l,2)].setValue((float)lane.rotation);
        }
    }
    void loadVoiceLOR(int v) {
        if (!module || !visualEditor) return;
        for (int l=0; l<4; ++l) {   // l = engine lane; write to mapped editor lane
            auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR[l]];
            lane.length   = std::max(1,(int)std::round(module->params[lorId(v,l,0)].getValue()));
            lane.offset   = (int)std::round(module->params[lorId(v,l,1)].getValue());
            lane.rotation = (int)std::round(module->params[lorId(v,l,2)].getValue());
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
        } else if (v1Editable()) {
            // V1 editable (no Mono): save to mono slot (kMonoSlot=0).
            saveVoiceLOR(0);
            saveVoiceSpread(0);
        }
        selectedVoice = nv;
        // Load the INCOMING voice.
        if (selectedVoice >= 1) {
            paramMgr->syncPatternEngineToEditor(polyVoice(), visualEditor->currentState);
            loadVoiceLOR(polyVoice());
            loadVoiceSpread(polyVoice());
            loadVoiceMacro(polyVoice());
        } else if (v1Editable()) {
            // V1 editable: load mono slot.
            loadVoiceLOR(0);
            loadVoiceSpread(0);
        }
    }

    Monsoon* getMonsoon() const {
        return module ? findMonsoonEitherSide(module) : nullptr;
    }
    // Macro visual attached on the chain?
    bool macroAttached() {
        Monsoon* m = getMonsoon();
        return m && m->expanderManager.cachedMacroSandsVisual != nullptr;
    }
    // For East's OWN controls (base-spread, CV-depth): inert only when Macro is present
    // AND owns the lane (East base bypassed). Fully usable solo.
    bool laneOwnedByMacro(int lane) {
        if (!macroAttached()) return false;
        return !(module && module->params[StraitsEastVisualIds::ownerDispId(lane)].getValue() > 0.5f);
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
        return onMonoTab() && !(m && m->expanderManager.cachedSandsVisualExpander != nullptr);
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

            auto& smgr = paramMgr->spreadMgr;
            smgr.setSpread(polyVoice(), 0, mod->params[SPREAD_R].getValue());
            smgr.setSpread(polyVoice(), 1, mod->params[SPREAD_M].getValue());
            smgr.setSpread(polyVoice(), 2, mod->params[SPREAD_O].getValue());
            smgr.setSpread(polyVoice(), 3, mod->params[SPREAD_A].getValue());
        }
        {
            auto& smgr = paramMgr->spreadMgr;
            smgr.setInterpolationTarget(
                monsoon->spreadInterpMono ? SpreadManager::MONO_DRAW : SpreadManager::AVERAGE_POLY);
        }

        // CV applied at control rate in Monsoon::process() — base + scaled offset, no mutation here.

        if (selectedVoice >= 1) {
            saveVoiceLOR(polyVoice());
            paramMgr->syncPatternEngineToEditor(polyVoice(), visualEditor->currentState);
            // Bug fix: for a lane CEDED to Macro, the editor's own probabilities aren't the
            // source of truth (Macro drives it) and the sync above reads slewedPoly* which
            // isn't populated under Macro ownership → blank lanes. For ceded lanes only,
            // overwrite the display from the resolver (polyRhythmRandom — the final output the
            // sequencer plays, populated regardless of owner; the prob-outs use the same).
            // East-OWNED lanes keep the editor's edit values so dragging a bar isn't clobbered.
            dotModular::VoiceResolver resolver(monsoon->engine);
            const int vnum = currentVoice();
            for (int lane = 0; lane < 4; ++lane) {
                if (!laneOwnedByMacro(lane)) continue;   // owned by East → keep editor's values
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
        visualEditor->setPlayDir(eng.lastPlayDir);   // direction cue (Mode E reverse)
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
            // l = mono param bank (0=REST 1=MEL 2=OCT) → editor lane
            for (int l=0; l<3; ++l) {
                int mLen = (int)std::round(monoVis->params[SandsMonoVisualIds::lenId(l)].getValue());
                int mOff = (int)std::round(monoVis->params[SandsMonoVisualIds::offId(l)].getValue());
                int mRot = (int)std::round(monoVis->params[SandsMonoVisualIds::rotId(l)].getValue());
                int el = dotModular::MONO_PARAM_TO_EDITOR[l];
                visualEditor->currentState.lanes[el].setDisplayLOR(mLen, mOff, mRot);
                visualEditor->setLanePlayStep(el, calcPlayhead(gs, mLen, mOff, mRot));
            }
        } else if (v1Editable()) {
            // V1 editable (no Mono): sync editor edits → engine mono strand params,
            // then show the engine's current mono effective LOR as display.
            // Editor writes drive the params; processDNA reads them next tick.
            if (paramMgr) {
                // Push editor state into the mono LOR params via the mono slot.
                for (int l=0; l<3; ++l) {
                    int el = dotModular::MONO_PARAM_TO_EDITOR[l];
                    auto& lane = visualEditor->currentState.lanes[el];
                    module->params[SandsMonoVisualIds::lenId(l)].setValue((float)lane.length);
                    module->params[SandsMonoVisualIds::offId(l)].setValue((float)lane.offset);
                    module->params[SandsMonoVisualIds::rotId(l)].setValue((float)lane.rotation);
                }
            }
            // Display: show engine's current effective mono strand LOR.
            for (int l=0; l<3; ++l) {
                int el = dotModular::MONO_PARAM_TO_EDITOR[l];
                int cvLen = (int)std::round(module->params[SandsMonoVisualIds::lenId(l)].getValue());
                int cvOff = (int)std::round(module->params[SandsMonoVisualIds::offId(l)].getValue());
                int cvRot = (int)std::round(module->params[SandsMonoVisualIds::rotId(l)].getValue());
                visualEditor->currentState.lanes[el].setDisplayLOR(cvLen, cvOff, cvRot);
                visualEditor->setLanePlayStep(el, calcPlayhead(gs, cvLen, cvOff, cvRot));
            }
        } else if (selectedVoice >= 1) {
            const int pv = polyVoice();
            // l = engine lane (0=REST 1=MEL 2=OCT 3=ACC) → editor lane
            for (int l=0; l<4; ++l) {
                int cvLen = eng.polyLen[pv][l];
                int cvOff = eng.polyOff[pv][l];
                int cvRot = eng.polyRot[pv][l];
                int el = dotModular::ENGINE_LANE_TO_EDITOR[l];
                visualEditor->currentState.lanes[el].setDisplayLOR(cvLen, cvOff, cvRot);
                visualEditor->setLanePlayStep(el, calcPlayhead(gs, cvLen, cvOff, cvRot));
            }
        }
    }

    // Labels for the Macro-blend control groups, drawn in NanoVG (the panel SVG
    // carries no baked text — same convention as the tab labels). Mirrors the
    // layout in panel_src/gen_east_clean.py: 3 groups (REST/MELODY/OCTAVE) below
    // the editor, each an owner button + a 2×2 Len/Off/Rot/Spr send grid. Greyed
    // when no Macro visual is attached (the controls are inert then).
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        NVGcontext* vg = args.vg;

        // Layout constants — MUST MATCH gen_east_clean.py (blend groups).
        const float ED_X = 88.0f, PROB_OUT_X = 207.0f, ED_W = PROB_OUT_X - ED_X - 8.0f;
        const float BLEND_TOP = 74.0f, GAP = 2.5f;
        const float GROUP_W = ED_W / 4.0f;
        const float OWN_DY = 13.0f;
        const char* laneName[4] = { "REST", "MELODY", "OCTAVE", "ACCENT" };

        bool macroPresent = false;
        bool isLight = false;
        if (auto* mon = getMonsoon()) {
            macroPresent = (mon->expanderManager.cachedMacroSandsVisual != nullptr);
            isLight = mon->lightTheme;
        }

        auto font = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!font) font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(vg, font->handle);

        NVGcolor head = macroPresent ? (isLight ? nvgRGB(40,44,52) : nvgRGB(210,214,222))
                                     : nvgRGBA(140,140,150, 90);
        NVGcolor item = macroPresent ? (isLight ? nvgRGB(150,120,20) : nvgRGB(190,160,60))
                                     : nvgRGBA(140,140,150, 70);

        // V1 / mono tab: the per-lane BASE opt-in band is meaningless (mono owns the base,
        // nothing to opt into). The latch widgets hide themselves (hideWhen), but the panel
        // bakes the group boxes, header rule and latch rings — mask the whole band with the
        // panel background and skip the BASE labels so V1 reads clean.
        if (tab1MonoMirror()) {
            NVGcolor bg = isLight ? nvgRGB(0xe8,0xe8,0xea) : nvgRGB(0x16,0x18,0x1c);
            nvgBeginPath(vg);
            nvgRect(vg, mm2px(ED_X) - 2.f, mm2px(BLEND_TOP - 6.f),
                        mm2px(ED_W) + 4.f, mm2px(22.f + 8.f));
            nvgFillColor(vg, bg);
            nvgFill(vg);
            return;   // no BASE labels on V1
        }

        // Section header (left-aligned, above the group row).
        nvgFontSize(vg, 8.0f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        nvgFillColor(vg, head);
        nvgText(vg, mm2px(ED_X), mm2px(BLEND_TOP - 3.5f), "BASE", nullptr);

        for (int l = 0; l < 4; ++l) {
            float gx = ED_X + l*GROUP_W + GAP*0.5f;
            float gw = GROUP_W - GAP;
            float gcx = gx + gw*0.5f;
            // lane-name header — centred at top of the box
            nvgFontSize(vg, 7.0f);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, head);
            nvgText(vg, mm2px(gcx), mm2px(BLEND_TOP + 4.0f), laneName[l], nullptr);
            // "BASE" under the opt-in latch (inherit Macro base / local East)
            nvgFontSize(vg, 5.0f);
            nvgFillColor(vg, item);
            nvgText(vg, mm2px(gcx), mm2px(BLEND_TOP + OWN_DY + 3.7f), "BASE", nullptr);
        }
    }
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
