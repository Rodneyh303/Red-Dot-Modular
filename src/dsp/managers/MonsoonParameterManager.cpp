#include "MonsoonParameterManager.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonInterchangeExpander.hpp"
#include "../../MonsoonStraitsEastExpander.hpp"

using namespace rack;
using namespace MonsoonIds;

// ──── Helper Methods ────────────────────────────────────────────────────────

float ParameterManager::readParam_(int paramId, float minVal, float maxVal) const {
    if (!mainModule) return minVal;
    auto& params = mainModule->params;
    if (paramId < 0 || paramId >= (int)params.size()) return minVal;
    
    float v = params[paramId].getValue();
    return clampv(v, minVal, maxVal);
}

float ParameterManager::readInput_(int inputId) const {
    if (!mainModule) return 0.f;
    auto& inputs = mainModule->inputs;
    if (inputId < 0 || inputId >= (int)inputs.size()) return 0.f;
    
    return inputs[inputId].getVoltage();
}

// ──── Core Parameter Getters ────────────────────────────────────────────────

float ParameterManager::getNoteValue() const {
    float v = readParam_(NOTE_VALUE_PARAM, 0.f, 8.f);
    v = clampv(v + cv2Offsets[0], 0.f, 8.f);
    return v;
}

float ParameterManager::getVariation() const {
    float v = readParam_(VARIATION_PARAM, 0.f, 1.f);
    v = clampv(v + cv2Offsets[1], 0.f, 1.f);
    return v;
}

float ParameterManager::getLegato() const {
    float v = readParam_(LEGATO_PARAM, 0.f, 1.f);
    v = clampv(v + cv2Offsets[2], 0.f, 1.f);
    return v;
}

float ParameterManager::getRest() const {
    float v = readParam_(REST_PARAM, 0.f, 1.f);
    v = clampv(v + cv2Offsets[3], 0.f, 1.f);
    return v;
}

float ParameterManager::getAccent() const {
    float v = readParam_(ACCENT_KNOB, 0.f, 1.f);
    // Direct CV input: 0–10V = 0–100%
    float cv = readInput_(ACCENT_CV_INPUT);
    v += cv * 0.1f;
    return clampv(v, 0.f, 1.f);
}

float ParameterManager::getTranspose() const {
    return readParam_(TRANSPOSE_PARAM, -12.f, 12.f);
}

float ParameterManager::getRhythmSlew() const { return readParam_(DICE_SLEW_R_PARAM, 0.f, 1.f); }
float ParameterManager::getMelodySlew() const { return readParam_(DICE_SLEW_M_PARAM, 0.f, 1.f); }
float ParameterManager::getRhythmMix() const { return readParam_(RHYTHM_MIX_PARAM, 0.f, 1.f); }
float ParameterManager::getMelodyMix() const { return readParam_(MELODY_MIX_PARAM, 0.f, 1.f); }

// ──── Octave Range Getters ──────────────────────────────────────────────────

float ParameterManager::getOctaveLo() const {
    float v = readParam_(OCT_LO_PARAM, 0.f, 8.f);
    
    // Apply expander CV if connected
    if (cachedExpander && *cachedExpander && (*cachedExpander)->inputs[EXPANDER_OCT_LO_CV_INPUT].isConnected()) {
        float att = (*cachedExpander)->params[EXPANDER_OCT_LO_ATTENUVERTER].getValue();
        float cv = (*cachedExpander)->inputs[EXPANDER_OCT_LO_CV_INPUT].getVoltage();
        v += (cv * att) / 10.0f * 8.0f;
    }
    
    // Apply transient CV1 offset (never persisted)
    v += cv1LoOffset;
    
    return clampv(v, 0.f, 8.f);
}

float ParameterManager::getOctaveHi() const {
    float v = readParam_(OCT_HI_PARAM, 0.f, 8.f);
    
    // Apply expander CV if connected
    if (cachedExpander && *cachedExpander && (*cachedExpander)->inputs[EXPANDER_OCT_HI_CV_INPUT].isConnected()) {
        float att = (*cachedExpander)->params[EXPANDER_OCT_HI_ATTENUVERTER].getValue();
        float cv = (*cachedExpander)->inputs[EXPANDER_OCT_HI_CV_INPUT].getVoltage();
        v += (cv * att) / 10.0f * 8.0f;
    }
    
    // Apply transient CV1 offset (never persisted)
    v += cv1HiOffset;
    
    return clampv(v, 0.f, 8.f);
}

// ──── Semitone Probability Getters ──────────────────────────────────────────

float ParameterManager::getSemitone(int semIdx) const {
    if (semIdx < 0 || semIdx > 11) return 0.f;
    
    if (!mainModule) return 0.f;
    
    // Get base semitone probability from main knob
    float v = readParam_(SEMI0_PARAM + semIdx, 0.f, 1.f);
    
    // Apply expander CV if connected
    if (cachedExpander && *cachedExpander) {
        auto& expanderParams = (*cachedExpander)->params;
        auto& expanderInputs = (*cachedExpander)->inputs;
        
        int cvInputId = EXPANDER_SEMI_CV_INPUT_0 + semIdx;
        int attenuverterId = EXPANDER_SEMI_ATTENUVERTER_0 + semIdx;
        
        if (cvInputId < (int)expanderInputs.size() && expanderInputs[cvInputId].isConnected()) {
            float att = attenuverterId < (int)expanderParams.size() ? 
                        expanderParams[attenuverterId].getValue() : 1.f;
            float cv = expanderInputs[cvInputId].getVoltage();
            v += (cv * att) / 10.0f;
        }
    }
    
    return clampv(v, 0.f, 1.f);
}

// ──── Poly Voice Rest Probability ───────────────────────────────────────────

float ParameterManager::getPolyRest(int voiceIdx) const {
    if (voiceIdx < 0 || voiceIdx > 14) return 0.1f;
    
    float v = 0.1f;  // Default fallback
    
    if (cachedPolyVoiceExpander && *cachedPolyVoiceExpander) {
        auto& params = (*cachedPolyVoiceExpander)->params;
        auto& inputs = (*cachedPolyVoiceExpander)->inputs;
        
        // Get rest probability for this voice
        int paramId = POLY_REST_PARAM_1 + voiceIdx;
        if (paramId < (int)params.size()) {
            v = params[paramId].getValue();
        }
        
        // Add poly rest CV input (shared, 0–10V = 0–100% additional rest)
        int cvInputId = POLY_REST_CV_INPUT;
        if (cvInputId < (int)inputs.size()) {
            float cv = inputs[cvInputId].getNormalVoltage(0.f);
            v += cv / 10.0f;
        }
    }
    
    return clampv(v, 0.f, 1.f);
}
