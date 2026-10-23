#pragma once
// ── WarpCurvePopup — the warp curve-editor popup (CC_PROMPT_04 STEP 3) ─────────────
// Hosted by the East + Macro Sands widgets; opened by the CRV-column buttons
// (param_warpedit_<lane>). Reuses the EXISTING Phase-2 editor (DistributionCurveWidget:
// the square dual-axis CDF/PDF display, drawn-curve point editing) and binds it to the
// target WarpF slot:
//   East  (macroGlobal=false): warpF[slot*7+lane], slot = the SELECTED TAB's voice
//         (captured at open — the host closes the popup on tab switch). Own-slot curve.
//   Macro (macroGlobal=true):  globalWarpF[lane] — Macro's delegated-default curve
//         ("Macro's curve for the lane"; the curve every ceded voice tracks).
//
// EDIT PATHS (all → setWarpF/setGlobalWarpF → the scoped mirror rebuild request, so the
// lane's bars on the same module redistribute LIVE — the prompt-04 keystone):
//   · PICK a kind from the right-click menu (defaults per kind; Identity commits
//     IdentityWarp directly).
//   · TUNE params with the two popup StoreKnobs (hidden per kind: 0/1/2 params).
//     Kumaraswamy is authored on the (mode, concentration) basis via
//     abFromModeConc/modeConcFromAb (Distribution.hpp) — the prompt's rule, NOT raw (a,b).
//     Other kinds use their own param names. Knob get/set lambdas capture ONLY value
//     state (slot/lane/kind-index) so Rack history undo can never dangle the popup.
//   · DRAWN curves: the Phase-2 widget edits the bank's pool slot IN PLACE (the pool
//     reference is live, so the display tracks the drag; WarpCurveEditor::onEdit fires
//     the rebuild request after every point mutation). Drawn curves are SHARED per
//     identical curve (WarpBank::putCurve dedup) — editing one edits every slot that
//     references it; pool full ⇒ pick falls back to IdentityWarp (the safe default).
//
// PLACEMENT: the HOST places the popup over the LEFT control cluster (explicit mm, no
// panel anchor) so the lane's probability bars stay visible while editing (prompt: don't
// occlude what you're previewing). Z-ORDER (CC_PROMPT_04-FIX): the host attaches the
// scrim + popup to APP->scene — the createMenu/MenuOverlay idiom — NOT to the module
// widget, so the popup floats above every layer (later-drawn module children, adjacent
// modules, cables) and owns its events. A module-sized scrim (WarpPopupScrim) below it
// closes on any click; the host also closes on tab switch / Monsoon detach.
#include <rack.hpp>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

#include "DistributionCurveWidget.hpp"
#include "StoreBound.hpp"
#include "Controls.hpp"       // Tag_Grey_Trim_Bar (the knob face)
#include "../Monsoon.hpp"

