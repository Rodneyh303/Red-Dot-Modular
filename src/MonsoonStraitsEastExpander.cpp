#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitsEastExpander.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace PolyVoiceExpanderIds;

struct MonsoonStraitsEastExpanderWidget : ModuleWidget {
    MonsoonStraitsEastExpanderWidget(MonsoonStraitsEastExpander* module) 
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/interchange_wide_straits_dark.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── 12HP Layout (6 columns) ──
        float mm2pxl = 2.9528;   

        float knobX   = 48.0f/mm2pxl;
        float attX    = 102.0f/mm2pxl;
        float modInX  = 168.0f/mm2pxl;
        float outGateX =  234.0f/mm2pxl;
        float outCvX   =  288.0f/mm2pxl;
        float outAccX  =  342.0f/mm2pxl;
        
        float startY  = 50.0f/mm2pxl; // Top margin
        float spacingY = 35.0f/mm2pxl;



        for (int i = 0; i < 7; i++) {
            float y = startY + i * spacingY;
            // Rest probability knob
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(knobX, y)), module, MonsoonIds::POLY_REST_PARAM_1 + i));
            // Rest modulation attenuverter
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(attX, y)), module, MonsoonIds::POLY_REST_MOD_ATT_1 + i));
            // Rest modulation input
            addInput(createInputCentered<DarkPJ301MPort>(
                mm2px(Vec(modInX, y)), module, MonsoonIds::POLY_REST_MOD_CV_INPUT_1 + i));
            // Gate output
            addOutput(createOutputCentered<DarkPJ301MPort>(
                mm2px(Vec(outGateX, y)), module, POLY_GATE_OUT_1 + i));
            // CV (pitch) output
            addOutput(createOutputCentered<DarkPJ301MPort>(
                mm2px(Vec(outCvX, y)), module, POLY_CV_OUT_1 + i));
            // Accent output
            addOutput(createOutputCentered<DarkPJ301MPort>(
                mm2px(Vec(outAccX, y)), module, POLY_ACCENT_OUT_1 + i));
        }

        // Poly Rest CV Input
        addInput(createInputCentered<DarkPJ301MPort>(
            mm2px(Vec(modInX, startY + 7 * spacingY + 5.0f)),
            module, MonsoonIds::POLY_REST_CV_INPUT));
        
        // Poly outputs for voices 1-8
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(outGateX, startY + 7 * spacingY + 5.0f)),
            module, POLY_GATE_1_8_OUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(outCvX, startY + 7 * spacingY + 5.0f)),
            module, POLY_CV_1_8_OUT));
    }

    void draw(const DrawArgs& args) override {
        // Force a solid opaque background fill to prevent transparency
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillColor(args.vg, nvgRGBA(0x23, 0x23, 0x23, 255)); // Match dark theme
        nvgFill(args.vg);

        ModuleWidget::draw(args);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0xdd, 0xdd, 0xdd, 0xff));
        nvgFontSize(args.vg, mm2px(3.0f));
        nvgText(args.vg, mm2px(30.48f), mm2px(10.0f), "STRAITS EAST", nullptr);
        nvgText(args.vg, mm2px(30.48f), mm2px(15.0f), "VOICES 1-8", nullptr);
    }
};

Model* modelMonsoonStraitsEastExpander = createModel<MonsoonStraitsEastExpander, MonsoonStraitsEastExpanderWidget>("MonsoonStraitsEastExpander");