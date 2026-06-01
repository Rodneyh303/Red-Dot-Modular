#include <rack.hpp>
#include "Monsoon.hpp"
#include "MonsoonDeepStraitsSands.hpp"

using namespace rack;

struct MonsoonDeepStraitsSandsEastWidget : ModuleWidget {
    MonsoonDeepStraitsSandsEastWidget(MonsoonDeepStraitsSandsEast* module) 
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/monsoon_polydnaexpander.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Layout Configuration ──
        float startX = 12.0f;     // Centered margin for 34 HP
        float startY = 25.0f;     // Top margin
        float rowHeight = 13.5f;  // More space for fewer voices
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
        
        // ── DNA Knobs for 7 voices (2-8) ──
        for (int v = 0; v < 7; v++) {
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
            addParam(createParamCentered<TL1105>(
                mm2px(Vec(buttonsX, y - 1.5f)), module,
                DeepStraitsSandsEastIds::SCRAMBLE_VOICE_1 + v));
            addParam(createParamCentered<TL1105>(
                mm2px(Vec(buttonsX + knobSpacing, y - 1.5f)), module,
                DeepStraitsSandsEastIds::RESET_VOICE_1 + v));
            
            // Gate inputs for scramble/reset
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(buttonsX + knobSpacing * 2.0f, y)), module,
                DeepStraitsSandsEastIds::SCRAMBLE_VOICE_1_INPUT + v));
            addInput(createInputCentered<PJ301MPort>(
                mm2px(Vec(buttonsX + knobSpacing * 3.0f, y)), module,
                DeepStraitsSandsEastIds::RESET_VOICE_1_INPUT + v));
            
            // Interpolation knobs: Rest, Melody, Octave (separate blending per dimension)
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(interpX, y)), module, MonsoonIds::POLY_REST_INTERP_1 + v));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(interpX + knobSpacing, y)), module, MonsoonIds::POLY_MELODY_INTERP_1 + v));
            addParam(createParamCentered<Trimpot>(
                mm2px(Vec(interpX + knobSpacing * 2.0f, y)), module, MonsoonIds::POLY_OCTAVE_INTERP_1 + v));
        }
        
        // ── Master Scramble/Reset Controls (bottom row) ──
        float masterY = startY + 7 * rowHeight + 5.0f;
        addParam(createParamCentered<TL1105>(
            mm2px(Vec(restX + knobSpacing, masterY)), module,
            DeepStraitsSandsEastIds::SCRAMBLE_ALL_PARAM));
        addParam(createParamCentered<TL1105>(
            mm2px(Vec(melodyX, masterY)), module,
            DeepStraitsSandsEastIds::RESET_ALL_PARAM));
        
        // Master gate inputs
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(octaveX, masterY)), module,
            DeepStraitsSandsEastIds::SCRAMBLE_ALL_INPUT));
        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(octaveX + knobSpacing, masterY)), module,
            DeepStraitsSandsEastIds::RESET_ALL_INPUT));
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0xdd, 0xdd, 0xdd, 0xff));
        nvgFontSize(args.vg, mm2px(3.0f));
        nvgText(args.vg, mm2px(15.0f), mm2px(10.0f), "DEEP EAST", nullptr);
        nvgText(args.vg, mm2px(15.0f), mm2px(15.0f), "VOICES 2-8", nullptr);
    }
};

struct MonsoonDeepStraitsSandsWestWidget : ModuleWidget {
    MonsoonDeepStraitsSandsWestWidget(MonsoonDeepStraitsSandsWest* module) 
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panels/monsoon_polydnaexpander.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        float startX = 12.0f, startY = 25.0f, rowH = 11.8f, kS = 9.0f;
        float rX = startX, mX = rX + kS * 3.5f, oX = mX + kS * 3.5f, bX = oX + kS * 3.5f, iX = bX + kS * 4.0f;

        for (int v = 7; v < 15; v++) {
            float y = startY + (v-7) * rowH;
            addParam(createParamCentered<Trimpot>(mm2px(Vec(rX, y)), module, MonsoonIds::POLY_REST_PARAM_1 + v));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(rX + kS, y)), module, MonsoonIds::POLY_DNA_VOICE_1_OFF + v * 3));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(rX + kS * 2.0f, y)), module, MonsoonIds::POLY_DNA_VOICE_1_ROT + v * 3));
            
            addParam(createParamCentered<Trimpot>(mm2px(Vec(mX, y)), module, MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(mX + kS, y)), module, MonsoonIds::POLY_MELODY_VOICE_1_OFF + v * 3));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(mX + kS * 2.0f, y)), module, MonsoonIds::POLY_MELODY_VOICE_1_ROT + v * 3));
            
            addParam(createParamCentered<Trimpot>(mm2px(Vec(oX, y)), module, MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(oX + kS, y)), module, MonsoonIds::POLY_OCTAVE_VOICE_1_OFF + v * 3));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(oX + kS * 2.0f, y)), module, MonsoonIds::POLY_OCTAVE_VOICE_1_ROT + v * 3));
            
            addParam(createParamCentered<TL1105>(mm2px(Vec(bX, y - 1.5f)), module, DeepStraitsSandsWestIds::SCRAMBLE_VOICE_8 + (v - 7)));
            addParam(createParamCentered<TL1105>(mm2px(Vec(bX + kS, y - 1.5f)), module, DeepStraitsSandsWestIds::RESET_VOICE_8 + (v - 7)));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(bX + kS * 2.0f, y)), module, DeepStraitsSandsWestIds::SCRAMBLE_VOICE_8_INPUT + (v - 7)));
            addInput(createInputCentered<PJ301MPort>(mm2px(Vec(bX + kS * 3.0f, y)), module, DeepStraitsSandsWestIds::RESET_VOICE_8_INPUT + (v - 7)));
            
            addParam(createParamCentered<Trimpot>(mm2px(Vec(iX, y)), module, MonsoonIds::POLY_REST_INTERP_1 + v));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(iX + kS, y)), module, MonsoonIds::POLY_MELODY_INTERP_1 + v));
            addParam(createParamCentered<Trimpot>(mm2px(Vec(iX + kS * 2.0f, y)), module, MonsoonIds::POLY_OCTAVE_INTERP_1 + v));
        }
        
        float masterY = startY + 8 * rowH + 5.0f;
        addParam(createParamCentered<TL1105>(mm2px(Vec(rX + kS, masterY)), module, DeepStraitsSandsWestIds::SCRAMBLE_ALL_PARAM));
        addParam(createParamCentered<TL1105>(mm2px(Vec(mX, masterY)), module, DeepStraitsSandsWestIds::RESET_ALL_PARAM));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(oX, masterY)), module, DeepStraitsSandsWestIds::SCRAMBLE_ALL_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(oX + kS, masterY)), module, DeepStraitsSandsWestIds::RESET_ALL_INPUT));
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(0xdd, 0xdd, 0xdd, 0xff));
        nvgFontSize(args.vg, mm2px(3.0f));
        nvgText(args.vg, mm2px(15.0f), mm2px(10.0f), "DEEP WEST", nullptr);
        nvgText(args.vg, mm2px(15.0f), mm2px(15.0f), "VOICES 9-16", nullptr);
    }
};

Model* modelMonsoonDeepStraitsSandsEast = createModel<MonsoonDeepStraitsSandsEast, MonsoonDeepStraitsSandsEastWidget>("MonsoonDeepStraitsSandsEast");
Model* modelMonsoonDeepStraitsSandsWest = createModel<MonsoonDeepStraitsSandsWest, MonsoonDeepStraitsSandsWestWidget>("MonsoonDeepStraitsSandsWest");
