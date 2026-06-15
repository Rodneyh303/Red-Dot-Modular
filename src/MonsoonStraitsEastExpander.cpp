#include <rack.hpp>
#include "ui/OutputAccent.hpp"
#include "Monsoon.hpp"
#include "MonsoonStraitsEastExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace PolyVoiceExpanderIds;

struct MonsoonStraitsEastExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonStraitsEastExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    int lastThemeLight = -1;

    MonsoonStraitsEastExpanderWidget(MonsoonStraitsEastExpander* module)
    {
        setModule(module);
        const char* darkPath  = "res/panels/straits_east_peranakan_dark.svg";
        const char* lightPath = "res/panels/straits_east_peranakan_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        // Route through the kit's loadPanel so bindParam/Input/Output can resolve
        // the #components markers by id.
        loadPanel(asset::plugin(pluginInstance, darkPath));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Controls bound by id from the SVG kit (#components layer). East adds
        //    voices 2-8 = 7 rows; labels carry the row index, mapped to the
        //    absolute MonsoonIds/PolyVoiceExpanderIds enums here (single place).
        for (int i = 0; i < 7; i++) {
            std::string r = std::to_string(i);
            bindParam <Trimpot>      ("param_knob_"   + r, MonsoonIds::POLY_REST_PARAM_1      + i);
            bindParam <Trimpot>      ("param_att_"    + r, MonsoonIds::POLY_REST_MOD_ATT_1    + i);
            bindInput <DarkPJ301MPort>("input_modcv_" + r, MonsoonIds::POLY_REST_MOD_CV_INPUT_1 + i);
            bindOutput<DarkPJ301MPort>("output_gate_" + r, POLY_GATE_OUT_1   + i);
            bindOutput<DarkPJ301MPort>("output_cv_"   + r, POLY_CV_OUT_1     + i);
            bindOutput<DarkPJ301MPort>("output_acc_"  + r, POLY_ACCENT_OUT_1 + i);
        }
        // Global utility row
        bindInput <DarkPJ301MPort>("input_global_modcv", MonsoonIds::POLY_REST_CV_INPUT);
        bindOutput<PJ301MPort>    ("output_global_gate", POLY_GATE_1_8_OUT);
        bindOutput<PJ301MPort>    ("output_global_cv",   POLY_CV_1_8_OUT);
    }

    void step() override {
        ModuleWidget::step();
        kitStep();   // kit dev poll-reload
        if (!module) return;
        Monsoon* m = redDot::findMonsoonEitherSide(module);
        int wantLight = (m && m->lightTheme) ? 1 : 0;
        if (wantLight != lastThemeLight) {
            lastThemeLight = wantLight;
            // swap the kit-loaded SvgPanel's background
            for (Widget* child : children) {
                if (auto* sp = dynamic_cast<app::SvgPanel*>(child)) {
                    sp->setBackground(wantLight ? panelSvgLight : panelSvgDark);
                    break;
                }
            }
        }
    }

    void draw(const DrawArgs& args) override {
        // Panel art (incl. title) comes from the SVG now; just draw children.
        ModuleWidget::draw(args);
    }
};

Model* modelMonsoonStraitsEastExpander = createModel<MonsoonStraitsEastExpander, MonsoonStraitsEastExpanderWidget>("MonsoonStraitsEastExpander");