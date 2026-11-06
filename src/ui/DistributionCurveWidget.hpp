#pragma once
// ── dotModular::DistributionCurveWidget — the .dmcurve curve display/editor (Phase 2) ─────────
// Renders any dotModular::Distribution on [0,1]: the CDF as the main curve and the PDF
// overlaid (the slope/density — the musically-meaningful view). No F⁻¹ overlay in v1.
//
// DRAW goes through the Distribution INTERFACE ONLY (const Distribution&) — never
// MonotoneCurve — so parametric types (Kumaraswamy, arcsine, triangular, ...) display for
// free and the draw path can't grow a drawn-case special case. EDITING is active only when
// an optional MonotoneCurve* is supplied (the drawn/Pchip case); a null editCurve means
// display-only. All edits go through MonotoneCurve's tested clamped API (setPoint /
// addPoint / removePoint), so no edit can ever produce an invalid curve:
//   • drag an interior point  — monotone-clamped via setPoint
//   • click empty space       — addPoint (lands ON the curve, rejects duplicates/cap)
//   • double-click a point    — removePoint (interior only)
//   • the (0,0)/(1,1) endpoints are FULLY FIXED — not draggable, not removable
// See docs/design/DMCURVE_EDITOR.md (Phase 2 + "Widget display refinements") and
// docs/design/DISTRIBUTION_LAYER.md.
//
// DISPLAY GEOMETRY (the refinements):
//   • SQUARE plot — the curve is a [0,1]→[0,1] map (same unit on both axes), so 1:1 is the
//     honest aspect: slope reads true ("steep = dense") and the ghosted diagonal
//     (0,0)→(1,1) — the uniform reference — is a true 45°. The square sits centred in the
//     widget box; a tall host panel reserves the space above/below for controls.
//   • DUAL y-axis — CDF ∈ [0,1] is bounded but PDF ∈ [0,∞) is a density: they cannot share
//     a scale (a shared axis is why the PDF used to clip). LHS = CDF 0→1, ticked, gold
//     (colour-matched to the CDF curve). RHS = PDF 0→AUTO-MAX, ticked, teal (matched to
//     the PDF fill): the fill is auto-normalized so the max sampled density defines the
//     top — nothing clips, and the scale follows the curve live as it is edited. (Deliberately
//     NOT pre-built, per the doc: smooth-on-release rescale, and a spike display-clamp —
//     plain auto-normalize only. The auto-max samples the OPEN interval so the finite
//     edge-proxy of a divergent density (arcsine, k<1 power) doesn't define the scale.)
//
// STYLE — the house LCD rule (SandsVisualEditorV4's documented lesson): the DISPLAY content
// (curve hues, alpha-encoded density, points) stays on the dark well on BOTH themes —
// high-luminance hues and alpha-encoded data lie on a light ground. Only the bezel, gutters
// and x-axis text follow the theme via the house isLight callback. Colours come from the
// existing tables: Monsoon slot recess #101216 / slotline #2a2f37, melody gold #d4af37 for
// the CDF, legato teal #26a6a9 for the PDF — one table, one meaning per hue.
//
// Feel parameters (grabRadius, doubleClickTime, stroke weights, gutter sizes) are
// Rodney-tuned in Rack via the DmcurveDevHost; this widget is the functional baseline.

#include <rack.hpp>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <functional>

#include "../dsp/Distribution.hpp"
#include "../dsp/MonotoneCurve.hpp"
#include "../dsp/WarpStages.hpp"   // §10's guarded f_W (the effective warp plot)

