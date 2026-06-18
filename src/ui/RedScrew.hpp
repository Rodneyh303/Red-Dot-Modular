#pragma once
#include <rack.hpp>

// dot.modular signature screw: a red disc with a darker slot, matching the
// panel-concept mockup. Drawn in NanoVG so it renders consistently on top of
// any panel SVG (dark or light), unlike a baked-in SVG screw. Reusable across
// all dot.modular modules via addRedScrews().
namespace redDot {

struct RedScrew : rack::widget::Widget {
    void draw(const DrawArgs& args) override {
        // Standard VCV screw render is ~13px diameter; draw centred in our box.
        const float R = 6.5f;                 // ~13px diameter = VCV-standard size
        float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        NVGcontext* vg = args.vg;
        // soft seat shadow
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy + 0.6f, R + 0.6f);
        nvgFillColor(vg, nvgRGBA(0, 0, 0, 90));
        nvgFill(vg);
        // red body
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, R);
        nvgFillColor(vg, nvgRGB(0xdc, 0x26, 0x26));
        nvgFill(vg);
        // subtle rim
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, R);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 110));
        nvgStrokeWidth(vg, 0.8f);
        nvgStroke(vg);
        // top highlight (gives a slight dome)
        nvgBeginPath(vg);
        nvgCircle(vg, cx - R * 0.28f, cy - R * 0.30f, R * 0.42f);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 40));
        nvgFill(vg);
        // slot (diagonal, like the mockup)
        nvgSave(vg);
        nvgTranslate(vg, cx, cy);
        nvgRotate(vg, -0.5f);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, -R * 0.78f, -R * 0.16f, R * 1.56f, R * 0.32f, R * 0.10f);
        nvgFillColor(vg, nvgRGBA(0x4a, 0x08, 0x08, 230));
        nvgFill(vg);
        nvgRestore(vg);
    }
};

// Add the four corner screws to a module widget. Box size must already be set.
// Screw CENTRES sit at the Eurorack standard: 7.5mm from the left/right edges,
// 3.0mm from the top/bottom edges (converted to px at Rack's 75 DPI: 1mm =
// 75/25.4 = 2.95276 px). The screw is drawn centred in a small box.
inline void addRedScrews(rack::app::ModuleWidget* mw) {
    using namespace rack;
    const float MM = 75.f / 25.4f;            // px per mm
    const float SZ = 14.f;                     // screw box (disc ~13px centred)
    const float W = mw->box.size.x, H = mw->box.size.y;
    const float ex = 7.5f * MM, ey = 3.0f * MM;   // centre insets from edges
    auto place = [&](float cx, float cy) {
        auto* s = new redDot::RedScrew;
        s->box.size = math::Vec(SZ, SZ);
        s->box.pos  = math::Vec(cx - SZ * 0.5f, cy - SZ * 0.5f);  // box from centre
        mw->addChild(s);
    };
    place(ex,     ey);        // top-left
    place(W - ex, ey);        // top-right
    place(ex,     H - ey);    // bottom-left
    place(W - ex, H - ey);    // bottom-right
}

} // namespace redDot
