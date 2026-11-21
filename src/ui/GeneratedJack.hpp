#pragma once
#include <rack.hpp>
#include <functional>

// dot.modular GENERATED jacks — the best-of-both-worlds port.
//
// WHY. The stock Rack PJ301M jack is visually large on a dense panel. These jacks
// come in Std / Small / Tiny sizes (gen_jacks.py), in the knob palette families, so a
// panel can shrink its jacks (knobs unchanged) to free room — e.g. for the voice-LOR
// controls next to each lane (UNIFIED_ADDRESSING_SYSTEM.md, voice-LOR gaps).
//
// HOW IT KEEPS THE PLUMBING. This inherits rack::app::SvgPort (the SAME base PJ301MPort
// uses), so ALL cable/drag/shadow/hover interaction is Rack's, correct and unchanged.
// We only swap the GRAPHIC via setSvg() — "optionally replace, don't reimplement". A
// use site can still use the stock PJ301MPort anywhere; these are a drop-in alternative
// where a smaller/custom jack is wanted. (Cf. GoldPolyPort, which inherits PJ301MPort and
// paints rings on top; this one inherits SvgPort and swaps the whole face, so size is
// free. Both reuse Rack's port plumbing rather than rolling their own.)
//
// THEME. Like the knobs' ThemedKnobT, the jack swaps its SVG for the light/dark panel:
// set lightTheme; the Dark face shows on a light panel and the OffWhite face on a dark
// panel. If lightTheme is unset, it stays on the constructed (dark-family) face.
//
// BRAND RINGS (optional). For signature jacks (e.g. poly outs) set brand = Red or Gold:
// a hairline NanoVG ring is painted on top (one code path, both themes), exactly as
// GoldPolyPort does — but here over the generated face and at the chosen size.
//
// USAGE:
//   auto* p = createInputCentered<redDot::Jack_Small>(pos, module, ID);
//   p->lightTheme = [module]{ Monsoon* m = redDot::lookupMonsoon(module); return m && m->lightTheme; };
//   addInput(p);
//   // output signature jack:
//   auto* o = createOutputCentered<redDot::Jack_Small>(pos, module, ID);
//   o->brand = redDot::JackBrand::Gold;

namespace redDot {

enum class JackBrand { None, Red, Gold };

// Tag types carry the two theme faces for one (size) so a single widget can swap.
// path pattern: res/controls/RDM_Jack<Palette>_<Size>.svg  (gen_jacks.py)
template <const char* DARK_PATH, const char* LIGHT_PATH>
struct GeneratedJackT : rack::app::SvgPort {
    std::function<bool()> lightTheme;     // true -> light-panel variant (swap to Dark face)
    JackBrand brand = JackBrand::None;    // optional signature ring
    bool appliedLight_ = false;
    bool haveApplied_  = false;

    GeneratedJackT() {
        // Default: dark panel -> OffWhite face (LIGHT_PATH). On a LIGHT panel we show the
        // DARK face (DARK_PATH) for contrast — EXACTLY ThemedKnobT's convention. SvgPort::
        // setSvg sizes the box from the SVG, so the footprint is the generated size.
        apply(false);
    }

    void apply(bool light) {
        // light==true  => LIGHT panel  => DARK face (DARK_PATH)
        // light==false => DARK panel   => OffWhite face (LIGHT_PATH)
        setSvg(APP->window->loadSvg(rack::asset::plugin(
            pluginInstance, light ? DARK_PATH : LIGHT_PATH)));
        if (shadow) shadow->opacity = 0.f;   // setSvg rebuilt it — match the knobs
        appliedLight_ = light;
        haveApplied_  = true;
    }

    void step() override {
        if (lightTheme) {
            const bool want = lightTheme();
            if (!haveApplied_ || want != appliedLight_) apply(want);
        }
        rack::app::SvgPort::step();
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        rack::app::SvgPort::drawLayer(args, layer);
        if (layer == 1 && brand != JackBrand::None) drawBrandRing(args);
    }
    void draw(const DrawArgs& args) override {
        rack::app::SvgPort::draw(args);
        if (brand != JackBrand::None) drawBrandRing(args);
    }

    void drawBrandRing(const DrawArgs& args) {
        NVGcontext* vg = args.vg;
        const float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
        const float R  = box.size.x * 0.5f - 1.0f;
        NVGcolor col = (brand == JackBrand::Gold) ? nvgRGB(0xc9, 0xa2, 0x27)
                                                  : nvgRGB(0xd4, 0x00, 0x1a);
        nvgBeginPath(vg);
        nvgCircle(vg, cx, cy, R);
        nvgStrokeColor(vg, col);
        nvgStrokeWidth(vg, 0.9f);
        nvgStroke(vg);
    }
};

// Path tag strings (external linkage so they can be template args).
// One pair (Dark face, OffWhite face) per size.
namespace jackpaths {
    inline constexpr char StdDark[]    = "res/controls/RDM_JackDark_Std.svg";
    inline constexpr char StdLight[]   = "res/controls/RDM_JackOffWhite_Std.svg";
    inline constexpr char SmallDark[]  = "res/controls/RDM_JackDark_Small.svg";
    inline constexpr char SmallLight[] = "res/controls/RDM_JackOffWhite_Small.svg";
    inline constexpr char TinyDark[]   = "res/controls/RDM_JackDark_Tiny.svg";
    inline constexpr char TinyLight[]  = "res/controls/RDM_JackOffWhite_Tiny.svg";
}

using Jack_Std   = GeneratedJackT<jackpaths::StdDark,   jackpaths::StdLight>;
using Jack_Small = GeneratedJackT<jackpaths::SmallDark, jackpaths::SmallLight>;
using Jack_Tiny  = GeneratedJackT<jackpaths::TinyDark,  jackpaths::TinyLight>;

} // namespace redDot
