#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonSandsExpander.hpp"

using namespace rack;

extern Model* modelMonsoon;

struct MonsoonSandsExpanderWidget : ModuleWidget {
    MonsoonSandsExpanderWidget(MonsoonSandsExpander* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/MeloDicer_DNAExpander.svg")));

        // ── Screws ───────────────────────────────────────────────────────────
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // ── DNA Strand Controls ─────────────────────────────────────────────}
        // For each strand: Length, Offset, Rotation
        // The IDs are grouped as LEN, OFF, ROT for each strand in MonsoonIds
        // DNA_R_LEN_PARAM, DNA_R_OFF_PARAM, DNA_R_ROT_PARAM,
        // DNA_V_LEN_PARAM, DNA_V_OFF_PARAM, DNA_V_ROT_PARAM, etc.
        // So, for strand 'i', the params are DNA_R_LEN_PARAM + i*3, + i*3+1, + i*3+2
        
        // Base X position for the first strand's knobs
        float baseKnobX = 14.0f;
        float knobSpacingX = 10.0f; // Spacing between strand groups
        float knobY_len = 30.0f;
        float knobY_off = 45.0f;
        float knobY_rot = 60.0f;

        for (int i = 0; i < 5; ++i) {
            float x = baseKnobX + i * knobSpacingX;
            addParam(createParamCentered<Trimpot>(mm2px(Vec(x, knobY_len)), module, MonsoonIds::DNA_R_LEN_PARAM + i * 3)); // Length
            addParam(createParamCentered<Trimpot>(mm2px(Vec(x, knobY_off)), module, MonsoonIds::DNA_R_LEN_PARAM + i * 3 + 1)); // Offset
            addParam(createParamCentered<Trimpot>(mm2px(Vec(x, knobY_rot)), module, MonsoonIds::DNA_R_LEN_PARAM + i * 3 + 2)); // Rotation

            // Add CV inputs for Length, Offset
            float jackY_len = knobY_len + 10.0f; // Below the knob
            float jackY_off = knobY_off + 10.0f;
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, jackY_len)), module, MonsoonIds::DNA_R_LEN_INPUT + i * 2)); // Length CV
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(x, jackY_off)), module, MonsoonIds::DNA_R_LEN_INPUT + i * 2 + 1)); // Offset CV
        }

        // ── DNA Action Buttons and Gate Inputs ──────────────────────────────
        // Scramble buttons
        float btnX_scramble = 14.0f;
        float btnY_scramble_start = 80.0f;
        float btnSpacingY = 8.0f;
        float jackOffset = 5.0f; // Offset for gate input next to button

        // Scramble ALL
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_scramble, btnY_scramble_start)), module, MonsoonIds::DNA_SCRAMBLE_ALL_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_scramble + jackOffset, btnY_scramble_start)), module, MonsoonIds::DNA_SCRAMBLE_ALL_INPUT));
        // Scramble Rhythm
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_scramble, btnY_scramble_start + btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_R_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_scramble + jackOffset, btnY_scramble_start + btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_R_INPUT));
        // Scramble Variation
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_scramble, btnY_scramble_start + 2 * btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_V_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_scramble + jackOffset, btnY_scramble_start + 2 * btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_V_INPUT));
        // Scramble Legato
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_scramble, btnY_scramble_start + 3 * btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_L_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_scramble + jackOffset, btnY_scramble_start + 3 * btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_L_INPUT));
        // Scramble Melody
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_scramble, btnY_scramble_start + 4 * btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_M_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_scramble + jackOffset, btnY_scramble_start + 4 * btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_M_INPUT));
        // Scramble Octave
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_scramble, btnY_scramble_start + 5 * btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_O_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_scramble + jackOffset, btnY_scramble_start + 5 * btnSpacingY)), module, MonsoonIds::DNA_SCRAMBLE_O_INPUT));

        // Reset buttons
        float btnX_reset = btnX_scramble + 20.0f; // Offset for reset buttons
        // Reset ALL
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_reset, btnY_scramble_start)), module, MonsoonIds::DNA_RESET_ALL_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_reset + jackOffset, btnY_scramble_start)), module, MonsoonIds::DNA_RESET_ALL_INPUT));
        // Reset Rhythm
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_reset, btnY_scramble_start + btnSpacingY)), module, MonsoonIds::DNA_RESET_R_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_reset + jackOffset, btnY_scramble_start + btnSpacingY)), module, MonsoonIds::DNA_RESET_R_INPUT));
        // Reset Variation
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_reset, btnY_scramble_start + 2 * btnSpacingY)), module, MonsoonIds::DNA_RESET_V_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_reset + jackOffset, btnY_scramble_start + 2 * btnSpacingY)), module, MonsoonIds::DNA_RESET_V_INPUT));
        // Reset Legato
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_reset, btnY_scramble_start + 3 * btnSpacingY)), module, MonsoonIds::DNA_RESET_L_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_reset + jackOffset, btnY_scramble_start + 3 * btnSpacingY)), module, MonsoonIds::DNA_RESET_L_INPUT));
        // Reset Melody
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_reset, btnY_scramble_start + 4 * btnSpacingY)), module, MonsoonIds::DNA_RESET_M_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_reset + jackOffset, btnY_scramble_start + 4 * btnSpacingY)), module, MonsoonIds::DNA_RESET_M_INPUT));
        // Reset Octave
        addParam(createParamCentered<TL1105>(mm2px(Vec(btnX_reset, btnY_scramble_start + 5 * btnSpacingY)), module, MonsoonIds::DNA_RESET_O_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(btnX_reset + jackOffset, btnY_scramble_start + 5 * btnSpacingY)), module, MonsoonIds::DNA_RESET_O_INPUT));
    }
    
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);

      
    }
};

Model* modelMonsoonSandsExpander = createModel<MonsoonSandsExpander, MonsoonSandsExpanderWidget>("MonsoonSandsExpander");