namespace dotModular {

// ── WarpCurveEditor — the Phase-2 widget + the edit hook (fires after any drawn-curve
//    mutation so the host can request the scoped mirror rebuild — the live preview). ──
struct WarpCurveEditor : DistributionCurveWidget {
    std::function<void()> onEdit;
    void onDragMove(const rack::event::DragMove& e) override {
        DistributionCurveWidget::onDragMove(e);
        if (dragPoint >= 0 && onEdit) onEdit();   // interior point dragged
    }
    void onButton(const rack::event::Button& e) override {
        const bool leftPress = (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT);
        DistributionCurveWidget::onButton(e);
        if (leftPress && editCurve && onEdit) onEdit();   // point added/removed (or a no-op press — idempotent)
    }
};

// ── WarpPopupScrim — full-panel transparent click-catcher under the popup. Any press
//    closes (modal-ish behaviour); added BEFORE the popup so the popup gets events first. ──
struct WarpPopupScrim : rack::widget::Widget {
    std::function<void()> onClick;
    void onButton(const rack::event::Button& e) override {
        if (e.action == GLFW_PRESS) {
            if (onClick) onClick();
            e.consume(this);
        }
    }
};

// A const MonotoneCurve viewed AS a Distribution — the drawn case's LIVE display (the
// pool slot is edited in place; a by-value PchipDistribution copy would go stale).
struct MonotoneCurveView : Distribution {
    const MonotoneCurve& c;
    explicit MonotoneCurveView(const MonotoneCurve& cv) : c(cv) {}
    double cdf(double x) const override    { return c.evalCDF(x); }
    double pdf(double x) const override    { return c.pdf(x); }
    double invCDF(double u) const override { return c.evalInverse(u); }
    const char* typeName() const override  { return "drawn"; }
    json_t* toJson() const override        { return c.toJson(); }
};

// ── FEEL-TEST (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START): the composing VIEW ──────────
// Wraps whatever member is current (Identity default, any parametric, the LIVE drawn
// pool view — the wrapper composes the member, so the drawn-curve edit path keeps
// working under it) in the two stages, member → tilt → atom, with the CLOSED FORMS from
// WarpStages.hpp:
//   G⁻¹ = R_p∘F⁻¹ · G = F∘R_p⁻¹ · g = f(R_p⁻¹)·(R_p⁻¹)' — then the atom CDF
//   x<ℓ → G(x)(1-m); x≥ℓ → G(x)(1-m)+m, and the pdf's CONTINUOUS part (1-m)·g (the
//   Dirac is NOT a density value — the widget draws it as the explicit bar).
// The CDF's riser at ℓ renders through the widget's sampling for free; the PDF bar is
// driven by the widget's atomLevel/atomMass, which refresh() sets beside this view.
// p/ℓ/m pass through RAW (the store's floats) — the eval-side clamps live in
// WarpStages, exactly like the engine path.
struct WarpStagesView : Distribution {
    const Distribution* m = nullptr;   // the member (owned by the popup's memberView_)
    double p = 0.0, ell = 0.5, mass = 0.0;
    WarpStagesView(const Distribution* member, double tilt, double level, double m_)
        : m(member), p(tilt), ell(level), mass(m_) {}
    double invCDF(double u) const override {
        return warpStages::invCDF(u, p, ell, mass,
            [this](double uu) { return m->invCDF(uu); },
            [this](double x)  { return m->cdf(x); });
    }
    double cdf(double x) const override {
        return warpStages::cdf(x, p, ell, mass, [this](double xx) { return m->cdf(xx); });
    }
    double pdf(double x) const override {
        return warpStages::pdf(x, p, mass, [this](double xx) { return m->pdf(xx); });
    }
    const char* typeName() const override { return m->typeName(); }   // same curve, viewed composed
    json_t* toJson() const override       { return m->toJson(); }     // the member persists; stages have their own keys
};

// ── WarpCurvePopup ─────────────────────────────────────────────────────────────────
// OpaqueWidget base (the MenuOverlay rule): children consume FIRST — a knob press or
// curve-point press keeps ITS EventContext target, which is what Rack tracks as the
// drag owner — then the popup consumes what's left (its own chrome) and stops
// hover/scroll/key events from leaking to the rack beneath.
struct WarpCurvePopup : rack::widget::OpaqueWidget {
    // ── target (set by the host BEFORE refresh(); monsoon resolved LIVE each use) ──
    std::function<Monsoon*()> resolveMon;
    int  slot = 0;            // East: the selected tab's voiceSlot (captured at open)
    int  lane = 0;            // editor lane
    bool macroGlobal = false; // Macro: edit globalWarpF[lane] (the delegated default)
    std::string laneName = "?", voiceName = "?";
    std::function<bool()> isLight;        // house theme callback (bezel/gutters/header)
    std::function<void()> requestClose;   // unused by the scrim path; kept for hosts