namespace dotModular {

// ── THE CHANGE-GATE (CC_FIX_CURVE_WIDGET_CPU): the 200-point sample sweep is
// CACHED, rebuilt ONLY when an input to the curve moved. The old draw() sampled
// every frame — a static curve cost the same as a moving one, and disconnect was
// irrelevant because the plot is UI — ~5–6% idle CPU on the phase panels, ~1.7%
// on the Sands distribution plot.
//
// THE FINGERPRINT = EXACTLY the current rebuild's reads (the SweepSig pattern —
// ui/SweepSigGate.hpp is the shape; the skip-on-unchanged law, NOT a new scheme).
// The sweep's reads are the CALLABLES' OUTPUTS and the bound scalars, so the sig
// PROBES them at kCurveSigN = 9 fixed u's and bit-compares the doubles:
//   · effective branch (warpT bound): warpT(u_k), warpTpdf(u_k) — the hosts
//     compose the live store/set values INTO these (the widget-lambdas read the
//     views each call), so any value drift shows in the probe bits;
//   · bare branch: dist->pdf(u_k), dist->cdf(u_k) + the dist pointer (the CDF
//     stroke reads the cdf per point in the bare draw — cached now too);
//   · the scalars the sweep + stroke read: warpDepth (signed — the sign IS the
//     reading), warpTDual, gapLo/gapHi, atomLevel/atomMass (the overlays);
//   · hostGeneration — the HOSTS' rebind counter: every refreshEditor()/refresh()
//     that re-binds warpT/warpTpdf/dist bumps it, so a deliberate host rebind
//     (a selection change, a kind menu, a channel flip) ALWAYS rebuilds exactly
//     once, even if the new callables probe-identically at the 9 points;
//   · box size — a resize changes nothing in the unit-space cache but is
//     conservative-complete (the fix doc lists geometry; one rebuild on resize).
// DELIBERATELY NOT FINGERPRINTED (the SweepSig law — an input the sweep cannot
// read only causes spurious rebuilds, zero correctness): editCurve's points
// (drawn live, never cached), isLight (colours only), the dist pointer in the
// EFFECTIVE branch (the sweep never touches dist there). Mode flips are caught
// by the effective flag itself, so a mode change can never hide a stale branch.
// Bit-compares throughout (memcmp) — the SweepSig conservatism: the gate can
// only re-run spuriously (±0, padding), never miss a change.
static constexpr int kCurveSigN = 9;   // probe points u_k = k/(kCurveSigN−1)

struct CurveSig {
    bool valid = false;                       // false ⟹ first draw, rebuild once
    bool effective = false;                   // bool(warpT) — the mode flag
    bool warpTDual = false;                   // the reading (dual/quantile)
    float warpDepth = 0.f;                    // the signed depth, k = |w|
    double gapLo = 0.0, gapHi = 0.0;
    double atomLevel = 0.0, atomMass = 0.0;
    uint32_t hostGeneration = 0;              // the hosts' rebind counter
    float boxW = 0.f, boxH = 0.f;             // geometry (conservative-complete)
    const void* dist = nullptr;               // read only in the BARE branch
    std::array<double, kCurveSigN> probeT = {};      // warpT(u_k)        — effective
    std::array<double, kCurveSigN> probePdf = {};    // warpTpdf(u_k)     — effective
    std::array<double, kCurveSigN> probeDistPdf = {};  // dist->pdf(u_k)  — bare
    std::array<double, kCurveSigN> probeDistCdf = {};  // dist->cdf(u_k)  — bare
};

// Capture the signature exactly as a rebuild is about to consume it (read-only
// on every fingerprinted input — the SweepSigGate placement law).
inline CurveSig captureCurveSig(const struct DistributionCurveWidget& w);

// The gate decision: true ⟹ the cache MUST rebuild; false ⟹ the cached arrays
// already hold the exact bits a rebuild would produce (pure function of the
// fingerprinted inputs, bit-unchanged).
inline bool curveSigChanged(const CurveSig& a, const CurveSig& b) {
    if (!a.valid || !b.valid) return true;
    if (a.effective != b.effective || a.warpTDual != b.warpTDual) return true;
    if (0 != std::memcmp(&a.warpDepth, &b.warpDepth, sizeof(float))) return true;
    if (0 != std::memcmp(&a.gapLo, &b.gapLo, sizeof(double))) return true;
    if (0 != std::memcmp(&a.gapHi, &b.gapHi, sizeof(double))) return true;
    if (0 != std::memcmp(&a.atomLevel, &b.atomLevel, sizeof(double))) return true;
    if (0 != std::memcmp(&a.atomMass, &b.atomMass, sizeof(double))) return true;
    if (a.hostGeneration != b.hostGeneration) return true;
    if (0 != std::memcmp(&a.boxW, &b.boxW, sizeof(float))) return true;
    if (0 != std::memcmp(&a.boxH, &b.boxH, sizeof(float))) return true;
    if (a.effective) {
        if (0 != std::memcmp(a.probeT.data(), b.probeT.data(), sizeof(a.probeT))) return true;
        if (0 != std::memcmp(a.probePdf.data(), b.probePdf.data(), sizeof(a.probePdf))) return true;
    } else {
        if (a.dist != b.dist) return true;
        if (0 != std::memcmp(a.probeDistPdf.data(), b.probeDistPdf.data(), sizeof(a.probeDistPdf))) return true;
        if (0 != std::memcmp(a.probeDistCdf.data(), b.probeDistCdf.data(), sizeof(a.probeDistCdf))) return true;
    }
    return false;
}

struct DistributionCurveWidget : rack::widget::Widget {
    // ── data bindings ────────────────────────────────────────────────────────────────────
    const Distribution* dist = nullptr;   // REQUIRED: what to draw (the interface — any type)
    MonotoneCurve* editCurve = nullptr;   // OPTIONAL: non-null only for the drawn/Pchip case
                                           // → live editing; null = display-only
    std::function<bool()> isLight;        // house theme callback (bezel + gutters only)

