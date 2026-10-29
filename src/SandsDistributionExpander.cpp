// SandsDistributionExpander — widget + models (the module struct lives in the
// header; see there for the spec mapping and the role/snapping design notes).
#include "SandsDistributionExpander.hpp"

#include <cmath>

#include "ui/SvgPanelKit.hpp"        // Compose<ShapeQuery, Bind, Reload> + loadPanel/findNamed
#include "ui/StoreBound.hpp"         // bindStoreKnob (the de-param law)
#include "ui/Controls.hpp"           // Tag_Grey_Trim_Bar (the popup's knob face)
#include "ui/StraitsSeamFiller.hpp"  // the family seam (spec: StraitsSeamFiller-style continuity)
#include "ui/ConnectMark.hpp"        // the connect mark (its own next-hop)
#include "ui/WarpCurvePopup.hpp"     // paramValue/withParam/warpParamSpec/distFromWarp/WarpStagesView
#include "ui/DistributionCurveWidget.hpp"  // the permanent editor (the enlarged popup editor)
#include "ui/MonsoonDiscovery.hpp"   // isSuiteChainModel (the hop set)
#include "dsp/VoiceResolver.hpp"     // the shared voice selection (read, never owned)
#include "dsp/WarpStages.hpp"        // kTiltMax / kAtomMassMax / kAtomLevelDefault

using namespace rack;

// ── module impl ───────────────────────────────────────────────────────────────

bool SandsDistributionExpander::isFamilyModel(const rack::Model* m) {
    // The Sands subpanel family (hop-through for the role walk and the left-link
    // check; the Macro send subpanel joins this set when it lands).
    return m == modelStraitsSandsMacroVisual
        || m == modelStraitsEastSandsVisual
        || m == modelSandsDistributionExpander;
}

bool SandsDistributionExpander::resolveMacroRole() {
    // Memoized by the topology epoch (ModuleLookupCore): the walk runs once per
    // adjacency event, never per-sample (the L5 law). Macro must be CLAIMED —
    // a foreign boundary or a bare chain resolves PER-VOICE (the safe default).
    return redDot::lookupMemoizedT(this, 0, [](SandsDistributionExpander* self, int) -> bool {
        namespace R = SandsDistributionRole;   // a NAMESPACE alias — `using R = <ns>` is ill-formed
        const R::Host h = R::resolve(
            self->leftExpander.module,
            [](rack::Module* m) { return m ? m->leftExpander.module : nullptr; },
            [](rack::Module* m) -> R::Host {
                if (!m || !m->model) return R::Host::NONE;
                if (m->model == modelStraitsSandsMacroVisual) return R::Host::MACRO;
                if (m->model == modelStraitsEastSandsVisual)  return R::Host::EAST;
                return R::Host::NONE;
            },
            [](rack::Module* m) { return m && redDot::isSuiteChainModel(m->model); });
        return h == R::Host::MACRO;
    });
}

void SandsDistributionExpander::process(const ProcessArgs&) {
    // The role is a TOPOLOGY answer — event-driven via the epoch memo above.
    const bool macro    = resolveMacroRole();
    const bool wasMacro = macroRole.load(std::memory_order_relaxed);
    macroRole.store(macro, std::memory_order_relaxed);
    Monsoon* m = redDot::lookupMonsoon(this);
    if (!m) {
        // Isolated from the suite (dragged away / host gone): nothing to publish
        // into — the heartbeat freezes and Monsoon's watchdog drops whatever this
        // expander last published. Nothing latches, including isolation.
        return;
    }

    // Role flip (drag between hosts / host vanished): drop the plane this expander
    // stops owning and reset the publish bookkeeping — nothing stale crosses the
    // flip (rare event; the wide mirror rebuild inside the clear is fine).
    if (macro != wasMacro) {
        if (wasMacro) m->clearDistGlobalDeltas();
        else          m->clearDistOwnDeltas();
        lastOwnSlot_ = -1;
        for (int l = 0; l < SandsDistributionIds::LANES; ++l)
            for (int p = 0; p < SandsDistributionIds::PARAMS_PER_LANE; ++p) {
                lastGlob_[l][p] = 0.f;
                lastOwn_[l][p]  = 0.f;
            }
    }

    if (macro) publishMacroSends(m);
    else       publishOwnMods(m);

    // The heartbeat (Monsoon::process's watchdog stamp — bumped every block we
    // live, on OUR role's slot: 0 = the cross-voice publisher, 1 = per-voice).
    ++m->editor.warp.distPublishSeq[macro ? 0 : 1];
}

