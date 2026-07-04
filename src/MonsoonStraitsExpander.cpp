#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitsExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace StraitsIds;

// Straits — base poly expander widget. Simplified from the old East widget:
// just the per-voice REST + ACCENT probability knobs and three 16ch poly-cable
// output jacks. No CV-mod inputs (→ Causeway), no per-voice out jacks (→ Changi),
// no mod-arc overlays.
struct MonsoonStraitsExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonStraitsExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    MonsoonStraitsExpanderWidget(MonsoonStraitsExpander* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/Straits_panel_dark.svg";
        const char* lightPath = "res/panels/Straits_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // 15 poly voices (voices 2..16): REST + ACCENT probability knobs.
        for (int i = 0; i < 15; i++) {
            std::string r = std::to_string(i);
            bindParam<Trimpot>("param_rest_"   + r, MonsoonIds::POLY_REST_PARAM_1   + i);
            bindParam<Trimpot>("param_accent_" + r, MonsoonIds::POLY_ACCENT_PARAM_1 + i);
        }

        // Three 16-channel poly-cable outputs (ch1 = mono, ch2.. = poly).
        bindOutput<PJ301MPort>("output_polygate",   POLY_GATE_OUT);
        bindOutput<PJ301MPort>("output_polycv",     POLY_CV_OUT);
        bindOutput<PJ301MPort>("output_polyaccent", POLY_ACCENT_OUT);

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

Model* modelMonsoonStraitsExpander =
    createModel<MonsoonStraitsExpander, MonsoonStraitsExpanderWidget>("Straits");
