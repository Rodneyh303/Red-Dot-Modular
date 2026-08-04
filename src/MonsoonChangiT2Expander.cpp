#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonChangiT2Expander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
using namespace ChangiT2Ids;

// Changi T2 — per-voice ARTICULATION output expander widget. 16 voices (index 0 =
// mono/voice 1, 1..15 = poly voices 2..16), each with STEP GATE / STEP LEGATO jacks.
struct MonsoonChangiT2ExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonChangiT2ExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    MonsoonChangiT2ExpanderWidget(MonsoonChangiT2Expander* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/ChangiT2_panel_dark.svg";
        const char* lightPath = "res/panels/ChangiT2_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        for (int i = 0; i < 16; ++i) {   // 0 = mono/voice-1, 1..15 = poly voices 2..16
            std::string r = std::to_string(i);
            bindOutput<PJ301MPort>("output_stepgate_"   + r, STEP_GATE_OUT_0   + i);
            bindOutput<PJ301MPort>("output_steplegato_" + r, STEP_LEGATO_OUT_0 + i);
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

Model* modelMonsoonChangiT2Expander =
    createModel<MonsoonChangiT2Expander, MonsoonChangiT2ExpanderWidget>("ChangiT2");
