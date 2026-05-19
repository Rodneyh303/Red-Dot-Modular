#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonStraitSandsExpander.hpp"

using namespace rack;
using namespace StraitSandsExpanderIds;

struct MonsoonStraitSandsExpanderWidget : ModuleWidget {
    MonsoonStraitSandsExpanderWidget(MonsoonStraitSandsExpander* module) 
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/MeloDicer_DNAExpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // TODO: Add UI layout with:
        // - "Scramble ALL" button + 15 individual scramble buttons
        // - "Reset ALL" button + 15 individual reset buttons  
        // - Gate inputs for each scramble/reset
        // - Display of current DNA values (length, offset, rotation) for each voice
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0xdd, 0xdd, 0xdd, 0xff));
        nvgFontSize(args.vg, mm2px(3.0f));
        nvgText(args.vg, mm2px(15.0f), mm2px(10.0f), "STRAITS SANDS", nullptr);
        nvgText(args.vg, mm2px(15.0f), mm2px(15.0f), "POLY DNA CONTROL", nullptr);
    }
};

Model* modelMonsoonStraitSandsExpander = createModel<MonsoonStraitSandsExpander, MonsoonStraitSandsExpanderWidget>("MonsoonStraitSandsExpander");
