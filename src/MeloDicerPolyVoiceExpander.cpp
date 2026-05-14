#include <rack.hpp>
#include "MeloDicer.hpp"
#include "MeloDicerPolyVoiceExpander.hpp"

using namespace rack;
using namespace MeloDicerIds;
using namespace PolyVoiceExpanderIds;


struct MeloDicerPolyVoiceExpanderWidget : ModuleWidget {
    MeloDicerPolyVoiceExpanderWidget(MeloDicerPolyVoiceExpander* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/MeloDicer_PolyVoiceExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // 7 Rest knobs for additional voices (left column)
        float knobX   = 10.0f;
        float outGateX =  22.0f;
        float outCvX   =  32.0f;
        float startY  = 25.0f;
        float spacingY = 12.0f;

        for (int i = 0; i < 7; i++) {
            float y = startY + i * spacingY;
            // Rest probability knob
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(knobX, y)), module, MeloDicerIds::POLY_REST_PARAM_1 + i));
            // Gate output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(outGateX, y)), module, POLY_GATE_OUT_1 + i));
            // CV (pitch) output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(outCvX, y)), module, POLY_CV_OUT_1 + i));
        }

        // Poly Rest CV Input (below the knobs)
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(knobX, startY + 7 * spacingY + 5.0f)),
            module, MeloDicerIds::POLY_REST_CV_INPUT));
    }
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0xdd, 0xdd, 0xdd, 0xff));
        nvgFontSize(args.vg, mm2px(3.0f)); // Adjust font size for better fit
        nvgText(args.vg, mm2px(15.0f), mm2px(15.0f), "POLY RESTS", nullptr);
    }
};

Model* modelMeloDicerPolyVoiceExpander = createModel<MeloDicerPolyVoiceExpander, MeloDicerPolyVoiceExpanderWidget>("MeloDicerPolyVoiceExpander");