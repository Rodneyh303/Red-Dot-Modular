#pragma once

#include <rack.hpp>

using namespace rack;

struct MeloDicer;

struct RDM_KnobLarge : SvgKnob { RDM_KnobLarge(); };
struct RDM_KnobMedium : SvgKnob { RDM_KnobMedium(); };
struct RDM_KnobSmall : SvgKnob { RDM_KnobSmall(); };
struct RDM_KnobDarkLarge : SvgKnob { RDM_KnobDarkLarge(); };
struct RDM_KnobDarkMedium : SvgKnob { RDM_KnobDarkMedium(); };
struct RDM_KnobCreamLarge : SvgKnob { RDM_KnobCreamLarge(); };
struct RDM_KnobCreamMedium : SvgKnob { RDM_KnobCreamMedium(); };

struct MeloDicerWidget : ModuleWidget {
    MeloDicerWidget(MeloDicer* module);

    bool getLightTheme() const;
    void setLightTheme(bool v);

    void applyTheme();
    void draw(const DrawArgs& args) override;
    void appendContextMenu(ui::Menu* menu) override;

    static constexpr float W_MM = 172.72f;
    static constexpr float SL_TOP = 45.f;
    static constexpr float SLH = 29.5f;
};

extern Model* modelMeloDicer;