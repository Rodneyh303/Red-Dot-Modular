#pragma once
#include <rack.hpp>
#include "Monsoon.hpp"

using namespace rack;
using namespace MonsoonIds;

/**
 * Straits Sands Expander - Poly DNA Control (Voices 1-15)
 * 
 * Provides DNA parameter control (length, offset, rotation) for all 15 poly voices.
 * Each voice gets independent control and scramble/reset buttons.
 * 
 * Modulation: Continuous (not quantized)
 * Controls: Per-voice + ALL at once
 * Actions: Scramble (randomize length/offset), Reset (restore defaults)
 */

namespace StraitSandsExpanderIds {
    enum ParamId {
        // Scramble buttons: randomize length & offset per voice
        SCRAMBLE_ALL_PARAM = 0,
        SCRAMBLE_VOICE_1,
        SCRAMBLE_VOICE_2,
        SCRAMBLE_VOICE_3,
        SCRAMBLE_VOICE_4,
        SCRAMBLE_VOICE_5,
        SCRAMBLE_VOICE_6,
        SCRAMBLE_VOICE_7,
        SCRAMBLE_VOICE_8,
        SCRAMBLE_VOICE_9,
        SCRAMBLE_VOICE_10,
        SCRAMBLE_VOICE_11,
        SCRAMBLE_VOICE_12,
        SCRAMBLE_VOICE_13,
        SCRAMBLE_VOICE_14,
        SCRAMBLE_VOICE_15,
        
        // Reset buttons: restore defaults per voice
        RESET_ALL_PARAM,
        RESET_VOICE_1,
        RESET_VOICE_2,
        RESET_VOICE_3,
        RESET_VOICE_4,
        RESET_VOICE_5,
        RESET_VOICE_6,
        RESET_VOICE_7,
        RESET_VOICE_8,
        RESET_VOICE_9,
        RESET_VOICE_10,
        RESET_VOICE_11,
        RESET_VOICE_12,
        RESET_VOICE_13,
        RESET_VOICE_14,
        RESET_VOICE_15,
        
        NUM_PARAMS
    };
    
    enum InputId {
        // Gate inputs for scramble actions
        SCRAMBLE_ALL_INPUT = 0,
        SCRAMBLE_VOICE_1_INPUT,
        SCRAMBLE_VOICE_2_INPUT,
        SCRAMBLE_VOICE_3_INPUT,
        SCRAMBLE_VOICE_4_INPUT,
        SCRAMBLE_VOICE_5_INPUT,
        SCRAMBLE_VOICE_6_INPUT,
        SCRAMBLE_VOICE_7_INPUT,
        SCRAMBLE_VOICE_8_INPUT,
        SCRAMBLE_VOICE_9_INPUT,
        SCRAMBLE_VOICE_10_INPUT,
        SCRAMBLE_VOICE_11_INPUT,
        SCRAMBLE_VOICE_12_INPUT,
        SCRAMBLE_VOICE_13_INPUT,
        SCRAMBLE_VOICE_14_INPUT,
        SCRAMBLE_VOICE_15_INPUT,
        
        // Gate inputs for reset actions
        RESET_ALL_INPUT,
        RESET_VOICE_1_INPUT,
        RESET_VOICE_2_INPUT,
        RESET_VOICE_3_INPUT,
        RESET_VOICE_4_INPUT,
        RESET_VOICE_5_INPUT,
        RESET_VOICE_6_INPUT,
        RESET_VOICE_7_INPUT,
        RESET_VOICE_8_INPUT,
        RESET_VOICE_9_INPUT,
        RESET_VOICE_10_INPUT,
        RESET_VOICE_11_INPUT,
        RESET_VOICE_12_INPUT,
        RESET_VOICE_13_INPUT,
        RESET_VOICE_14_INPUT,
        RESET_VOICE_15_INPUT,
        
        NUM_INPUTS
    };
};

struct MonsoonStraitSandsExpander : Module {
    MonsoonStraitSandsExpander() {
        // Size to full parent params/inputs to access DNA controls
        config(MonsoonIds::NUM_PARAMS, MonsoonIds::NUM_INPUTS, 0, 0);

        // Configure DNA controls for voices 1-15 (using parent MonsoonIds)
        for (int v = 0; v < 15; v++) {
            std::string voiceLabel = "Voice " + std::to_string(v + 1);
            int paramBase = POLY_DNA_VOICE_1_LEN + v * 3;
            configParam(paramBase,     1.f, 16.f, 16.f, voiceLabel + " DNA Length");
            configParam(paramBase + 1, 0.f, 15.f,  0.f, voiceLabel + " DNA Offset");
            configParam(paramBase + 2, 0.f, 15.f,  0.f, voiceLabel + " DNA Rotation");
        }
        
        // Configure DNA CV modulation inputs for voices 8-15
        for (int v = 7; v < 15; v++) {
            std::string voiceLabel = "Voice " + std::to_string(v + 1);
            configInput(POLY_DNA_VOICE_8_LEN_INPUT + (v - 7) * 2,
                       voiceLabel + " Length CV");
            configInput(POLY_DNA_VOICE_8_OFF_INPUT + (v - 7) * 2,
                       voiceLabel + " Offset CV");
        }

        // Scramble buttons - randomize length and offset for each voice
        configButton(StraitSandsExpanderIds::SCRAMBLE_ALL_PARAM, "Scramble ALL");
        for (int v = 0; v < 15; v++) {
            configButton(StraitSandsExpanderIds::SCRAMBLE_VOICE_1 + v,
                        "Scramble Voice " + std::to_string(v + 1));
        }
        
        // Reset buttons - restore defaults
        configButton(StraitSandsExpanderIds::RESET_ALL_PARAM, "Reset ALL");
        for (int v = 0; v < 15; v++) {
            configButton(StraitSandsExpanderIds::RESET_VOICE_1 + v,
                        "Reset Voice " + std::to_string(v + 1));
        }
        
        // Gate inputs for scramble/reset external control
        configInput(StraitSandsExpanderIds::SCRAMBLE_ALL_INPUT, "Scramble ALL Gate");
        for (int v = 0; v < 15; v++) {
            configInput(StraitSandsExpanderIds::SCRAMBLE_VOICE_1_INPUT + v,
                       "Scramble Voice " + std::to_string(v + 1) + " Gate");
        }
        
        configInput(StraitSandsExpanderIds::RESET_ALL_INPUT, "Reset ALL Gate");
        for (int v = 0; v < 15; v++) {
            configInput(StraitSandsExpanderIds::RESET_VOICE_1_INPUT + v,
                       "Reset Voice " + std::to_string(v + 1) + " Gate");
        }
    }

    void process(const ProcessArgs& args) override {}
};
