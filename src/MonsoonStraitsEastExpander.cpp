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
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/StraitsEast_panel_dark_12HP.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── 12HP Layout (6 columns) ──
        float col1 = 6.0f;  // Rest Knob
        float col2 = 16.0f; // Attenuverter
        float col3 = 26.0f; // Mod Input
        float col4 = 36.0f; // Gate Output
        float col5 = 46.0f; // CV Output
        float col6 = 56.0f; // Accent Output
        
        float startY  = 25.0f;
        float spacingY = 12.0f;

        for (int i = 0; i < 7; i++) {
            float y = startY + i * spacingY;
            // Rest probability knob
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(col1, y)), module, MonsoonIds::POLY_REST_PARAM_1 + i));
            // Rest modulation attenuverter
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(col2, y)), module, MonsoonIds::POLY_REST_MOD_ATT_1 + i));
            // Rest modulation input
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(col3, y)), module, MonsoonIds::POLY_REST_MOD_CV_INPUT_1 + i));
            // Gate output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(col4, y)), module, POLY_GATE_OUT_1 + i));
            // CV (pitch) output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(col5, y)), module, POLY_CV_OUT_1 + i));
            // Accent output
            addOutput(createOutputCentered<PJ301MPort>(
                mm2px(Vec(col6, y)), module, POLY_ACCENT_OUT_1 + i));
        }

        // Poly Rest CV Input
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(col1, startY + 7 * spacingY + 5.0f)),
            module, MonsoonIds::POLY_REST_CV_INPUT));
        
        // Poly outputs for voices 1-8
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(col4, startY + 7 * spacingY + 5.0f)),
            module, POLY_GATE_1_8_OUT));
        addOutput(createOutputCentered<PJ301MPort>(
            mm2px(Vec(col5, startY + 7 * spacingY + 5.0f)),
            module, POLY_CV_1_8_OUT));
    }

    void draw(const DrawArgs& args) override {
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