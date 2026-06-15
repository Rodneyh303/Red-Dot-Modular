#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitWestExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace StraitWestExpanderIds;

struct MonsoonStraitWestExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonStraitWestExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;
    MonsoonStraitWestExpanderWidget(MonsoonStraitWestExpander* module)
    {
        setModule(module);
        const char* darkPath  = "res/panels/straits_west_peranakan_dark.svg";
        const char* lightPath = "res/panels/straits_west_peranakan_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Controls bound by id from the SVG kit. West adds voices 9-16 = 8 rows;
        // id bases are the *_8 entries (voice 9 = index 8). Labels carry the row.
        for (int i = 0; i < 8; i++) {
            std::string r = std::to_string(i);
            bindParam <Trimpot>      ("param_knob_"   + r, MonsoonIds::POLY_REST_PARAM_8      + i);
            bindParam <Trimpot>      ("param_att_"    + r, MonsoonIds::POLY_REST_MOD_ATT_8    + i);
            bindInput <DarkPJ301MPort>("input_modcv_" + r, MonsoonIds::POLY_REST_MOD_CV_INPUT_8 + i);
            bindOutput<DarkPJ301MPort>("output_gate_" + r, POLY_GATE_OUT_1   + i);
            bindOutput<DarkPJ301MPort>("output_cv_"   + r, POLY_CV_OUT_1     + i);
            bindOutput<DarkPJ301MPort>("output_acc_"  + r, POLY_ACCENT_OUT_1 + i);
        }
        // Global utility row (West: CV input sits in the knob column)
        bindInput <DarkPJ301MPort>("input_global_cv_in", MonsoonIds::POLY_REST_CV_INPUT);
        bindOutput<PJ301MPort>    ("output_global_gate", POLY_GATE_1_16_OUT);
        bindOutput<PJ301MPort>    ("output_global_cv",   POLY_CV_1_16_OUT);
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

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
    }
};

Model* modelMonsoonStraitWestExpander = createModel<MonsoonStraitWestExpander, MonsoonStraitWestExpanderWidget>("MonsoonStraitWestExpander");
