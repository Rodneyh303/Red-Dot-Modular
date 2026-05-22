#pragma once

/**
 * StraitsWestSands.hpp
 * 
 * Straits West Sands Expander - Per-Voice DNA Control (Voices 9-16)
 * 
 * Connected to the right of Straits West outputs.
 * Provides per-voice DNA control for the 8 voices (9-16) output by Straits West.
 * 
 * Each voice row has:
 * - Rest DNA: Length, Offset, Rotation
 * - Melody DNA: Length, Offset, Rotation
 * - Octave DNA: Length, Offset, Rotation
 * 
 * 24 HP module
 */

#include "plugin.hpp"
#include "dsp/dna/DNAEngine.hpp"

namespace RDM {

struct StraitsWestSands : Module {
    enum ParamId {
        // Voice 9 parameters
        V9_REST_LEN_PARAM, V9_REST_OFF_PARAM, V9_REST_ROT_PARAM,
        V9_MELODY_LEN_PARAM, V9_MELODY_OFF_PARAM, V9_MELODY_ROT_PARAM,
        V9_OCTAVE_LEN_PARAM, V9_OCTAVE_OFF_PARAM, V9_OCTAVE_ROT_PARAM,
        
        // Voice 10 parameters
        V10_REST_LEN_PARAM, V10_REST_OFF_PARAM, V10_REST_ROT_PARAM,
        V10_MELODY_LEN_PARAM, V10_MELODY_OFF_PARAM, V10_MELODY_ROT_PARAM,
        V10_OCTAVE_LEN_PARAM, V10_OCTAVE_OFF_PARAM, V10_OCTAVE_ROT_PARAM,
        
        // Voice 11 parameters
        V11_REST_LEN_PARAM, V11_REST_OFF_PARAM, V11_REST_ROT_PARAM,
        V11_MELODY_LEN_PARAM, V11_MELODY_OFF_PARAM, V11_MELODY_ROT_PARAM,
        V11_OCTAVE_LEN_PARAM, V11_OCTAVE_OFF_PARAM, V11_OCTAVE_ROT_PARAM,
        
        // Voice 12 parameters
        V12_REST_LEN_PARAM, V12_REST_OFF_PARAM, V12_REST_ROT_PARAM,
        V12_MELODY_LEN_PARAM, V12_MELODY_OFF_PARAM, V12_MELODY_ROT_PARAM,
        V12_OCTAVE_LEN_PARAM, V12_OCTAVE_OFF_PARAM, V12_OCTAVE_ROT_PARAM,
        
        // Voice 13 parameters
        V13_REST_LEN_PARAM, V13_REST_OFF_PARAM, V13_REST_ROT_PARAM,
        V13_MELODY_LEN_PARAM, V13_MELODY_OFF_PARAM, V13_MELODY_ROT_PARAM,
        V13_OCTAVE_LEN_PARAM, V13_OCTAVE_OFF_PARAM, V13_OCTAVE_ROT_PARAM,
        
        // Voice 14 parameters
        V14_REST_LEN_PARAM, V14_REST_OFF_PARAM, V14_REST_ROT_PARAM,
        V14_MELODY_LEN_PARAM, V14_MELODY_OFF_PARAM, V14_MELODY_ROT_PARAM,
        V14_OCTAVE_LEN_PARAM, V14_OCTAVE_OFF_PARAM, V14_OCTAVE_ROT_PARAM,
        
        // Voice 15 parameters
        V15_REST_LEN_PARAM, V15_REST_OFF_PARAM, V15_REST_ROT_PARAM,
        V15_MELODY_LEN_PARAM, V15_MELODY_OFF_PARAM, V15_MELODY_ROT_PARAM,
        V15_OCTAVE_LEN_PARAM, V15_OCTAVE_OFF_PARAM, V15_OCTAVE_ROT_PARAM,
        
        // Voice 16 parameters
        V16_REST_LEN_PARAM, V16_REST_OFF_PARAM, V16_REST_ROT_PARAM,
        V16_MELODY_LEN_PARAM, V16_MELODY_OFF_PARAM, V16_MELODY_ROT_PARAM,
        V16_OCTAVE_LEN_PARAM, V16_OCTAVE_OFF_PARAM, V16_OCTAVE_ROT_PARAM,
        
