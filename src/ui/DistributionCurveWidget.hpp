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
#include <cstdio>
#include <functional>

#include "../dsp/Distribution.hpp"
#include "../dsp/MonotoneCurve.hpp"

namespace dotModular {

struct DistributionCurveWidget : rack::widget::Widget {
    // ── data bindings ────────────────────────────────────────────────────────────────────
    const Distribution* dist = nullptr;   // REQUIRED: what to draw (the interface — any type)
    MonotoneCurve* editCurve = nullptr;   // OPTIONAL: non-null only for the drawn/Pchip case
                                          // → live editing; null = display-only
    std::function<bool()> isLight;        // house theme callback (bezel + gutters only)

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

    // ── drawing ──────────────────────────────────────────────────────────────────────────
    void draw(const rack::widget::Widget::DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        const bool light = isLight && isLight();
        const float px = plotX(), py = plotY(), pw = plotW(), ph = plotH();
        const NVGcolor well    = nvgRGB(0x10, 0x12, 0x16);   // Monsoon slot recess
        const NVGcolor bezel   = light ? nvgRGB(0xb0, 0xa8, 0x98) : nvgRGB(0x2a, 0x2f, 0x37);
        const NVGcolor bezelBg = light ? nvgRGB(0xd8, 0xda, 0xde) : nvgRGB(0x13, 0x15, 0x1a);
        const NVGcolor gridCol = nvgRGBAf(0.16f, 0.19f, 0.22f, 0.55f);
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

        // Sample the density once. AUTO-MAX over the OPEN interval (endpoints excluded —
        // the finite edge-proxy of a divergent density must not define the scale); floor
        // keeps a zero-density degenerate from producing a 0/0 scale.
        const int N = 200;
        double pdfs[N + 1];
        double maxD = 1e-6;
        for (int i = 0; i <= N; ++i) {
            pdfs[i] = dist->pdf((double)i / (double)N);
            if (i > 0 && i < N && pdfs[i] > maxD) maxD = pdfs[i];
        }

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
        // to the well so a divergent edge reads as the full-height spike it is.)
        nvgBeginPath(vg);
        nvgMoveTo(vg, px, py + ph);
        for (int i = 0; i <= N; ++i) {
            const double x = (double)i / (double)N;
            double d = pdfs[i] / maxD;
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

        // The CDF — the main (and, when drawn, EDITED) curve
        nvgBeginPath(vg);
        for (int i = 0; i <= N; ++i) {
            const double x = (double)i / (double)N;
            const rack::math::Vec c = toPx(x, dist->cdf(x));
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
        nvgStrokeColor(vg, pdfCol);
        nvgFillColor(vg, pdfCol);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        for (int i = 0; i <= 4; ++i) {
            const double f = (double)i / 4.0;
            const double v = maxD * f;
            const float y = (float)(py + (1.0 - f) * (double)ph);
            nvgBeginPath(vg);
            nvgMoveTo(vg, px + pw, y);
            nvgLineTo(vg, px + pw + 2.f, y);
            nvgStroke(vg);
            char buf[16];
            if (maxD >= 100.0)      std::snprintf(buf, sizeof buf, "%.0f", v);
            else if (maxD >= 10.0)  std::snprintf(buf, sizeof buf, "%.1f", v);
            else if (maxD >= 0.01)  std::snprintf(buf, sizeof buf, "%.2f", v);
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
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
        nvgText(vg, px + pw - 2.f, py + 2.f, editCurve ? "edit" : "view", nullptr);

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

} // namespace dotModular