    WarpCurveEditor*              curve = nullptr;
    redDot::StoreKnob<Monsoon>*   p1 = nullptr;
    redDot::StoreKnob<Monsoon>*   p2 = nullptr;
    // FEEL-TEST: the three stage knobs ("rail tilt" p, "atom level" ℓ, "atom amount" m)
    // — visible ALWAYS (the stages apply to every kind incl. Identity/Drawn), bound in
    // refreshStages() with the same capture-free pattern as p1/p2.
    redDot::StoreKnob<Monsoon>*   pTilt  = nullptr;
    redDot::StoreKnob<Monsoon>*   pLevel = nullptr;
    redDot::StoreKnob<Monsoon>*   pMass  = nullptr;
    std::unique_ptr<Distribution> memberView_;   // the bare member (what refresh() used to bind)
    std::unique_ptr<Distribution> view_;         // the COMPOSING view over memberView_ (feel-test)
    WarpF lastF_{};                     // external-change detector (undo): memcmp per step
    double lastTilt_ = 0.0, lastLevel_ = 0.5, lastMass_ = 0.0;   // ...and the stage triple

    Monsoon* mon() const { return resolveMon ? resolveMon() : nullptr; }
    const WarpF& cur() const {
        Monsoon* m = mon();
        static const WarpF kNone{};
        if (!m) return kNone;
        return macroGlobal ? m->getGlobalWarpF(lane) : m->getWarpF(slot, lane);
    }
    float curW() const {
        Monsoon* m = mon();
        if (!m) return 0.f;
        return macroGlobal ? m->getGlobalWarpW(lane) : m->getWarpW(slot, lane);
    }
    // The FEEL-TEST stage triple from the same source (own slot vs Macro global).
    double curTilt() const {
        Monsoon* m = mon();
        return !m ? 0.0 : (macroGlobal ? m->getGlobalWarpTilt(lane) : m->getWarpTilt(slot, lane));
    }
    double curLevel() const {
        Monsoon* m = mon();
        return !m ? warpStages::kAtomLevelDefault
                  : (macroGlobal ? m->getGlobalWarpAtomLevel(lane) : m->getWarpAtomLevel(slot, lane));
    }
    double curMass() const {
        Monsoon* m = mon();
        return !m ? 0.0 : (macroGlobal ? m->getGlobalWarpAtomMass(lane) : m->getWarpAtomMass(slot, lane));
    }

    // Write-through: store + the scoped rebuild request (prompt-03's dirty scope, UI side).
    // East = per-(voice,lane) (16 cells); Macro = the lane across DELEGATING voices.
    void commitF(const WarpF& f) {
        Monsoon* m = mon();
        if (!m) return;
        if (macroGlobal) { m->setGlobalWarpF(lane, f); m->requestWarpRebuildLaneDelegating(lane); }
        else             { m->setWarpF(slot, lane, f); m->requestWarpRebuildVoiceLane(slot, lane); }
    }
    // The set-time lowering path: authored Distribution → bank (parametrics lower via
    // lowerWarp; a drawn curve allocates/reuses a pool slot). Identity commits directly.
    void commitDist(std::unique_ptr<Distribution> d) {
        Monsoon* m = mon();
        if (!m || !d) return;
        commitF(m->editor.warp.fromDistribution(*d));
        refresh();
    }

