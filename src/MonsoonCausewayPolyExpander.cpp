#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonCausewayPolyExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
using namespace MonsoonIds;

// Causeway — poly CV modulation expander widget. Two 16ch poly CV input jacks
// (rest, accent) + two attenuverters. Simplified from the old East/West per-voice
// mod jacks: a poly cable addresses voices per-channel.
struct MonsoonCausewayPolyExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonCausewayPolyExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    MonsoonCausewayPolyExpanderWidget(MonsoonCausewayPolyExpander* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Causeway_panel_dark.svg";
        const char* lightPath = "res/panels/Causeway_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // voice 0 = mono/voice-1 → the MONO attenuators; markers 1..15 = poly voices 2..16.
        bindParam<Trimpot>("param_restatt_0", MonsoonIds::MONO_REST_MOD_ATT);
        bindParam<Trimpot>("param_accatt_0",  MonsoonIds::MONO_ACCENT_MOD_ATT);
        for (int i = 1; i < 16; ++i) {
            std::string r = std::to_string(i);
            bindParam<Trimpot>("param_restatt_" + r, MonsoonIds::POLY_REST_MOD_ATT_1   + (i - 1));
            bindParam<Trimpot>("param_accatt_"  + r, MonsoonIds::POLY_ACCENT_MOD_ATT_1 + (i - 1));
        }
        bindParam<Trimpot>   ("param_restatt_global", MonsoonIds::POLY_REST_MOD_ATT_GLOBAL);
        bindParam<Trimpot>   ("param_accatt_global",  MonsoonIds::POLY_ACCENT_MOD_ATT_GLOBAL);
        bindInput<PJ301MPort>("input_restcv",   MonsoonIds::POLY_REST_CV_INPUT);
        bindInput<PJ301MPort>("input_accentcv", MonsoonIds::POLY_ACCENT_CV_INPUT);

        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            addChild(connectMark);
        }
    }

    void step() override {
        ModuleWidget::step();
        kitStep();
        if (!module) return;
        Monsoon* m = redDot::findMonsoonEitherSide(module);
        int wantLight = (m && m->lightTheme) ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }
    }
};

// Slug "Causeway" — the revived name (the old dice-gen Causeway was renamed to Raffles;
// this poly-CV module takes the freed name per MODULE_NAMING_AND_ROADMAP.md).
Model* modelMonsoonCausewayPolyExpander =
    createModel<MonsoonCausewayPolyExpander, MonsoonCausewayPolyExpanderWidget>("Causeway");
