// src/DmcurveDevHost.cpp — the dev-only DistributionCurveWidget host (see the .hpp).
#include "DmcurveDevHost.hpp"
#include "ui/DistributionCurveWidget.hpp"

using namespace rack;
extern Plugin* pluginInstance;   // defined in plugin.cpp (Monsoon.hpp holds the family externs)

DmcurveDevHostWidget::DmcurveDevHostWidget(DmcurveDevHost* module) {
    setModule(module);
    panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/DmcurveDevHost_panel_dark.svg"));
    panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/DmcurveDevHost_panel_light.svg"));
    loadPanel(asset::plugin(pluginInstance, "res/panels/DmcurveDevHost_panel_dark.svg"));

    // The curve display binds to the panel's named rect — the SVG marker is the single
    // source of geometry truth (the house kit pattern; boundsOf gives position AND size).
    auto* m = module;
    curveWidget = bindWidget<dotModular::DistributionCurveWidget>("curve_display",
        [this, m](dotModular::DistributionCurveWidget* w) {
            if (auto* s = findNamed("curve_display"))
                w->box.size = boundsOf(s).size;
            if (m) {
                w->dist = m->current();
                w->editCurve = (m->which == DmcurveDevHost::DRAWN) ? &m->pchip.curve : nullptr;
            }
            w->isLight = [this] { return lastThemeLight == 1; };
        });
}

void DmcurveDevHostWidget::step() {
    ModuleWidget::step();
    // Theme: the dev host is STANDALONE — there is no host Monsoon whose lightTheme flag
    // could drive the plugin-wide expander convention, so it follows Rack's global panel
    // preference (the natural standalone default). Phase 3 integration binds the real
    // host's flag; the widget only needs the isLight callback either way.
    const int wantLight = settings::preferDarkPanels ? 0 : 1;
    if (wantLight != lastThemeLight) {
        lastThemeLight = wantLight;
        if (kit_.panel)
            kit_.panel->setBackground(wantLight ? panelSvgLight : panelSvgDark);
    }
    // Keep the widget's bindings live — the context menu can switch distributions at any
    // time (parametrics flip dist; the drawn case also arms editCurve).
    if (curveWidget) {
        if (auto* m = dynamic_cast<DmcurveDevHost*>(module)) {
            curveWidget->dist = m->current();
            curveWidget->editCurve = (m->which == DmcurveDevHost::DRAWN) ? &m->pchip.curve : nullptr;
        }
    }
}

void DmcurveDevHostWidget::appendContextMenu(ui::Menu* menu) {
    auto* m = dynamic_cast<DmcurveDevHost*>(module);
    if (!m) return;
    menu->addChild(createMenuLabel(".dmcurve dev host"));
    menu->addChild(createMenuLabel("Distribution"));
    static const char* names[DmcurveDevHost::N_WHICH] = {
        "Kumaraswamy (2,3)",
        "Arcsine",
        "Triangular (mode 0.3)",
        "Drawn (PCHIP)",
    };
    for (int i = 0; i < DmcurveDevHost::N_WHICH; ++i) {
        const bool editable = (i == DmcurveDevHost::DRAWN);
        menu->addChild(createMenuItem(names[i],
            editable ? (m->which == i ? "editable " CHECKMARK_STRING : "editable") : CHECKMARK(m->which == i),
            [m, i] { m->which = i; }));
    }
    menu->addChild(createMenuItem("Reset drawn curve", "", [m] { m->starterS(); }));
}

Model* modelDmcurveDevHost = createModel<DmcurveDevHost, DmcurveDevHostWidget>("DmcurveDevHost");
