#pragma once
#include <rack.hpp>
#include <functional>

namespace dotModular {

// ── WarpCurveButton — the per-lane warp curve-editor trigger (CC_PROMPT_04 STEP 2) ──
// A small lane-coloured button in the "CRV" column, right of the dir cell: click opens
// the warp curve popup for (this panel, lane). Grey/dim + inert when lockWhen() holds —
// East locks a lane that is Macro-delegated for the CURRENTLY-SELECTED tab's voice
// (owner != EAST); Macro's is always available (it edits Macro's own delegated-default
// curve), locked only with no Monsoon attached (the store lives on Monsoon).
//
// Draws itself (the OwnerCell/DirCell idiom — the panel carries only the anchor + a
// faint column backing): a rounded dark well in the lane's colour carrying a small
// monotone S-curve glyph (the CDF curve it opens), gold like the editor's CDF stroke.
// House LCD rule: the content stays dark-on-both-themes; only the ring dims when locked.
struct WarpCurveButton : rack::widget::Widget {
    NVGcolor laneCol = nvgRGB(0x80, 0x80, 0x80);
    std::function<bool()>  lockWhen;   // grey + swallow clicks while true
    std::function<void()>  onOpen;     // left-click: host opens the popup
    bool hovered = false;

    bool locked() const { return lockWhen && lockWhen(); }

    void onButton(const rack::event::Button& e) override {
        if (e.action == GLFW_PRESS) {
            if (locked()) { e.consume(this); return; }   // inert, but swallow the click
            if (e.button == GLFW_MOUSE_BUTTON_LEFT && onOpen) {
                onOpen();
                e.consume(this);
                return;
            }
        }
        rack::widget::Widget::onButton(e);
    }
    void onEnter(const rack::event::Enter& e) override { hovered = true;  rack::widget::Widget::onEnter(e); }
    void onLeave(const rack::event::Leave& e) override { hovered = false; rack::widget::Widget::onLeave(e); }

    void draw(const rack::widget::Widget::DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        const float w = box.size.x, h = box.size.y;
        const bool lock = locked();
        const float dim = lock ? 0.40f : 1.0f;   // the DimmableLatch alpha idiom

        // The well: rounded recess, ringed in the lane colour (ring brightens on hover).
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.75f, 0.75f, w - 1.5f, h - 1.5f, 2.0f);
        nvgFillColor(vg, nvgRGB(0x10, 0x12, 0x16));
        nvgFill(vg);
        NVGcolor ring = laneCol; ring.a = dim;
        nvgStrokeColor(vg, ring);
        nvgStrokeWidth(vg, (hovered && !lock) ? 1.7f : 1.1f);
        nvgStroke(vg);

        // The glyph: a monotone (0,0)->(1,1) CDF S-curve, gold like the editor's CDF stroke.
        NVGcolor gold = nvgRGB(0xd4, 0xaf, 0x37); gold.a = dim;
        nvgBeginPath(vg);
        nvgMoveTo(vg, w * 0.20f, h * 0.80f);
        nvgBezierTo(vg, w * 0.32f, h * 0.58f, w * 0.58f, h * 0.42f, w * 0.80f, h * 0.20f);
        nvgStrokeColor(vg, gold);
        nvgStrokeWidth(vg, 1.6f);
        nvgLineCap(vg, NVG_ROUND);
        nvgStroke(vg);

        rack::widget::Widget::draw(args);
    }
};

} // namespace dotModular