// ── the CROSS-VOICE publish (Macro host): the jacks' unit deltas land in
// distGlobalDelta — the Macro-sent component every voice receives (delegated
// cells fold it into Macro's default, own cells take it as the send). A changed
// lane rebuilds the FULL lane (the Macro-edit scope can't express "delegated
// AND own") — rebuildWarpMirrorLane, inline on this audio thread.
void SandsDistributionExpander::publishMacroSends(Monsoon* m) {
    namespace ID = SandsDistributionIds;   // a NAMESPACE alias — `using ID = <ns>` is ill-formed
    for (int l = 0; l < ID::LANES; ++l) {
        bool laneDirty = false;
        for (int p = 0; p < ID::PARAMS_PER_LANE; ++p) {
            const int   id = ID::warpModId(l, p);
            const float d  = inputs[id].isConnected() ? inputs[id].getVoltage() * 0.1f : 0.f;
            if (std::fabs(d - lastGlob_[l][p]) > kPubEps) {
                m->setDistGlobalDelta(l, p, d);
                lastGlob_[l][p] = d;
                laneDirty = true;
            }
        }
        if (laneDirty) m->engine.pe.rebuildWarpMirrorLane(l);
    }
}

// ── the PER-VOICE publish (East host): the jacks modulate the SHARED-SELECTED
// voice's own rows (distOwnDelta[slot]) — the spec's "read the selected voice
// from the shared state, don't have their own independent voice-select". On a
// voice switch the previously written row is zeroed (and its lanes rebuilt) so
// the mod follows the selection without leaving anything stale behind.
void SandsDistributionExpander::publishOwnMods(Monsoon* m) {
    namespace ID = SandsDistributionIds;   // a NAMESPACE alias — `using ID = <ns>` is ill-formed
    const int slot = m->engine.pe.displayedVoice().v;
    if (slot != lastOwnSlot_) {
        if (lastOwnSlot_ >= 0) {
            for (int l = 0; l < ID::LANES; ++l) {
                bool rowHad = false;
                for (int p = 0; p < ID::PARAMS_PER_LANE; ++p) {
                    if (m->editor.warp.distOwnDelta[lastOwnSlot_][l][p] != 0.f) {
                        m->setDistOwnDelta(lastOwnSlot_, l, p, 0.f);
                        rowHad = true;
                    }
                    lastOwn_[l][p] = 0.f;
                }
                if (rowHad) m->engine.pe.rebuildWarpMirrorVoiceLane(lastOwnSlot_, l);
            }
        }
        lastOwnSlot_ = slot;
    }
    for (int l = 0; l < ID::LANES; ++l) {
        bool laneDirty = false;
        for (int p = 0; p < ID::PARAMS_PER_LANE; ++p) {
            const int   id = ID::warpModId(l, p);
            const float d  = inputs[id].isConnected() ? inputs[id].getVoltage() * 0.1f : 0.f;
            if (std::fabs(d - lastOwn_[l][p]) > kPubEps) {
                m->setDistOwnDelta(slot, l, p, d);
                lastOwn_[l][p] = d;
                laneDirty = true;
            }
        }
        if (laneDirty) m->engine.pe.rebuildWarpMirrorVoiceLane(slot, l);
    }
}

// ── widget ────────────────────────────────────────────────────────────────────

namespace {

using namespace dotModular;

// The lane-focus button (which lane the permanent editor shows — spec §"LANE/VOICE
// FOCUS BUTTONS"). UI-only state, deliberately NOT a store/param: a VIEW choice,
// not musical state (the graphView precedent). Voice focus lives on the Sands
// main panel (the shared selection — this widget only READS it).
struct DexpFocusButton : rack::widget::Widget {
    int lane = 0;
    int* selected = nullptr;
    std::function<bool()> isLight;
    bool hovered = false;

