#include <rack.hpp>
#include "MeloDicer.hpp"
#include "MeloDicerPolyVoiceExpander.hpp"

using namespace rack;

Model* modelMeloDicerPolyVoiceExpander = createModel<MeloDicerPolyVoiceExpander, MeloDicerPolyVoiceExpanderWidget>("MeloDicerPolyVoiceExpander");

struct MeloDicerPolyVoiceExpanderWidget : ModuleWidget {
    MeloDicerPolyVoiceExpanderWidget(MeloDicerPolyVoiceExpander* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/MeloDicer_PolyVoiceExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // 7 Rest knobs for additional voices
        float knobX = 15.0f;
        float startY = 25.0f;
        float spacingY = 12.0f;

        for (int i = 0; i < 7; i++) {
            addParam(createParamCentered<Trimpot>(mm2px(Vec(knobX, startY + i * spacingY)), module, MeloDicerIds::POLY_REST_PARAM_1 + i));
        }

        // Poly Rest CV Input
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(knobX, startY + 7 * spacingY + 5.0f)), module, MeloDicerIds::POLY_REST_CV_INPUT));
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