        // Control buttons
        SCRAMBLE_ALL_PARAM,
        RESET_ALL_PARAM,
        
        PARAMS_LEN
    };
    
    enum InputId {
        SCRAMBLE_INPUT,
        RESET_INPUT,
        
        INPUTS_LEN
    };
    
    enum OutputId {
        OUTPUTS_LEN
    };
    
    enum LightId {
        LIGHTS_LEN
    };
    
    // ===== DNA ENGINES (One per voice) =====
    DNAEngine dnaEngines[8];  // Voices 9-16
    
    // ===== EXPANDER MESSAGE =====
    struct ExpanderMessage {
        // Per-voice DNA windows (8 voices × 3 dimensions × 64 steps)
        float voiceDNAWindows[8][3][DNAWindow::WINDOW_SIZE];
        bool hasStraitsWestSands = true;
    };
    
    ExpanderMessage expanderMessage;
    
    StraitsWestSands() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Configure parameters for voices 9-16
        for (int v = 9; v <= 16; v++) {
            int voiceIdx = v - 9;  // 0-7
            
            // Rest DNA
            configParam(V9_REST_LEN_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.5f,
                string::f("Voice %d Rest DNA Length", v), " (1-64)");
            configParam(V9_REST_OFF_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Rest DNA Offset", v));
            configParam(V9_REST_ROT_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Rest DNA Rotation", v));
            
            // Melody DNA
            configParam(V9_MELODY_LEN_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.375f,
                string::f("Voice %d Melody DNA Length", v), " (1-64)");
            configParam(V9_MELODY_OFF_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Melody DNA Offset", v));
            configParam(V9_MELODY_ROT_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Melody DNA Rotation", v));
            
            // Octave DNA
            configParam(V9_OCTAVE_LEN_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.25f,
                string::f("Voice %d Octave DNA Length", v), " (1-64)");
            configParam(V9_OCTAVE_OFF_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Octave DNA Offset", v));
            configParam(V9_OCTAVE_ROT_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Octave DNA Rotation", v));
        }
        
        // Control buttons
        configParam(SCRAMBLE_ALL_PARAM, 0.0f, 1.0f, 0.0f, "Scramble All");
        configParam(RESET_ALL_PARAM, 0.0f, 1.0f, 0.0f, "Reset All");
        
        // Inputs
        configInput(SCRAMBLE_INPUT, "Scramble Trigger");
        configInput(RESET_INPUT, "Reset Trigger");
    }
    
    void process(const ProcessArgs& args) override {
        // ===== UPDATE DNA PARAMETERS =====
        
        for (int v = 9; v <= 16; v++) {
            int voiceIdx = v - 9;
            
            // Get parameters for this voice
            int restLenParam = V9_REST_LEN_PARAM + (voiceIdx * 9);
            int melodyLenParam = V9_MELODY_LEN_PARAM + (voiceIdx * 9);
            int octaveLenParam = V9_OCTAVE_LEN_PARAM + (voiceIdx * 9);
            
            // Update DNA engines
            DNAEngine::DNAParameters restParams{
                params[restLenParam].getValue(),
                params[restLenParam + 1].getValue(),
                params[restLenParam + 2].getValue()
            };
            dnaEngines[voiceIdx].setParameters(DNAEngine::REST, restParams);
            
            DNAEngine::DNAParameters melodyParams{
                params[melodyLenParam].getValue(),
                params[melodyLenParam + 1].getValue(),
                params[melodyLenParam + 2].getValue()
            };
            dnaEngines[voiceIdx].setParameters(DNAEngine::MELODY, melodyParams);
            
            DNAEngine::DNAParameters octaveParams{
                params[octaveLenParam].getValue(),
                params[octaveLenParam + 1].getValue(),
                params[octaveLenParam + 2].getValue()
            };
            dnaEngines[voiceIdx].setParameters(DNAEngine::OCTAVE, octaveParams);
        }
        
        // ===== HANDLE BUTTON PRESSES =====
        
        if (params[SCRAMBLE_ALL_PARAM].getValue() > 0.5f ||
            inputs[SCRAMBLE_INPUT].getVoltage() > 2.0f) {
            for (int v = 0; v < 8; v++) {
                dnaEngines[v].scrambleDimension(DNAEngine::REST);
                dnaEngines[v].scrambleDimension(DNAEngine::MELODY);
                dnaEngines[v].scrambleDimension(DNAEngine::OCTAVE);
            }
        }
        
        if (params[RESET_ALL_PARAM].getValue() > 0.5f ||
            inputs[RESET_INPUT].getVoltage() > 2.0f) {
            for (int v = 0; v < 8; v++) {
                dnaEngines[v].resetAll();
            }
        }
        
        // ===== UPDATE EXPANDER MESSAGE =====
        
        for (int v = 0; v < 8; v++) {
            // Get DNA windows for this voice
            const DNAWindow& restWindow = dnaEngines[v].getWindow(DNAEngine::REST);
            const DNAWindow& melodyWindow = dnaEngines[v].getWindow(DNAEngine::MELODY);
            const DNAWindow& octaveWindow = dnaEngines[v].getWindow(DNAEngine::OCTAVE);
            
            // Copy to expander message
            for (int i = 0; i < DNAWindow::WINDOW_SIZE; i++) {
                expanderMessage.voiceDNAWindows[v][0][i] = restWindow.probabilities[i];
                expanderMessage.voiceDNAWindows[v][1][i] = melodyWindow.probabilities[i];
                expanderMessage.voiceDNAWindows[v][2][i] = octaveWindow.probabilities[i];
            }
        }
    }
    
    json_t* dataToJson() override {
        json_t* root = json_object();
        
        // Save all parameters
        for (int v = 9; v <= 16; v++) {
            int voiceIdx = v - 9;
            string voiceKey = string::f("v%d", v);
            json_t* voiceObj = json_object();
            
            // Rest DNA
            json_object_set_new(voiceObj, "rest_len",
                json_real(params[V9_REST_LEN_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "rest_off",
                json_real(params[V9_REST_OFF_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "rest_rot",
                json_real(params[V9_REST_ROT_PARAM + (voiceIdx * 9)].getValue()));
            
            // Melody DNA
            json_object_set_new(voiceObj, "melody_len",
                json_real(params[V9_MELODY_LEN_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "melody_off",
                json_real(params[V9_MELODY_OFF_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "melody_rot",
                json_real(params[V9_MELODY_ROT_PARAM + (voiceIdx * 9)].getValue()));
            
            // Octave DNA
            json_object_set_new(voiceObj, "octave_len",
                json_real(params[V9_OCTAVE_LEN_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "octave_off",
                json_real(params[V9_OCTAVE_OFF_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "octave_rot",
                json_real(params[V9_OCTAVE_ROT_PARAM + (voiceIdx * 9)].getValue()));
            
            json_object_set_new(root, voiceKey.c_str(), voiceObj);
        }
        
        return root;
    }
    
    void dataFromJson(json_t* root) override {
        // Load all parameters
        for (int v = 9; v <= 16; v++) {
            int voiceIdx = v - 9;
            string voiceKey = string::f("v%d", v);
            json_t* voiceObj = json_object_get(root, voiceKey.c_str());
            
            if (!voiceObj) continue;
            
            json_t* j;
            
            // Rest DNA
            if ((j = json_object_get(voiceObj, "rest_len")))
                params[V9_REST_LEN_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "rest_off")))
                params[V9_REST_OFF_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "rest_rot")))
                params[V9_REST_ROT_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            
            // Melody DNA
            if ((j = json_object_get(voiceObj, "melody_len")))
                params[V9_MELODY_LEN_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "melody_off")))
                params[V9_MELODY_OFF_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "melody_rot")))
                params[V9_MELODY_ROT_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            
            // Octave DNA
            if ((j = json_object_get(voiceObj, "octave_len")))
                params[V9_OCTAVE_LEN_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "octave_off")))
                params[V9_OCTAVE_OFF_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "octave_rot")))
                params[V9_OCTAVE_ROT_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
        }
    }
};

}  // namespace RDM
