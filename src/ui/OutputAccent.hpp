#pragma once
#include <rack.hpp>

// Rack-standard in/out differentiation: a rounded, luminance-contrasting region
// drawn BEHIND output jack groups. On a dark panel the region is lighter; on a
// light panel it is darker — so outputs read distinctly from inputs at a glance.
// Drawn as a low-z widget (add it before the ports). Theme-aware via a supplied
// predicate so one instance follows the host's light/dark state.
namespace redDot {

struct OutputAccent : rack::widget::Widget {
    std::function<bool()> isLight;   // returns true when host is in light theme
    float radius = 4.0f;

    void draw(const DrawArgs& args) override {
        bool light = isLight ? isLight() : false;
        NVGcontext* vg = args.vg;
        // contrasting fill + hairline border
        NVGcolor fill   = light ? nvgRGBA(0x20, 0x22, 0x28, 28)   // darker inset on light
                                : nvgRGBA(0xc8, 0xcc, 0xd4, 24);  // lighter inset on dark
        NVGcolor border = light ? nvgRGBA(0x20, 0x22, 0x28, 70)
                                : nvgRGBA(0xc8, 0xcc, 0xd4, 60);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, radius);
        nvgFillColor(vg, fill);
        nvgFill(vg);
        nvgStrokeColor(vg, border);
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }
};

// Add an output-accent region covering an mm rectangle (x,y,w,h) to a widget.
// Call BEFORE adding the output ports so it sits behind them. `lightFn` lets the
// region follow the host theme.
inline void addOutputAccent(rack::app::ModuleWidget* mw,
                            float x_mm, float y_mm, float w_mm, float h_mm,
                            std::function<bool()> lightFn) {
    using namespace rack;
    auto* a = new redDot::OutputAccent;
    a->isLight = lightFn;
    a->box.pos  = mm2px(math::Vec(x_mm, y_mm));
    a->box.size = mm2px(math::Vec(w_mm, h_mm));
    mw->addChild(a);
}

} // namespace redDot