    // ── per-kind param plumbing (all capture-free — Rack-history-safe) ──────────────
    static double paramValue(const WarpF& f, int which) {
        return std::visit([which](const auto& d) -> double {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, TriangularWarp>)        return which == 0 ? d.mode : 0.0;
            else if constexpr (std::is_same_v<T, PowerWarp>)        return which == 0 ? d.k : 0.0;
            else if constexpr (std::is_same_v<T, ReversedPowerWarp>)return which == 0 ? d.theta : 0.0;
            else if constexpr (std::is_same_v<T, KumaraswamyWarp>) {
                // Read on the (mode, conc) authoring basis — conc:=a, mode solved closed-form.
                double mode = 0.5, conc = 2.0;
                KumaraswamyDistribution::modeConcFromAb(d.a, d.b, mode, conc);
                return which == 0 ? mode : conc;
            }
            else if constexpr (std::is_same_v<T, LogitNormalWarp>)  return which == 0 ? d.mu : d.sigma;
            else if constexpr (std::is_same_v<T, LogitSigmoidWarp>) return which == 0 ? d.a : d.b;
            else if constexpr (std::is_same_v<T, MobiusWarp>)       return which == 0 ? d.a : 0.0;
            else if constexpr (std::is_same_v<T, TruncNormalWarp>)  return which == 0 ? d.mu : d.sigma;
            else return 0.0;   // IdentityWarp / DrawnWarp: no parametric params
        }, f);
    }
    static WarpF withParam(const WarpF& f, int which, double v, WarpBank& bank) {
        return std::visit([&f, &bank, which, v](const auto& d) -> WarpF {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, TriangularWarp>)
                return bank.fromDistribution(TriangularDistribution(which == 0 ? v : d.mode));
            else if constexpr (std::is_same_v<T, PowerWarp>)
                return bank.fromDistribution(PowerDistribution(which == 0 ? v : d.k));
            else if constexpr (std::is_same_v<T, ReversedPowerWarp>)
                return bank.fromDistribution(ReversedPowerDistribution(which == 0 ? v : d.theta));
            else if constexpr (std::is_same_v<T, KumaraswamyWarp>) {
                // Author on (mode, conc): b is SOLVED to hold the mode (exact orthogonality).
                double mode = 0.5, conc = 2.0;
                KumaraswamyDistribution::modeConcFromAb(d.a, d.b, mode, conc);
                if (which == 0) mode = v; else conc = v;
                double a, b;
                KumaraswamyDistribution::abFromModeConc(mode, conc, a, b);
                return bank.fromDistribution(KumaraswamyDistribution(a, b));
            }
            else if constexpr (std::is_same_v<T, LogitNormalWarp>)
                return bank.fromDistribution(LogitNormalDistribution(which == 0 ? v : d.mu,
                                                                    which == 1 ? v : d.sigma));
            else if constexpr (std::is_same_v<T, LogitSigmoidWarp>)
                return bank.fromDistribution(LogitSigmoidDistribution(which == 0 ? v : d.a,
                                                                     which == 1 ? v : d.b));
            else if constexpr (std::is_same_v<T, MobiusWarp>)
                return bank.fromDistribution(MobiusDistribution(which == 0 ? v : d.a));
            else if constexpr (std::is_same_v<T, TruncNormalWarp>)
                return bank.fromDistribution(TruncatedNormalDistribution(which == 0 ? v : d.mu,
                                                                        which == 1 ? v : d.sigma));
            else { (void)d; (void)bank; return f; }   // Identity / Drawn: unreachable (knobs hidden)
        }, f);
    }
    // The authored Distribution for DISPLAY: identity shows the diagonal (a default
    // PchipDistribution IS the (0,0)-(1,1) line); parametrics re-construct via .dist().
    static std::unique_ptr<Distribution> distFromWarp(const WarpF& f) {
        return std::visit([](const auto& d) -> std::unique_ptr<Distribution> {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, IdentityWarp>)
                return std::make_unique<PchipDistribution>();   // default MonotoneCurve = the diagonal
            else if constexpr (std::is_same_v<T, DrawnWarp>)
                return std::make_unique<PchipDistribution>();   // unreachable (refresh binds the LIVE
                                                                // pool view for drawn) — safe default
            else return std::make_unique<decltype(d.dist())>(d.dist());   // copy of the subclass value
        }, f);
    }

    // ── construction ────────────────────────────────────────────────────────────────
    WarpCurvePopup() {
        // SIZE THE POPUP (feel-test bug 2): Rack's recursePositionEvent only forwards an
        // event INTO this subtree when the POPUP's own box contains the point — a
        // zero-size widget (this ctor previously never set box.size) receives nothing:
        // the right-click kind menu, the StoreKnobs and the curve drags were all
        // unreachable while the full-panel scrim below closed the popup on any click.
        // Cover the children with margin: curve (2,8)..(78,68)mm; member-param knobs
        // centred (20,74)/(56,74)mm (9mm SVG → y 69.5..78.5mm); the FEEL-TEST stage
        // knobs centred (12,85)/(40,85)/(68,85)mm (y 80.5..89.5mm) with tiny labels at
        // y=90.5mm; hint text at y=93.5mm — all inside (80,96)mm.
        box.size = rack::mm2px(rack::math::Vec(80.f, 96.f));
        curve = new WarpCurveEditor();
        curve->box.pos  = rack::mm2px(rack::math::Vec(2.f, 8.f));
        curve->box.size = rack::mm2px(rack::math::Vec(76.f, 60.f));
        addChild(curve);
        auto mkKnob = [&](rack::math::Vec centreMm) {
            auto* k = rack::createWidget<redDot::StoreKnob<Monsoon>>(rack::math::Vec(0, 0));
            k->setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance,
                redDot::Tag_Grey_Trim_Bar::path())));
            k->box.pos = rack::mm2px(centreMm).minus(k->box.size.div(2));
            k->visible = false;   // rebound + shown per-kind in refreshParams()
            addChild(k);
            return k;
        };
        p1 = mkKnob(rack::math::Vec(20.f, 74.f));
        p2 = mkKnob(rack::math::Vec(56.f, 74.f));
        pTilt  = mkKnob(rack::math::Vec(12.f, 85.f));   // FEEL-TEST: the stage row (always visible)
        pLevel = mkKnob(rack::math::Vec(40.f, 85.f));
        pMass  = mkKnob(rack::math::Vec(68.f, 85.f));
    }

    // Rebind everything to the CURRENT target state (open, kind pick, undo, external edit).
    void refresh() {
        Monsoon* m = mon();
        if (!m || !curve) return;
        const WarpF& f = cur();
        lastF_ = f;

        // The member + (for drawn) the live in-place edit curve.
        curve->editCurve = nullptr;
        if (const auto* dw = std::get_if<DrawnWarp>(&f)) {
            if (dw->idx < kWarpPoolCap) {
                // LIVE view over the pool slot (edits mutate it in place; the display tracks).
                memberView_ = std::make_unique<MonotoneCurveView>(m->editor.warp.pool[dw->idx]);
                curve->editCurve = &m->editor.warp.pool[dw->idx];
            } else {
                memberView_ = std::make_unique<PchipDistribution>();   // corrupt idx: safe default view
            }
        } else {
            memberView_ = distFromWarp(f);
        }
        // FEEL-TEST: compose member → tilt → atom for the DISPLAY (the engine's
        // warpedValue composes the same stages over the same member — at w=1 the editor
        // shows EXACTLY what the engine hears), and hand the widget the atom-bar params
        // (the Dirac is drawn explicitly; the CDF riser renders through sampling).
        const double tp = curTilt(), al = curLevel(), am = curMass();
        lastTilt_ = tp; lastLevel_ = al; lastMass_ = am;
        view_ = std::make_unique<WarpStagesView>(memberView_.get(), tp, al, am);
        curve->dist = view_.get();
        curve->isLight = isLight;
        curve->atomLevel = al;
        curve->atomMass  = am;

        // Drawn edits → the scoped mirror rebuild (the pool slot itself changed; the
        // WarpF still points at the same index, so ONLY this hook can fire the rebuild).
        curve->onEdit = [slot = slot, lane = lane, macroGlobal = macroGlobal, this]() {
            Monsoon* mm = mon();
            if (!mm) return;
            if (macroGlobal) mm->requestWarpRebuildLaneDelegating(lane);
            else             mm->requestWarpRebuildVoiceLane(slot, lane);
        };

        refreshParams(f);
        refreshStages();
    }

    // ── FEEL-TEST: the three stage knobs (bound ONCE per refresh; always visible) ────
    // The p1/p2 pattern exactly (capture-free setters, resolveStore live): "rail tilt"
    // (bipolar ±kTiltMax) · "atom level" (ℓ ∈ [0,1], interior default) · "atom amount"
    // (m ∈ [0, kAtomMassMax], 0 = neutral). Setters write the store + bump the SAME
    // scoped rebuild requests the member knobs use, so the mirror + bars move live.
    void refreshStages() {
        struct StageSpec { const char* label; float lo, hi, def; };
        const StageSpec spec[3] = {
            { "rail tilt",  -(float)warpStages::kTiltMax, (float)warpStages::kTiltMax, 0.f },
            { "atom level", 0.f, 1.f, (float)warpStages::kAtomLevelDefault },
            { "atom amount", 0.f, (float)warpStages::kAtomMassMax, 0.f },
        };
        redDot::StoreKnob<Monsoon>* knobs[3] = { pTilt, pLevel, pMass };
        for (int which = 0; which < 3; ++which) {
            auto* k = knobs[which];
            if (!k) continue;
            const StageSpec& s = spec[which];
            k->visible = true;   // the stages apply to EVERY kind, incl. Identity/Drawn
            k->label = std::string(laneName) + " warp " + s.label;
            k->minValue = s.lo; k->maxValue = s.hi; k->defaultValue = s.def;
            k->resolveStore = [this]() { return mon(); };
            k->getValue = [this, which]() {
                return which == 0 ? (float)curTilt()
                     : which == 1 ? (float)curLevel()
                                  : (float)curMass();
            };
            k->setter = [slot = slot, lane = lane, macroGlobal = macroGlobal, which](Monsoon& m, float v) {
                if (macroGlobal) {
                    if (which == 0)      m.setGlobalWarpTilt(lane, v);
                    else if (which == 1) m.setGlobalWarpAtomLevel(lane, v);
                    else                 m.setGlobalWarpAtomMass(lane, v);
                    m.requestWarpRebuildLaneDelegating(lane);
                } else {
                    if (which == 0)      m.setWarpTilt(slot, lane, v);
                    else if (which == 1) m.setWarpAtomLevel(slot, lane, v);
                    else                 m.setWarpAtomMass(slot, lane, v);
                    m.requestWarpRebuildVoiceLane(slot, lane);
                }
            };
        }
    }

    void refreshParams(const WarpF& f) {
        const int kind = (int)f.index();
        // { label, lo, hi, def } per param; present=false hides the knob.
        struct Spec { const char* label; float lo, hi, def; bool present; };
        const Spec s1[] = {
            /* identity    */ { "",        0.f,  1.f, 0.f,   false },
            /* triangular  */ { "mode",    0.f,  1.f, 0.5f,  true  },
            /* power       */ { "k",       0.2f, 5.f, 2.f,   true  },
            /* kumaraswamy */ { "mode", 0.02f, 0.98f, 0.5f,  true  },
            /* logit-norm  */ { "mu",     -3.f,  3.f, 0.f,   true  },
            /* logit-sig   */ { "a",      0.25f, 4.f, 1.5f,  true  },
            /* mobius      */ { "a",      0.25f, 4.f, 1.5f,  true  },
            /* trunc-norm  */ { "mu",    -0.5f, 1.5f, 0.5f,  true  },
            /* rev-power   */ { "theta",  0.2f, 5.f, 2.f,   true  },
            /* drawn       */ { "",        0.f,  1.f, 0.f,   false },
        };
        const Spec s2[] = {
            /* identity    */ { "",        0.f,  1.f, 0.f,   false },
            /* triangular  */ { "",        0.f,  1.f, 0.f,   false },
            /* power       */ { "",        0.f,  1.f, 0.f,   false },
            /* kumaraswamy */ { "conc",    1.f,  5.f, 2.f,   true  },   // CONC_MAX=5 (load-bearing)
            /* logit-norm  */ { "sigma",  0.2f,  3.f, 1.f,   true  },
            /* logit-sig   */ { "b",      -3.f,  3.f, 0.f,   true  },
            /* mobius      */ { "",        0.f,  1.f, 0.f,   false },
            /* trunc-norm  */ { "sigma", 0.05f, 1.f, 0.2f,  true  },
            /* rev-power   */ { "",        0.f,  1.f, 0.f,   false },
            /* drawn       */ { "",        0.f,  1.f, 0.f,   false },
        };
        auto bind = [&](redDot::StoreKnob<Monsoon>* k, const Spec& s, int which) {
            k->visible = s.present;
            if (!s.present) return;
            k->label = std::string(laneName) + " warp " + s.label;
            k->minValue = s.lo; k->maxValue = s.hi; k->defaultValue = s.def;
            // getValue is NULLARY (StoreKnob calls it at draw/drag time with the popup
            // alive) — resolve Monsoon live via `this`. The SETTER is capture-free
            // (slot/lane/macroGlobal/which only): Rack history replays it after the
            // popup is gone and never dangles.
            k->resolveStore = [this]() { return mon(); };
            k->getValue = [this, which]() {
                Monsoon* m = mon();
                if (!m) return 0.f;
                const WarpF& f = macroGlobal ? m->getGlobalWarpF(lane) : m->getWarpF(slot, lane);
                return (float)paramValue(f, which);
            };
            k->setter = [slot = slot, lane = lane, macroGlobal = macroGlobal, which](Monsoon& m, float v) {
                const WarpF& f = macroGlobal ? m.getGlobalWarpF(lane) : m.getWarpF(slot, lane);
                WarpF nf = withParam(f, which, (double)v, m.editor.warp);
                if (macroGlobal) { m.setGlobalWarpF(lane, nf); m.requestWarpRebuildLaneDelegating(lane); }
                else             { m.setWarpF(slot, lane, nf);  m.requestWarpRebuildVoiceLane(slot, lane); }
            };
        };
        bind(p1, s1[kind], 0);
        bind(p2, s2[kind], 1);
    }

    // External-change detector: undo / another surface editing the same target rebinds
    // the display + knobs (a stale curve after Ctrl+Z is a lying display). memcmp over
    // the trivially-copyable variant is exact for the active bytes (padded tail bytes
    // can only cause a harmless extra refresh). The FEEL-TEST stage triple is compared
    // too — a tilt/atom edit from ANY surface must rebind the composing view + bar.
    void step() override {
        rack::widget::Widget::step();
        Monsoon* m = mon();
        if (!m || !curve) return;
        const WarpF& f = cur();
        if (std::memcmp(&f, &lastF_, sizeof(WarpF)) != 0
            || curTilt() != lastTilt_ || curLevel() != lastLevel_ || curMass() != lastMass_)
            refresh();
    }

    // Right-click anywhere on the popup: the curve-type picker.
    void onButton(const rack::event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
            openKindMenu();
            e.consume(this);
            return;
        }
        // OpaqueWidget::onButton = children FIRST, then stopPropagating, then a
        // GUARDED consume (LEFT button, and only if no child consumed). The
        // previous unconditional `if (PRESS) e.consume(this)` ran AFTER the
        // children and OVERWROTE their EventContext target — Rack tracks the
        // press's target as the drag owner, so every knob press and curve-point
        // press was re-targeted at the POPUP: the press itself still ran the
        // child's onButton (points could be ADDED) but DragStart/DragMove never
        // came back to the child — knobs could not be grabbed, points could not
        // be dragged. The guard preserves the child's gesture; stopPropagating
        // keeps a chrome press from falling through to the scrim UNDER the popup
        // (Rack keeps offering an unconsumed event to lower siblings — the
        // EventContext is shared across the recursion).
        rack::widget::OpaqueWidget::onButton(e);
    }
    void openKindMenu() {
        using rack::ui::Menu;
        using rack::ui::MenuItem;
        Menu* menu = rack::createMenu();
        menu->addChild(rack::createMenuLabel(std::string("Warp curve — ") + laneName));
        menu->addChild(rack::createMenuLabel("click a type to author it (drawn = drag points)"));
        const WarpF& f = cur();
        const int curKind = (int)f.index();
        struct KindItem : MenuItem {
            WarpCurvePopup* pop;
            int kind;
            void onAction(const rack::event::Action&) override {
                Monsoon* m = pop->mon();
                if (!m) return;
                if (kind == 0) { pop->commitF(IdentityWarp{}); pop->refresh(); return; }
                std::unique_ptr<Distribution> d = pop->makeKindDefault(kind);
                if (d) pop->commitDist(std::move(d));
            }
        };
        static const char* const kNames[] = {
            "Identity (uniform)", "Triangular", "Power", "Kumaraswamy", "Logit-normal",
            "Logit-sigmoid", "Mobius", "Truncated normal", "Reversed power", "Drawn (freehand)",
        };
        for (int k = 0; k < 10; ++k) {
            auto* it = new KindItem;
            it->text = kNames[k];
            it->rightText = (k == curKind) ? "✓" : "";
            it->pop = this; it->kind = k;
            menu->addChild(it);
        }
    }
    static std::unique_ptr<Distribution> makeKindDefault(int kind) {
        switch (kind) {
            case 1:  return std::make_unique<TriangularDistribution>(0.5);
            case 2:  return std::make_unique<PowerDistribution>(2.0);
            case 3: { double a, b; KumaraswamyDistribution::abFromModeConc(0.5, 2.0, a, b);
                      return std::make_unique<KumaraswamyDistribution>(a, b); }
            case 4:  return std::make_unique<LogitNormalDistribution>(0.0, 1.0);
            case 5:  return std::make_unique<LogitSigmoidDistribution>(1.5, 0.0);
            case 6:  return std::make_unique<MobiusDistribution>(1.5);
            case 7:  return std::make_unique<TruncatedNormalDistribution>(0.5, 0.2);
            case 8:  return std::make_unique<ReversedPowerDistribution>(2.0);
            case 9:  return std::make_unique<PchipDistribution>();   // default diagonal → pool slot
            default: return nullptr;
        }
    }

    // ── the popup chrome: bezel + header (lane · voice — kind · w) + hint ────────────
    void draw(const rack::widget::Widget::DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        const float w = box.size.x, h = box.size.y;
        const bool light = isLight && isLight();
        const NVGcolor bezelBg = light ? nvgRGB(0xd8, 0xda, 0xde) : nvgRGB(0x13, 0x15, 0x1a);
        const NVGcolor bezel   = light ? nvgRGB(0xb0, 0xa8, 0x98) : nvgRGB(0x2a, 0x2f, 0x37);
        const NVGcolor textCol = light ? nvgRGB(0x5a, 0x5f, 0x66) : nvgRGB(0x8a, 0x8f, 0x98);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.f, 0.f, w, h, 5.f);
        nvgFillColor(vg, bezelBg);
        nvgFill(vg);
        nvgStrokeColor(vg, bezel);
        nvgStrokeWidth(vg, 1.2f);
        nvgStroke(vg);

        auto font = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!font) font = APP->window->uiFont;
        if (font) {
            nvgFontFaceId(vg, font->handle);
            nvgFillColor(vg, textCol);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            // The readout now carries the FEEL-TEST stages (p/ℓ/m) beside w — 7.5px keeps
            // the longer line inside the 80mm bezel.
            char buf[128];
            std::snprintf(buf, sizeof buf, "%s %s  —  %s  ·  w %.2f  ·  p %+.2f  ·  ℓ %.2f  ·  m %.2f",
                          voiceName.c_str(), laneName.c_str(),
                          warpTypeName(cur()), (double)curW(),
                          curTilt(), curLevel(), curMass());
            nvgFontSize(vg, 7.5f);
            nvgText(vg, w * 0.5f, rack::mm2px(4.0f), buf, nullptr);
            // Tiny labels under the FEEL-TEST stage-knob row (tooltips carry the full
            // "rail tilt"/"atom level"/"atom amount" names; these make the row readable
            // at a glance during the feel-test).
            nvgFontSize(vg, 6.5f);
            nvgText(vg, rack::mm2px(12.f), rack::mm2px(90.5f), "tilt", nullptr);
            nvgText(vg, rack::mm2px(40.f), rack::mm2px(90.5f), "level", nullptr);
            nvgText(vg, rack::mm2px(68.f), rack::mm2px(90.5f), "amount", nullptr);
            const int kind = (int)cur().index();
            if (kind == 0 || kind == 9) {   // identity/drawn: point at the picker
                nvgFontSize(vg, 7.f);
                nvgText(vg, w * 0.5f, rack::mm2px(93.5f),
                        kind == 0 ? "right-click: pick a curve type"
                                  : "drag points to shape · right-click: type",
                        nullptr);
            }
        }
        rack::widget::Widget::draw(args);   // children (curve editor + knobs) on top
    }
};

} // namespace dotModular
