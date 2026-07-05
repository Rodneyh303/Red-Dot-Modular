#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

/**
 * Straits Sands Expander - Poly DNA Control (Voices 2-16)
 * 
 * Provides DNA parameter control (length, offset, rotation) for all 15 poly voices (2-16).
 * Each voice gets independent control and scramble/reset buttons.
 * 
 * Modulation: Continuous (not quantized)
 * Controls: Per-voice + ALL at once
 * Actions: Scramble (randomize length/offset), Reset (restore defaults)
 */

namespace DeepStraitsSandsEastIds {
    enum ParamId {
        SCRAMBLE_ALL_PARAM = 0,
        SCRAMBLE_VOICE_1, SCRAMBLE_VOICE_2, SCRAMBLE_VOICE_3, SCRAMBLE_VOICE_4,
        SCRAMBLE_VOICE_5, SCRAMBLE_VOICE_6, SCRAMBLE_VOICE_7,
        RESET_ALL_PARAM,
        RESET_VOICE_1, RESET_VOICE_2, RESET_VOICE_3, RESET_VOICE_4,
        RESET_VOICE_5, RESET_VOICE_6, RESET_VOICE_7,
        NUM_PARAMS
    };
    
    enum InputId {
        SCRAMBLE_ALL_INPUT = 0,
        SCRAMBLE_VOICE_1_INPUT, SCRAMBLE_VOICE_2_INPUT, SCRAMBLE_VOICE_3_INPUT, SCRAMBLE_VOICE_4_INPUT,
        SCRAMBLE_VOICE_5_INPUT, SCRAMBLE_VOICE_6_INPUT, SCRAMBLE_VOICE_7_INPUT,
        RESET_ALL_INPUT,
        RESET_VOICE_1_INPUT, RESET_VOICE_2_INPUT, RESET_VOICE_3_INPUT, RESET_VOICE_4_INPUT,
        RESET_VOICE_5_INPUT, RESET_VOICE_6_INPUT, RESET_VOICE_7_INPUT,
        NUM_INPUTS
    };
};

namespace DeepStraitsSandsWestIds {
    enum ParamId {
        SCRAMBLE_ALL_PARAM = 0,
        SCRAMBLE_VOICE_8, SCRAMBLE_VOICE_9, SCRAMBLE_VOICE_10, SCRAMBLE_VOICE_11,
        SCRAMBLE_VOICE_12, SCRAMBLE_VOICE_13, SCRAMBLE_VOICE_14, SCRAMBLE_VOICE_15,
        RESET_ALL_PARAM,
        RESET_VOICE_8, RESET_VOICE_9, RESET_VOICE_10, RESET_VOICE_11,
        RESET_VOICE_12, RESET_VOICE_13, RESET_VOICE_14, RESET_VOICE_15,
        NUM_PARAMS
    };
    
    enum InputId {
        SCRAMBLE_ALL_INPUT = 0,
        SCRAMBLE_VOICE_8_INPUT, SCRAMBLE_VOICE_9_INPUT, SCRAMBLE_VOICE_10_INPUT, SCRAMBLE_VOICE_11_INPUT,
        SCRAMBLE_VOICE_12_INPUT, SCRAMBLE_VOICE_13_INPUT, SCRAMBLE_VOICE_14_INPUT, SCRAMBLE_VOICE_15_INPUT,
        RESET_ALL_INPUT,
        RESET_VOICE_8_INPUT, RESET_VOICE_9_INPUT, RESET_VOICE_10_INPUT, RESET_VOICE_11_INPUT,
        RESET_VOICE_12_INPUT, RESET_VOICE_13_INPUT, RESET_VOICE_14_INPUT, RESET_VOICE_15_INPUT,
        NUM_INPUTS
    };
};

