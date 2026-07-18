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
#include "ui/Controls.hpp"

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
    // Cached once per frame in step(); the 32 knobs' lightWhen read THIS, not a
    // per-frame findMonsoonEitherSide() each (that would be 32 chain-walks/frame).
    bool themeLight_ = false;
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
            // voice == -1 → the MONO lane (voice 1): set = Monsoon's own mono base, mod = the
            // Causeway ch0-modulated effective value. Gives the Straits voice-1 knobs the same mod
            // arc that Monsoon's own rest/accent knobs show. voice 0..14 → poly voices 2..16.
            arc->getSetNorm = [self, voice, lane]() -> float {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                if (!m) return 0.f;
                if (voice == -1) return lane == 0 ? m->getMonoRestBase() : m->getMonoAccentBase();
                if (voice < 0 || voice >= 15) return 0.f;
                return lane == 0 ? m->getBasePolyRest(voice) : m->getBasePolyAccent(voice);
            };
            arc->getModNorm = [self, voice, lane]() -> float {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                if (!m) return 0.f;
                if (voice == -1) return lane == 0 ? m->getRestParam() : m->getAccentParam();
                if (voice < 0 || voice >= 15) return 0.f;
                return lane == 0 ? m->getEffectivePolyRest(voice) : m->getEffectivePolyAccent(voice);
            };
            arc->isActive = [self, voice, lane]() -> bool {
                Monsoon* m = redDot::findMonsoonEitherSide(self->module);
                if (!m || !m->modVizEast) return false;
                float set, eff;
                if (voice == -1) {
                    set = lane == 0 ? m->getMonoRestBase() : m->getMonoAccentBase();
                    eff = lane == 0 ? m->getRestParam()    : m->getAccentParam();
                } else {
                    if (voice < 0 || voice >= 15) return false;
                    set = lane == 0 ? m->getBasePolyRest(voice) : m->getBasePolyAccent(voice);
                    eff = lane == 0 ? m->getEffectivePolyRest(voice) : m->getEffectivePolyAccent(voice);
                }
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

        // 16 voices. New panel convention: marker 0 = mono/voice-1, markers 1..15 = poly voices 2..16.
        // ── voice 0 = mono: binds the parent's mono REST/ACCENT params. No poly mod-arc (the arc reads
        //    getBasePolyRest which is poly-indexed 0..14; there is no mono equivalent, so the mono knob
        //    shows no East-mod arc — its value is the mono base directly). ──
        // ── voice 0 = mono: LOCKED knobs that MIRROR the parent Monsoon's mono rest/accent.
        //    Uses the shared Dimmable idiom (ui/Controls.hpp; same as Sands ceded-lane spread):
        //      lockWhen       → always true: swallow input, so the knob can't be dragged.
        //      displayValueFn → show the parent's value WITHOUT clobbering the local store.
        //    Monsoon stays authoritative (the engine reads its knob); these just display it.
        //    Arc queued with voice -1 = the MONO lane, so it shows the same Causeway/mono mod arc
        //    that Monsoon's own rest/accent knobs show. ──
        bindParam<redDot::Themed_Trim_Cog_Dim>("param_rest_0", MonsoonIds::REST_PARAM,
            std::function<void(redDot::Themed_Trim_Cog_Dim*)>([this](redDot::Themed_Trim_Cog_Dim* k){
                k->lightWhen = [this](){ return themeLight_; };
                k->lockWhen = [](){ return true; };
                k->displayValueFn = [this]() -> float {
                    Monsoon* m = redDot::findMonsoonEitherSide(module);
                    return m ? m->params[MonsoonIds::REST_PARAM].getValue() : NAN;
                };
                queueArc(k, -1, 0);
            }));
        bindParam<redDot::Themed_Trim_Cog_Dim>("param_accent_0", MonsoonIds::ACCENT_KNOB,
            std::function<void(redDot::Themed_Trim_Cog_Dim*)>([this](redDot::Themed_Trim_Cog_Dim* k){
                k->lightWhen = [this](){ return themeLight_; };
                k->lockWhen = [](){ return true; };
                k->displayValueFn = [this]() -> float {
                    Monsoon* m = redDot::findMonsoonEitherSide(module);
                    return m ? m->params[MonsoonIds::ACCENT_KNOB].getValue() : NAN;
                };
                queueArc(k, -1, 1);
            }));
        // ── voices 1..15 = poly. Param = POLY_*_PARAM_1 + (i-1); arc voice index = poly index (i-1),
        //    which maps to getBasePolyRest(0..14). ──
        for (int i = 1; i < 16; i++) {
            std::string r = std::to_string(i);
            int polyIdx = i - 1;                 // 0..14 for the mod-arc + POLY_*_PARAM offset
            bindParam<redDot::Themed_Trim_Cog>("param_rest_"   + r, MonsoonIds::POLY_REST_PARAM_1   + polyIdx,
                std::function<void(redDot::Themed_Trim_Cog*)>([this, polyIdx](redDot::Themed_Trim_Cog* k){
                    k->lightWhen = [this](){ return themeLight_; };
                    queueArc(k, polyIdx, 0);
                }));
            bindParam<redDot::Themed_Trim_Cog>("param_accent_" + r, MonsoonIds::POLY_ACCENT_PARAM_1 + polyIdx,
                std::function<void(redDot::Themed_Trim_Cog*)>([this, polyIdx](redDot::Themed_Trim_Cog* k){
                    k->lightWhen = [this](){ return themeLight_; };
                    queueArc(k, polyIdx, 1);
                }));
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
        // Resolve the theme BEFORE stepping children: the knobs' lightWhen reads
        // themeLight_, and ModuleWidget::step() is what steps them. Doing this after
        // would leave every knob a frame behind the panel on a toggle.
        if (module) {
            Monsoon* mm = redDot::findMonsoonEitherSide(module);
            themeLight_ = (mm && mm->lightTheme);
        }
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
