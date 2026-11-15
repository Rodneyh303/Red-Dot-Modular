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
    // The right-click KIND picker (the spec's single-source discipline): the host
    // that owns the addressing sets this — the popup keeps its own chrome-level
    // menu; the Distribution expander's permanent home aims the SHARED builder
    // (openWarpKindMenu, defined below) at its role-appropriate store.
    std::function<void()> onKindMenu;
    void onDragMove(const rack::event::DragMove& e) override {
        DistributionCurveWidget::onDragMove(e);
        if (dragPoint >= 0 && onEdit) onEdit();   // interior point dragged
    }
    void onButton(const rack::event::Button& e) override {
        // Right-click = the kind menu, when this editor has a host-owned one
        // (consumed HERE so it never falls through to the module context menu).
        if (onKindMenu && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
            onKindMenu();
            e.consume(this);
            return;
        }
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

// ── FEEL-TEST (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START) + the GAP/DUAL chain
// (PHASE_DISTRIBUTION_SPEC): the composing VIEW ─────────────────────────────────
// Wraps whatever member is current (Identity default, any parametric, the LIVE drawn
// pool view — the wrapper composes the member, so the drawn-curve edit path keeps
// working under it) in the chain member → tilt → gap → atom, with the CLOSED FORMS
// from WarpStages.hpp:
//   G⁻¹ = R_p∘F⁻¹ · G = F∘R_p⁻¹ · g = f(R_p⁻¹)·(R_p⁻¹)' — the gap on the TILTED
//   coordinates (the zero-density interval, the uniform 1/(1-p) rescale) — then the
//   atom CDF x<ℓ → G(x)(1-m); x≥ℓ → G(x)(1-m)+m, and the pdf's CONTINUOUS part
//   ((1-m)·g/(1-p) outside the gap, 0 inside; the Dirac is NOT a density value — the
//   widget draws it as the explicit bar).
// THE DUAL READING (spec §6, Rodney: "Atom dual is a gap and vice versa"): dual=true
// swaps the COMPOSED PAIR — this view's cdf becomes the composed QUANTILE and its
// invCDF becomes the composed CDF, so an atom on K reads as a gap on the display and
// K's gap reads as the display's atom riser. The pdf for the dual is the reciprocal
// density: 0 on the K-atom's plateau (the dual's gap — the K Dirac's u-interval,
// excluded from the density exactly like the Dirac itself) and 1/g(Q(x)) elsewhere
// with the house finite proxy (the dual's density diverges inside K's zero-density
// regions — the bar/band markers carry what a density cannot).
// p/ℓ/m/(loc,size) pass through RAW (the store's floats) — the eval-side clamps live
// in WarpStages, exactly like the engine path.
struct WarpStagesView : Distribution {
    const Distribution* m = nullptr;   // the member (owned by the popup's memberView_)
    double p = 0.0, ell = 0.5, mass = 0.0;
    double gapLoc = 0.5, gapSize = 0.0;   // the gap stage (authored loc/size; inert at size 0)
    bool dual = false;                    // the DK reading: the composed pair exchanged
    WarpStagesView(const Distribution* member, double tilt, double level, double m_)
        : m(member), p(tilt), ell(level), mass(m_) {}
    double invCDF(double u) const override {
        if (dual)   // the dual's quantile IS the composed CDF (spec §6)
            return warpStages::cdf(u, p, gapLoc, gapSize, ell, mass,
                                   [this](double xx) { return m->cdf(xx); });
        return warpStages::invCDF(u, p, gapLoc, gapSize, ell, mass,
            [this](double uu) { return m->invCDF(uu); },
            [this](double x)  { return m->cdf(x); });
    }
    double cdf(double x) const override {
        if (dual)   // the dual's CDF IS the composed quantile
            return warpStages::invCDF(x, p, gapLoc, gapSize, ell, mass,
                                      [this](double uu) { return m->invCDF(uu); },
                                      [this](double x2) { return m->cdf(x2); });
        return warpStages::cdf(x, p, gapLoc, gapSize, ell, mass,
                               [this](double xx) { return m->cdf(xx); });
    }
    double pdf(double x) const override {
        if (dual) {
            // the reciprocal density: 0 on the K-atom's plateau [q0, q0+m] (the dual's
            // gap — the K Dirac's u-interval), else 1/g(Q(x)) with the finite proxy
            // (the dual's density diverges inside K's zero-density regions — the
            // markers carry what a density cannot).
            const double mc = warpStages::clampMass(mass);
            if (mc > 0.0) {
                const double G_ell = warpStages::cdf(ell, p, gapLoc, gapSize, ell, mass,
                                                     [this](double xx) { return m->cdf(xx); });
                // the PRE-ATOM gapped CDF at ℓ: H(ℓ) = G(ℓ)(1-m) + m ⟹ G(ℓ) = (H(ℓ)-m)/(1-m)
                const double Gpre = (G_ell - mc) / (1.0 - mc);
                const double q0 = Gpre * (1.0 - mc);
                if (x >= q0 && x <= q0 + mc) return 0.0;
            }
            const double y = this->cdf(x);   // Q(x) — the composed quantile AT x
            const double g = warpStages::pdf(y, p, gapLoc, gapSize, mass,
                                             [this](double xx) { return m->pdf(xx); },
                                             [this](double xx) { return m->cdf(xx); });
            return 1.0 / (g < DIST_EDGE_EPS ? DIST_EDGE_EPS : g);
        }
        return warpStages::pdf(x, p, gapLoc, gapSize, mass,
                               [this](double xx) { return m->pdf(xx); },
                               [this](double xx) { return m->cdf(xx); });
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
    // THE GAP PAIR (PHASE_DISTRIBUTION_SPEC): "gap location" + "gap size", the atom
    // knobs' twins — visible ALWAYS (the gap applies to every kind), bound in
    // refreshStages() beside the tilt/atom triple.
    redDot::StoreKnob<Monsoon>*   pGapLoc  = nullptr;
    redDot::StoreKnob<Monsoon>*   pGapSize = nullptr;
    std::unique_ptr<Distribution> memberView_;   // the bare member (what refresh() used to bind)
    std::unique_ptr<Distribution> view_;         // the COMPOSING view over memberView_ (feel-test)
    WarpStagesView* stagesView_ = nullptr;       // view_'s derived alias (the gap/dual fields)
    std::unique_ptr<WarpStagesView> kView_;      // the ALWAYS-K density view (the §10 fill's
                                                 // factor — see refresh(); CC_FIX_PHASE_PLOT_NEGATIVE_WARP)
    WarpF lastF_{};                     // external-change detector (undo): memcmp per step
    double lastTilt_ = 0.0, lastLevel_ = 0.5, lastMass_ = 0.0;   // ...and the stage triple
    double lastGapLoc_ = 0.5, lastGapSize_ = 0.0;                // ...and the gap pair
    float lastW_ = 0.f;                 // ...and the DEPTH (the sign flips the READING —
                                        // the display must follow it live)

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
    // THE GAP PAIR (PHASE_DISTRIBUTION_SPEC) from the same source (own slot vs Macro
    // global) — the atom knobs' twins.
    double curGapLoc() const {
        Monsoon* m = mon();
        return !m ? warpStages::kGapLocDefault
                  : (macroGlobal ? m->getGlobalWarpGapLoc(lane) : m->getWarpGapLoc(slot, lane));
    }
    double curGapSize() const {
        Monsoon* m = mon();
        return !m ? 0.0 : (macroGlobal ? m->getGlobalWarpGapSize(lane) : m->getWarpGapSize(slot, lane));
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

    // ── per-kind param plumbing — DELEGATED (CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC):
    // the ONE authority moved to WarpF.hpp (dsp) so the expander's mod-jack fold can
    // share it; these stay as the UI's thin aliases (every existing call site keeps
    // compiling, and there is still exactly one implementation to change).
    using WarpParamSpec = dotModular::WarpParamSpec;
    static double paramValue(const WarpF& f, int which) {
        return dotModular::warpParamValue(f, which);
    }
    static WarpF withParam(const WarpF& f, int which, double v, WarpBank& bank) {
        (void)bank;   // the dsp re-mint lowers through lowerWarp — no pool touch
        return dotModular::warpWithParam(f, which, v);
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
            else if constexpr (std::is_same_v<T, DualWarp>)
                return distFromWarp(toWarpF(d.member));   // the MEMBER's display object (the
                                                          // composing view does the swap)
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
        // FEEL-TEST + the GAP PAIR: the five stage knobs (always visible) — pitch 15mm
        // across the 80mm bezel: tilt, level, amount | gap loc, gap size (the gap pair
        // beside the atom pair, the panel's visual law carried onto the popup row).
        pTilt    = mkKnob(rack::math::Vec(8.f, 85.f));
        pLevel   = mkKnob(rack::math::Vec(24.f, 85.f));
        pMass    = mkKnob(rack::math::Vec(40.f, 85.f));
        pGapLoc  = mkKnob(rack::math::Vec(56.f, 85.f));
        pGapSize = mkKnob(rack::math::Vec(72.f, 85.f));
    }

    // Rebind everything to the CURRENT target state (open, kind pick, undo, external edit).
    void refresh() {
        Monsoon* m = mon();
        if (!m || !curve) return;
        const WarpF& f = cur();
        lastF_ = f;

        // The member + (for drawn) the live in-place edit curve — the AUTHORED curve
        // itself (the reading lives in the SIGN of w now, never in f: the bank holds
        // bare members only, so there is nothing to unwrap).
        const WarpF member = f;
        curve->editCurve = nullptr;
        if (const auto* dw = std::get_if<DrawnWarp>(&member)) {
            if (dw->idx < kWarpPoolCap) {
                // LIVE view over the pool slot (edits mutate it in place; the display tracks).
                memberView_ = std::make_unique<MonotoneCurveView>(m->editor.warp.pool[dw->idx]);
                curve->editCurve = &m->editor.warp.pool[dw->idx];
            } else {
                memberView_ = std::make_unique<PchipDistribution>();   // corrupt idx: safe default view
            }
        } else {
            memberView_ = distFromWarp(member);
        }
        // FEEL-TEST + the GAP/DUAL chain: compose member → tilt → gap → atom for the
        // DISPLAY (the engine's warpedValue composes the same chain over the same
        // member — at w=1 the editor shows EXACTLY what the engine hears), and hand
        // the widget the READING's marker params (the atom bar + gap band): on the K
        // reading the authored pair; on the DUAL reading the EXCHANGED pair (an atom
        // on K is a gap on DK and K's gap is the display's atom riser — §6/§8.8).
        const double tp = curTilt(), al = curLevel(), am = curMass();
        const double gl = curGapLoc(), gs = curGapSize();
        lastTilt_ = tp; lastLevel_ = al; lastMass_ = am; lastGapLoc_ = gl; lastGapSize_ = gs;
        lastW_ = curW();
        view_ = std::make_unique<WarpStagesView>(memberView_.get(), tp, al, am);
        stagesView_ = static_cast<WarpStagesView*>(view_.get());
        // THE ALWAYS-K DENSITY VIEW (CC_FIX_PHASE_PLOT_NEGATIVE_WARP): the effective
        // fill's derivative factor composes over the K chain for BOTH readings — the
        // K reading needs f(Q(u)) and the dual needs g'(u), both plain reads off ONE
        // non-dual view (stagesView_'s own dual branch is the reciprocal DISPLAY
        // density, not the factor the chain rule wants). Same member pointer — the
        // live drawn-edit binding rides it, same as view_.
        kView_ = std::make_unique<WarpStagesView>(memberView_.get(), tp, al, am);
        kView_->gapLoc = gl; kView_->gapSize = gs; kView_->dual = false;
        // THE READING FOLLOWS THE SIGN (the bidirectional warp): negative w IS the dual
        // reading — the display shows exactly what the engine composes on that side (the
        // composed pair exchanged; the atom bar / gap band read the READING's markers).
        stagesView_->gapLoc = gl; stagesView_->gapSize = gs; stagesView_->dual = (curW() < 0.f);
        curve->dist = view_.get();
        curve->isLight = isLight;
        double barLevel = al, barMass = am, bandLo = gl, bandHi = gl + gs;
        if (stagesView_->dual) {
            const double a = warpStages::clampGapLoc(gl), b = warpStages::clampGapLoc(gl + gs);
            auto cT = [&](double x) { return memberView_->cdf(warpStages::tiltMapInv(x, warpStages::clampTilt(tp))); };
            warpStages::dualMarkers(a, b, al, am, cT,
                                    [&](double x) { return warpStages::gapCdf(x, a, b, cT); },
                                    barLevel, barMass, bandLo, bandHi);
        }
        curve->atomLevel = barLevel;
        curve->atomMass  = barMass;
        curve->gapLo = warpStages::clampGapLoc(bandLo);
        curve->gapHi = warpStages::clampGapLoc(bandHi);
        // THE EFFECTIVE PLOT (PHASE_DISTRIBUTION_SPEC §10, display-only): the drawn
        // curve is the warp mix at the CURRENT signed depth — W(u) = (1−k)u + k·T(u),
        // T the READING's target (the SIGN picked it: w ≥ 0 the composed quantile via
        // this view's invCDF, w < 0 the dual — its dual branch IS the exchange) and
        // the fill factor the READING's, composed off the always-K view — so the plot
        // tracks the knob continuously through zero (at w = 0 the drawn curve IS the
        // diagonal, either side). The widget never inspects the sign: it gets the two
        // callables + the signed depth + the fill form (CC_FIX_PHASE_PLOT_NEGATIVE_WARP).
        // The step() detector refreshes on depth/stage/param change, never per sample.
        curve->warpDepth = lastW_;
        const bool dualReading = (lastW_ < 0.f);
        curve->warpTDual = dualReading;
        curve->warpT     = [this](double u) { return view_ ? view_->invCDF(u) : u; };
        curve->warpTpdf  = [this, dualReading](double u) {
            if (!kView_) return 1.0;
            // K reading: T = Q, T'(u) = 1/f(T(u))  → the factor is f(Q(u));
            // dual:      T = g, T'(u) = g'(u)      → the factor is g'(u) — the chain
            // pdf AT u (NOT the reciprocal — that was the fill's spec gap).
            return dualReading ? kView_->pdf(u) : kView_->pdf(kView_->invCDF(u));
        };

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
        // THE CHANGE-GATE'S HOST PIN (CC_FIX_CURVE_WIDGET_CPU): this refresh re-binds
        // the plot's callables — bump the host generation (one rebuild per refresh,
        // even if the new bindings probe-identically at the fingerprint's 9 points).
        curve->hostGeneration++;
    }

    // ── FEEL-TEST: the three stage knobs (bound ONCE per refresh; always visible) ────
    // The p1/p2 pattern exactly (capture-free setters, resolveStore live): "rail tilt"
    // (bipolar ±kTiltMax) · "atom level" (ℓ ∈ [0,1], interior default) · "atom amount"
    // (m ∈ [0, kAtomMassMax], 0 = neutral). Setters write the store + bump the SAME
    // scoped rebuild requests the member knobs use, so the mirror + bars move live.
    void refreshStages() {
        struct StageSpec { const char* label; float lo, hi, def; };
        const StageSpec spec[5] = {
            { "rail tilt",  -(float)warpStages::kTiltMax, (float)warpStages::kTiltMax, 0.f },
            { "atom level", 0.f, 1.f, (float)warpStages::kAtomLevelDefault },
            { "atom amount", 0.f, (float)warpStages::kAtomMassMax, 0.f },
            { "gap location", 0.f, 1.f, (float)warpStages::kGapLocDefault },
            { "gap size", 0.f, 1.f, (float)warpStages::kGapSizeDefault },
        };
        redDot::StoreKnob<Monsoon>* knobs[5] = { pTilt, pLevel, pMass, pGapLoc, pGapSize };
        for (int which = 0; which < 5; ++which) {
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
                     : which == 2 ? (float)curMass()
                     : which == 3 ? (float)curGapLoc()
                                  : (float)curGapSize();
            };
            k->setter = [slot = slot, lane = lane, macroGlobal = macroGlobal, which](Monsoon& m, float v) {
                if (macroGlobal) {
                    if (which == 0)      m.setGlobalWarpTilt(lane, v);
                    else if (which == 1) m.setGlobalWarpAtomLevel(lane, v);
                    else if (which == 2) m.setGlobalWarpAtomMass(lane, v);
                    else if (which == 3) m.setGlobalWarpGapLoc(lane, v);
                    else                 m.setGlobalWarpGapSize(lane, v);
                    m.requestWarpRebuildLaneDelegating(lane);
                } else {
                    if (which == 0)      m.setWarpTilt(slot, lane, v);
                    else if (which == 1) m.setWarpAtomLevel(slot, lane, v);
                    else if (which == 2) m.setWarpAtomMass(slot, lane, v);
                    else if (which == 3) m.setWarpGapLoc(slot, lane, v);
                    else                 m.setWarpGapSize(slot, lane, v);
                    m.requestWarpRebuildVoiceLane(slot, lane);
                }
            };
        }
    }

    // ── the per-kind p1/p2 spec — DELEGATED (the authority moved to WarpF.hpp) ─
    // The table's one home is now the dsp layer (the expander's mod-jack fold
    // window-clamps against the SAME table the knobs bind — a fold and a lock can
    // never disagree about what a kind owns). This stays as the popup's alias.
    static WarpParamSpec warpParamSpec(int kind, int which) {
        return dotModular::warpParamSpec(kind, which);
    }

    void refreshParams(const WarpF& f) {
        const int kind = warpEffKind(f);   // a dual's params are the MEMBER's
        using Spec = WarpParamSpec;
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
        bind(p1, warpParamSpec(kind, 0), 0);
        bind(p2, warpParamSpec(kind, 1), 1);
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
            || curTilt() != lastTilt_ || curLevel() != lastLevel_ || curMass() != lastMass_
            || curGapLoc() != lastGapLoc_ || curGapSize() != lastGapSize_
            || curW() != lastW_)
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
    void openKindMenu();   // defined below openWarpKindMenu — the ONE menu authority
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
            // The readout carries the chain (w · p · ℓ · m · gap) — 7px keeps the longer
            // line inside the 80mm bezel; the READING follows the warp sign — negative w
            // reads dual-<kind> (warpReadingName; the header must not lie about what the
            // engine composes on the side the knob is parked on).
            const WarpF& fNow = cur();
            char nameBuf[48];
            char buf[160];
            std::snprintf(buf, sizeof buf, "%s %s  —  %s  ·  w %+.2f  ·  p %+.2f  ·  ℓ %.2f  ·  m %.2f  ·  gap %.2f+%.2f",
                          voiceName.c_str(), laneName.c_str(),
                          warpReadingName(fNow, curW(), nameBuf, sizeof nameBuf), (double)curW(),
                          curTilt(), curLevel(), curMass(), curGapLoc(), curGapSize());
            nvgFontSize(vg, 7.f);
            nvgText(vg, w * 0.5f, rack::mm2px(4.0f), buf, nullptr);
            // Tiny labels under the stage-knob row (tooltips carry the full names; these
            // make the five-knob row readable at a glance).
            nvgFontSize(vg, 6.5f);
            nvgText(vg, rack::mm2px(8.f),  rack::mm2px(90.5f), "tilt", nullptr);
            nvgText(vg, rack::mm2px(24.f), rack::mm2px(90.5f), "level", nullptr);
            nvgText(vg, rack::mm2px(40.f), rack::mm2px(90.5f), "amount", nullptr);
            nvgText(vg, rack::mm2px(56.f), rack::mm2px(90.5f), "gap loc", nullptr);
            nvgText(vg, rack::mm2px(72.f), rack::mm2px(90.5f), "gap size", nullptr);
            const WarpF& fHint = cur();
            const int kind = (int)fHint.index();
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

// ── THE WARP KIND MENU — ONE authority for BOTH hosts of the curve editor ─────────
// (Rodney: "Popup panel has a context menu to select the distribution type, missing
// from the distribution expander" — the fix is a SHARED builder, not a copy: the
// popup and the expander's permanent home must offer the SAME 10 kinds with the
// SAME defaults.) The target carries the addressing + the commit path — the
// role-appropriate setter + scoped rebuild live with the OWNER (the popup's
// macroGlobal branch; the expander's macroRoleNow() branch), the menu stays dumb.
// THE BARE PALETTE (as amended — Rodney: "we go back to making only the original a
// menu choice"): the duals LEFT the menu — the DUAL READING rides the warp depth's
// NEGATIVE side now (the bidirectional warp), so a stored dual state would be a
// second copy of a decision the sign already makes. Ten entries, one reading rule.
struct WarpKindMenuTarget {
    std::function<Monsoon*()> resolveMon;
    std::function<WarpF()> cur;                  // the current authored F (for the ✓)
    std::function<void(const WarpF&)> commitF;   // store + scoped rebuild (the commitF shape)
    std::function<void()> afterCommit;           // the owner's refresh
    std::string laneName;
};

inline void openWarpKindMenu(const WarpKindMenuTarget& t) {
    using rack::ui::Menu;
    using rack::ui::MenuItem;
    Menu* menu = rack::createMenu();
    menu->addChild(rack::createMenuLabel(std::string("Warp curve — ") + t.laneName));
    menu->addChild(rack::createMenuLabel("click a type to author it (drawn = drag points)"));
    const WarpF curF = t.cur();
    const int curKind = (int)curF.index();   // the bare palette's own kind (the ✓)
    struct KindItem : MenuItem {
        WarpKindMenuTarget t;
        int kind;
        void onAction(const rack::event::Action&) override {
            Monsoon* m = t.resolveMon ? t.resolveMon() : nullptr;
            if (!m) return;
            if (kind == 0) { t.commitF(IdentityWarp{}); if (t.afterCommit) t.afterCommit(); return; }
            std::unique_ptr<Distribution> d = WarpCurvePopup::makeKindDefault(kind);
            if (!d) return;
            t.commitF(m->editor.warp.fromDistribution(*d));   // the bare member — the menu
                                                              // no longer wraps a reading
            if (t.afterCommit) t.afterCommit();
        }
    };
    static const char* const kNames[] = {
        "Identity (uniform)", "Triangular", "Power", "Kumaraswamy", "Logit-normal",
        "Logit-sigmoid", "Mobius", "Truncated normal", "Reversed power", "Drawn (freehand)",
    };
    // THE BARE PALETTE — ten entries, no dual section: every parametric + drawn kind
    // as its ORIGINAL self only. The dual reading did not disappear — it moved onto
    // the warp depth's NEGATIVE side (the bidirectional warp), where the sign picks
    // it per cell; a menu entry would freeze as a stored state what the knob now
    // decides live. Identity takes none anywhere: the uniform IS its own dual.
    for (int k = 0; k < 10; ++k) {
        auto* it = new KindItem;
        it->text = kNames[k];
        it->rightText = (k == curKind) ? "✓" : "";
        it->t = t; it->kind = k;
        menu->addChild(it);
    }
    menu->addChild(rack::createMenuLabel("negative WARP = the dual reading (CDF/quantile exchanged)"));
}

inline void WarpCurvePopup::openKindMenu() {
    WarpKindMenuTarget t;
    t.resolveMon  = [this]() -> Monsoon* { return mon(); };
    t.cur         = [this]() -> WarpF { return cur(); };
    t.commitF     = [this](const WarpF& f) { commitF(f); };
    t.afterCommit = [this]() { refresh(); };
    t.laneName    = laneName;
    openWarpKindMenu(t);
}

} // namespace dotModular
