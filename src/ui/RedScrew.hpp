#pragma once
#include <rack.hpp>

// dot.modular signature screw: a red disc with a darker slot, matching the
// panel-concept mockup. Drawn in NanoVG so it renders consistently on top of
// any panel SVG (dark or light), unlike a baked-in SVG screw. Reusable across
// all dot.modular modules via addRedScrews().
namespace redDot {

struct RedScrew : rack::widget::Widget {
    void draw(const DrawArgs& args) override {
        const float R = 5.0f;                 // ~ standard screw radius
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
inline void addRedScrews(rack::app::ModuleWidget* mw) {
    using namespace rack;
    const float SZ = 2.f * RACK_GRID_WIDTH;   // standard screw cell ~ 15.24px wide hole region
    auto place = [&](float x, float y) {
        auto* s = new redDot::RedScrew;
        s->box.size = math::Vec(SZ, SZ);
        s->box.pos  = math::Vec(x, y);
        mw->addChild(s);
    };
    float right = mw->box.size.x - 2.f * RACK_GRID_WIDTH;
    float bot   = RACK_GRID_HEIGHT - RACK_GRID_WIDTH;
    place(RACK_GRID_WIDTH, 0);
    place(right, 0);
    place(RACK_GRID_WIDTH, bot);
    place(right, bot);
}

} // namespace redDot
