#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonChangiExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
using namespace ChangiIds;

// Changi — per-voice output expander widget. 15 poly voices (2..16), each with
// gate / CV / accent individual jacks.
struct MonsoonChangiExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonChangiExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    MonsoonChangiExpanderWidget(MonsoonChangiExpander* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Changi_panel_dark.svg";
        const char* lightPath = "res/panels/Changi_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        for (int i = 0; i < 15; ++i) {
            std::string r = std::to_string(i);
            bindOutput<PJ301MPort>("output_gate_"   + r, GATE_OUT_0   + i);
            bindOutput<PJ301MPort>("output_cv_"     + r, CV_OUT_0     + i);
            bindOutput<PJ301MPort>("output_accent_" + r, ACCENT_OUT_0 + i);
        }

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

Model* modelMonsoonChangiExpander =
    createModel<MonsoonChangiExpander, MonsoonChangiExpanderWidget>("Changi");
