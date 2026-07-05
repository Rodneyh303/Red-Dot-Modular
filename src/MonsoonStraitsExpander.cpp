#include <rack.hpp>
#include <vector>
#include <tuple>
#include <cmath>
#include "Monsoon.hpp"
#include "MonsoonStraitsExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/ModArcOverlay.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace StraitsIds;

// Straits — base poly expander widget. Simplified from the old East widget:
// the per-voice REST + ACCENT probability knobs and three 16ch poly-cable output
// jacks. CV-mod inputs live on Causeway; per-voice out jacks on Changi. The
// per-voice REST and ACCENT knobs carry mod-arc overlays showing the effective
// (CV-modulated, via Causeway) value vs the set value — restored from the old East.
struct MonsoonStraitsExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonStraitsExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    // Per-voice mod-arc overlays, queued during binding and attached after. lane 0 = REST,
    // lane 1 = ACCENT. Each arc shows the effective (Causeway-modulated) value vs the set value.
    std::vector<std::tuple<rack::ParamWidget*, int, int>> pendingArcs;  // (knob, voice, lane)
    void queueArc(rack::ParamWidget* knob, int voice, int lane) {
        if (knob) pendingArcs.push_back({knob, voice, lane});
    }
    void flushArcs() {
        for (auto& pr : pendingArcs) {
            rack::ParamWidget* knob = std::get<0>(pr);
            int voice = std::get<1>(pr);
            int lane  = std::get<2>(pr);
            auto* self = this;
            auto* arc = new redDot::ModArcOverlay();
            arc->radius = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            arc->attachOverKnob(knob, mm2px(2.5f));
            arc->getSetNorm = [self, voice, lane]() -> float {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                if (!m || voice < 0 || voice >= 15) return 0.f;
                return lane == 0 ? m->cachedPolyRest[voice] : m->cachedPolyAccent[voice];
            };
            arc->getModNorm = [self, voice, lane]() -> float {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                if (!m || voice < 0 || voice >= 15) return 0.f;
                return lane == 0 ? m->cachedPolyRestEffective[voice] : m->cachedPolyAccentEffective[voice];
            };
            arc->isActive = [self, voice, lane]() -> bool {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                if (!m || !m->modVizEast || voice < 0 || voice >= 15) return false;
                float set = lane == 0 ? m->cachedPolyRest[voice] : m->cachedPolyAccent[voice];
                float eff = lane == 0 ? m->cachedPolyRestEffective[voice] : m->cachedPolyAccentEffective[voice];
                return std::fabs(eff - set) > 1e-4f;
            };
            addChild(arc);
        }
        pendingArcs.clear();
    }

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

        // 15 poly voices (voices 2..16): REST + ACCENT probability knobs, each with a mod-arc.
        for (int i = 0; i < 15; i++) {
            std::string r = std::to_string(i);
            int voice = i;
            bindParam<Trimpot>("param_rest_"   + r, MonsoonIds::POLY_REST_PARAM_1   + i,
                std::function<void(Trimpot*)>([this, voice](Trimpot* k){ queueArc(k, voice, 0); }));
            bindParam<Trimpot>("param_accent_" + r, MonsoonIds::POLY_ACCENT_PARAM_1 + i,
                std::function<void(Trimpot*)>([this, voice](Trimpot* k){ queueArc(k, voice, 1); }));
        }

        // Three 16-channel poly-cable outputs (ch1 = mono, ch2.. = poly).
        bindOutput<PJ301MPort>("output_polygate",   POLY_GATE_OUT);
        bindOutput<PJ301MPort>("output_polycv",     POLY_CV_OUT);
        bindOutput<PJ301MPort>("output_polyaccent", POLY_ACCENT_OUT);

        flushArcs();   // attach per-voice REST + ACCENT mod-arcs on top of the knobs

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
