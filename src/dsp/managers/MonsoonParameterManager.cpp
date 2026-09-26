#include "MonsoonParameterManager.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonInterchangeExpander.hpp"
#include "../../MonsoonStraitsExpander.hpp"

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
    // Param range is 0..7 (8 note values, indices 0..7) — was mistakenly read/
    // clamped to 0..8 (an index 8 doesn't exist) with junction scaled by 8.
    float v = readParam_(NOTE_VALUE_PARAM, 0.f, 7.f);
    v = clampv(v + cv2Offsets[0] + junctionOffsets[0]*7.f, 0.f, 7.f);
    return v;
}

float ParameterManager::getVariation() const {
    float v = readParam_(VARIATION_PARAM, 0.f, 1.f);
    v = clampv(v + cv2Offsets[1] + junctionOffsets[1], 0.f, 1.f);
    return v;
}

float ParameterManager::getLegato() const {
    float v = readParam_(LEGATO_PARAM, 0.f, 1.f);
    v = clampv(v + cv2Offsets[2] + junctionOffsets[2], 0.f, 1.f);
    return v;
}

// UNCLAMPED mono rest/accent: knob + all local modulation (CV2 + Junction [+ direct accent CV]),
// summed with NO clamp. Causeway's mono mod is added on top downstream (getEffectiveMono*), and the
// result is clamped ONCE there. Clamping each term as it's added silently discards headroom — e.g.
// Junction pushing rest to 1.4 would truncate to 1.0, so a later negative Causeway CV would pull down
// from 1.0 instead of 1.4, making the result order-dependent. Sum all mods, clamp the end result.
// (Same principle as the clampSpread per-term→end fix.)
float ParameterManager::getRestUnclamped() const {
    // Monsoon's OWN mono rest knob is authoritative. When a Straits expander is attached, its voice-1
    // knob MIRRORS this value (display-only), rather than overriding it.
    return readParam_(REST_PARAM, 0.f, 1.f) + cv2Offsets[3] + junctionOffsets[3];
}

float ParameterManager::getAccentUnclamped() const {
    // Monsoon's own mono accent knob is authoritative (Straits voice-1 mirrors it — see getRest note).
    // Direct CV input: 0–10V = 0–100%.
    return readParam_(ACCENT_KNOB, 0.f, 1.f)
         + readInput_(ACCENT_CV_INPUT) * 0.1f
         + junctionOffsets[4] + cv2Offsets[4];
}

// Clamped convenience forms — for callers that want the local-modulation result on its own (e.g. the
// modViz base test). The ENGINE path must go through getEffectiveMono* so Causeway is included and the
// clamp happens once, at the end.
float ParameterManager::getRest() const {
    return clampv(getRestUnclamped(), 0.f, 1.f);
}

float ParameterManager::getAccent() const {
    return clampv(getAccentUnclamped(), 0.f, 1.f);
}

// Task 4 (QMIX): the level the mono voice's q-mix draw is thresholded against, mirroring
// getRest()/getAccent() (0..1 probability domain). Rack param auto-serialized; no CV yet.
float ParameterManager::getQmixLevel() const {
    return clampv(readParam_(QMIX_LEVEL_PARAM, 0.f, 1.f), 0.f, 1.f);
}

bool ParameterManager::anyPitchModulated() const {
    for (int i = 0; i < 12; ++i)
        if (std::fabs(getSemitone(i) - readParam_(SEMI0_PARAM + i, 0.f, 1.f)) > 1e-4f) return true;
    if (std::fabs(getOctaveLo() - readParam_(OCT_LO_PARAM, 0.f, 8.f)) > 1e-4f) return true;
    if (std::fabs(getOctaveHi() - readParam_(OCT_HI_PARAM, 0.f, 8.f)) > 1e-4f) return true;
    return false;
}

// Per-lane: 0..11 = semitones, 12 = octaveLo, 13 = octaveHi. Each light slider's
// arc gates on its OWN lane so an unmodulated slider never draws even when a
// sibling pitch lane is modulated (the group-trail bug).
bool ParameterManager::pitchLaneModulated(int lane) const {
    if (lane >= 0 && lane < 12)
        return std::fabs(getSemitone(lane) - readParam_(SEMI0_PARAM + lane, 0.f, 1.f)) > 1e-4f;
    if (lane == 12)
        return std::fabs(getOctaveLo() - readParam_(OCT_LO_PARAM, 0.f, 8.f)) > 1e-4f;
    if (lane == 13)
        return std::fabs(getOctaveHi() - readParam_(OCT_HI_PARAM, 0.f, 8.f)) > 1e-4f;
    return false;
}

float ParameterManager::getTranspose() const {
    return readParam_(TRANSPOSE_PARAM, -12.f, 12.f);
}

