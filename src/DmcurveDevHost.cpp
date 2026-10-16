// src/DmcurveDevHost.cpp — the dev-only DistributionCurveWidget host (see the .hpp).
#include "DmcurveDevHost.hpp"
#include "ui/DistributionCurveWidget.hpp"
#include "ui/Controls.hpp"

#include <cmath>
#include <functional>

using namespace rack;
extern Plugin* pluginInstance;   // defined in plugin.cpp (Monsoon.hpp holds the family externs)

// ── sidebar widgets (dev-grade nvg, the house palette — no extra SVG assets) ──────────────────
namespace {

const char* const kWhichNames[DmcurveDevHost::N_WHICH] = {
    "Kumaraswamy (2,3)", "Arcsine", "Triangular (0.3)", "Drawn (PCHIP)",
};

// Live name + mode readout under the distribution knob (gold when the drawn case is live).
struct DistributionLabel : rack::widget::Widget {
    DmcurveDevHost* m = nullptr;
    std::function<bool()> isLight;
    void draw(const rack::widget::Widget::DrawArgs& args) override {
        if (!m) return;
        NVGcontext* vg = args.vg;
        const bool light = isLight && isLight();
        const int w = m->which;
        nvgFontSize(vg, 9.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, (w == DmcurveDevHost::DRAWN) ? nvgRGB(0xd4, 0xaf, 0x37)
                                                      : (light ? nvgRGB(0x5a, 0x5f, 0x66)
                                                               : nvgRGB(0x8a, 0x8f, 0x98)));
        nvgText(vg, box.size.x * 0.5f, box.size.y * 0.35f, kWhichNames[w], nullptr);
        nvgFontSize(vg, 7.f);
        nvgFillColor(vg, light ? nvgRGB(0x70, 0x75, 0x7d) : nvgRGB(0x6a, 0x6e, 0x76));
        const char* mode = (w == DmcurveDevHost::DRAWN)
            ? (m->editEnabled ? "editable" : "view (edit off)")
            : "display only";
        nvgText(vg, box.size.x * 0.5f, box.size.y * 0.72f, mode, nullptr);
    }
};

// A small dev button (EDIT HANDLES / RESET CURVE). Gold label when its activeWhen is true.
struct SidebarButton : rack::widget::Widget {
    std::function<bool()> isLight;
    std::function<bool()> activeWhen;
    std::function<void()> onPress;
    const char* label = "";
    void draw(const rack::widget::Widget::DrawArgs& args) override {
        NVGcontext* vg = args.vg;
        const bool light = isLight && isLight();
        const bool active = activeWhen && activeWhen();
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.5f, 0.5f, box.size.x - 1.f, box.size.y - 1.f, 3.f);
        nvgFillColor(vg, light ? nvgRGB(0xd0, 0xd2, 0xd6) : nvgRGB(0x13, 0x15, 0x1a));
        nvgFill(vg);
        nvgStrokeColor(vg, light ? nvgRGB(0xb0, 0xa8, 0x98) : nvgRGB(0x2a, 0x2f, 0x37));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
        nvgFontSize(vg, 8.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, active ? nvgRGB(0xd4, 0xaf, 0x37)
                                : (light ? nvgRGB(0x5a, 0x5f, 0x66) : nvgRGB(0x8a, 0x8f, 0x98)));
        nvgText(vg, box.size.x * 0.5f, box.size.y * 0.5f, label, nullptr);
    }
    void onButton(const rack::event::Button& e) override {
        if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && onPress) {
            onPress();
            e.consume(this);
            return;
        }
        rack::widget::Widget::onButton(e);   // right-click falls through to the context menu
    }
};

} // namespace

