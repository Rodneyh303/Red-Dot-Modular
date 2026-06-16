#pragma once
#include <rack.hpp>
#include <functional>

// ModArcOverlay — a transparent overlay drawn on TOP of an SvgKnob to show the
// live MODULATED value as an arc from the knob's SET position to its effective
// (post-CV) position. Only drawn when modulation is actually active and the
// delta is visible, so an unmodulated knob looks completely normal.
//
// Decoupled from the engine: the host wires two callbacks —
//   getSetNorm():  the knob's SET value, normalised 0..1 (usually the param's
//                  normalised value, which the widget already knows).
//   getModNorm():  the effective MODULATED value, normalised 0..1 (read from the
//                  module's published ModViz snapshot).
//   isActive():    whether any modulation source is contributing this frame.
//
// The arc uses the SAME angle sweep as the RDM knobs (minAngle/maxAngle) so it
// lines up with the pointer exactly.
namespace redDot {

struct ModArcOverlay : rack::TransparentWidget {
    std::function<float()> getSetNorm;
    std::function<float()> getModNorm;
    std::function<bool()>  isActive;

    // Match the RDM knob sweep (see MonsoonWidget.cpp knob ctors).
    float minAngle = -0.83f * M_PI;
    float maxAngle =  0.83f * M_PI;
    float radius   = 0.f;          // arc radius in px; set by host (knob radius + a little)
    NVGcolor color = nvgRGBAf(0.85f, 0.0f, 0.10f, 0.9f);  // dot.modular red
    float minDelta = 0.01f;        // don't draw for imperceptible modulation

    // Knob "value angle" convention: Rack SvgKnob measures rotation with 0 at the
    // top (pointer up). In nvg screen space the top is -PI/2. So a normalised
    // value v maps to a nvg angle of: (-PI/2) + (minAngle + v*(maxAngle-minAngle)).
    float nvgAngleForNorm(float v) const {
        v = rack::math::clamp(v, 0.f, 1.f);
        float knobAngle = minAngle + v * (maxAngle - minAngle);   // 0 = up
        return -float(M_PI) * 0.5f + knobAngle;                   // to nvg space
    }

    void draw(const DrawArgs& args) override {
        if (!getSetNorm || !getModNorm) return;
        if (isActive && !isActive()) return;

        float setN = getSetNorm();
        float modN = getModNorm();
        if (std::fabs(modN - setN) < minDelta) return;   // nothing meaningful to show

        float cx = box.size.x * 0.5f;
        float cy = box.size.y * 0.5f;
        float r  = (radius > 0.f) ? radius : (std::min(box.size.x, box.size.y) * 0.5f + 2.f);

        float aSet = nvgAngleForNorm(setN);
        float aMod = nvgAngleForNorm(modN);

        // Arc from set → mod. nvgArc needs a direction; pick the short way that
        // matches the sign of (mod - set).
        int dir = (modN >= setN) ? NVG_CW : NVG_CCW;

        nvgBeginPath(args.vg);
        nvgArc(args.vg, cx, cy, r, aSet, aMod, dir);
        nvgStrokeColor(args.vg, color);
        nvgStrokeWidth(args.vg, 2.0f);
        nvgLineCap(args.vg, NVG_ROUND);
        nvgStroke(args.vg);

        // A small dot at the modulated end so the live position reads clearly.
        float dotX = cx + r * std::cos(aMod);
        float dotY = cy + r * std::sin(aMod);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, dotX, dotY, 2.0f);
        nvgFillColor(args.vg, color);
        nvgFill(args.vg);
    }
};

} // namespace redDot
