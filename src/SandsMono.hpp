#pragma once

/**
 * SandsMono.hpp
 * 
 * Sands Mono Expander - Global DNA Control
 * 
 * Connected to the right side of Monsoon, Sands Mono provides:
 * - Rest DNA controls (Length, Offset, Rotation)
 * - Melody DNA controls (Length, Offset, Rotation)
 * - Octave DNA controls (Length, Offset, Rotation)
 * - Scramble and Reset buttons
 * 
 * DNA controls are GLOBAL - they apply to all 16 voices equally.
 * (Per-voice DNA overrides are provided by Straits East/West Sands)
 * 
 * 12 HP module
 */

#include "plugin.hpp"
#include "dsp/dna/DNAEngine.hpp"

namespace RDM {

struct SandsMono : Module {
    enum ParamId {
        // ===== REST DNA =====
        DNA_REST_LEN_PARAM,
        DNA_REST_OFF_PARAM,
        DNA_REST_ROT_PARAM,
        
        // ===== MELODY DNA =====
        DNA_MELODY_LEN_PARAM,
        DNA_MELODY_OFF_PARAM,
        DNA_MELODY_ROT_PARAM,
        
        // ===== OCTAVE DNA =====
        DNA_OCTAVE_LEN_PARAM,
        DNA_OCTAVE_OFF_PARAM,
        DNA_OCTAVE_ROT_PARAM,
        
        // ===== BUTTONS =====
        DNA_SCRAMBLE_ALL_PARAM,
        DNA_RESET_ALL_PARAM,
        
        PARAMS_LEN
    };
    
    enum InputId {
        // Gate inputs for external scramble/reset triggers
        SCRAMBLE_INPUT,
        RESET_INPUT,
        
        INPUTS_LEN
    };
    
    enum OutputId {
        OUTPUTS_LEN
    };
    
    enum LightId {
        SCRAMBLE_LIGHT,
        RESET_LIGHT,
        
        LIGHTS_LEN
    };
    
    // ===== DNA ENGINE =====
    DNAEngine dnaEngine;
    
    // ===== EXPANDER COMMUNICATION =====
    struct ExpanderMessage {
        // Send to Monsoon
        float restDNAWindow[DNAWindow::WINDOW_SIZE];    // Current rest DNA
        float melodyDNAWindow[DNAWindow::WINDOW_SIZE];  // Current melody DNA
        float octaveDNAWindow[DNAWindow::WINDOW_SIZE];  // Current octave DNA
        
        bool hasMonoSands = true;  // Flag that this expander exists
    };
    
    ExpanderMessage expanderMessage;
    
    SandsMono() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // ===== REST DNA CONTROLS =====
        configParam(DNA_REST_LEN_PARAM, 0.0f, 1.0f, 0.5f,
            "Rest DNA Length", " (1-64 steps)");
        configParam(DNA_REST_OFF_PARAM, 0.0f, 1.0f, 0.0f,
            "Rest DNA Offset");
        configParam(DNA_REST_ROT_PARAM, 0.0f, 1.0f, 0.0f,
            "Rest DNA Rotation");
        
        // ===== MELODY DNA CONTROLS =====
        configParam(DNA_MELODY_LEN_PARAM, 0.0f, 1.0f, 0.375f,
            "Melody DNA Length", " (1-64 steps)");
        configParam(DNA_MELODY_OFF_PARAM, 0.0f, 1.0f, 0.0f,
            "Melody DNA Offset");
        configParam(DNA_MELODY_ROT_PARAM, 0.0f, 1.0f, 0.0f,
            "Melody DNA Rotation");
        
        // ===== OCTAVE DNA CONTROLS =====
        configParam(DNA_OCTAVE_LEN_PARAM, 0.0f, 1.0f, 0.25f,
            "Octave DNA Length", " (1-64 steps)");
        configParam(DNA_OCTAVE_OFF_PARAM, 0.0f, 1.0f, 0.0f,
            "Octave DNA Offset");
        configParam(DNA_OCTAVE_ROT_PARAM, 0.0f, 1.0f, 0.0f,
            "Octave DNA Rotation");
        
        // ===== BUTTONS =====
        configParam(DNA_SCRAMBLE_ALL_PARAM, 0.0f, 1.0f, 0.0f,
            "Scramble All DNA");
        configParam(DNA_RESET_ALL_PARAM, 0.0f, 1.0f, 0.0f,
            "Reset All DNA");
        
        // ===== INPUTS =====
        configInput(SCRAMBLE_INPUT, "Scramble Trigger");
        configInput(RESET_INPUT, "Reset Trigger");
        
        // Initialize expander message
        std::memset(&expanderMessage, 0, sizeof(expanderMessage));
        expanderMessage.hasMonoSands = true;
    }
    
