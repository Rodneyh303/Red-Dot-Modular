#include <rack.hpp>
#include "MonsoonInterchangeExpander.hpp"

using namespace rack;

extern Model* modelMonsoon;

struct MonsoonInterchangeExpanderWidget : ModuleWidget {
MonsoonInterchangeExpanderWidget(MonsoonInterchangeExpander* module) {
    setModule(module);
    //box.size = mm2px(Vec(270, 380));
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/interchange_wide_dark.svg")));

    // ... (Screws same as before) ...
    
// Semitone Loop (i=0 to 5)
for (int i = 0; i < 6; i++) {
    float rowY = 80.0f + (i * 40.0f); 
    addInput(createInputCentered<PJ301MPort>(Vec(48.0f, rowY), module, MonsoonIds::EXPANDER_SEMI_CV_INPUT_0 + i));
    addParam(createParamCentered<Trimpot>(Vec(102.0f, rowY), module, MonsoonIds::EXPANDER_SEMI_ATTENUVERTER_0 + i));
    addParam(createParamCentered<Trimpot>(Vec(168.0f, rowY), module, MonsoonIds::EXPANDER_SEMI_ATTENUVERTER_0 + 6 + i));
    addInput(createInputCentered<PJ301MPort>(Vec(222.0f, rowY), module, MonsoonIds::EXPANDER_SEMI_CV_INPUT_0 + 6 + i));
}

// Octave Section (Position 7, Y=320)
float octY = 320.0f;
addInput(createInputCentered<PJ301MPort>(Vec(48.0f, octY), module, MonsoonIds::EXPANDER_OCT_LO_CV_INPUT));
addParam(createParamCentered<Trimpot>(Vec(102.0f, octY), module, MonsoonIds::EXPANDER_OCT_LO_ATTENUVERTER));
addParam(createParamCentered<Trimpot>(Vec(168.0f, octY), module, MonsoonIds::EXPANDER_OCT_HI_ATTENUVERTER));
addInput(createInputCentered<PJ301MPort>(Vec(222.0f, octY), module, MonsoonIds::EXPANDER_OCT_HI_CV_INPUT));
}
    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);

        // ── Panel Labels ─────────────────────────────────────────────────────
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0x23, 0x23, 0x23, 0xff)); // Dark gray for light panels, adjust as needed

        // Header
        nvgFontSize(args.vg, mm2px(3.5f));
        nvgText(args.vg, mm2px(20.0f), mm2px(10.0f), "EXPANDER CV", nullptr);

        // Semitone Labels
        nvgFontSize(args.vg, mm2px(2.5f));
        const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        for (int i = 0; i < 12; i++) {
            int col = i / 6;
            int row = i % 6;
            float jackX = (col == 0) ? 8.0f : 28.0f;
            float labelX = jackX + 3.75f;
            float rowY = 25.0f + (row * 15.0f);
            
            nvgText(args.vg, mm2px(labelX), mm2px(rowY - 8.0f), noteNames[i], nullptr);
        }

        // Octave Labels
        float octY = 115.0f;
        nvgText(args.vg, mm2px(11.75f), mm2px(octY - 8.0f), "OCT LO", nullptr);
        nvgText(args.vg, mm2px(31.75f), mm2px(octY - 8.0f), "OCT HI", nullptr);

        // Attenuation guides
        nvgFontSize(args.vg, mm2px(1.8f));
        nvgFillColor(args.vg, nvgRGBA(0x66, 0x66, 0x66, 0xff));
        for (float jackX : {8.0f, 28.0f}) {
            float knobX = jackX + 7.5f;
            for (int row = 0; row < 6; row++) {
                float y = 25.0f + (row * 15.0f);
                nvgText(args.vg, mm2px(knobX - 4.5f), mm2px(y), "-", nullptr);
                nvgText(args.vg, mm2px(knobX + 4.5f), mm2px(y), "+", nullptr);
            }
            // Octave row guides
            nvgText(args.vg, mm2px(knobX - 4.5f), mm2px(octY), "-", nullptr);
            nvgText(args.vg, mm2px(knobX + 4.5f), mm2px(octY), "+", nullptr);
        }
    }
};

Model* modelMonsoonInterchangeExpander = createModel<MonsoonInterchangeExpander, MonsoonInterchangeExpanderWidget>("MonsoonInterchangeExpander");