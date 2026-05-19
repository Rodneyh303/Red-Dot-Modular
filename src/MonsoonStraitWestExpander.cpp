#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitWestExpander.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace StraitWestExpanderIds;

struct MonsoonStraitWestExpanderWidget : ModuleWidget {
    MonsoonStraitWestExpanderWidget(MonsoonStraitWestExpander* module) 
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/MeloDicer_PolyVoiceExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Layout: Same as East expander
        float knobX   = 10.0f;
        float attX    = 20.0f;
        float modInX  = 30.0f;
        float outGateX =  42.0f;
        float outCvX   =  52.0f;
        float outAccX  =  62.0f;
        float startY  = 25.0f;
        float spacingY = 12.0f;

        // 8 Rest knobs + mod controls + outputs for voices 9-16
        for (int i = 0; i < 8; i++) {
            float y = startY + i * spacingY;
            // Rest probability knob
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(knobX, y)), module, MonsoonIds::POLY_REST_PARAM_8 + i));
            // Rest modulation attenuverter
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(attX, y)), module, MonsoonIds::POLY_REST_MOD_ATT_8 + i));
            // Rest modulation input
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(modInX, y)), module, MonsoonIds::POLY_REST_MOD_CV_INPUT_8 + i));
            // Gate output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(outGateX, y)), module, POLY_GATE_OUT_1 + i));
            // CV (pitch) output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(outCvX, y)), module, POLY_CV_OUT_1 + i));
            // Accent output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(outAccX, y)), module, POLY_ACCENT_OUT_1 + i));
        }

        // Poly Rest CV Input
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(knobX, startY + 8 * spacingY + 5.0f)),
            module, MonsoonIds::POLY_REST_CV_INPUT));
        
        // Poly outputs for voices 1-16
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(outGateX, startY + 8 * spacingY + 5.0f)),
            module, POLY_GATE_1_16_OUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(outCvX, startY + 8 * spacingY + 5.0f)),
            module, POLY_CV_1_16_OUT));
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0xdd, 0xdd, 0xdd, 0xff));
        nvgFontSize(args.vg, mm2px(3.0f));
        nvgText(args.vg, mm2px(15.0f), mm2px(10.0f), "STRAITS WEST", nullptr);
        nvgText(args.vg, mm2px(15.0f), mm2px(15.0f), "VOICES 9-16", nullptr);
    }
};

Model* modelMonsoonStraitWestExpander = createModel<MonsoonStraitWestExpander, MonsoonStraitWestExpanderWidget>("MonsoonStraitWestExpander");