struct MonsoonDeepStraitsSandsEast : Module {
    MonsoonDeepStraitsSandsEast() {
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, 0, 0);
        for (int v = 0; v < 7; v++) {
            std::string voiceLabel = "Voice " + std::to_string(v + 2);
            int b = POLY_DNA_VOICE_1_LEN + v * 3;
            configParam(b, 1.f, 16.f, 16.f, voiceLabel + " Rhythm Length");
            configParam(b + 1, 0.f, 15.f,  0.f, voiceLabel + " Rhythm Offset");
            configParam(b + 2, 0.f, 15.f,  0.f, voiceLabel + " Rhythm Rotation");
            
            int mb = POLY_MELODY_VOICE_1_LEN + v * 3;
            configParam(mb, 1.f, 16.f, 16.f, voiceLabel + " Melody Length");
            configParam(mb + 1, 0.f, 15.f,  0.f, voiceLabel + " Melody Offset");
            configParam(mb + 2, 0.f, 15.f,  0.f, voiceLabel + " Melody Rotation");
            
            int ob = POLY_OCTAVE_VOICE_1_LEN + v * 3;
            configParam(ob, 1.f, 16.f, 16.f, voiceLabel + " Octave Length");
            configParam(ob + 1, 0.f, 15.f,  0.f, voiceLabel + " Octave Offset");
            configParam(ob + 2, 0.f, 15.f,  0.f, voiceLabel + " Octave Rotation");

            configParam(POLY_REST_INTERP_1 + v, 0.f, 1.f, 0.f, voiceLabel + " Rest Interp");
            configParam(POLY_MELODY_INTERP_1 + v, 0.f, 1.f, 0.f, voiceLabel + " Melody Interp");
            configParam(POLY_OCTAVE_INTERP_1 + v, 0.f, 1.f, 0.f, voiceLabel + " Octave Interp");
        }
        using namespace DeepStraitsSandsEastIds;
        configButton(SCRAMBLE_ALL_PARAM, "Scramble All (2-8)");
        configButton(RESET_ALL_PARAM, "Reset All (2-8)");
        for (int v = 0; v < 7; v++) {
            configButton(SCRAMBLE_VOICE_1 + v, "Scramble Voice " + std::to_string(v + 2));
            configButton(RESET_VOICE_1 + v, "Reset Voice " + std::to_string(v + 2));
        }
    }
    void process(const ProcessArgs& args) override {}
};

struct MonsoonDeepStraitsSandsWest : Module {
    MonsoonDeepStraitsSandsWest() {
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, 0, 0);
        for (int v = 7; v < 15; v++) {
            std::string voiceLabel = "Voice " + std::to_string(v + 2);
            int b = POLY_DNA_VOICE_1_LEN + v * 3;
            configParam(b, 1.f, 16.f, 16.f, voiceLabel + " Rhythm Length");
            configParam(b + 1, 0.f, 15.f,  0.f, voiceLabel + " Rhythm Offset");
            configParam(b + 2, 0.f, 15.f,  0.f, voiceLabel + " Rhythm Rotation");
            
            int mb = POLY_MELODY_VOICE_1_LEN + v * 3;
            configParam(mb, 1.f, 16.f, 16.f, voiceLabel + " Melody Length");
            configParam(mb + 1, 0.f, 15.f,  0.f, voiceLabel + " Melody Offset");
            configParam(mb + 2, 0.f, 15.f,  0.f, voiceLabel + " Melody Rotation");
            
            int ob = POLY_OCTAVE_VOICE_1_LEN + v * 3;
            configParam(ob, 1.f, 16.f, 16.f, voiceLabel + " Octave Length");
            configParam(ob + 1, 0.f, 15.f,  0.f, voiceLabel + " Octave Offset");
            configParam(ob + 2, 0.f, 15.f,  0.f, voiceLabel + " Octave Rotation");

            configParam(POLY_REST_INTERP_1 + v, 0.f, 1.f, 0.f, voiceLabel + " Rest Interp");
            configParam(POLY_MELODY_INTERP_1 + v, 0.f, 1.f, 0.f, voiceLabel + " Melody Interp");
            configParam(POLY_OCTAVE_INTERP_1 + v, 0.f, 1.f, 0.f, voiceLabel + " Octave Interp");

            configInput(POLY_DNA_VOICE_8_LEN_INPUT + (v - 7) * 2, voiceLabel + " Len CV");
            configInput(POLY_DNA_VOICE_8_OFF_INPUT + (v - 7) * 2, voiceLabel + " Off CV");
        }
        using namespace DeepStraitsSandsWestIds;
        configButton(SCRAMBLE_ALL_PARAM, "Scramble All (9-16)");
        configButton(RESET_ALL_PARAM, "Reset All (9-16)");
        for (int v = 7; v < 15; v++) {
            configButton(SCRAMBLE_VOICE_8 + (v - 7), "Scramble Voice " + std::to_string(v + 2));
            configButton(RESET_VOICE_8 + (v - 7), "Reset Voice " + std::to_string(v + 2));
        }
    }
    void process(const ProcessArgs& args) override {}
};
