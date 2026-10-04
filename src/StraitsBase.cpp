// StraitsBase — the frame + IO + voice count, NO lane knobs.
// This is a REFACTOR of the working Straits widget (MonsoonStraitsExpander.cpp):
// the IO bindings, voice-count knob, connect mark, and step() logic are COPIED
// from MonsoonStraitsExpanderWidget — only the lane knob bindings are removed.
// The base module IS MonsoonStraitsExpander (same config, same params, same IO).
// Lane expanders dock right and their params sync into the base in step().
//
// See docs/design/STRAITS_CAUSEWAY_LANE_EXTENSIONS.md REFINED MODEL + METHOD:REFACTOR.
#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitsExpander.hpp"
#include "ui/VisualExpanderHelpers.hpp"
#include "ui/SvgPanelKit.hpp"
#include "ui/ConnectMark.hpp"
#include "ui/Controls.hpp"
#include "StraitsLaneExpander.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace StraitsIds;

struct StraitsBaseWidget : ModuleWidget,
    dotModular::Compose<StraitsBaseWidget,
                        dotModular::ShapeQuery, dotModular::Bind, dotModular::Reload> {
    std::shared_ptr<rack::window::Svg> panelSvgDark, panelSvgLight;
    bool themeLight_ = false;
    int activeVoices_ = -1;
    bool voiceCountSynced_ = false;
    redDot::ConnectMark* connectMark = nullptr;
    int lastThemeLight = -1;

    StraitsBaseWidget(MonsoonStraitsExpander* mod) {
        setModule(mod);
        const char* darkPath  = "res/panels/StraitsBase_panel_dark.svg";
        const char* lightPath = "res/panels/StraitsBase_panel_light.svg";
        panelSvgDark  = APP->window->loadSvg(asset::plugin(pluginInstance, darkPath));
        panelSvgLight = APP->window->loadSvg(asset::plugin(pluginInstance, lightPath));
        loadPanel(asset::plugin(pluginInstance, darkPath));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── IO bindings (COPIED from MonsoonStraitsExpanderWidget, unchanged) ──
        bindOutput<PJ301MPort>("output_polygate",     POLY_GATE_OUT);
        bindOutput<PJ301MPort>("output_polystepgate", POLY_STEP_GATE_OUT);
        bindOutput<PJ301MPort>("output_polyslegato",  POLY_STEP_LEGATO_GATE_OUT);
        bindOutput<PJ301MPort>("output_polycv",       POLY_CV_OUT);
        bindOutput<PJ301MPort>("output_polyaccent",   POLY_ACCENT_OUT);
        bindInput<PJ301MPort>("input_quantcv", StraitsIds::QUANT_CV_INPUT);

        // Voice-count knob (COPIED from MonsoonStraitsExpanderWidget)
        bindParam<redDot::Themed_Trim_Slot>("param_voicecount", StraitsIds::VOICE_COUNT_PARAM,
            std::function<void(redDot::Themed_Trim_Slot*)>([this](redDot::Themed_Trim_Slot* k){
                k->lightWhen = [this](){ return themeLight_; };
            }));

        if (auto* s = findNamed("light_connect")) {
            connectMark = redDot::makeConnectMark(module, centerOf(s), mm2px(8.f));
            addChild(connectMark);
        }
    }

    void step() override {
        // ── step() logic COPIED from MonsoonStraitsExpanderWidget, plus lane sync ──
        if (module) {
            Monsoon* mm = redDot::findMonsoonEitherSide(module);
            themeLight_ = (mm && mm->lightTheme);
            if (mm) {
                if (!voiceCountSynced_) {
                    float knob = (float)(mm->engine.numPolyVoices + 1);
                    module->params[StraitsIds::VOICE_COUNT_PARAM].setValue(knob);
                    voiceCountSynced_ = true;
                }
                int want = (int)module->params[StraitsIds::VOICE_COUNT_PARAM].getValue() - 1;
                if (want < 0) want = 0; else if (want > 15) want = 15;
                mm->engine.numPolyVoices = want;
                activeVoices_ = want;
            } else {
                activeVoices_ = -1;
                voiceCountSynced_ = false;
            }

            // ── Lane expander param sync ────────────────────────────────────────
            // Walk the right-neighbour chain and sync each lane expander's params
            // into the base's params. The engine reads from the base (as
            // cachedPolyVoiceExpander), so lane knob edits reach the engine here.
            // Fixed-order: read rightExpander until a non-LaneExpander is found.
            rack::Module* right = module->rightExpander.module;
            while (right) {
                auto* lane = dynamic_cast<StraitsLaneExpander*>(right);
                if (!lane || !lane->desc) break;
                // Sync mono + 15 poly params for this lane
                module->params[lane->desc->monoParamId].setValue(
                    right->params[lane->desc->monoParamId].getValue());
                for (int i = 0; i < 15; i++) {
                    module->params[lane->desc->polyParamIdBase + i].setValue(
                        right->params[lane->desc->polyParamIdBase + i].getValue());
                }
                right = right->rightExpander.module;
            }
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

Model* modelStraitsBase =
    createModel<MonsoonStraitsExpander, StraitsBaseWidget>("StraitsBase");
