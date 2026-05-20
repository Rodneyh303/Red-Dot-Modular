#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitsSands.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace StraitsSandsIds;

struct MonsoonStraitsSandsWidget : ModuleWidget {
    MonsoonStraitsSandsWidget(MonsoonStraitsSands* module) 
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/MeloDicer_DNAExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Compact Layout (3 columns: Rest / Melody / Octave) ──
        float col1X = 5.0f;    // Rest DNA
        float col2X = 15.0f;   // Melody DNA
        float col3X = 25.0f;   // Octave DNA
        float interpY = 35.0f; // Interpolation row
        float dnaY = 50.0f;    // DNA controls start
        float buttonY = 80.0f; // Master buttons
        float gateY = 95.0f;   // Gate inputs
        
        // ── Labels (drawn in draw()) ──
        
        // ── Rest DNA Row ──
        // Length
        addParam(createParamCentered<Knob25>(
            mm2px(Vec(col1X, dnaY)), module, GLOBAL_REST_DNA_LEN));
        // Offset
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(col1X, dnaY + 12.0f)), module, GLOBAL_REST_DNA_OFF));
        // Rotation
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(col1X, dnaY + 24.0f)), module, GLOBAL_REST_DNA_ROT));
        // Interpolation
        addParam(createParamCentered<Slider>(
            mm2px(Vec(col1X, interpY)), module, GLOBAL_REST_INTERP));
        
        // ── Melody DNA Row ──
        // Length
        addParam(createParamCentered<Knob25>(
            mm2px(Vec(col2X, dnaY)), module, GLOBAL_MELODY_DNA_LEN));
        // Offset
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(col2X, dnaY + 12.0f)), module, GLOBAL_MELODY_DNA_OFF));
        // Rotation
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(col2X, dnaY + 24.0f)), module, GLOBAL_MELODY_DNA_ROT));
        // Interpolation
        addParam(createParamCentered<Slider>(
            mm2px(Vec(col2X, interpY)), module, GLOBAL_MELODY_INTERP));
        
        // ── Octave DNA Row ──
        // Length
        addParam(createParamCentered<Knob25>(
            mm2px(Vec(col3X, dnaY)), module, GLOBAL_OCTAVE_DNA_LEN));
        // Offset
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(col3X, dnaY + 12.0f)), module, GLOBAL_OCTAVE_DNA_OFF));
        // Rotation
        addParam(createParamCentered<Trimpot>(
            mm2px(Vec(col3X, dnaY + 24.0f)), module, GLOBAL_OCTAVE_DNA_ROT));
        // Interpolation
        addParam(createParamCentered<Slider>(
            mm2px(Vec(col3X, interpY)), module, GLOBAL_OCTAVE_INTERP));
        
        // ── Master Scramble/Reset Buttons ──
        addParam(createParamCentered<PushButton>(
            mm2px(Vec(col1X + 5.0f, buttonY)), module, SCRAMBLE_ALL));
        addParam(createParamCentered<PushButton>(
            mm2px(Vec(col3X - 5.0f, buttonY)), module, RESET_ALL));
        
        // ── Gate Inputs ──
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(col1X + 5.0f, gateY)), module, SCRAMBLE_ALL_GATE));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(col3X - 5.0f, gateY)), module, RESET_ALL_GATE));
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0xdd, 0xdd, 0xdd, 0xff));
        
        // Title
        nvgFontSize(args.vg, mm2px(3.0f));
        nvgText(args.vg, mm2px(15.0f), mm2px(5.0f), "STRAITS SANDS", nullptr);
        nvgText(args.vg, mm2px(15.0f), mm2px(10.0f), "Macro Global", nullptr);
        
        // Column headers
        nvgFontSize(args.vg, mm2px(2.0f));
        nvgText(args.vg, mm2px(5.0f), mm2px(40.0f), "Rest", nullptr);
        nvgText(args.vg, mm2px(15.0f), mm2px(40.0f), "Melody", nullptr);
        nvgText(args.vg, mm2px(25.0f), mm2px(40.0f), "Octave", nullptr);
    }
};

Model* modelMonsoonStraitsSands = createModel<MonsoonStraitsSands, MonsoonStraitsSandsWidget>("MonsoonStraitsSands");
