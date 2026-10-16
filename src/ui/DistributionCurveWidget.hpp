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
// See docs/design/DMCURVE_EDITOR.md (Phase 2) and docs/design/DISTRIBUTION_LAYER.md.
//
// STYLE — the house LCD rule (SandsVisualEditorV4's documented lesson): the DISPLAY content
// (curve hues, alpha-encoded density, points) stays on the dark well on BOTH themes —
// high-luminance hues and alpha-encoded data lie on a light ground. Only the bezel and
// legend follow the theme via the house isLight callback. Colours come from the existing
// tables: Monsoon slot recess #101216 / slotline #2a2f37, melody gold #d4af37 for the CDF,
// legato teal #26a6a9 for the PDF — one table, one meaning per hue.
//
// Feel parameters (grabRadius, doubleClickTime, stroke weights) are Rodney-tuned in Rack
// via the DmcurveDevHost; this widget is the functional, clean baseline.

#include <rack.hpp>
#include <functional>

#include "../dsp/Distribution.hpp"
#include "../dsp/MonotoneCurve.hpp"

namespace dotModular {

struct DistributionCurveWidget : rack::widget::Widget {
    // ── data bindings ────────────────────────────────────────────────────────────────────
    const Distribution* dist = nullptr;   // REQUIRED: what to draw (the interface — any type)
    MonotoneCurve* editCurve = nullptr;   // OPTIONAL: non-null only for the drawn/Pchip case
                                          // → live editing; null = display-only
    std::function<bool()> isLight;        // house theme callback (bezel + legend only)

    // ── feel (tuned in Rack via the dev host) ────────────────────────────────────────────
    float  grabRadius    = 12.f;          // px — control-point hit radius
    double doubleClickTime = 0.33;        // s  — double-click window for point removal
    float  cdfStroke     = 2.2f;          // px — the CDF main curve
    float  pad           = 8.f;           // px — plot inset inside the widget box

    // ── interaction state ────────────────────────────────────────────────────────────────
    int dragPoint = -1;                   // index currently being dragged (-1 = none)
    int hoverPoint = -1;                  // index under the cursor (ring highlight)
    rack::math::Vec dragPos;              // accumulated drag position (Rack gives deltas)
    double lastClickTime = -1e9;          // double-click tracking
    int lastClickIdx = -1;

    // ── plot geometry ([0,1]² ↔ px) ──────────────────────────────────────────────────────
    float plotX() const { return pad; }
    float plotY() const { return pad; }
    float plotW() const { return box.size.x - 2.f * pad; }
    float plotH() const { return box.size.y - 2.f * pad; }
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

        // Bezel + recessed well (the display stays dark on both themes — the LCD rule)
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
        nvgFillColor(vg, bezelBg);
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, px, py, pw, ph, 3.f);
        nvgFillColor(vg, well);
        nvgFill(vg);
        nvgStrokeColor(vg, bezel);
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);

        if (!dist) {
            rack::widget::Widget::draw(args);
            return;
        }

        // Faint grid at quarters + the y=x ghost (CDF intuition: above = favour high)
        nvgSave(vg);
        nvgScissor(vg, px, py, pw, ph);
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
        nvgBeginPath(vg);
        nvgMoveTo(vg, px, py + ph);
        nvgLineTo(vg, px + pw, py);
        nvgStroke(vg);

        // PDF overlay — the density, filled under the curve. FIXED SCALE: density 1.0 (the
        // uniform) maps to full well height; anything above clips at the top (a density > 1
        // means more-than-uniform concentration — clipping is honest, and the divergent
        // edges of arcsine / k<1 power flat-top visibly, which is the truth of the shape).
        const int N = 200;
        nvgBeginPath(vg);
        nvgMoveTo(vg, px, py + ph);
        for (int i = 0; i <= N; ++i) {
            const double x = (double)i / (double)N;
            const double d = dist->pdf(x);
            const float h = (float)(d < 0.0 ? 0.0 : d) * ph;
            const rack::math::Vec c = rack::math::Vec(px + (float)x * pw, py + ph - h);
            nvgLineTo(vg, c.x, c.y);
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

        // Legend (theme-following, outside the data colours)
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
            // out-of-domain x — clicks in the padded margin are harmless no-ops).
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
