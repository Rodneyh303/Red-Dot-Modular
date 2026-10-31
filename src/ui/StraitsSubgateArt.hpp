#pragma once
// ── StraitsSubgateArt — the subgate surfaces' shared lettering/tint helpers ──
// CC_PROMPT_FIX_SUBGATE_PANELS: the subgate family's widget-layer bits live here
// ONCE (the "one mechanism, not twins" discipline) — the Cantonment lettering
// idiom (nanosvg drops <text>, the widget draws the captions) and the level
// tints (ONE authority with the generators: gen_straits_subgate_base.py TINTS +
// gen_straits_lane.py's subgate keys #c05850 / #5878b8).
#include <rack.hpp>
#include <functional>
#include <string>

#include "../dsp/SubgateKnobs.hpp"   // the level ids (RATCHET/GHOST) — the tint authority pairs with it

namespace redDot {

// The Cantonment lettering idiom: mm-anchored, theme-aware via colourFn.
struct SubgateCaption : rack::widget::Widget {
    std::string text;
    float sizePx = 7.f;
    std::function<NVGcolor()> colourFn;
    void draw(const DrawArgs& args) override {
        auto font = APP->window->loadFont(rack::asset::system("res/fonts/DejaVuSans-Bold.ttf"));
        if (!font) font = APP->window->uiFont;
        if (!font) return;
        nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, colourFn ? colourFn() : nvgRGB(0xc8, 0xcd, 0xd4));
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFontSize(args.vg, sizePx);
        nvgText(args.vg, 0, 0, text.c_str(), nullptr);
    }
};

inline void addSubgateCaption(rack::widget::Widget* parent, const std::string& text,
                              float xmm, float ymm, float sizePx,
                              std::function<NVGcolor()> colourFn) {
    auto* c = rack::createWidget<SubgateCaption>(rack::Vec(0, 0));
    c->text = text;
    c->sizePx = sizePx;
    c->colourFn = std::move(colourFn);
    c->box.pos = rack::mm2px(rack::Vec(xmm, ymm));
    parent->addChild(c);
}

// The level tints — ONE authority with the generators.
inline NVGcolor subgateTint(int level) {
    return level == dotModular::SubgateKnobs::GHOST ? nvgRGB(0x58, 0x78, 0xb8)
                                                    : nvgRGB(0xc0, 0x58, 0x50);
}

} // namespace redDot
