// src/DmcurveDevHost.cpp — the dev-only DistributionCurveWidget host (see the .hpp).
#include "DmcurveDevHost.hpp"
#include "ui/DistributionCurveWidget.hpp"
#include "ui/Controls.hpp"

#include <cmath>
#include <cstdio>
#include <functional>

using namespace rack;
extern Plugin* pluginInstance;   // defined in plugin.cpp (Monsoon.hpp holds the family externs)

// ── sidebar widgets (dev-grade nvg, the house palette — no extra SVG assets) ──────────────────
namespace {

const char* const kWhichNames[DmcurveDevHost::N_WHICH] = {
    "Kumaraswamy", "Arcsine", "Triangular", "Power",
    "Logit-normal", "Trunc-normal", "Drawn (PCHIP)",
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

// Live param readout beside a generic knob: "a: 2.31" (the symbol + range follow the
// selected distribution; an inert slot shows a dim "none" — arcsine has no params, the
// single-param distributions leave knob 2 unused, and the drawn case is edited by drag).
struct ParamLabel : rack::widget::Widget {
    DmcurveDevHost* m = nullptr;
    int slot = 0;
    std::function<bool()> isLight;
    void draw(const rack::widget::Widget::DrawArgs& args) override {
        if (!m) return;
        NVGcontext* vg = args.vg;
        const bool light = isLight && isLight();
        const auto ps = DmcurveDevHost::paramSlot(m->which, slot);
        char buf[32];
        if (ps.used) {
            const double v = m->paramValue(slot);
            std::snprintf(buf, sizeof buf, "%s: %s%.2f", ps.sym, (v >= 0.0 && ps.lo < 0.0) ? "+" : "", v);
        } else {
            std::snprintf(buf, sizeof buf, "none");
        }
        nvgFontSize(vg, 8.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, ps.used ? (light ? nvgRGB(0x5a, 0x5f, 0x66) : nvgRGB(0x8a, 0x8f, 0x98))
                                 : (light ? nvgRGB(0xa8, 0xac, 0xb2) : nvgRGB(0x4a, 0x4e, 0x55)));
        nvgText(vg, box.size.x * 0.5f, box.size.y * 0.5f, buf, nullptr);
    }
};

// A small dev button (EDIT / RESET / CONVERT). Gold label when its activeWhen is true.
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
        nvgFontSize(vg, 7.5f);
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
    // Distribution selector + the two GENERIC param knobs (normalized [0,1]; the meaning,
    // range and label follow the selection — see DmcurveDevHost::paramSlot). All themed
    // house knobs.
    bindParam<redDot::Themed_Small_Cog>("param_distribution", DmcurveDevHost::DISTRIBUTION_PARAM,
        [themeCb](redDot::Themed_Small_Cog* w) { w->lightWhen = themeCb; });
    bindWidget<DistributionLabel>("label_distribution",
        [m, themeCb](DistributionLabel* w) {
            w->box.size = rack::math::Vec(104.f, 26.f);
            w->m = m;
            w->isLight = themeCb;
        });
    // The param knobs are Dimmable: when their slot is INERT for the selected distribution
    // (arcsine's two, the single-param distributions' knob 2, or both on the drawn case)
    // they take the grey scrim AND lock — turning does nothing, so inert can't read as a
    // bug. The predicate is live (reads m->which each frame, like every other binding).
    bindParam<redDot::Themed_Small_Cog_Dim>("param_p1", DmcurveDevHost::PARAM1_PARAM,
        [themeCb, m](redDot::Themed_Small_Cog_Dim* w) {
            w->lightWhen = themeCb;
            const auto inert = [m] { return !m || !DmcurveDevHost::paramSlot(m->which, 0).used; };
            w->dimWhen = inert;
            w->lockWhen = inert;
        });
    bindWidget<ParamLabel>("label_p1",
        [m, themeCb](ParamLabel* w) {
            w->box.size = rack::math::Vec(104.f, 12.f);
            w->m = m;
            w->slot = 0;
            w->isLight = themeCb;
        });
    bindParam<redDot::Themed_Small_Cog_Dim>("param_p2", DmcurveDevHost::PARAM2_PARAM,
        [themeCb, m](redDot::Themed_Small_Cog_Dim* w) {
            w->lightWhen = themeCb;
            const auto inert = [m] { return !m || !DmcurveDevHost::paramSlot(m->which, 1).used; };
            w->dimWhen = inert;
            w->lockWhen = inert;
        });
    bindWidget<ParamLabel>("label_p2",
        [m, themeCb](ParamLabel* w) {
            w->box.size = rack::math::Vec(104.f, 12.f);
            w->m = m;
            w->slot = 1;
            w->isLight = themeCb;
        });
    bindWidget<SidebarButton>("btn_edit",
        [m, themeCb](SidebarButton* w) {
            w->box.size = rack::math::Vec(48.f, 20.f);
            w->label = "EDIT";
            w->isLight = themeCb;
            w->activeWhen = [m] { return m && m->editEnabled; };
            w->onPress = [m] { if (m) m->editEnabled = !m->editEnabled; };
        });
    bindWidget<SidebarButton>("btn_reset",
        [m, themeCb](SidebarButton* w) {
            w->box.size = rack::math::Vec(48.f, 20.f);
            w->label = "RESET";
            w->isLight = themeCb;
            w->onPress = [m] { if (m) m->starterS(); };
        });
    // CONVERT → EDITABLE: the start-from-a-shape workflow. Sample the CURRENT (parametric)
    // distribution into a nearby 9-point PCHIP, then switch to the drawn case with handles
    // live. (No-op when already drawn.)
    bindWidget<SidebarButton>("btn_convert",
        [m, themeCb](SidebarButton* w) {
            w->box.size = rack::math::Vec(104.f, 20.f);
            w->label = "TO EDITABLE";
            w->isLight = themeCb;
            w->activeWhen = [m] { return m && m->which != DmcurveDevHost::DRAWN; };
            w->onPress = [m] {
                if (!m || m->which == DmcurveDevHost::DRAWN) return;
                m->pchip.curve = dotModular::MonotoneCurve::fromDistribution(*m->current(), 9);
                m->editEnabled = true;
                m->params[DmcurveDevHost::DISTRIBUTION_PARAM].setValue((float)DmcurveDevHost::DRAWN);
            };
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
        // The distribution param is the single source of truth for `which` (knob + context
        // menu both write it; Rack auto-persists it). Quantize on the UI thread.
        if (m->params.size() > (size_t)DmcurveDevHost::DISTRIBUTION_PARAM) {
            int w = (int)std::lround(m->params[DmcurveDevHost::DISTRIBUTION_PARAM].getValue());
            if (w < 0) w = 0;
            if (w >= DmcurveDevHost::N_WHICH) w = DmcurveDevHost::N_WHICH - 1;
            m->which = w;
        }
        // The generic param knobs: normalized [0,1] mapped through paramSlot(). On a
        // distribution CHANGE they back-fill from the new distribution's values; otherwise
        // they drive the live param (the curve + PDF + auto-max reshaping in real time).
        if (m->which != lastWhich) {
            lastWhich = m->which;
            for (int slot = 0; slot < 2; ++slot) {
                const auto ps = DmcurveDevHost::paramSlot(m->which, slot);
                const int id = (slot == 0) ? DmcurveDevHost::PARAM1_PARAM : DmcurveDevHost::PARAM2_PARAM;
                if (ps.used && ps.hi > ps.lo && m->params.size() > (size_t)id) {
                    double norm = (m->paramValue(slot) - ps.lo) / (ps.hi - ps.lo);
                    if (norm < 0.0) norm = 0.0;
                    if (norm > 1.0) norm = 1.0;
                    m->params[id].setValue((float)norm);
                }
            }
        } else {
            for (int slot = 0; slot < 2; ++slot) {
                const auto ps = DmcurveDevHost::paramSlot(m->which, slot);
                const int id = (slot == 0) ? DmcurveDevHost::PARAM1_PARAM : DmcurveDevHost::PARAM2_PARAM;
                if (ps.used && m->params.size() > (size_t)id)
                    m->setParamValue(slot, ps.lo + (double)m->params[id].getValue() * (ps.hi - ps.lo));
            }
        }
        // Keep the curve widget's bindings live (selector / knobs / edit toggle / convert
        // all change them at runtime).
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
    menu->addChild(createMenuLabel("drag points - click: add - dbl-click: remove"));
    menu->addChild(createMenuLabel("Distribution"));
    for (int i = 0; i < DmcurveDevHost::N_WHICH; ++i) {
        const bool editable = (i == DmcurveDevHost::DRAWN);
        menu->addChild(createMenuItem(kWhichNames[i],
            editable ? (m->which == i ? "editable " CHECKMARK_STRING : "editable") : CHECKMARK(m->which == i),
            [m, i] { m->params[DmcurveDevHost::DISTRIBUTION_PARAM].setValue((float)i); }));
    }
    menu->addChild(createMenuItem("Edit handles", CHECKMARK(m->editEnabled),
        [m] { m->editEnabled = !m->editEnabled; }));
    menu->addChild(createMenuItem("Convert to editable (9-pt PCHIP)", "",
        [m] {
            if (m->which == DmcurveDevHost::DRAWN) return;
            m->pchip.curve = dotModular::MonotoneCurve::fromDistribution(*m->current(), 9);
            m->editEnabled = true;
            m->params[DmcurveDevHost::DISTRIBUTION_PARAM].setValue((float)DmcurveDevHost::DRAWN);
        }));
    menu->addChild(createMenuItem("Reset drawn curve", "", [m] { m->starterS(); }));
}

Model* modelDmcurveDevHost = createModel<DmcurveDevHost, DmcurveDevHostWidget>("DmcurveDevHost");
