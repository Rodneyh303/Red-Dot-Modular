#include <rack.hpp>
#include "MeloDicerExpander.hpp"

using namespace rack;

extern Model* modelMeloDicer;

struct MeloDicerExpanderWidget : ModuleWidget {
    MeloDicerExpanderWidget(MeloDicerExpander* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/MeloDicer_Expander3.svg")));

        // ── Screws ───────────────────────────────────────────────────────────
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // ── Vertical 2-Column Layout ─────────────────────────────────────────
        // Col 0: C to F (0-5), Col 1: F# to B (6-11)
        for (int i = 0; i < 12; i++) {
            int col = i / 6;
            int row = i % 6;
            float jackX = (col == 0) ? 14.0f : 36.5f;
            float knobX = jackX + 7.5f;
            float rowY = 25.0f + (row * 15.0f);

            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(jackX, rowY)), module, MeloDicerIds::EXPANDER_SEMI_CV_INPUT_0 + i));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(knobX, rowY)), module, MeloDicerIds::EXPANDER_SEMI_ATTENUVERTER_0 + i));
        }
        
        // ── Octave Section (Bottom) ──────────────────────────────────────────
        // Align with the last row of semitone CVs (row 5, which is index 5 for 0-indexed rows)
        float octY = 25.0f + (5 * 15.0f); // This is 100.0f
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.0f, octY)), module, MeloDicerIds::EXPANDER_OCT_LO_CV_INPUT)); // OCT LO Jack
        addParam(createParamCentered<Trimpot>(mm2px(Vec(21.5f, octY)), module, MeloDicerIds::EXPANDER_OCT_LO_ATTENUVERTER)); // OCT LO Knob
        
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34.0f, octY)), module, MeloDicerIds::EXPANDER_OCT_HI_CV_INPUT)); // OCT HI Jack
        addParam(createParamCentered<Trimpot>(mm2px(Vec(41.5f, octY)), module, MeloDicerIds::EXPANDER_OCT_HI_ATTENUVERTER)); // OCT HI Knob
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

Model* modelMeloDicerExpander = createModel<MeloDicerExpander, MeloDicerExpanderWidget>("MeloDicerExpander");