    // ── FEEL-TEST: the injected atom's Dirac bar (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START)
    // A Dirac is not a density value, so the wrapped distribution's pdf() returns the
    // CONTINUOUS part only and the atom is drawn HERE, EXPLICITLY: a distinct finite bar
    // at atomLevel, height ∝ atomMass (read against the well height — m is a probability,
    // like the CDF axis), in a distinct colour so it reads as "a lump of probability
    // here" vs the teal continuous density. It MOVES with atomLevel and GROWS with
    // atomMass. Purely additive: mass 0 (the neutral default) draws NOTHING — every
    // other host of this widget is unchanged. The CDF's riser at ℓ needs no help: it
    // appears through the CDF sampling (the vertical jump between the last x<ℓ and the
    // first x≥ℓ sample).
    double atomLevel = 0.0;   // ℓ — where the bar sits (only drawn when atomMass > 0)
    double atomMass  = 0.0;   // m — the bar's height ∝ this; 0 = no atom = no bar

    // ── THE EFFECTIVE WARP PLOT (PHASE_DISTRIBUTION_SPEC §10, display-only) ──────────
    // When a target callable is BOUND (warpT) the plot draws the EFFECTIVE warped
    // curve at the current SIGNED depth instead of the bare target:
    //     W(u) = (1−k)·u + k·T(u),  k = |w| (warpEffectiveK — sign-agnostic)
    // The SIGN picks the READING and the host binds T ACCORDINGLY before handing it
    // over (CC_FIX_PHASE_PLOT_NEGATIVE_WARP — this widget never inspects the sign;
    // it receives the two callables + the signed depth + the fill form):
    //   w ≥ 0 (the quantile reading): T = the composed quantile Q — the fill factor
    //         warpTpdf(u) = f(Q(u)), the composed density AT the target point;
    //   w < 0 (the dual reading):     T = the composed CDF g — the fill factor
    //         warpTpdf(u) = g'(u), the composed density AT u.
    // ONE sweep of u over [0,1] gives both drawn curves exactly:
    //   · CDF trace: the PARAMETRIC point (W(u), u) — exact, no bisection, no
    //     integration (P(W(U) ≤ W(u)) = u);
    //   · PDF fill:  the READING's guarded closed form at x = W(u) —
    //     quantile (warpTDual = false): f_W = 1/[(1−k) + k/f_T(T(u))]  (warpEffectivePdf)
    //     CDF      (warpTDual = true):  f_W = 1/[(1−k) + k·g'(u)]      (warpEffectivePdfDual)
    //     both divisions guarded (gaps open at once for any k > 0, atoms fade in as
    //     1/(1−k) — the dual's faces flipped with the exchange — k = 0 the uniform
    //     f_W = 1: the zero crossing is continuous, the drawn curve never jumps at
    //     w = 0, either sign).
    // warpT unbound draws the bare target exactly as before — the hosts without a
    // warp target (the .dmcurve dev host) are untouched. Display-only: the engine
    // path never changes; the sweep recomputes on warp-depth/parameter change (the
    // hosts' refresh detectors), never per sample.
    float warpDepth = 0.f;                    // the SIGNED depth — the sign IS the reading, k = |w|
    std::function<double(double)> warpT;      // T(u): the composed READING's target — unbound = the bare draw
    std::function<double(double)> warpTpdf;   // the READING's derivative factor AT u (host-composed)
    bool warpTDual = false;                   // true: T is a CDF (the dual) — the fill takes the dual form

    // ── THE CACHE (CC_FIX_CURVE_WIDGET_CPU): the sample sweep's output lives HERE,
    // not on draw()'s stack. draw() strokes these; rebuildCurve() fills them.
    // (std::array members — the array-guard's P-B form.)
    static constexpr int kPlotN = 200;                       // the sweep's resolution (unchanged)
    std::array<double, kPlotN + 1> xs_{};    // effective: x = W(u_i); bare: u_i
    std::array<double, kPlotN + 1> pdfs_{};  // the drawn densities (the fill's samples)
    std::array<double, kPlotN + 1> cdfY_{};  // the CDF trace's y: effective u_i, bare cdf(u_i)
    double maxD_ = 1e-6;                     // the fill's auto-max (open interval, unchanged)
    bool   effective_ = false;               // bool(warpT) AT the last rebuild
    CurveSig sig_{};                         // the fingerprint the cache was built under
    // The hosts' rebind counter: bump on EVERY refreshEditor()/refresh() that
    // re-binds warpT/warpTpdf/dist (see CurveSig). Public — the hosts bump it.
    uint32_t hostGeneration = 0;
    // The gate's test pin: how many rebuilds actually ran. A static state holds it
    // still; each parameter/depth/selection/resize change steps it by exactly one.
    uint32_t rebuildCount = 0;