    void process(const ProcessArgs& args) override {
        // ===== UPDATE DNA PARAMETERS FROM KNOBS =====
        
        DNAEngine::DNAParameters restParams{
            params[DNA_REST_LEN_PARAM].getValue(),
            params[DNA_REST_OFF_PARAM].getValue(),
            params[DNA_REST_ROT_PARAM].getValue()
        };
        dnaEngine.setParameters(DNAEngine::REST, restParams);
        
        DNAEngine::DNAParameters melodyParams{
            params[DNA_MELODY_LEN_PARAM].getValue(),
            params[DNA_MELODY_OFF_PARAM].getValue(),
            params[DNA_MELODY_ROT_PARAM].getValue()
        };
        dnaEngine.setParameters(DNAEngine::MELODY, melodyParams);
        
        DNAEngine::DNAParameters octaveParams{
            params[DNA_OCTAVE_LEN_PARAM].getValue(),
            params[DNA_OCTAVE_OFF_PARAM].getValue(),
            params[DNA_OCTAVE_ROT_PARAM].getValue()
        };
        dnaEngine.setParameters(DNAEngine::OCTAVE, octaveParams);
        
        // ===== HANDLE BUTTON PRESSES =====
        
        // Scramble All
        if (params[DNA_SCRAMBLE_ALL_PARAM].getValue() > 0.5f ||
            inputs[SCRAMBLE_INPUT].getVoltage() > 2.0f) {
            dnaEngine.scrambleDimension(DNAEngine::REST);
            dnaEngine.scrambleDimension(DNAEngine::MELODY);
            dnaEngine.scrambleDimension(DNAEngine::OCTAVE);
            lights[SCRAMBLE_LIGHT].setBrightness(1.0f);
        } else {
            lights[SCRAMBLE_LIGHT].setBrightness(0.0f);
        }
        
        // Reset All
        if (params[DNA_RESET_ALL_PARAM].getValue() > 0.5f ||
            inputs[RESET_INPUT].getVoltage() > 2.0f) {
            dnaEngine.resetAll();
            lights[RESET_LIGHT].setBrightness(1.0f);
        } else {
            lights[RESET_LIGHT].setBrightness(0.0f);
        }
        
        // ===== UPDATE EXPANDER MESSAGE =====
        
        // Copy current DNA windows to expander message
        const DNAWindow& restWindow = dnaEngine.getWindow(DNAEngine::REST);
        const DNAWindow& melodyWindow = dnaEngine.getWindow(DNAEngine::MELODY);
        const DNAWindow& octaveWindow = dnaEngine.getWindow(DNAEngine::OCTAVE);
        
        for (int i = 0; i < DNAWindow::WINDOW_SIZE; i++) {
            expanderMessage.restDNAWindow[i] = restWindow.probabilities[i];
            expanderMessage.melodyDNAWindow[i] = melodyWindow.probabilities[i];
            expanderMessage.octaveDNAWindow[i] = octaveWindow.probabilities[i];
        }
    }
    
    json_t* dataToJson() override {
        json_t* root = json_object();
        
        // Save DNA parameters
        json_object_set_new(root, "restLen",
            json_real(params[DNA_REST_LEN_PARAM].getValue()));
        json_object_set_new(root, "restOff",
            json_real(params[DNA_REST_OFF_PARAM].getValue()));
        json_object_set_new(root, "restRot",
            json_real(params[DNA_REST_ROT_PARAM].getValue()));
        
        json_object_set_new(root, "melodyLen",
            json_real(params[DNA_MELODY_LEN_PARAM].getValue()));
        json_object_set_new(root, "melodyOff",
            json_real(params[DNA_MELODY_OFF_PARAM].getValue()));
        json_object_set_new(root, "melodyRot",
            json_real(params[DNA_MELODY_ROT_PARAM].getValue()));
        
        json_object_set_new(root, "octaveLen",
            json_real(params[DNA_OCTAVE_LEN_PARAM].getValue()));
        json_object_set_new(root, "octaveOff",
            json_real(params[DNA_OCTAVE_OFF_PARAM].getValue()));
        json_object_set_new(root, "octaveRot",
            json_real(params[DNA_OCTAVE_ROT_PARAM].getValue()));
        
        return root;
    }
    
    void dataFromJson(json_t* root) override {
        // Load DNA parameters
        json_t* j;
        
        if ((j = json_object_get(root, "restLen")))
            params[DNA_REST_LEN_PARAM].setValue(json_real_value(j));
        if ((j = json_object_get(root, "restOff")))
            params[DNA_REST_OFF_PARAM].setValue(json_real_value(j));
        if ((j = json_object_get(root, "restRot")))
            params[DNA_REST_ROT_PARAM].setValue(json_real_value(j));
        
        if ((j = json_object_get(root, "melodyLen")))
            params[DNA_MELODY_LEN_PARAM].setValue(json_real_value(j));
        if ((j = json_object_get(root, "melodyOff")))
            params[DNA_MELODY_OFF_PARAM].setValue(json_real_value(j));
        if ((j = json_object_get(root, "melodyRot")))
            params[DNA_MELODY_ROT_PARAM].setValue(json_real_value(j));
        
        if ((j = json_object_get(root, "octaveLen")))
            params[DNA_OCTAVE_LEN_PARAM].setValue(json_real_value(j));
        if ((j = json_object_get(root, "octaveOff")))
            params[DNA_OCTAVE_OFF_PARAM].setValue(json_real_value(j));
        if ((j = json_object_get(root, "octaveRot")))
            params[DNA_OCTAVE_ROT_PARAM].setValue(json_real_value(j));
    }
};

}  // namespace RDM
