// Compile test for the VARIADIC composable-mixin SvgPanelKit (experiment).
// Defines a Compose<>-based widget for the existing Junction module, exercising the
// full variadic machinery: Compose<T, ShapeQuery, Bind, Reload>, state access via
// T::kit_, bind-by-name, dev reload, step + context menu.
//
// This translation unit's job is to COMPILE — proving the variadic design is
// sound under Rack's -std=c++11. It is intentionally NOT registered as a separate
// plugin Model (no plugin.cpp churn); building the plugin compiles this file.
//
// If it fails to compile, that is the experiment's answer: the variadic /
// virtual-inheritance approach is too fragile to adopt, and the plain CRTP
// SvgHelper.hpp stays the choice.

#include <rack.hpp>
#include "MonsoonJunctionExpander.hpp"
#include "Monsoon.hpp"
#include "ui/SvgPanelKit.hpp"

using namespace rack;
using namespace dotModular;

struct JunctionKitTestWidget
    : ModuleWidget,
      Compose<JunctionKitTestWidget, ShapeQuery, Bind, Reload> {

    JunctionKitTestWidget(MonsoonJunctionExpander* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/panels/Junction_panel_dark.svg"));

        // bind 5 CV inputs + 5 attenuverters by name
        const char* lanes[5] = { "NOTEVAL", "VARIATION", "LEGATO", "REST", "ACCENT" };
        const int cvId[5]  = { MonsoonIds::JUNCTION_NOTEVAL_CV, MonsoonIds::JUNCTION_VARIATION_CV,
            MonsoonIds::JUNCTION_LEGATO_CV, MonsoonIds::JUNCTION_REST_CV, MonsoonIds::JUNCTION_ACCENT_CV };
        const int attId[5] = { MonsoonIds::JUNCTION_NOTEVAL_ATT, MonsoonIds::JUNCTION_VARIATION_ATT,
            MonsoonIds::JUNCTION_LEGATO_ATT, MonsoonIds::JUNCTION_REST_ATT, MonsoonIds::JUNCTION_ACCENT_ATT };
        for (int r = 0; r < 5; ++r) {
            bindInput<PJ301MPort>(std::string("input_JUNCTION_") + lanes[r] + "_CV", cvId[r]);
            bindParam<Trimpot>(   std::string("param_JUNCTION_") + lanes[r] + "_ATT", attId[r]);
        }

        // exercise the variadic pack bind too (binds nothing real here, just
        // proves it instantiates under c++11 — names won't all resolve, which is
        // fine; the point is the template compiles)
        // bindParams<Trimpot>("param_JUNCTION_", attId[0], attId[1]);  // (left commented: ids not prefixed _0/_1)

        setDevMode(true);
    }

    void step() override {
        ModuleWidget::step();
        kitStep();              // Reload mixin's poll
    }
    void appendContextMenu(Menu* menu) override {
        appendKitMenu(menu);    // Reload mixin's dev menu
    }
};