    // ── THE GAP BAND (PHASE_DISTRIBUTION_SPEC — the atom bar's twin): the
    // zero-density interval [gapLo, gapHi] shaded as a RECESS (a cool slate hatching
    // stand-in: a faint fill + two edge ticks), visually the NEGATIVE of the ember
    // atom bar — a hole in the density, not a lump. The hosts set the READING's
    // markers (on a dual these are the EXCHANGED pair — an atom on K lands here as
    // the dual's gap). Purely additive: gapHi <= gapLo draws NOTHING (width zero IS
    // the neutral body); the CDF's flat renders through sampling regardless.
    double gapLo = 0.0;   // a — the band's left edge (inert at width 0)
    double gapHi = 0.0;   // b — the band's right edge; gapHi <= gapLo = no gap

    // ── feel (tuned in Rack via the dev host) ────────────────────────────────────────────
    float  grabRadius    = 12.f;          // px — control-point hit radius
    double doubleClickTime = 0.33;        // s  — double-click window for point removal
    float  cdfStroke     = 2.2f;          // px — the CDF main curve
    float  axisL = 16.f, axisR = 16.f;    // px — y-axis gutters (CDF left / PDF right)
    float  axisB = 14.f, axisT = 6.f;     // px — x-axis gutter / top margin

    // ── interaction state ────────────────────────────────────────────────────────────────
    int dragPoint = -1;                   // index currently being dragged (-1 = none)
    int hoverPoint = -1;                  // index under the cursor (ring highlight)
    rack::math::Vec dragPos;              // accumulated drag position (Rack gives deltas)
    double lastClickTime = -1e9;          // double-click tracking
    int lastClickIdx = -1;

    // ── plot geometry: a SQUARE [0,1]² canvas inside the box ─────────────────────────────
    // side = min(width, height) after the axis gutters; centred in the leftover. A box whose
    // height is exactly axisT + (width - axisL - axisR) + axisB fits with no leftover (the
    // dev host's panel marker is cut that way); any other box still lays out correctly.
    float plotSide() const {
        const float w = box.size.x - axisL - axisR;
        const float h = box.size.y - axisT - axisB;
        return w < h ? w : h;
    }
    float plotX() const { return axisL; }
    float plotY() const { return axisT + 0.5f * (box.size.y - axisT - axisB - plotSide()); }
    float plotW() const { return plotSide(); }
    float plotH() const { return plotSide(); }
    rack::math::Vec toPx(double x, double y) const {
        return rack::math::Vec((float)(plotX() + x * (double)plotW()),
                               (float)(plotY() + (1.0 - y) * (double)plotH()));
    }
    double toUnitX(float px) const { return (px - plotX()) / (double)plotW(); }
    double toUnitY(float py) const { return 1.0 - (py - plotY()) / (double)plotH(); }

    // Nearest control point within grabRadius (-1 = none). Endpoints count as hits (they
    // are visible anchors) but are never draggable/removable — see onButton.
    int hitPoint(const rack::math::Vec& pos) const {
        if (!editCurve) return -1;
        const auto& pts = editCurve->getPoints();
        int best = -1;
        float bestD = grabRadius * grabRadius;
        for (int i = 0; i < (int)pts.size(); ++i) {
            const rack::math::Vec c = toPx(pts[i].x, pts[i].y);
            const float dx = c.x - pos.x, dy = c.y - pos.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= bestD) { best = i; bestD = d2; }
        }
        return best;
    }

