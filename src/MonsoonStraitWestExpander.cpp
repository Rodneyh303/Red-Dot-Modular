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
        float outGateX =  22.0f;
        float outCvX   =  32.0f;
        float dnaLenX  =  42.0f;
        float dnaOffX  =  52.0f;
        float dnaRotX  =  62.0f;
        float startY  = 25.0f;
        float spacingY = 12.0f;

        // 8 Rest knobs + outputs for voices 9-16
        for (int i = 0; i < 8; i++) {
            float y = startY + i * spacingY;
            // Rest probability knob
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(knobX, y)), module, MonsoonIds::POLY_REST_PARAM_8 + i));
            // Gate output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(outGateX, y)), module, POLY_GATE_OUT_1 + i));
            // CV (pitch) output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(outCvX, y)), module, POLY_CV_OUT_1 + i));
            
            // Individual Poly DNA Controls (voices 9-16)
            addParam(createParamCentered<Trimpot>(mm2px(Vec(dnaLenX, y)), module, MonsoonIds::POLY_DNA_VOICE_8_LEN + i * 3));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(dnaOffX, y)), module, MonsoonIds::POLY_DNA_VOICE_8_OFF + i * 3));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(dnaRotX, y)), module, MonsoonIds::POLY_DNA_VOICE_8_ROT + i * 3));
        }

        // Poly Rest CV Input
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(knobX, startY + 8 * spacingY + 5.0f)),
            module, MonsoonIds::POLY_REST_CV_INPUT));
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0xdd, 0xdd, 0xdd, 0xff));
        nvgFontSize(args.vg, mm2px(3.0f));
        nvgText(args.vg, mm2px(15.0f), mm2px(15.0f), "STRAITS WEST (9-16)", nullptr);
        nvgText(args.vg, mm2px(42.0f), mm2px(15.0f), "LEN", nullptr);
        nvgText(args.vg, mm2px(52.0f), mm2px(15.0f), "OFF", nullptr);
        nvgText(args.vg, mm2px(62.0f), mm2px(15.0f), "ROT", nullptr);
        nvgText(args.vg, mm2px(52.0f), mm2px(10.0f), "POLY DNA", nullptr);
    }
};

Model* modelMonsoonStraitWestExpander = createModel<MonsoonStraitWestExpander, MonsoonStraitWestExpanderWidget>("MonsoonStraitWestExpander");
