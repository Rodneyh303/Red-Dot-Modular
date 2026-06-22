#pragma once
#include <rack.hpp>

// dot.modular signature output jack for POLY outputs (probability outs, and future
// poly outs). Extends the standard PJ301M port and paints a GOLD insert ring + a
// hairline Singapore-red outer ring ON TOP — the "medium" treatment: clearly special
// at a glance, still unmistakably a jack. The rings are NanoVG overlays (like
// RedScrew) so one code path renders on dark AND light panels; colours are chosen at
// draw time from a theme query. Inheriting PJ301MPort keeps all cable/drag/shadow
// plumbing correct (vs reimplementing a bare PortWidget).
//
// Usage:
//   auto* p = createOutputCentered<redDot::GoldPolyPort>(pos, module, ID);
//   p->lightTheme = [module]{ Monsoon* m = redDot::findMonsoonEitherSide(module);
//                             return m && m->lightTheme; };
//   addOutput(p);
namespace redDot {

struct GoldPolyPort : rack::componentlibrary::PJ301MPort {
    std::function<bool()> lightTheme;   // true -> light-panel variant

    // Extends the standard PJ301M port (so all cable/drag/shadow plumbing and the
    // base jack graphic are inherited and correct), then paints the dot.modular
    // poly-out signature ON TOP: a gold insert ring + a hairline Singapore-red
    // outer ring. NanoVG overlay (like RedScrew) → one code path, dark + light.
    void draw(const DrawArgs& args) override {
        PJ301MPort::draw(args);          // standard jack underneath
        drawBrandRings(args);
    }
    void drawLayer(const DrawArgs& args, int layer) override {
        PJ301MPort::drawLayer(args, layer);
        if (layer == 1) drawBrandRings(args);   // keep rings crisp on the lit layer too
    }

    void drawBrandRings(const DrawArgs& args) {
        NVGcontext* vg = args.vg;
        const bool isLight = lightTheme && lightTheme();
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float R  = box.size.x * 0.5f - 1.5f;

        NVGcolor gold = isLight ? nvgRGB(0xa0, 0x78, 0x08) : nvgRGB(0xc8, 0x96, 0x0c);
        NVGcolor red  = isLight ? nvgRGB(0xc0, 0x20, 0x2a) : nvgRGB(0xd4, 0x00, 0x1a);

        // hairline red OUTER ring — the dot.modular signature
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, R);
        nvgStrokeColor(vg, red);
        nvgStrokeWidth(vg, 1.2f);
        nvgStroke(vg);

        // GOLD insert ring (the "CV-out family" signal), sits over the metal face
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, R * 0.62f);
        nvgStrokeColor(vg, gold);
        nvgStrokeWidth(vg, 1.6f);
        nvgStroke(vg);
    }
};

} // namespace redDot
