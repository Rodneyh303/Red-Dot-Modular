#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitWestExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ModArcOverlay.hpp"
#include "ui/ConnectMark.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace StraitWestExpanderIds;

struct MonsoonStraitWestExpanderWidget : ModuleWidget,
    dotModular::Compose<MonsoonStraitWestExpanderWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    // Per-voice REST mod-arc overlays (West voices = engine indices 7..14).
    std::vector<std::pair<rack::ParamWidget*, int>> pendingRestArcs;
    void attachRestArc(rack::ParamWidget* knob, int voice) {
        if (knob) pendingRestArcs.push_back({knob, voice});
    }
    void flushRestArcs() {
        for (auto& pr : pendingRestArcs) {
            auto* knob = pr.first; int voice = pr.second;
            if (!knob) continue;
            auto* arc = new redDot::ModArcOverlay();
            arc->box.pos  = knob->box.pos;
            arc->box.size = knob->box.size;
            arc->radius   = std::min(knob->box.size.x, knob->box.size.y) * 0.5f + mm2px(0.6f);
            rack::Module* self = module;
            // Per-voice CV modulation: effective vs same-frame base (lag-free).
            arc->getSetNorm = [self, voice]() -> float {
                Monsoon* m = redDot::findMonsoonEitherSide(self);
                return (m && voice >= 0 && voice < 15) ? m->cachedPolyRest[voice] : 0.f;
            };
            arc->getModNorm = [self, voice]() -> float {
                Monsoon* m = redDot::findMonsoonEitherSide(self);
                return (m && voice >= 0 && voice < 15) ? m->cachedPolyRestEffective[voice] : 0.f;
            };
            arc->isActive = [self, voice]() -> bool {
                Monsoon* m = redDot::findMonsoonEitherSide(self);
                if (!m || !m->modVizWest || voice < 0 || voice >= 15) return false;
                return std::fabs(m->cachedPolyRestEffective[voice] - m->cachedPolyRest[voice]) > 1e-4f;
            };
            addChild(arc);
        }
        pendingRestArcs.clear();
    }
    MonsoonStraitWestExpanderWidget(MonsoonStraitWestExpander* mod)
    {
        setModule(mod);
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
            int voice = 7 + i;   // West row i → engine poly voice 7+i (POLY_REST_PARAM_8+i)
            bindParam <Trimpot>      ("param_knob_"   + r, MonsoonIds::POLY_REST_PARAM_8      + i,
                std::function<void(Trimpot*)>([this, voice](Trimpot* k){ attachRestArc(k, voice); }));
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
        flushRestArcs();

        // dot.modular connect mark (brand mark; greyed when no Monsoon attached).
        connectMark = bindChild<redDot::ConnectMark>("light_connect",
            std::function<void(redDot::ConnectMark*)>([this](redDot::ConnectMark* w){
                w->box.size = mm2px(rack::math::Vec(8.f, 8.f));
                w->box.pos  = w->box.pos.minus(w->box.size.div(2));
                w->connected  = [this]() { return redDot::isConnectedAndClaimed(module); };
                w->lightTheme = [this]() { Monsoon* mm = module ? redDot::findMonsoonEitherSide(module) : nullptr; return mm && mm->lightTheme; };
            }));
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
