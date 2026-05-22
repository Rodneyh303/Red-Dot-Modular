#pragma once

/**
 * SandsMacro.hpp
 * 
 * Sands Macro Expander - Global Spread Interpolation Control
 * 
 * Connected to the right side of Sands Mono, Sands Macro provides:
 * - Per-dimension spread amount (Rest, Melody, Octave)
 * - Spread mode selection (Poly Average or Mono Source)
 * - Context menu for mode toggle
 * 
 * Spread controls how much poly voices blend toward either:
 * - POLY_AVERAGE: Each voice blends toward average of all voices
 * - MONO_SOURCE: Each voice blends toward mono sequencer's value
 * 
 * 16 HP module
 */

#include "plugin.hpp"
#include "dsp/spread/SpreadInterpolation.hpp"

namespace RDM {

struct SandsMacro : Module {
    enum ParamId {
        // Per-dimension spread amounts (vertical sliders)
        SPREAD_REST_PARAM,
        SPREAD_MELODY_PARAM,
        SPREAD_OCTAVE_PARAM,
        
        // Spread mode toggle (context menu only)
        SPREAD_MODE_PARAM,
        
        // Scramble/Reset buttons (for consistency)
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
        SPREAD_MODE_LIGHT,  // Indicates current mode (color-coded)
        
        LIGHTS_LEN
    };
    
    // ===== SPREAD STATE =====
    SpreadState spreadState;
    
    // ===== EXPANDER COMMUNICATION =====
    struct ExpanderMessage {
        // Send to Monsoon via Sands Mono
        float restSpread = 0.0f;
        float melodySpread = 0.0f;
        float octaveSpread = 0.0f;
        SpreadMode mode = SpreadMode::POLY_AVERAGE;
        
        bool hasMacroSands = true;  // Flag that this expander exists
    };
    
    ExpanderMessage expanderMessage;
    
    SandsMacro() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        
        // ===== SPREAD AMOUNT SLIDERS (Vertical) =====
        
        configParam(SPREAD_REST_PARAM, 0.0f, 1.0f, 0.0f,
            "Rest Spread", "%", 0.0f, 100.0f);
        configParam(SPREAD_MELODY_PARAM, 0.0f, 1.0f, 0.0f,
            "Melody Spread", "%", 0.0f, 100.0f);
        configParam(SPREAD_OCTAVE_PARAM, 0.0f, 1.0f, 0.0f,
            "Octave Spread", "%", 0.0f, 100.0f);
        
        // Spread mode (context menu)
        configParam(SPREAD_MODE_PARAM, 0.0f, 1.0f, 0.0f,
            "Spread Mode");
        
        // Buttons
        configParam(SCRAMBLE_ALL_PARAM, 0.0f, 1.0f, 0.0f,
            "Scramble All");
        configParam(RESET_ALL_PARAM, 0.0f, 1.0f, 0.0f,
            "Reset All");
        
        // Inputs
        configInput(SCRAMBLE_INPUT, "Scramble Trigger");
        configInput(RESET_INPUT, "Reset Trigger");
    }
    
    void process(const ProcessArgs& args) override {
        // ===== UPDATE SPREAD MODE =====
        
        int modeValue = (int)(params[SPREAD_MODE_PARAM].getValue() + 0.5f);
        spreadState.mode = (modeValue == 0) ? SpreadMode::POLY_AVERAGE : SpreadMode::MONO_SOURCE;
        
        // Update light to indicate mode
        lights[SPREAD_MODE_LIGHT].setBrightness(
            (spreadState.mode == SpreadMode::POLY_AVERAGE) ? 0.0f : 1.0f
        );
        
        // ===== UPDATE SPREAD AMOUNTS =====
        
        spreadState.restSpread = params[SPREAD_REST_PARAM].getValue();
        spreadState.melodySpread = params[SPREAD_MELODY_PARAM].getValue();
        spreadState.octaveSpread = params[SPREAD_OCTAVE_PARAM].getValue();
        
        // ===== UPDATE EXPANDER MESSAGE =====
        
        expanderMessage.restSpread = spreadState.restSpread;
        expanderMessage.melodySpread = spreadState.melodySpread;
        expanderMessage.octaveSpread = spreadState.octaveSpread;
        expanderMessage.mode = spreadState.mode;
    }
    
    void appendContextMenu(Menu* menu) override {
        menu->addChild(new MenuEntry);
        
        menu->addChild(createIndexPtrSubmenuItem("Spread Mode",
            {"Poly Average", "Mono Source"},
            [this]() { return (int)(params[SPREAD_MODE_PARAM].getValue() + 0.5f); },
            [this](int i) { params[SPREAD_MODE_PARAM].setValue((float)i); }
        ));
        
        menu->addChild(new MenuEntry);
        menu->addChild(createMenuItem("Reset Spread Values",
            "",
            [this]() {
                params[SPREAD_REST_PARAM].setValue(0.0f);
                params[SPREAD_MELODY_PARAM].setValue(0.0f);
                params[SPREAD_OCTAVE_PARAM].setValue(0.0f);
            }
        ));
    }
    
    json_t* dataToJson() override {
        json_t* root = json_object();
        
        json_object_set_new(root, "spreadMode",
            json_integer((int)spreadState.mode));
        json_object_set_new(root, "restSpread",
            json_real(spreadState.restSpread));
        json_object_set_new(root, "melodySpread",
            json_real(spreadState.melodySpread));
        json_object_set_new(root, "octaveSpread",
            json_real(spreadState.octaveSpread));
        
        return root;
    }
    
    void dataFromJson(json_t* root) override {
        json_t* j;
        
        if ((j = json_object_get(root, "spreadMode")))
            spreadState.mode = (SpreadMode)json_integer_value(j);
        if ((j = json_object_get(root, "restSpread")))
            spreadState.restSpread = json_real_value(j);
        if ((j = json_object_get(root, "melodySpread")))
            spreadState.melodySpread = json_real_value(j);
        if ((j = json_object_get(root, "octaveSpread")))
            spreadState.octaveSpread = json_real_value(j);
    }
};

}  // namespace RDM
