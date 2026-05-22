#pragma once

/**
 * StraitsEastSands.hpp
 * 
 * Straits East Sands Expander - Per-Voice DNA Control (Voices 2-8)
 * 
 * Connected to the right of Straits East outputs.
 * Provides per-voice DNA control for the 7 voices (2-8) output by Straits East.
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

struct StraitsEastSands : Module {
    enum ParamId {
        // Voice 2 parameters
        V2_REST_LEN_PARAM, V2_REST_OFF_PARAM, V2_REST_ROT_PARAM,
        V2_MELODY_LEN_PARAM, V2_MELODY_OFF_PARAM, V2_MELODY_ROT_PARAM,
        V2_OCTAVE_LEN_PARAM, V2_OCTAVE_OFF_PARAM, V2_OCTAVE_ROT_PARAM,
        
        // Voice 3 parameters
        V3_REST_LEN_PARAM, V3_REST_OFF_PARAM, V3_REST_ROT_PARAM,
        V3_MELODY_LEN_PARAM, V3_MELODY_OFF_PARAM, V3_MELODY_ROT_PARAM,
        V3_OCTAVE_LEN_PARAM, V3_OCTAVE_OFF_PARAM, V3_OCTAVE_ROT_PARAM,
        
        // Voice 4 parameters
        V4_REST_LEN_PARAM, V4_REST_OFF_PARAM, V4_REST_ROT_PARAM,
        V4_MELODY_LEN_PARAM, V4_MELODY_OFF_PARAM, V4_MELODY_ROT_PARAM,
        V4_OCTAVE_LEN_PARAM, V4_OCTAVE_OFF_PARAM, V4_OCTAVE_ROT_PARAM,
        
        // Voice 5 parameters
        V5_REST_LEN_PARAM, V5_REST_OFF_PARAM, V5_REST_ROT_PARAM,
        V5_MELODY_LEN_PARAM, V5_MELODY_OFF_PARAM, V5_MELODY_ROT_PARAM,
        V5_OCTAVE_LEN_PARAM, V5_OCTAVE_OFF_PARAM, V5_OCTAVE_ROT_PARAM,
        
        // Voice 6 parameters
        V6_REST_LEN_PARAM, V6_REST_OFF_PARAM, V6_REST_ROT_PARAM,
        V6_MELODY_LEN_PARAM, V6_MELODY_OFF_PARAM, V6_MELODY_ROT_PARAM,
        V6_OCTAVE_LEN_PARAM, V6_OCTAVE_OFF_PARAM, V6_OCTAVE_ROT_PARAM,
        
        // Voice 7 parameters
        V7_REST_LEN_PARAM, V7_REST_OFF_PARAM, V7_REST_ROT_PARAM,
        V7_MELODY_LEN_PARAM, V7_MELODY_OFF_PARAM, V7_MELODY_ROT_PARAM,
        V7_OCTAVE_LEN_PARAM, V7_OCTAVE_OFF_PARAM, V7_OCTAVE_ROT_PARAM,
        
        // Voice 8 parameters
        V8_REST_LEN_PARAM, V8_REST_OFF_PARAM, V8_REST_ROT_PARAM,
        V8_MELODY_LEN_PARAM, V8_MELODY_OFF_PARAM, V8_MELODY_ROT_PARAM,
        V8_OCTAVE_LEN_PARAM, V8_OCTAVE_OFF_PARAM, V8_OCTAVE_ROT_PARAM,
        
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
    DNAEngine dnaEngines[7];  // Voices 2-8
    
    // ===== EXPANDER MESSAGE =====
    struct ExpanderMessage {
        // Per-voice DNA windows (7 voices × 3 dimensions × 64 steps)
        float voiceDNAWindows[7][3][DNAWindow::WINDOW_SIZE];
        bool hasStraitsEastSands = true;
    };
    
    ExpanderMessage expanderMessage;
    
    StraitsEastSands() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // Configure parameters for voices 2-8
        for (int v = 2; v <= 8; v++) {
            int voiceIdx = v - 2;  // 0-6
            
            // Rest DNA
            configParam(V2_REST_LEN_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.5f,
                string::f("Voice %d Rest DNA Length", v), " (1-64)");
            configParam(V2_REST_OFF_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Rest DNA Offset", v));
            configParam(V2_REST_ROT_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Rest DNA Rotation", v));
            
            // Melody DNA
            configParam(V2_MELODY_LEN_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.375f,
                string::f("Voice %d Melody DNA Length", v), " (1-64)");
            configParam(V2_MELODY_OFF_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Melody DNA Offset", v));
            configParam(V2_MELODY_ROT_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Melody DNA Rotation", v));
            
            // Octave DNA
            configParam(V2_OCTAVE_LEN_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.25f,
                string::f("Voice %d Octave DNA Length", v), " (1-64)");
            configParam(V2_OCTAVE_OFF_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
                string::f("Voice %d Octave DNA Offset", v));
            configParam(V2_OCTAVE_ROT_PARAM + (voiceIdx * 9), 0.0f, 1.0f, 0.0f,
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
        
        for (int v = 2; v <= 8; v++) {
            int voiceIdx = v - 2;
            
            // Get parameters for this voice
            int restLenParam = V2_REST_LEN_PARAM + (voiceIdx * 9);
            int melodyLenParam = V2_MELODY_LEN_PARAM + (voiceIdx * 9);
            int octaveLenParam = V2_OCTAVE_LEN_PARAM + (voiceIdx * 9);
            
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
            for (int v = 0; v < 7; v++) {
                dnaEngines[v].scrambleDimension(DNAEngine::REST);
                dnaEngines[v].scrambleDimension(DNAEngine::MELODY);
                dnaEngines[v].scrambleDimension(DNAEngine::OCTAVE);
            }
        }
        
        if (params[RESET_ALL_PARAM].getValue() > 0.5f ||
            inputs[RESET_INPUT].getVoltage() > 2.0f) {
            for (int v = 0; v < 7; v++) {
                dnaEngines[v].resetAll();
            }
        }
        
        // ===== UPDATE EXPANDER MESSAGE =====
        
        for (int v = 0; v < 7; v++) {
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
        for (int v = 2; v <= 8; v++) {
            int voiceIdx = v - 2;
            string voiceKey = string::f("v%d", v);
            json_t* voiceObj = json_object();
            
            // Rest DNA
            json_object_set_new(voiceObj, "rest_len",
                json_real(params[V2_REST_LEN_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "rest_off",
                json_real(params[V2_REST_OFF_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "rest_rot",
                json_real(params[V2_REST_ROT_PARAM + (voiceIdx * 9)].getValue()));
            
            // Melody DNA
            json_object_set_new(voiceObj, "melody_len",
                json_real(params[V2_MELODY_LEN_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "melody_off",
                json_real(params[V2_MELODY_OFF_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "melody_rot",
                json_real(params[V2_MELODY_ROT_PARAM + (voiceIdx * 9)].getValue()));
            
            // Octave DNA
            json_object_set_new(voiceObj, "octave_len",
                json_real(params[V2_OCTAVE_LEN_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "octave_off",
                json_real(params[V2_OCTAVE_OFF_PARAM + (voiceIdx * 9)].getValue()));
            json_object_set_new(voiceObj, "octave_rot",
                json_real(params[V2_OCTAVE_ROT_PARAM + (voiceIdx * 9)].getValue()));
            
            json_object_set_new(root, voiceKey.c_str(), voiceObj);
        }
        
        return root;
    }
    
    void dataFromJson(json_t* root) override {
        // Load all parameters
        for (int v = 2; v <= 8; v++) {
            int voiceIdx = v - 2;
            string voiceKey = string::f("v%d", v);
            json_t* voiceObj = json_object_get(root, voiceKey.c_str());
            
            if (!voiceObj) continue;
            
            json_t* j;
            
            // Rest DNA
            if ((j = json_object_get(voiceObj, "rest_len")))
                params[V2_REST_LEN_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "rest_off")))
                params[V2_REST_OFF_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "rest_rot")))
                params[V2_REST_ROT_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            
            // Melody DNA
            if ((j = json_object_get(voiceObj, "melody_len")))
                params[V2_MELODY_LEN_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "melody_off")))
                params[V2_MELODY_OFF_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "melody_rot")))
                params[V2_MELODY_ROT_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            
            // Octave DNA
            if ((j = json_object_get(voiceObj, "octave_len")))
                params[V2_OCTAVE_LEN_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "octave_off")))
                params[V2_OCTAVE_OFF_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
            if ((j = json_object_get(voiceObj, "octave_rot")))
                params[V2_OCTAVE_ROT_PARAM + (voiceIdx * 9)].setValue(json_real_value(j));
        }
    }
};

}  // namespace RDM
