// Compile test for the VARIADIC composable-mixin SvgPanelKit (experiment).
// Defines a Compose<>-based widget for the existing Surge module, exercising the
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
#include "MonsoonSurgeExpander.hpp"
#include "Monsoon.hpp"
#include "ui/SvgPanelKit.hpp"

using namespace rack;
using namespace dotModular;

struct SurgeKitTestWidget
    : ModuleWidget,
      Compose<SurgeKitTestWidget, ShapeQuery, Bind, Reload> {

    SurgeKitTestWidget(MonsoonSurgeExpander* module) {
        setModule(module);
        loadPanel(asset::plugin(pluginInstance, "res/panels/Surge_panel_dark.svg"));

        // bind 5 CV inputs + 5 attenuverters by name
        const char* lanes[5] = { "NOTEVAL", "VARIATION", "LEGATO", "REST", "ACCENT" };
        const int cvId[5]  = { MonsoonIds::SURGE_NOTEVAL_CV, MonsoonIds::SURGE_VARIATION_CV,
            MonsoonIds::SURGE_LEGATO_CV, MonsoonIds::SURGE_REST_CV, MonsoonIds::SURGE_ACCENT_CV };
        const int attId[5] = { MonsoonIds::SURGE_NOTEVAL_ATT, MonsoonIds::SURGE_VARIATION_ATT,
            MonsoonIds::SURGE_LEGATO_ATT, MonsoonIds::SURGE_REST_ATT, MonsoonIds::SURGE_ACCENT_ATT };
        for (int r = 0; r < 5; ++r) {
            bindInput<PJ301MPort>(std::string("input_SURGE_") + lanes[r] + "_CV", cvId[r]);
            bindParam<Trimpot>(   std::string("param_SURGE_") + lanes[r] + "_ATT", attId[r]);
        }

        // exercise the variadic pack bind too (binds nothing real here, just
        // proves it instantiates under c++11 — names won't all resolve, which is
        // fine; the point is the template compiles)
        // bindParams<Trimpot>("param_SURGE_", attId[0], attId[1]);  // (left commented: ids not prefixed _0/_1)

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
