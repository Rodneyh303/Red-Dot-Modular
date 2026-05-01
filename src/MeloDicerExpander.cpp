#include <rack.hpp>
#include "MeloDicer.hpp"

using namespace rack;

extern Model* modelMeloDicer;

struct MeloDicerExpander : Module {
    // enum InputIds {
    //     SEMI_CV_INPUT_0, SEMI_CV_INPUT_1, SEMI_CV_INPUT_2, SEMI_CV_INPUT_3,
    //     SEMI_CV_INPUT_4, SEMI_CV_INPUT_5, SEMI_CV_INPUT_6, SEMI_CV_INPUT_7,
    //     SEMI_CV_INPUT_8, SEMI_CV_INPUT_9, SEMI_CV_INPUT_10, SEMI_CV_INPUT_11,
    //     OCT_LO_CV_INPUT,
    //     OCT_HI_CV_INPUT,
    //     NUM_INPUTS
    // };

    MeloDicerLeftMessage messages[2];

    MeloDicerExpander() {
        config(0, MeloDicerIds::NUM_EXPANDER_INPUTS, 0, 0);
        for (int i = 0; i < 12; i++) {
            configInput(MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + i, string::f("Semitone %d CV", i + 1));
        }
        configInput(MeloDicerIds::EXPANDER_OCT_LO_CV_INPUT, "Octave Lo CV");
        configInput(MeloDicerIds::EXPANDER_OCT_HI_CV_INPUT, "Octave Hi CV");

        rightExpander.producerMessage = &messages[0];
        rightExpander.consumerMessage = &messages[1];
    }

    void process(const ProcessArgs& args) override {
        // Left expander: mother is to the right
        // if (rightExpander.module && rightExpander.module->model == modelMeloDicer) {
        //     MeloDicerLeftMessage* msg = (MeloDicerLeftMessage*)rightExpander.module->leftExpander.producerMessage;
        //     for (int i = 0; i < 12; i++) {
        //         if(inputs[MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + i].isConnected()) {
        //             msg->semiCv[i] = inputs[MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + i].getVoltage();
        //         }
            
        //     }
        //     if(inputs[MeloDicerIds::EXPANDER_OCT_LO_CV_INPUT].isConnected()) {
        //         msg->octLoCv = inputs[MeloDicerIds::EXPANDER_OCT_LO_CV_INPUT].getVoltage();
        //     }
        //     if(inputs[MeloDicerIds::EXPANDER_OCT_HI_CV_INPUT].isConnected()) {
        //         msg->octHiCv = inputs[MeloDicerIds::EXPANDER_OCT_HI_CV_INPUT].getVoltage();
        //     }
        //     rightExpander.module->leftExpander.messageFlipRequested = true;
        // }
    }
};

struct MeloDicerExpanderWidget : ModuleWidget {
    MeloDicerExpanderWidget(MeloDicerExpander* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/MeloDicer_Expander.svg")));

        // Add screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // CV jacks moved from main panel
        float padding = 5.0f;
        for (int i = 0; i < 6; i++) {
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(padding + i * 8.0f, 30.f)), module, MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + i));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(padding + i * 8.0f, 45.f)), module, MeloDicerIds::SEMI_CV_INPUT_0 + 6 + i));
        }
        
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.f, 70.f)), module, MeloDicerIds::EXPANDER_OCT_LO_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(25.f, 70.f)), module, MeloDicerIds::OCT_HI_CV_INPUT));
    }
};

Model* modelMeloDicerExpander = createModel<MeloDicerExpander, MeloDicerExpanderWidget>("MeloDicerExpander");