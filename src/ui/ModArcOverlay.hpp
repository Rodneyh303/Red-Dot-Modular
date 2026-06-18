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

    // Mode: radial (knobs) vs linear (vertical sliders).
    enum Mode { RADIAL, LINEAR_V } mode = RADIAL;
    // Linear mode: the handle's travel within the box (px from top/bottom edges).
    // The modulated value v maps to y = top + (1-v)*(bottom-top). Defaults assume
    // a small symmetric inset; the host can tune to match the slider art.
    float travelTopPx = 4.f;
    float travelBotPx = 4.f;

    // Position this overlay over `knob`, padded by `padPx` on every side. The pad
    // matters: the radial arc (and round line caps) can draw a little OUTSIDE the
    // knob's bounds, and the widget framework only repaints a widget's own box —
    // so without the pad, stale arc pixels linger as "trails" when the knob is
    // turned manually. Padding symmetrically keeps the knob centred (box centre ==
    // knob centre), so the radial draw math (which centres on box/2) is unchanged.
    // For LINEAR_V, the travel insets are shifted by the pad so the tick still maps
    // to the slider's real travel.
    void attachOverKnob(rack::widget::Widget* knob, float padPx) {
        box.pos  = knob->box.pos.minus(rack::math::Vec(padPx, padPx));
        box.size = knob->box.size.plus(rack::math::Vec(2*padPx, 2*padPx));
        if (mode == LINEAR_V) {
            travelTopPx += padPx;
            travelBotPx += padPx;
        }
    }

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

        // ── TEMP TRAIL PROBE ── logs why/whether the arc draws. Throttled to ~1/sec
        // per overlay via a frame counter so the log isn't flooded. Remove once the
        // trail cause is found. Captures: isActive, setN, modN, delta vs minDelta.
        bool act = !isActive || isActive();
        float pSet = getSetNorm();
        float pMod = getModNorm();
        static int _pn = 0;
        if ((_pn++ % 200) == 0) {
            INFO("[modarc] active=%d setN=%.4f modN=%.4f delta=%.4f minDelta=%.4f willDraw=%d",
                 (int)act, pSet, pMod, std::fabs(pMod - pSet), minDelta,
                 (int)(act && std::fabs(pMod - pSet) >= minDelta));
        }

        if (isActive && !isActive()) return;

        float setN = getSetNorm();
        float modN = getModNorm();
        if (std::fabs(modN - setN) < minDelta) return;   // nothing meaningful to show

        if (mode == LINEAR_V) {
            // Vertical slider: draw a tick at the modulated height + a connecting
            // stem from the set height, so you see the offset direction/size.
            // attachOverKnob() already folded the box pad into travelTopPx/BotPx,
            // so the slider travel maps within [travelTopPx, box.size.y-travelBotPx].
            float top = travelTopPx;
            float bot = box.size.y - travelBotPx;
            auto yFor = [&](float v){ v = rack::math::clamp(v,0.f,1.f); return top + (1.f - v) * (bot - top); };
            float ySet = yFor(setN);
            float yMod = yFor(modN);
            float x0 = box.size.x * 0.18f;
            float x1 = box.size.x * 0.82f;
            // Stem from set→mod
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, box.size.x * 0.5f, ySet);
            nvgLineTo(args.vg, box.size.x * 0.5f, yMod);
            nvgStrokeColor(args.vg, color);
            nvgStrokeWidth(args.vg, 2.0f);
            nvgLineCap(args.vg, NVG_ROUND);
            nvgStroke(args.vg);
            // Tick at modulated height
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x0, yMod);
            nvgLineTo(args.vg, x1, yMod);
            nvgStrokeWidth(args.vg, 2.0f);
            nvgStroke(args.vg);
            return;
        }

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