DmcurveDevHostWidget::DmcurveDevHostWidget(DmcurveDevHost* module) {
    setModule(module);
    panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/DmcurveDevHost_panel_dark.svg"));
    panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/DmcurveDevHost_panel_light.svg"));
    loadPanel(asset::plugin(pluginInstance, "res/panels/DmcurveDevHost_panel_dark.svg"));

    auto* m = module;
    const auto themeCb = [this] { return lastThemeLight == 1; };

    // The square diagram — bound to the panel's named rect (the SVG marker is the single
    // source of geometry truth; boundsOf gives position AND size).
    curveWidget = bindWidget<dotModular::DistributionCurveWidget>("curve_display",
        [this, m, themeCb](dotModular::DistributionCurveWidget* w) {
            if (auto* s = findNamed("curve_display"))
                w->box.size = boundsOf(s).size;
            if (m) {
                w->dist = m->current();
                w->editCurve = (m->which == DmcurveDevHost::DRAWN && m->editEnabled) ? &m->pchip.curve : nullptr;
            }
            w->isLight = themeCb;
        });

    // ── the control sidebar ─────────────────────────────────────────────────────────────
    // Distribution selector: a themed house knob over the param (the single source of truth
    // for `which`; quantized in step()).
    bindParam<redDot::Themed_Small_Cog>("param_distribution", DmcurveDevHost::DISTRIBUTION_PARAM,
        [themeCb](redDot::Themed_Small_Cog* w) { w->lightWhen = themeCb; });
    bindWidget<DistributionLabel>("label_distribution",
        [m, themeCb](DistributionLabel* w) {
            w->box.size = rack::math::Vec(104.f, 26.f);
            w->m = m;
            w->isLight = themeCb;
        });
    bindWidget<SidebarButton>("btn_edit",
        [m, themeCb](SidebarButton* w) {
            w->box.size = rack::math::Vec(90.f, 22.f);
            w->label = "EDIT HANDLES";
            w->isLight = themeCb;
            w->activeWhen = [m] { return m && m->editEnabled; };
            w->onPress = [m] { if (m) m->editEnabled = !m->editEnabled; };
        });
    bindWidget<SidebarButton>("btn_reset",
        [m, themeCb](SidebarButton* w) {
            w->box.size = rack::math::Vec(90.f, 22.f);
            w->label = "RESET CURVE";
            w->isLight = themeCb;
            w->onPress = [m] { if (m) m->starterS(); };
        });
}

void DmcurveDevHostWidget::step() {
    ModuleWidget::step();
    // Theme: the dev host is STANDALONE — there is no host Monsoon whose lightTheme flag
    // could drive the plugin-wide expander convention, so it follows Rack's global panel
    // preference (the natural standalone default). Phase 3 integration binds the real
    // host's flag; the widgets only need the callback either way.
    const int wantLight = settings::preferDarkPanels ? 0 : 1;
    if (wantLight != lastThemeLight) {
        lastThemeLight = wantLight;
        if (kit_.panel)
            kit_.panel->setBackground(wantLight ? panelSvgLight : panelSvgDark);
    }
    if (auto* m = dynamic_cast<DmcurveDevHost*>(module)) {
        // The param is the single source of truth for `which` (knob + context menu both
        // write it; Rack auto-persists it). Quantize on the UI thread.
        if (m->params.size() > (size_t)DmcurveDevHost::DISTRIBUTION_PARAM) {
            int w = (int)std::lround(m->params[DmcurveDevHost::DISTRIBUTION_PARAM].getValue());
            if (w < 0) w = 0;
            if (w >= DmcurveDevHost::N_WHICH) w = DmcurveDevHost::N_WHICH - 1;
            m->which = w;
        }
        // Keep the curve widget's bindings live (selector / edit toggle / reset all change
        // them at runtime).
        if (curveWidget) {
            curveWidget->dist = m->current();
            curveWidget->editCurve = (m->which == DmcurveDevHost::DRAWN && m->editEnabled)
                ? &m->pchip.curve : nullptr;
        }
    }
}

void DmcurveDevHostWidget::appendContextMenu(ui::Menu* menu) {
    auto* m = dynamic_cast<DmcurveDevHost*>(module);
    if (!m) return;
    menu->addChild(createMenuLabel(".dmcurve dev host"));
    menu->addChild(createMenuLabel("Distribution"));
    for (int i = 0; i < DmcurveDevHost::N_WHICH; ++i) {
        const bool editable = (i == DmcurveDevHost::DRAWN);
        menu->addChild(createMenuItem(kWhichNames[i],
            editable ? (m->which == i ? "editable " CHECKMARK_STRING : "editable") : CHECKMARK(m->which == i),
            [m, i] { m->params[DmcurveDevHost::DISTRIBUTION_PARAM].setValue((float)i); }));
    }
    menu->addChild(createMenuItem("Edit handles", CHECKMARK(m->editEnabled),
        [m] { m->editEnabled = !m->editEnabled; }));
    menu->addChild(createMenuItem("Reset drawn curve", "", [m] { m->starterS(); }));
}

Model* modelDmcurveDevHost = createModel<DmcurveDevHost, DmcurveDevHostWidget>("DmcurveDevHost");
