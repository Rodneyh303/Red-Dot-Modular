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

        // ── Layout Configuration ──
        float startX = 5.0f;      // Left margin
        float startY = 25.0f;     // Top margin
        float rowHeight = 11.0f;  // Vertical spacing between voices
        float knobSpacing = 9.0f; // Horizontal spacing between knobs
        
        // Columns for each DNA type (Rest, Melody, Octave)
        // Each type has 3 knobs: Length (L), Offset (O), Rotation (R)
        float restX = startX;
        float melodyX = restX + knobSpacing * 3.5f;
        float octaveX = melodyX + knobSpacing * 3.5f;
        float buttonsX = octaveX + knobSpacing * 3.5f;
        float interpX = buttonsX + knobSpacing * 4.0f;
        
        // ── Label Row ──
        // (Labels would go here in a real SVG panel)
        
        // ── DNA Knobs for 15 voices ──
        for (int v = 0; v < 15; v++) {
            float y = startY + v * rowHeight;
            
            // Rest DNA (Length, Offset, Rotation)
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(restX, y)), module, MonsoonIds::POLY_REST_PARAM_1 + v));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(restX + knobSpacing, y)), module, 
                MonsoonIds::POLY_DNA_VOICE_1_OFF + v * 3)); // Offset (reusing DNA struct)
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(restX + knobSpacing * 2.0f, y)), module,
                MonsoonIds::POLY_DNA_VOICE_1_ROT + v * 3)); // Rotation
            
            // Melody DNA (Length, Offset, Rotation)
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(melodyX, y)), module, MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(melodyX + knobSpacing, y)), module,
                MonsoonIds::POLY_MELODY_VOICE_1_OFF + v * 3));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(melodyX + knobSpacing * 2.0f, y)), module,
                MonsoonIds::POLY_MELODY_VOICE_1_ROT + v * 3));
            
            // Octave DNA (Length, Offset, Rotation)
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(octaveX, y)), module, MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(octaveX + knobSpacing, y)), module,
                MonsoonIds::POLY_OCTAVE_VOICE_1_OFF + v * 3));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(octaveX + knobSpacing * 2.0f, y)), module,
                MonsoonIds::POLY_OCTAVE_VOICE_1_ROT + v * 3));
            
            // Scramble + Reset buttons for this voice
            addParam(createParamCentered<TinyButton>(
                mm2px(Vec(buttonsX, y - 1.5f)), module,
                StraitSandsExpanderIds::SCRAMBLE_VOICE_1 + v));
            addParam(createParamCentered<TinyButton>(
                mm2px(Vec(buttonsX + knobSpacing, y - 1.5f)), module,
                StraitSandsExpanderIds::RESET_VOICE_1 + v));
            
            // Gate inputs for scramble/reset
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(buttonsX + knobSpacing * 2.0f, y)), module,
                StraitSandsExpanderIds::SCRAMBLE_VOICE_1_INPUT + v));
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(buttonsX + knobSpacing * 3.0f, y)), module,
                StraitSandsExpanderIds::RESET_VOICE_1_INPUT + v));
            
            // Interpolation knob: blend per-voice random vs average random
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(interpX, y)), module, MonsoonIds::POLY_VOICE_1_INTERP + v));
        }
        
        // ── Master Scramble/Reset Controls (bottom row) ──
        float masterY = startY + 15 * rowHeight + 3.0f;
        addParam(createParamCentered<PushButton>(
            mm2px(Vec(restX + knobSpacing, masterY)), module,
            StraitSandsExpanderIds::SCRAMBLE_ALL_PARAM));
        addParam(createParamCentered<PushButton>(
            mm2px(Vec(melodyX, masterY)), module,
            StraitSandsExpanderIds::RESET_ALL_PARAM));
        
        // Master gate inputs
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(octaveX, masterY)), module,
            StraitSandsExpanderIds::SCRAMBLE_ALL_INPUT));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(octaveX + knobSpacing, masterY)), module,
            StraitSandsExpanderIds::RESET_ALL_INPUT));
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