float ParameterManager::getRhythmSlew() const { return clampv(readParam_(DICE_SLEW_R_PARAM, -1.f, 1.f) + cv3Offsets[0], -1.f, 1.f); }
float ParameterManager::getMelodySlew() const { return clampv(readParam_(DICE_SLEW_M_PARAM, -1.f, 1.f) + cv3Offsets[1], -1.f, 1.f); }
float ParameterManager::getRhythmMix() const { return clampv(readParam_(RHYTHM_MIX_PARAM, 0.f, 1.f) + cv3Offsets[2], 0.f, 1.f); }
float ParameterManager::getMelodyMix() const { return clampv(readParam_(MELODY_MIX_PARAM, 0.f, 1.f) + cv3Offsets[3], 0.f, 1.f); }
// Task 4 (QMIX): mirror R/M slew/mix; CV3 lanes 4 (slew) and 5 (mix), appended so R/M indices stay stable.
float ParameterManager::getQmixSlew() const { return clampv(readParam_(DICE_SLEW_Q_PARAM, -1.f, 1.f) + cv3Offsets[4], -1.f, 1.f); }
float ParameterManager::getQmixMix() const { return clampv(readParam_(QMIX_MIX_PARAM, 0.f, 1.f) + cv3Offsets[5], 0.f, 1.f); }

// ──── Octave Range Getters ──────────────────────────────────────────────────

float ParameterManager::getOctaveLo() const {
    float v = readParam_(OCT_LO_PARAM, 0.f, 8.f);
    
    // Apply expander CV if connected AND this Interchange targets Monsoon (§16 item 1: an AUTO
    // Interchange bound to a Micro drives the Micro's weights instead, so it must NOT also push
    // Monsoon's octave faders — drivesMonsoonCached() gates that).
    if (cachedExpander && *cachedExpander && (*cachedExpander)->drivesMonsoonCached()
        && (*cachedExpander)->inputs[EXPANDER_OCT_LO_CV_INPUT].isConnected()) {
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
    
    // Apply expander CV if connected AND targeting Monsoon (§16 item 1 — see getOctaveLo).
    if (cachedExpander && *cachedExpander && (*cachedExpander)->drivesMonsoonCached()
        && (*cachedExpander)->inputs[EXPANDER_OCT_HI_CV_INPUT].isConnected()) {
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
    
    // Apply expander CV if connected AND targeting Monsoon (§16 item 1 — see getOctaveLo). When an
    // AUTO Interchange is bound to a Micro it modulates the Micro's weight faders instead; the Micro
    // is the single writer there, so Monsoon's semi faders must not also move.
    if (cachedExpander && *cachedExpander && (*cachedExpander)->drivesMonsoonCached()) {
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

float ParameterManager::getPolyAccent(int voiceIdx) const {
    // Per-voice accent probability (accent as a poly lane), mirroring getPolyRest EXACTLY.
    // BUG FIX: this previously used readParam_(POLY_ACCENT_PARAM_1 + voiceIdx) — the HOST param
    // space — but the per-voice poly accent params live on the poly-voice EXPANDER (Straits
    // East), same as the poly rest params. So readParam_ never saw the expander's accent knobs
    // and returned ~0, so poly voices NEVER accented even with accent turned up on East. Read
    // from cachedPolyVoiceExpander like getPolyRest does. (Per-voice CV×attenuverter modulation
    // is still added on top in Monsoon's process loop, exactly like rest.)
    if (voiceIdx < 0 || voiceIdx > 14) return 0.f;

    float v = 0.f;  // default: no accent

    if (cachedPolyVoiceExpander && *cachedPolyVoiceExpander) {
        auto& params = (*cachedPolyVoiceExpander)->params;
        auto& inputs = (*cachedPolyVoiceExpander)->inputs;

        int paramId = POLY_ACCENT_PARAM_1 + voiceIdx;
        if (paramId < (int)params.size()) {
            v = params[paramId].getValue();
        }
        // shared accent CV input (0–10V → 0–1 additional), mirroring poly rest's CV add
        int cvInputId = POLY_ACCENT_CV_INPUT;
        if (cvInputId < (int)inputs.size()) {
            float cv = inputs[cvInputId].getNormalVoltage(0.f);
            v += cv / 10.0f;
        }
    }

    return clampv(v, 0.f, 1.f);
}

// Per-voice q-mix LEVEL (Task 4 poly), mirroring getPolyRest EXACTLY. Reads the Straits
// expander's POLY_QMIX_PARAM_* knobs (per-voice). Unlike rest/accent there is NO shared q-mix
// CV input yet, so no CV add — just the knob (matching the mono q-mix path, which is also
// Rack-param only, no CV). Voice-1/mono q-mix level is Monsoon's own QMIX_LEVEL_PARAM.
float ParameterManager::getPolyQmixLevel(int voiceIdx) const {
    if (voiceIdx < 0 || voiceIdx > 14) return 0.f;

    float v = 0.f;  // default: never generated → always quantised (legacy behaviour)

    if (cachedPolyVoiceExpander && *cachedPolyVoiceExpander) {
        auto& params = (*cachedPolyVoiceExpander)->params;

        int paramId = POLY_QMIX_PARAM_1 + voiceIdx;
        if (paramId < (int)params.size()) {
            v = params[paramId].getValue();
        }
    }

    return clampv(v, 0.f, 1.f);
}

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