    void onButton(const rack::event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
            if (selected) *selected = lane;
            e.consume(this);
        }
    }
    void onHover(const rack::event::Hover& e) override {
        hovered = true;
        e.consume(this);
    }
    void onLeave(const rack::event::Leave& e) override {
        hovered = false;
        Widget::onLeave(e);
    }
    void draw(const DrawArgs& args) override {
        const bool lit = isLight && isLight();
        NVGcontext* vg = args.vg;
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float r  = std::min(box.size.x, box.size.y) * 0.42f;
        const bool sel = selected && (*selected == lane);
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, r);
        nvgFillColor(vg, sel ? nvgRGB(0xd4, 0x00, 0x1a)
                             : (lit ? nvgRGB(0x9a, 0x9a, 0x9a) : nvgRGB(0x5a, 0x64, 0x70)));
        nvgFill(vg);
        if (sel) {
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r * 0.45f);
            nvgFillColor(vg, lit ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0xff, 0xf2, 0xe0));
            nvgFill(vg);
        } else if (hovered) {
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, r);
            nvgStrokeColor(vg, lit ? nvgRGB(0x60, 0x60, 0x60) : nvgRGB(0x9a, 0xa4, 0xb0));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
        }
        Widget::draw(args);
    }
};

struct SandsDistributionExpanderWidget : ModuleWidget,
    dotModular::Compose<SandsDistributionExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {

    static constexpr int LANES = SandsDistributionIds::LANES;
    static constexpr int NPAR  = SandsDistributionIds::PARAMS_PER_LANE;

    // panel + theme (the lane-widget idiom)
    std::shared_ptr<window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;
    bool themeLight_ = false;

    // the seam (family continuity, spec §HARD REQUIREMENT)
    redDot::StraitsSeamFiller* leftFiller  = nullptr;
    redDot::StraitsSeamFiller* rightFiller = nullptr;
    redDot::ConnectMark* connectMark = nullptr;

    // the permanent editor + its view holders (the popup's refresh shape)
    WarpCurveEditor* curve = nullptr;   // the popup's editor subclass (the onEdit hook)
    std::unique_ptr<Distribution> memberView_, view_;
    WarpF lastF_{};
    double lastTilt_ = -1e9, lastLevel_ = -1e9, lastMass_ = -1e9;
    bool lastRole_ = false;
    int  lastSlot_ = -1, lastLane_ = -1;

    // lane focus (UI-only) + knob registry for the dynamic per-kind windows
    int editorLane_ = 0;
    redDot::StoreKnob<Monsoon>* knobs_[LANES][NPAR] = {};
    int lastKind_[LANES] = {-1, -1, -1, -1, -1, -1, -1};

    SandsDistributionExpander* mod() const {
        return dynamic_cast<SandsDistributionExpander*>(module);
    }

    // The shared voice selection — READ, never owned (spec: "read the selected
    // voice from the shared state, don't have their own independent voice-select").
    // PatternEngine::displayedVoice is THE shared state (the East visual's voice
    // tabs publish it; every subpanel honours it).
    static int slotLiveFrom(Monsoon* m) { return m ? m->engine.pe.displayedVoice().v : 0; }
    int slotLive() const { return slotLiveFrom(redDot::lookupMonsoon(module)); }
    bool macroRoleNow() const {
        SandsDistributionExpander* s = mod();
        return s && s->macroRole.load(std::memory_order_relaxed);
    }

    // The role-aware WarpF the (lane) controls address, live per call — DISPLAY
    // edition: store + the send-bus deltas, the same fold the engine runs at
    // rebuild time (same table, same clamps), so the permanent editor breathes
    // with the mods exactly as the cells do (the spec's "you WATCH the
    // distribution get modulated live").
    WarpF curF(int lane) const {
        Monsoon* m = redDot::lookupMonsoon(module);
        if (!m) return WarpF{};
        const bool role = macroRoleNow();
        WarpF f = role ? m->getGlobalWarpF(lane) : m->getWarpF(slotLive(), lane);
        if (m->editor.warp.distDeltasActive) {
            const float* glob = m->editor.warp.distGlobalDelta[lane];
            const float* own  = distOwnRow_(m, role, lane);
            const int kind = (int)f.index();
            for (int p = 0; p <= 1; ++p) {
                const float d = glob[p] + (own ? own[p] : 0.f);
                if (d == 0.f) continue;
                const dotModular::WarpParamSpec win = dotModular::warpParamSpec(kind, p);
                if (!win.present) continue;                    // unused-param: the no-op
                const double base = dotModular::warpParamValue(f, p);
                f = dotModular::warpWithParam(f, p,
                    std::clamp(base + (double)d, (double)win.lo, (double)win.hi));
            }
        }
        return f;
    }

    // The East-local delta row this voice's cells actually take: the per-voice
    // expander's row for the shared slot, NULL on a delegated lane (delegated
    // cells mirror Macro entirely — the resolver's law; the display must match
    // the engine or the editor would show a modulation the cells never play).
    const float* distOwnRow_(Monsoon* m, bool role, int lane) const {
        if (role) return nullptr;
        const int slot = slotLive();
        if (m->engine.pe.warpDelegated_[slot][lane]) return nullptr;
        return m->editor.warp.distOwnDelta[slot][lane];
    }

    // The curve editor's composing-view inputs (the popup's cur* twins) — the
    // stage deltas ride the same way (the eval-side clamps in WarpStages catch
    // overshoot in both the engine path and this display path).
    void curStages(int lane, double& tp, double& al, double& am) const {
        Monsoon* m = redDot::lookupMonsoon(module);
        const int slot = slotLive();
        const bool role = macroRoleNow();
        tp = role ? (m ? m->getGlobalWarpTilt(lane) : 0.f)
                  : (m ? m->getWarpTilt(slot, lane) : 0.f);
        al = role ? (m ? m->getGlobalWarpAtomLevel(lane) : 0.5f)
                  : (m ? m->getWarpAtomLevel(slot, lane) : 0.5f);
        am = role ? (m ? m->getGlobalWarpAtomMass(lane) : 0.f)
                  : (m ? m->getWarpAtomMass(slot, lane) : 0.f);
        if (m && m->editor.warp.distDeltasActive) {
            const float* glob = m->editor.warp.distGlobalDelta[lane];
            const float* own  = distOwnRow_(m, role, lane);
            tp += glob[2] + (own ? own[2] : 0.f);
            al += glob[3] + (own ? own[3] : 0.f);
            am += glob[4] + (own ? own[4] : 0.f);
        }
    }

    void refreshEditor(int lane) {
        Monsoon* m = redDot::lookupMonsoon(module);
        if (!m || !curve) return;
        const WarpF f = curF(lane);
        // (re)build the member view + the live drawn-edit binding — the popup's
        // refresh() shape, permanent-home edition.
        curve->editCurve = nullptr;
        if (const auto* dw = std::get_if<DrawnWarp>(&f)) {
            if (dw->idx < kWarpPoolCap) {
                memberView_ = std::make_unique<MonotoneCurveView>(m->editor.warp.pool[dw->idx]);
                curve->editCurve = &m->editor.warp.pool[dw->idx];
            } else {
                memberView_ = std::make_unique<PchipDistribution>();
            }
        } else {
            memberView_ = WarpCurvePopup::distFromWarp(f);
        }
        double tp = 0, al = 0.5, am = 0;
        curStages(lane, tp, al, am);
        lastF_ = f; lastTilt_ = tp; lastLevel_ = al; lastMass_ = am;
        lastRole_ = macroRoleNow(); lastSlot_ = slotLive(); lastLane_ = lane;
        view_ = std::make_unique<WarpStagesView>(memberView_.get(), tp, al, am);
        curve->dist = view_.get();
        curve->isLight = [this]() { return themeLight_; };
        curve->atomLevel = al;
        curve->atomMass  = am;
        // Drawn edits → the scoped mirror rebuild (the popup's onEdit hook; the
        // pool slot changed in place, ONLY this hook can fire the rebuild).
        curve->onEdit = [this, lane]() {
            Monsoon* mm = redDot::lookupMonsoon(module);
            if (!mm) return;
            if (macroRoleNow()) mm->requestWarpRebuildLaneDelegating(lane);
            else                mm->requestWarpRebuildVoiceLane(slotLive(), lane);
        };
    }

    SandsDistributionExpanderWidget(SandsDistributionExpander* mod) {
        setModule(mod);
        // Browser preview creates widgets with mod=nullptr; guard against that.
        if (!mod) {
            loadPanel(asset::plugin(pluginInstance, "res/panels/SandsDistribution_dark.svg"));
            return;
        }

        // Panel SVG paths (both themes, the lane-widget idiom).
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/SandsDistribution_dark.svg"));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/SandsDistribution_light.svg"));
        loadPanel(asset::plugin(pluginInstance, "res/panels/SandsDistribution_dark.svg"));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        auto resolveMon = [this]() -> Monsoon* { return redDot::lookupMonsoon(module); };
        static const char* const* laneNames = dotModular::SandsLaneNames::EDITOR;

        // ── the 42 store-backed distribution controls (7 lanes x 6 params) ────
        // SETTER LIFETIME LAW: setters may be replayed by Rack history, so they
        // capture ONLY the module pointer (the anchor Rack validates — the same
        // class of capture Rack's own history entries hold) + by-value indices;
        // role and the shared slot resolve at CALL time. The widget never rides
        // a setter (it dies with the module; a replay window would dangle).
        for (int l = 0; l < LANES; ++l) {
            for (int p = 0; p < NPAR; ++p) {
                float lo = 0.f, hi = 1.f, def = 0.f;
                if (p == 2) { lo = -(float)warpStages::kTiltMax; hi = (float)warpStages::kTiltMax; def = 0.f; }
                if (p == 3) { lo = 0.f; hi = 1.f; def = (float)warpStages::kAtomLevelDefault; }
                if (p == 4) { lo = 0.f; hi = (float)warpStages::kAtomMassMax; def = 0.f; }
                auto* k = redDot::bindStoreKnob<Monsoon, redDot::Tag_Grey_Trim_Bar>(
                    this, "param_dexp_" + std::to_string(l) + "_" + std::to_string(p),
                    resolveMon, lo, hi, def,
                    std::string(laneNames[l]) + " " + SandsDistributionIds::PARAM_CAPTIONS[p],
                    // getValue — NULLARY (StoreKnob calls it at draw/drag time;
                    // role + shared slot resolve LIVE — the spec's "read the
                    // selected voice from the shared state").
                    [mod, l, p](Monsoon& m) -> float {
                        const bool role = mod && mod->macroRole.load(std::memory_order_relaxed);
                        const int slot = slotLiveFrom(&m);
                        switch (p) {
                            case 0: case 1: {
                                const WarpF f = role ? m.getGlobalWarpF(l) : m.getWarpF(slot, l);
                                return (float)WarpCurvePopup::paramValue(f, p);
                            }
                            case 2: return role ? m.getGlobalWarpTilt(l)      : m.getWarpTilt(slot, l);
                            case 3: return role ? m.getGlobalWarpAtomLevel(l) : m.getWarpAtomLevel(slot, l);
                            case 4: return role ? m.getGlobalWarpAtomMass(l)  : m.getWarpAtomMass(slot, l);
                            default: return role ? m.getGlobalWarpW(l)        : m.getWarpW(slot, l);
                        }
                    },
                    [mod, l, p](Monsoon& m, float v) {
                        if (!mod) return;
                        const bool role = mod->macroRole.load(std::memory_order_relaxed);
                        const int slot = slotLiveFrom(&m);
                        switch (p) {
                            case 0: case 1: {
                                const WarpF f = role ? m.getGlobalWarpF(l) : m.getWarpF(slot, l);
                                WarpF nf = WarpCurvePopup::withParam(f, p, (double)v, m.editor.warp);
                                if (role) { m.setGlobalWarpF(l, nf); m.requestWarpRebuildLaneDelegating(l); }
                                else      { m.setWarpF(slot, l, nf);  m.requestWarpRebuildVoiceLane(slot, l); }
                                break;
                            }
                            case 2:
                                if (role) { m.setGlobalWarpTilt(l, v);      m.requestWarpRebuildLaneDelegating(l); }
                                else      { m.setWarpTilt(slot, l, v);      m.requestWarpRebuildVoiceLane(slot, l); }
                                break;
                            case 3:
                                if (role) { m.setGlobalWarpAtomLevel(l, v); m.requestWarpRebuildLaneDelegating(l); }
                                else      { m.setWarpAtomLevel(slot, l, v); m.requestWarpRebuildVoiceLane(slot, l); }
                                break;
                            case 4:
                                if (role) { m.setGlobalWarpAtomMass(l, v);  m.requestWarpRebuildLaneDelegating(l); }
                                else      { m.setWarpAtomMass(slot, l, v);  m.requestWarpRebuildVoiceLane(slot, l); }
                                break;
                            default:
                                if (role) { m.setGlobalWarpW(l, v);         m.requestWarpRebuildLaneDelegating(l); }
                                else      { m.setWarpW(slot, l, v);         m.requestWarpRebuildVoiceLane(slot, l); }
                                break;
                        }
                    });
                knobs_[l][p] = k;
                // P1/P2: the kind window + the absent-param lock refresh in step()
                // (the spec's unused-param law: absent → a locked, harmless control).
                if (p <= 1) {
                    k->lockWhen = [mod, l, p]() -> bool {
                        if (!mod) return true;
                        Monsoon* m = redDot::lookupMonsoon(mod);
                        if (!m) return true;
                        const bool role = mod->macroRole.load(std::memory_order_relaxed);
                        const WarpF& f = role ? m->getGlobalWarpF(l) : m->getWarpF(slotLiveFrom(m), l);
                        return !WarpCurvePopup::warpParamSpec((int)f.index(), p).present;
                    };
                }
            }
        }

        // ── the 42 mod jacks (one per param per lane — the spec's patchable
        // dynamic-MO surface; inert until the send-bus commit) ─────────────────
        for (int l = 0; l < LANES; ++l)
            for (int p = 0; p < NPAR; ++p)
                bindInput<PJ301MPort>(
                    "input_dexpmod_" + std::to_string(l) + "_" + std::to_string(p),
                    SandsDistributionIds::warpModId(l, p));

        // ── the 7 lane-focus buttons (which lane the editor shows) ────────────
        for (int l = 0; l < LANES; ++l) {
            bindWidget<DexpFocusButton>(
                "param_dfocus_" + std::to_string(l),
                std::function<void(DexpFocusButton*)>([this, l](DexpFocusButton* b) {
                    b->lane = l;
                    b->selected = &editorLane_;
                    b->isLight = [this]() { return themeLight_; };
                    b->box.size = mm2px(Vec(5.4f, 5.4f));
                }));
        }

        // ── the permanent DistributionCurveWidget (spec: "the ENLARGED popup
        // editor ... gets a PERMANENT HOME on the Distribution expander") ─────
        curve = new WarpCurveEditor();
        curve->box.pos = mm2px(Vec(141.0f, 20.0f));
        curve->box.size = mm2px(Vec(119.16f, 90.0f));
        addChild(curve);
        refreshEditor(0);

        // ── the seam fillers (family continuity — the lane-widget wiring) ─────
        float rulePx = 1.2f * 75.f / 25.4f;
        leftFiller = new redDot::StraitsSeamFiller();
        leftFiller->isLeft = true;
        leftFiller->box.pos = Vec(-2, 0);
        leftFiller->box.size.y = box.size.y;
        leftFiller->ruleHeightPx = rulePx;
        addChild(leftFiller);
        rightFiller = new redDot::StraitsSeamFiller();
        rightFiller->isLeft = false;
        rightFiller->box.pos = Vec(box.size.x - 2, 0);
        rightFiller->box.size.y = box.size.y;
        rightFiller->ruleHeightPx = rulePx;
        addChild(rightFiller);

        // ── the connect mark (its own next-hop: the left neighbour is the host
        // visual or another family subpanel) ──────────────────────────────────
        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            connectMark->connected = [this]() {
                if (!module) return false;
                auto* left = module->leftExpander.module;
                return left && SandsDistributionExpander::isFamilyModel(left->model);
            };
            addChild(connectMark);
        }
    }

    // ── per-frame upkeep: theme swap, seam toggles, per-kind windows, editor ──
    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* mm = redDot::lookupMonsoon(module);
        themeLight_ = (mm && mm->lightTheme);

        // theme swap (the lane-widget idiom)
        int wantLight = themeLight_ ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }

        // seam fillers: family neighbours (the lane's isStraitsRPanel predicate,
        // family swapped)
        NVGcolor bg = themeLight_ ? nvgRGB(0xdc, 0xdc, 0xdc) : nvgRGB(0x14, 0x17, 0x1b);
        auto familyModel = [](rack::Module* m) -> bool {
            return m && SandsDistributionExpander::isFamilyModel(m->model);
        };
        if (leftFiller)  { leftFiller->active  = familyModel(module->leftExpander.module);  leftFiller->bgColour  = bg; }
        if (rightFiller) { rightFiller->active = familyModel(module->rightExpander.module); rightFiller->bgColour = bg; }

        // per-kind windows for P1/P2 (+label) — only on a kind flip per lane
        for (int l = 0; l < LANES; ++l) {
            const int kind = (int)curF(l).index();
            if (kind == lastKind_[l]) continue;
            lastKind_[l] = kind;
            for (int p = 0; p <= 1; ++p) {
                auto* k = knobs_[l][p];
                if (!k) continue;
                const auto spec = WarpCurvePopup::warpParamSpec(kind, p);
                k->minValue = spec.lo; k->maxValue = spec.hi; k->defaultValue = spec.def;
                k->label = std::string(dotModular::SandsLaneNames::EDITOR[l]) + " warp "
                         + (spec.present ? spec.label : "—");
            }
        }

        // the permanent editor: rebuild on ANY external change (the popup's
        // step-detector: memcmp over the trivially-copyable variant + the stage
        // triple + the role/slot/lane addressing)
        const WarpF f = curF(editorLane_);
        double tp = 0, al = 0.5, am = 0;
        curStages(editorLane_, tp, al, am);
        const bool role = macroRoleNow();
        const int slot = slotLive();
        if (std::memcmp(&f, &lastF_, sizeof(WarpF)) != 0
                || tp != lastTilt_ || al != lastLevel_ || am != lastMass_
                || role != lastRole_ || slot != lastSlot_ || editorLane_ != lastLane_) {
            refreshEditor(editorLane_);
        }
    }

    // ── lettering (nanosvg drops <text> — the Cantonment widget-lettering idiom)
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        std::shared_ptr<window::Font> font = APP->window->uiFont;
        if (!font) return;
        NVGcontext* vg = args.vg;
        nvgFontFaceId(vg, font->handle);
        const bool lit = themeLight_;
        const NVGcolor ink = lit ? nvgRGB(0x2a, 0x2a, 0x2e) : nvgRGB(0xe8, 0xe2, 0xd0);
        const NVGcolor dim = lit ? nvgRGB(0x88, 0x8d, 0x96) : nvgRGB(0x8a, 0x85, 0x78);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

        // title
        nvgFillColor(vg, ink);
        nvgFontSize(vg, mm2px(3.6f));
        nvgTextLetterSpacing(vg, mm2px(0.4f));
        nvgText(vg, box.size.x * 0.5f, mm2px(8.f), "DISTRIBUTION", NULL);
        nvgTextLetterSpacing(vg, 0.f);

        // column captions over the knob block + the mod block
        nvgFillColor(vg, dim);
        nvgFontSize(vg, mm2px(1.8f));
        for (int p = 0; p < NPAR; ++p) {
            const float knobXmm = (20.0f + p * 10.5f);
            nvgText(vg, mm2px(knobXmm), mm2px(21.5f), SandsDistributionIds::PARAM_CAPTIONS[p], NULL);
            const float jackXmm = (86.0f + p * 9.0f);
            nvgText(vg, mm2px(jackXmm), mm2px(21.5f), "MOD", NULL);
        }

        // lane names, left of each focus button
        static const char* const* laneNames = dotModular::SandsLaneNames::EDITOR;
        nvgFontSize(vg, mm2px(2.0f));
        for (int l = 0; l < LANES; ++l) {
            const float rowYmm = 27.0f + l * 12.6f;
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgText(vg, mm2px(9.6f), mm2px(rowYmm), laneNames[l], NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }

        // the editor caption: what the permanent editor shows + the role badge
        if (module) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "EDITOR: %s", laneNames[editorLane_]);
            nvgFillColor(vg, ink);
            nvgFontSize(vg, mm2px(2.2f));
            nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgText(vg, mm2px(141.f), mm2px(115.5f), buf, NULL);
            const bool role = macroRoleNow();
            nvgFillColor(vg, dim);
            nvgFontSize(vg, mm2px(1.8f));
            nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            nvgText(vg, mm2px(260.f), mm2px(115.5f),
                    role ? "CROSS-VOICE (MACRO) — MODS BECOME PER-VOICE SENDS"
                         : "PER-VOICE (EAST) — FOLLOWS THE SHARED VOICE SELECT", NULL);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
    }

    // ── the resnap hardening: the SUBPANEL's own drag-end flags (the host's
    // poll/consumer re-seats the run) ─────────────────────────────────────────
    void onDragEnd(const rack::event::DragEnd& e) override {
        ModuleWidget::onDragEnd(e);
        if (auto* g = dynamic_cast<redDot::SandsSnapGuest*>(module))
            g->needsResnap.store(true, std::memory_order_relaxed);
    }
};

} // namespace

// ── model ─────────────────────────────────────────────────────────────────────
Model* modelSandsDistributionExpander =
    createModel<SandsDistributionExpander, SandsDistributionExpanderWidget>("SandsDistributionExpander");