    // ── THE REBUILD (CC_FIX_CURVE_WIDGET_CPU): the sample sweep moved OUT of draw()
    // — same loop, same evals, same order, same math (the fix doc's DO-NOT: only
    // WHEN it is evaluated changed, never WHAT). Fills xs_/pdfs_/cdfY_/maxD_ and
    // captures the fingerprint the cache was built under. One evaluation of the
    // curve per genuine change; a static state never reaches here.
    void rebuildCurve() {
        effective_ = bool(warpT);
        double maxD = 1e-6;   // the floor keeps a zero-density degenerate off 0/0 (unchanged)
        // THE GATE IS THE BINDING (CC_FIX_PHASE_PLOT_NEGATIVE_WARP): a bound target
        // draws the effective mix at the signed depth — EITHER sign (the old
        // warpDepth >= 0 gate threw the whole negative side to the bare target, so
        // w = −0.01 snapped to the full dual while the engine barely moved). An
        // unbound target is the legacy bare draw, regardless of depth.
        if (effective_) {
            const double k = warpStages::warpEffectiveK(warpDepth);
            for (int i = 0; i <= kPlotN; ++i) {
                const double u = (double)i / (double)kPlotN;
                const double t = warpT(u);       // the target point T(u)
                xs_[(size_t)i] = (1.0 - k) * u + k * t;   // W(u) — monotone: T is
                // the fill factor is the READING's, already composed by the host AT
                // u (f(T(u)) for the quantile, g'(u) for the dual — see the binding
                // note), and the closed form follows the reading with it
                const double fac = warpTpdf(u);
                pdfs_[(size_t)i] = warpTDual ? warpStages::warpEffectivePdfDual(k, fac)
                                             : warpStages::warpEffectivePdf(k, fac);   // f_W, guarded
                if (i > 0 && i < kPlotN && pdfs_[(size_t)i] > maxD) maxD = pdfs_[(size_t)i];
                cdfY_[(size_t)i] = u;            // the §10 PARAMETRIC trace's y
            }
        } else {
            for (int i = 0; i <= kPlotN; ++i) {
                const double u = (double)i / (double)kPlotN;
                xs_[(size_t)i] = u;
                pdfs_[(size_t)i] = dist->pdf(u);
                if (i > 0 && i < kPlotN && pdfs_[(size_t)i] > maxD) maxD = pdfs_[(size_t)i];
                cdfY_[(size_t)i] = dist->cdf(u);   // the bare CDF trace (was per-frame, now cached)
            }
        }
        maxD_ = maxD;
        sig_ = captureCurveSig(*this);
        ++rebuildCount;
    }

    // ── drawing ──────────────────────────────────────────────────────────────────────────
    void draw(const rack::widget::Widget::DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        const bool light = isLight && isLight();
        const float px = plotX(), py = plotY(), pw = plotW(), ph = plotH();
        const NVGcolor well    = nvgRGB(0x10, 0x12, 0x16);   // Monsoon slot recess
        const NVGcolor bezel   = light ? nvgRGB(0xb0, 0xa8, 0x98) : nvgRGB(0x2a, 0x2f, 0x37);
        const NVGcolor bezelBg = light ? nvgRGB(0xd8, 0xda, 0xde) : nvgRGB(0x13, 0x15, 0x1a);
        const NVGcolor gridCol = nvgRGBAf(0.30f, 0.34f, 0.39f, 0.75f);   // slotline, lifted off the near-black well
        const NVGcolor cdfCol  = nvgRGB(0xd4, 0xaf, 0x37);   // melody gold — the drawn curve
        const NVGcolor pdfCol  = nvgRGB(0x26, 0xa6, 0x9a);   // legato teal — the density
        const NVGcolor textCol = light ? nvgRGB(0x5a, 0x5f, 0x66) : nvgRGB(0x8a, 0x8f, 0x98);

        // Bezel fills the whole box; the recessed dark well is the SQUARE plot only (the
        // display stays dark on both themes — the LCD rule; the gutters follow the theme).
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
        nvgFillColor(vg, bezelBg);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRect(vg, px, py, pw, ph);
        nvgFillColor(vg, well);
        nvgFill(vg);
        nvgStrokeColor(vg, bezel);
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);

        if (!dist) {
            rack::widget::Widget::draw(args);
            return;
        }

        // Sample ONCE per CHANGE: the bare target's density (legacy) or — §10 — one
        // sweep of the effective warp mix, giving the output points xs_[] and the
        // warped densities pdfs_[] together (plus the CDF trace's ys — cached in the
        // bare mode too, where the stroke used to evaluate cdf per point per frame).
        // The CHANGE-GATE fingerprints the sweep's exact reads (CurveSig — the
        // SweepSig pattern) and rebuilds ONLY on a move; a static curve —
        // disconnected, idle, any static params — strokes the cache and evaluates
        // NOTHING (the fix doc: pixel-identical output, the ~5–6% idle cost falls
        // to the stroke-only path). First draw (no cache yet) rebuilds once.
        // AUTO-MAX over the OPEN interval (endpoints excluded — the finite
        // edge-proxy of a divergent density must not define the scale).
        CurveSig now = captureCurveSig(*this);
        if (curveSigChanged(sig_, now)) rebuildCurve();
        const bool effective = effective_;

        nvgSave(vg);
        nvgScissor(vg, px, py, pw, ph);

        // Faint grid at quarters
        nvgStrokeColor(vg, gridCol);
        nvgStrokeWidth(vg, 0.5f);
        for (int i = 1; i < 4; ++i) {
            const float g = (float)i / 4.f;
            nvgBeginPath(vg);
            nvgMoveTo(vg, px + g * pw, py);
            nvgLineTo(vg, px + g * pw, py + ph);
            nvgMoveTo(vg, px, py + g * ph);
            nvgLineTo(vg, px + pw, py + g * ph);
            nvgStroke(vg);
        }
        // The ghosted diagonal (0,0)→(1,1): the UNIFORM reference — a true 45° now that the
        // canvas is square (above the diagonal = favour high values, below = favour low).
        nvgBeginPath(vg);
        nvgMoveTo(vg, px, py + ph);
        nvgLineTo(vg, px + pw, py);
        nvgStroke(vg);

