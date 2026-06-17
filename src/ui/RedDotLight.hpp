#pragma once
#include <rack.hpp>

// dot.modular signature "connected" indicator: the brand red dot, lit when the
// expander is connected to a Monsoon core. Singapore red (#d4001a) to match the
// dot.modular identity (the "o" oscilloscope glyph / tagline "the little red
// dot"). A ModuleLightWidget so it works with the SvgKit bindLight / Rack's
// createLightCentered and is driven by module->lights[id].setBrightness().
//
// Placed via a named SVG marker (id="light_connect") so the panel generator
// controls its position and it can be repositioned without touching code.
namespace redDot {

struct RedDotLight : rack::app::ModuleLightWidget {
    RedDotLight() {
        // Single-colour light: Singapore red.
        this->addBaseColor(nvgRGB(0xd4, 0x00, 0x1a));
        this->bgColor   = nvgRGBA(0xd4, 0x00, 0x1a, 40);   // faint unlit halo
        this->borderColor = nvgRGBA(0, 0, 0, 60);
    }
};

} // namespace redDot
