#include "MonsoonSandsManager.hpp"
#include "../../Monsoon.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonSandsExpander.hpp"
#include "../../MonsoonStraitsEastExpander.hpp"

using namespace rack;
using namespace MonsoonIds;

void MonsoonSandsManager::processDNA(const MonsoonExpanderManager& expanderManager) {
    if (expanderManager.cachedDnaExpander) {
        auto processStrand = [&](int pLen, int iLen, int pOff, int iOff, int pRot, int& tLen, int& tOff, int& tRot) {
            float lCV = expanderManager.cachedDnaExpander->inputs[iLen].getNormalVoltage(0.f) * 1.6f;
            float oCV = expanderManager.cachedDnaExpander->inputs[iOff].getNormalVoltage(0.f) * 1.5f;
            tLen = clampv<int>((int)std::round(expanderManager.cachedDnaExpander->params[pLen].getValue() + lCV), 1, 16);
            int rawOff = (int)std::round(expanderManager.cachedDnaExpander->params[pOff].getValue() + oCV);
            tOff = ((rawOff % 16) + 16) % 16;
            int rawRot = (int)std::round(expanderManager.cachedDnaExpander->params[pRot].getValue());
            tRot = ((rawRot % 16) + 16) % 16;
        };

        processStrand(DNA_R_LEN_PARAM, DNA_R_LEN_INPUT, DNA_R_OFF_PARAM, DNA_R_OFF_INPUT, DNA_R_ROT_PARAM, 
                        engine.rhythmLen, engine.rhythmOff, engine.rhythmRot);
        processStrand(DNA_V_LEN_PARAM, DNA_V_LEN_INPUT, DNA_V_OFF_PARAM, DNA_V_OFF_INPUT, DNA_V_ROT_PARAM, 
                        engine.variationLen, engine.variationOff, engine.variationRot);
        processStrand(DNA_L_LEN_PARAM, DNA_L_LEN_INPUT, DNA_L_OFF_PARAM, DNA_L_OFF_INPUT, DNA_L_ROT_PARAM, 
                        engine.legatoLen, engine.legatoOff, engine.legatoRot);
        processStrand(DNA_A_LEN_PARAM, DNA_A_LEN_INPUT, DNA_A_OFF_PARAM, DNA_A_OFF_INPUT, DNA_A_ROT_PARAM, 
                        engine.accentLen, engine.accentOff, engine.accentRot);
        processStrand(DNA_M_LEN_PARAM, DNA_M_LEN_INPUT, DNA_M_OFF_PARAM, DNA_M_OFF_INPUT, DNA_M_ROT_PARAM, 
                        engine.melodyLen, engine.melodyOff, engine.melodyRot);
        processStrand(DNA_O_LEN_PARAM, DNA_O_LEN_INPUT, DNA_O_OFF_PARAM, DNA_O_OFF_INPUT, DNA_O_ROT_PARAM, 
                        engine.octaveLen, engine.octaveOff, engine.octaveRot);

        // Action Triggers
        auto dnaExp = expanderManager.cachedDnaExpander;
        
        #define CHECK_DNA_ACT(suffix, func) \
            if (scrambleParamTrig_##suffix.process(dnaExp->params[DNA_SCRAMBLE_##suffix##_PARAM].getValue())) scramble##func(); \
            if (scrambleInputTrig_##suffix.process(dnaExp->inputs[DNA_SCRAMBLE_##suffix##_INPUT].getVoltage())) scramble##func(); \
            if (resetParamTrig_##suffix.process(dnaExp->params[DNA_RESET_##suffix##_PARAM].getValue())) reset##func(); \
            if (resetInputTrig_##suffix.process(dnaExp->inputs[DNA_RESET_##suffix##_INPUT].getVoltage())) reset##func();

        CHECK_DNA_ACT(ALL, All);
        CHECK_DNA_ACT(R, RhythmGroup);
        CHECK_DNA_ACT(M, MelodyGroup);
        CHECK_DNA_ACT(V, Variation);
        CHECK_DNA_ACT(L, Legato);
        CHECK_DNA_ACT(A, Accent);
        CHECK_DNA_ACT(O, Octave);
        
        #undef CHECK_DNA_ACT
    } else {
        // Fallback defaults if expander is disconnected
        if (engine.rhythmLen != 16) {
            engine.rhythmLen = engine.variationLen = engine.legatoLen = engine.accentLen = engine.melodyLen = engine.octaveLen = 16;
            engine.rhythmOff = engine.variationOff = engine.legatoOff = engine.accentOff = engine.melodyOff = engine.octaveOff = 0;
            engine.rhythmRot = engine.variationRot = engine.legatoRot = engine.accentRot = engine.melodyRot = engine.octaveRot = 0;
        }
    }

    // PolyVoice DNA Windows
    if (expanderManager.cachedPolyVoiceExpander) {
        for (int i = 0; i < 7; i++) {
            engine.polyLen[i] = clampv<int>((int)std::round(expanderManager.cachedPolyVoiceExpander->params[POLY_DNA_VOICE_1_LEN + i * 3].getValue()), 1, 16);
            int rawOff = (int)std::round(expanderManager.cachedPolyVoiceExpander->params[POLY_DNA_VOICE_1_OFF + i * 3].getValue());
            engine.polyOff[i] = ((rawOff % 16) + 16) % 16;
            int rawRot = (int)std::round(expanderManager.cachedPolyVoiceExpander->params[POLY_DNA_VOICE_1_ROT + i * 3].getValue());
            engine.polyRot[i] = ((rawRot % 16) + 16) % 16;
        }
    } else {
        for (int i = 0; i < 7; i++) {
            engine.polyLen[i] = 16;
            engine.polyOff[i] = engine.polyRot[i] = 0;
        }
    }
}

// ──── Scramble Operations ────────────────────────────────────────────────

void MonsoonSandsManager::scrambleRhythm() {
    patternEngine.rotateRhythm(random::u32() % 16);
    patternEngine.rotateVariation(random::u32() % 16);
    patternEngine.rotateLegato(random::u32() % 16);
    refreshVisualCache_();
}

void MonsoonSandsManager::scrambleMelody() {
    patternEngine.rotateMelody(random::u32() % 16);
    patternEngine.rotateOctave(random::u32() % 16);
    refreshVisualCache_();
}

void MonsoonSandsManager::scrambleVariation() {
    patternEngine.rotateVariation(random::u32() % 16);
    refreshVisualCache_();
}

void MonsoonSandsManager::scrambleAccent() {
    patternEngine.rotateAccent(random::u32() % 16);
    refreshVisualCache_();
}

void MonsoonSandsManager::scrambleOctave() {
    patternEngine.rotateOctave(random::u32() % 16);
    refreshVisualCache_();
}

void MonsoonSandsManager::scrambleLegato() {
    patternEngine.rotateLegato(random::u32() % 16);
    refreshVisualCache_();
}

void MonsoonSandsManager::scrambleRhythmGroup() {
    scrambleRhythm();  // Also scrambles variation + legato
}

void MonsoonSandsManager::scrambleMelodyGroup() {
    scrambleMelody();  // Also scrambles octave
}

void MonsoonSandsManager::scrambleAll() {
    scrambleRhythm();
    scrambleMelody();
}

// ──── Reset Operations ───────────────────────────────────────────────────

void MonsoonSandsManager::resetRhythm() {
    for (int i = 0; i < 16; ++i) {
        patternEngine.rhythmRandom[i]    = patternEngine.rhythmSource[i];
        patternEngine.variationRandom[i] = patternEngine.variationSource[i];
        patternEngine.legatoRandom[i]    = patternEngine.legatoSource[i];
    }
    refreshVisualCache_();
}

void MonsoonSandsManager::resetMelody() {
    for (int i = 0; i < 16; ++i) {
        patternEngine.melodyRandom[i] = patternEngine.melodySource[i];
        patternEngine.octaveRandom[i] = patternEngine.octaveSource[i];
    }
    refreshVisualCache_();
}

void MonsoonSandsManager::resetVariation() {
    for (int i = 0; i < 16; ++i) {
        patternEngine.variationRandom[i] = patternEngine.variationSource[i];
    }
    refreshVisualCache_();
}

void MonsoonSandsManager::resetAccent() {
    for (int i = 0; i < 16; ++i) {
        patternEngine.accentRandom[i] = patternEngine.accentSource[i];
    }
    refreshVisualCache_();
}

void MonsoonSandsManager::resetOctave() {
    for (int i = 0; i < 16; ++i) {
        patternEngine.octaveRandom[i] = patternEngine.octaveSource[i];
    }
    refreshVisualCache_();
}

void MonsoonSandsManager::resetLegato() {
    for (int i = 0; i < 16; ++i) {
        patternEngine.legatoRandom[i] = patternEngine.legatoSource[i];
    }
    refreshVisualCache_();
}

void MonsoonSandsManager::resetRhythmGroup() {
    resetRhythm();  // Also resets variation + legato
}

void MonsoonSandsManager::resetMelodyGroup() {
    resetMelody();  // Also resets octave
}

void MonsoonSandsManager::resetAll() {
    resetRhythm();
    resetMelody();
}

// ──── Helper ─────────────────────────────────────────────────────────────

void MonsoonSandsManager::refreshVisualCache_() {
    // Minimal PatternInput for cache refresh
    // Only stepIndex matters; rest are unused for visual cache
    PatternInput dummy{};
    patternEngine.refreshVisualCache(dummy);
}
