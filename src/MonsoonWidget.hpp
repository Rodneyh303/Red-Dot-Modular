#pragma once

#include <rack.hpp>
#include "ui/SvgPanelKit.hpp"

using namespace rack;

struct Monsoon;

struct RDM_KnobLarge : SvgKnob { RDM_KnobLarge(); };
struct RDM_KnobMedium : SvgKnob { RDM_KnobMedium(); };
struct RDM_KnobSmall : SvgKnob { RDM_KnobSmall(); };
struct RDM_KnobDarkLarge : SvgKnob { RDM_KnobDarkLarge(); };
struct RDM_KnobDarkMedium : SvgKnob { RDM_KnobDarkMedium(); };
struct RDM_KnobCreamLarge : SvgKnob { RDM_KnobCreamLarge(); };
struct RDM_KnobCreamMedium : SvgKnob { RDM_KnobCreamMedium(); };

struct MonsoonWidget : ModuleWidget, dotModular::Compose<MonsoonWidget, dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    MonsoonWidget(Monsoon* module);
    // Mod-arc overlays are collected during knob binding and attached AFTER all
    // knobs are added (so they draw on top). Each entry: the knob + a getter for
    // its normalised MODULATED value + an active flag. Plain std::function so the
    // header needn't see Monsoon::ModViz (Monsoon is only forward-declared here).
    struct PendingModArc {
        rack::ParamWidget* knob = nullptr;
        bool linear = false;   // false = radial knob arc; true = vertical-slider tick
        float radiusFrac = 0.5f; // arc radius as a fraction of the knob box half-extent.
                                 // 0.5 matches knobs whose art fills the box (e.g. Trimpot);
                                 // smaller for knobs whose visible art is inset within a
                                 // larger SVG box (e.g. RDM_KnobSmall: art r=4 in r=8 box).
        std::function<float()> getModNorm;
        std::function<bool()>  isActive;
    };
    std::vector<PendingModArc> pendingModArcs;

    bool getLightTheme() const;
    void setLightTheme(bool v);

    void applyTheme();
<<<<<<< HEAD
    void draw(const DrawArgs& args) override;
    void appendContextMenu(ui::Menu* menu) override;

    static constexpr float W_MM = 172.72f; // widht of module in mm, used for centering text and other elements`
=======
    void draw(const DrawArgs& args) override; // Keep draw, but it will no longer call drawPeranakanLattice directly
    void appendContextMenu(ui::Menu* menu) override;
    void step() override;  // forwards to kitStep() for dev live-reload

    static constexpr float W_MM = 203.2f; // Updated to 40HP (172.72 + 30.48)
>>>>>>> 091ed97df88f5f836c12b99b805c203028fdcdf8
    static constexpr float SL_TOP = 45.f; //top of slider
    static constexpr float SLH = 29.5f; //slider hieght, used for label positioning

    static constexpr float EXP_LIGHT_X = 4.f;
    static constexpr float EXP_LIGHT_Y = 4.f;
    static constexpr float EXP_LIGHT_S = 5.f;
};

extern Model* modelMonsoon;