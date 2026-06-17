#pragma once
#include <rack.hpp>

// dot.modular connection indicator built from the brand MARK (the little red dot
// with its Lissajous figure), instead of a plain LED. When the expander is
// connected to a Monsoon core the mark shows in full brand colour; when not, it
// is greyed/faded. Uses the existing res/logo marks (nanosvg-safe: no masks or
// gradients) and theme-swaps dark/light to match the panel.
//
// Placed via a named SVG marker (id="light_connect") so the panel generator owns
// its position and it can be repositioned without code changes. The widget sizes
// itself to markPx centred on the marker.
namespace redDot {

struct ConnectMark : rack::widget::Widget {
    std::shared_ptr<rack::window::Svg> svgDark, svgLight;
    std::function<bool()> connected;     // true -> full colour; false -> greyed
    std::function<bool()> lightTheme;    // true -> light-theme mark variant
    float markPx = 18.f;                 // rendered mark size (square), centred

    ConnectMark() {
        svgDark  = APP->window->loadSvg(rack::asset::plugin(
            pluginInstance, "res/logo/dot-modular-mark-dark-clear.svg"));
        svgLight = APP->window->loadSvg(rack::asset::plugin(
            pluginInstance, "res/logo/dot-modular-mark-light-clear.svg"));
    }

    void draw(const DrawArgs& args) override {
        bool isLight = lightTheme && lightTheme();
        auto svg = isLight ? svgLight : svgDark;
        if (!svg || !svg->handle) return;
        bool on = connected && connected();

        NVGcontext* vg = args.vg;
        nvgSave(vg);
        float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        float vb = 200.f;                       // mark viewBox is 0 0 200 200
        float s  = markPx / vb;
        nvgTranslate(vg, cx - markPx * 0.5f, cy - markPx * 0.5f);
        nvgScale(vg, s, s);
        nvgGlobalAlpha(vg, on ? 1.0f : 0.32f);
        rack::window::svgDraw(vg, svg->handle);
        nvgRestore(vg);

        if (!on) {
            nvgBeginPath(vg);
            nvgCircle(vg, cx, cy, markPx * 0.5f);
            nvgFillColor(vg, isLight ? nvgRGBA(150,150,150,120)
                                     : nvgRGBA(40,40,40,140));
            nvgFill(vg);
        }
    }
};

} // namespace redDot