        // PDF overlay — the density, filled under the curve, auto-normalized to maxD: the
        // tallest sampled density reaches exactly the top of the well, NOTHING clips, and
        // the scale follows the curve live as it is edited. (Endpoint samples are clamped
        // to the well so a divergent edge reads as the full-height spike it is.) The x
        // coordinate is the SAMPLE's output point: i/kPlotN bare, W(u_i) effective — monotone
        // either way, so the fill polygon never folds.
        nvgBeginPath(vg);
        nvgMoveTo(vg, px, py + ph);
        for (int i = 0; i <= kPlotN; ++i) {
            const double x = effective ? xs_[(size_t)i] : (double)i / (double)kPlotN;
            double d = pdfs_[(size_t)i] / maxD_;
            if (d < 0.0) d = 0.0;
            if (d > 1.0) d = 1.0;
            nvgLineTo(vg, px + (float)x * pw, py + ph - (float)d * ph);
        }
        nvgLineTo(vg, px + pw, py + ph);
        nvgClosePath(vg);
        NVGcolor pdfFill = pdfCol;
        pdfFill.a = 0.22f;
        nvgFillColor(vg, pdfFill);
        nvgFill(vg);

        // THE GAP BAND — the zero-density interval drawn explicitly (the atom bar's
        // twin: a hole, not a lump). A faint cool-slate fill between the edges + two
        // edge ticks; the CDF's flat and the PDF's hole render through sampling, the
        // band makes the WIDTH readable at small masses.
        if (gapHi > gapLo) {
            const double a = gapLo < 0.0 ? 0.0 : gapLo;
            const double b = gapHi > 1.0 ? 1.0 : gapHi;
            if (b > a) {
                const float ax = px + (float)a * pw, bx = px + (float)b * pw;
                NVGcolor gapCol = nvgRGB(0x5a, 0x6b, 0x7a);   // cool slate — the density's hole
                gapCol.a = 0.16f;
                nvgBeginPath(vg);
                nvgRect(vg, ax, py, bx - ax, ph);
                nvgFillColor(vg, gapCol);
                nvgFill(vg);
                NVGcolor tick = nvgRGB(0x8a, 0x9b, 0xaa);
                tick.a = 0.85f;
                nvgStrokeColor(vg, tick);
                nvgStrokeWidth(vg, 1.1f);
                for (float ex : {ax, bx}) {
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, ex, py + 1.f);
                    nvgLineTo(vg, ex, py + ph - 1.f);
                    nvgStroke(vg);
                }
            }
        }

        // The ATOM BAR — the Dirac drawn explicitly (see the atomLevel/atomMass binding
        // note above): a warm ember rectangle, visually distinct from both the teal
        // density and the gold CDF, sitting at ℓ with height ∝ m. Drawn INSIDE the
        // scissor before the CDF stroke so the gold curve rides over it.
        if (atomMass > 0.0) {
            const double m = atomMass > 1.0 ? 1.0 : atomMass;
            const double l = atomLevel < 0.0 ? 0.0 : (atomLevel > 1.0 ? 1.0 : atomLevel);
            const float bx = px + (float)l * pw;
            const float bw = 2.6f;                       // px — reads as a bar, not a line
            const float byTop = py + ph - (float)m * ph; // height ∝ mass, from the well floor
            NVGcolor atomCol = nvgRGB(0xff, 0x7a, 0x45); // ember — distinct from gold/teal
            atomCol.a = 0.88f;
            nvgBeginPath(vg);
            nvgRect(vg, bx - bw * 0.5f, byTop, bw, py + ph - byTop);
            nvgFillColor(vg, atomCol);
            nvgFill(vg);
            // A thin bright core + a floor tick mark the exact ℓ even at small m.
            NVGcolor core = nvgRGB(0xff, 0xc8, 0xa8);
            nvgBeginPath(vg);
            nvgRect(vg, bx - 0.5f, byTop, 1.0f, py + ph - byTop);
            nvgFillColor(vg, core);
            nvgFill(vg);
            nvgBeginPath(vg);
            nvgMoveTo(vg, bx - 3.f, py + ph - 0.5f);
            nvgLineTo(vg, bx + 3.f, py + ph - 0.5f);
            nvgStrokeColor(vg, core);
            nvgStrokeWidth(vg, 1.4f);
            nvgStroke(vg);
        }

        // The CDF — the main (and, when drawn, EDITED) curve. Effective mode draws the
        // §10 PARAMETRIC trace (W(u_i), u_i) — the exact warped CDF, no inversion.
        nvgBeginPath(vg);
        for (int i = 0; i <= kPlotN; ++i) {
            const double x = effective ? xs_[(size_t)i] : (double)i / (double)kPlotN;
            const double y = effective ? cdfY_[(size_t)i] : dist->cdf(x);
            const rack::math::Vec c = toPx(x, y);
            if (i == 0) nvgMoveTo(vg, c.x, c.y);
            else        nvgLineTo(vg, c.x, c.y);
        }
        nvgStrokeColor(vg, cdfCol);
        nvgStrokeWidth(vg, cdfStroke);
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_ROUND);
        nvgStroke(vg);
        nvgRestore(vg);   // end scissor

        // Control points — drawn ONLY in edit mode (a display-only distribution is a clean
        // readout; points would imply draggability it doesn't have)
        if (editCurve) {
            const auto& pts = editCurve->getPoints();
            for (int i = 0; i < (int)pts.size(); ++i) {
                const rack::math::Vec c = toPx(pts[i].x, pts[i].y);
                const bool endpoint = (i == 0 || i == (int)pts.size() - 1);
                const bool active = (i == dragPoint) || (i == hoverPoint);
                if (endpoint) {
                    // FIXED anchors: hollow square + gold ring — visibly not draggable
                    nvgBeginPath(vg);
                    nvgRect(vg, c.x - 3.5f, c.y - 3.5f, 7.f, 7.f);
                    nvgFillColor(vg, well);
                    nvgFill(vg);
                    nvgStrokeColor(vg, cdfCol);
                    nvgStrokeWidth(vg, 1.2f);
                    nvgStroke(vg);
                } else {
                    nvgBeginPath(vg);
                    nvgCircle(vg, c.x, c.y, active ? 4.5f : 3.5f);
                    nvgFillColor(vg, active ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0xe8, 0xe8, 0xe8));
                    nvgFill(vg);
                    nvgStrokeColor(vg, well);
                    nvgStrokeWidth(vg, 1.f);
                    nvgStroke(vg);
                    if (active) {
                        nvgBeginPath(vg);
                        nvgCircle(vg, c.x, c.y, 7.5f);
                        nvgStrokeColor(vg, cdfCol);
                        nvgStrokeWidth(vg, 1.f);
                        nvgStroke(vg);
                    }
                }
            }
        }

        // ── axes + ticks ─────────────────────────────────────────────────────────────────
        nvgFontSize(vg, 7.f);
        nvgStrokeWidth(vg, 0.8f);
        // LHS: the CDF scale, 0→1, colour-matched to the curve it reads
        nvgStrokeColor(vg, cdfCol);
        nvgFillColor(vg, cdfCol);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        for (int i = 0; i <= 4; ++i) {
            const double q = (double)i / 4.0;
            const float y = (float)(py + (1.0 - q) * (double)ph);
            nvgBeginPath(vg);
            nvgMoveTo(vg, px - 2.f, y);
            nvgLineTo(vg, px, y);
            nvgStroke(vg);
            char buf[16];
            std::snprintf(buf, sizeof buf, "%g", q);
            nvgText(vg, px - 3.5f, y, buf, nullptr);
        }
        // RHS: the PDF density scale, 0→auto-max (the fill's normalization), colour-matched
        // (the cached maxD_ — the SAME bits the fill normalizes against, CC_FIX_CURVE_WIDGET_CPU)
        nvgStrokeColor(vg, pdfCol);
        nvgFillColor(vg, pdfCol);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        for (int i = 0; i <= 4; ++i) {
            const double f = (double)i / 4.0;
            const double v = maxD_ * f;
            const float y = (float)(py + (1.0 - f) * (double)ph);
            nvgBeginPath(vg);
            nvgMoveTo(vg, px + pw, y);
            nvgLineTo(vg, px + pw + 2.f, y);
            nvgStroke(vg);
            char buf[16];
            if (maxD_ >= 100.0)      std::snprintf(buf, sizeof buf, "%.0f", v);
            else if (maxD_ >= 10.0)  std::snprintf(buf, sizeof buf, "%.1f", v);
            else if (maxD_ >= 0.01)  std::snprintf(buf, sizeof buf, "%.2f", v);
            else                    std::snprintf(buf, sizeof buf, "%.2g", v);
            nvgText(vg, px + pw + 3.5f, y, buf, nullptr);
        }
        // X: the value scale, 0→1 (theme text)
        nvgStrokeColor(vg, bezel);
        nvgFillColor(vg, textCol);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        for (int i = 0; i <= 4; ++i) {
            const double q = (double)i / 4.0;
            const float x = (float)(px + q * (double)pw);
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, py + ph);
            nvgLineTo(vg, x, py + ph + 2.f);
            nvgStroke(vg);
            char buf[16];
            std::snprintf(buf, sizeof buf, "%g", q);
            nvgText(vg, x, py + ph + 3.f, buf, nullptr);
        }

        // Legend — the curve names + the mode tag (kept from the first render)
        nvgFontSize(vg, 9.f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, cdfCol);
        nvgText(vg, px + 2.f, py + 2.f, "CDF", nullptr);
        nvgFillColor(vg, pdfCol);
        nvgText(vg, px + 26.f, py + 2.f, "PDF", nullptr);
        nvgFillColor(vg, textCol);
        // Bottom-right INSIDE the well: the top-right corner collides with the RHS axis
        // labels, and the bottom-right of a CDF plot is reliably empty (below the diagonal).
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
        nvgText(vg, px + pw - 2.f, py + ph - 2.f, editCurve ? "edit" : "view", nullptr);

        rack::widget::Widget::draw(args);
    }

    // ── editing events ───────────────────────────────────────────────────────────────────
    void onButton(const rack::event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && editCurve) {
            const int hit = hitPoint(e.pos);
            if (hit >= 0) {
                const double now = rack::system::getTime();
                const bool dbl = (now - lastClickTime < doubleClickTime) && (hit == lastClickIdx);
                lastClickTime = now;
                lastClickIdx = hit;
                const bool endpoint = (hit == 0 || hit == editCurve->size() - 1);
                if (dbl && !endpoint) {
                    editCurve->removePoint(hit);   // interior only — endpoints never removable
                    dragPoint = -1;
                    hoverPoint = -1;
                } else if (!endpoint) {
                    dragPoint = hit;
                    const auto& p = editCurve->getPoints()[(size_t)hit];
                    dragPos = toPx(p.x, p.y);
                }
                // Endpoints: consume without action (they anchor the CDF; a click there must
                // not fall through and add a point at x≈0/1).
                e.consume(this);
                return;
            }
            // Empty space: add a point ON the curve at this x (addPoint lands it on the CDF
            // with no discontinuity and internally rejects duplicates / the 32-point cap /
            // out-of-domain x — clicks in the gutters are harmless no-ops).
            editCurve->addPoint(toUnitX(e.pos.x));
            e.consume(this);
            return;
        }
        // Right-click (context menu) and display-only mode fall through untouched.
        rack::widget::Widget::onButton(e);
    }

    void onDragStart(const rack::event::DragStart& e) override {
        if (dragPoint < 0) { rack::widget::Widget::onDragStart(e); return; }
        e.consume(this);
    }

    void onDragMove(const rack::event::DragMove& e) override {
        if (dragPoint < 0 || !editCurve) { rack::widget::Widget::onDragMove(e); return; }
        // Rack 2 gives DELTA, not absolute position — accumulate (the house idiom).
        dragPos = dragPos.plus(e.mouseDelta);
        // setPoint enforces every invariant (monotone clamp, MIN_SPACING, fixed endpoints),
        // so the drag can never corrupt the curve; the display follows live.
        editCurve->setPoint(dragPoint, toUnitX(dragPos.x), toUnitY(dragPos.y));
        e.consume(this);
    }

    void onDragEnd(const rack::event::DragEnd& e) override {
        if (dragPoint >= 0) { dragPoint = -1; e.consume(this); return; }
        rack::widget::Widget::onDragEnd(e);
    }

    void onHover(const rack::event::Hover& e) override {
        hoverPoint = editCurve ? hitPoint(e.pos) : -1;
        rack::widget::Widget::onHover(e);   // not consumed — tooltips / cursor still work
    }
};

// The signature capture (defined AFTER the widget — it reads its bindings).
inline CurveSig captureCurveSig(const struct DistributionCurveWidget& w) {
    CurveSig s;
    s.valid = true;
    s.effective = bool(w.warpT);
    s.warpTDual = w.warpTDual;
    s.warpDepth = w.warpDepth;
    s.gapLo = w.gapLo;   s.gapHi = w.gapHi;
    s.atomLevel = w.atomLevel; s.atomMass = w.atomMass;
    s.hostGeneration = w.hostGeneration;
    s.boxW = w.box.size.x; s.boxH = w.box.size.y;
    if (s.effective) {
        for (int i = 0; i < kCurveSigN; ++i) {
            const double u = (double)i / (double)(kCurveSigN - 1);
            s.probeT[i]   = w.warpT(u);
            s.probePdf[i] = w.warpTpdf(u);
        }
    } else if (w.dist) {
        s.dist = w.dist;
        for (int i = 0; i < kCurveSigN; ++i) {
            const double u = (double)i / (double)(kCurveSigN - 1);
            s.probeDistPdf[i] = w.dist->pdf(u);
            s.probeDistCdf[i] = w.dist->cdf(u);
        }
    }
    return s;
}

} // namespace dotModular
