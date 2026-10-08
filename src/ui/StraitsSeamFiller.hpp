#pragma once
/**
 * StraitsSeamFiller.hpp — runtime draw-over seam filler (VGLabs/MindMeld technique).
 *
 * A TransparentWidget that draws across the panel boundary when a compatible
 * neighbour is docked. Covers the sub-pixel gap + border with the panel bg +
 * top-rule, so the seam vanishes. The edge-tiling bottom wave already matches
 * at panel edges (f(0) = f(W) = 0), so the filler just needs to bridge the gap
 * with bg — the wave lines on both sides are at the same Y, so the interruption
 * is invisible at typical zoom.
 *
 * Visibility is toggled by the parent widget's step() based on neighbour detection.
 */
#include <rack.hpp>

namespace redDot {

struct StraitsSeamFiller : rack::TransparentWidget {
    // Config (set by parent)
    bool isLeft = true;          // true = left edge, false = right edge
    bool active = false;         // toggled by parent's step()
    NVGcolor bgColour;           // panel bg
    NVGcolor ruleColour = nvgRGB(0xd4, 0x00, 0x1a);  // red top-rule
    float ruleHeightPx = 0.f;    // top-rule height in px
    float panelHeightPx = 0.f;   // full panel height in px

    StraitsSeamFiller() {
        box.size = rack::Vec(4, 128.5 * 75 / 25.4);  // 4px wide, full height
    }

    void draw(const DrawArgs& args) override {
        if (!active) return;
        TransparentWidget::draw(args);

        // Draw a rectangle of panel bg covering the gap (the widget is 4px wide,
        // centred on the seam). This covers any sub-pixel gap or border.
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(args.vg, bgColour);
        nvgFill(args.vg);

        // Draw the red top-rule across the gap (matches the panel's top-rule).
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, ruleHeightPx);
        nvgFillColor(args.vg, ruleColour);
        nvgFill(args.vg);
    }
};

} // namespace redDot
