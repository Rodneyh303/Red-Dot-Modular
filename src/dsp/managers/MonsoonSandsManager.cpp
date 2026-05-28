#include "MonsoonSandsManager.hpp"
#include "../../Monsoon.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonSandsExpander.hpp"
#include "../../MonsoonSandsVisualExpander.hpp"
#include "../../MonsoonStraitsEastExpander.hpp"

using namespace rack;
using namespace MonsoonIds;

void MonsoonSandsManager::processDNA(const MonsoonExpanderManager& expanderManager) {
    // Visual expander takes priority over knob expander when both are present.
    // Both are zero-delay: Monsoon reads their params directly here on the
    // audio thread — no message passing, no write-back from the UI thread.
    const bool hasVisual = (expanderManager.cachedSandsVisualExpander != nullptr);
    const bool hasKnobs  = (expanderManager.cachedDnaExpander          != nullptr);

    if (hasVisual || hasKnobs) {
        // Pick the active source for each parameter.
        // Visual expander owns L/O/R when present; knob expander also
        // provides CV inputs and action buttons (scramble/reset).
        auto readStrand = [&](int pLen, int iLen, int pOff, int iOff, int pRot,
                               int& tLen, int& tOff, int& tRot) {
            // L/O/R source: visual expander if present, else knob expander
            rack::Module* lorSrc = hasVisual
                ? static_cast<rack::Module*>(expanderManager.cachedSandsVisualExpander)
                : static_cast<rack::Module*>(expanderManager.cachedDnaExpander);

            // CV inputs only available from knob expander
            float lCV = hasKnobs
                ? expanderManager.cachedDnaExpander->inputs[iLen].getNormalVoltage(0.f) * 1.6f
                : 0.f;
            float oCV = hasKnobs
                ? expanderManager.cachedDnaExpander->inputs[iOff].getNormalVoltage(0.f) * 1.5f
                : 0.f;

            tLen = clampv<int>((int)std::round(lorSrc->params[pLen].getValue() + lCV), 1, 16);
            int rawOff = (int)std::round(lorSrc->params[pOff].getValue() + oCV);
            tOff = ((rawOff % 16) + 16) % 16;
            int rawRot = (int)std::round(lorSrc->params[pRot].getValue());
            tRot = ((rawRot % 16) + 16) % 16;
        };

        readStrand(DNA_R_LEN_PARAM, DNA_R_LEN_INPUT, DNA_R_OFF_PARAM, DNA_R_OFF_INPUT, DNA_R_ROT_PARAM,
                   engine.rhythmLen,    engine.rhythmOff,    engine.rhythmRot);
        readStrand(DNA_V_LEN_PARAM, DNA_V_LEN_INPUT, DNA_V_OFF_PARAM, DNA_V_OFF_INPUT, DNA_V_ROT_PARAM,
                   engine.variationLen, engine.variationOff, engine.variationRot);
        readStrand(DNA_L_LEN_PARAM, DNA_L_LEN_INPUT, DNA_L_OFF_PARAM, DNA_L_OFF_INPUT, DNA_L_ROT_PARAM,
                   engine.legatoLen,    engine.legatoOff,    engine.legatoRot);
        readStrand(DNA_A_LEN_PARAM, DNA_A_LEN_INPUT, DNA_A_OFF_PARAM, DNA_A_OFF_INPUT, DNA_A_ROT_PARAM,
                   engine.accentLen,    engine.accentOff,    engine.accentRot);
        readStrand(DNA_M_LEN_PARAM, DNA_M_LEN_INPUT, DNA_M_OFF_PARAM, DNA_M_OFF_INPUT, DNA_M_ROT_PARAM,
                   engine.melodyLen,    engine.melodyOff,    engine.melodyRot);
        readStrand(DNA_O_LEN_PARAM, DNA_O_LEN_INPUT, DNA_O_OFF_PARAM, DNA_O_OFF_INPUT, DNA_O_ROT_PARAM,
                   engine.octaveLen,    engine.octaveOff,    engine.octaveRot);

        // Action buttons (scramble/reset) remain on the knob expander only
        if (hasKnobs) {
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
        }
    } else {
        // Fallback defaults if expander is disconnected
        if (engine.rhythmLen != 16) {
            engine.rhythmLen = engine.variationLen = engine.legatoLen = engine.accentLen = engine.melodyLen = engine.octaveLen = 16;
            engine.rhythmOff = engine.variationOff = engine.legatoOff = engine.accentOff = engine.melodyOff = engine.octaveOff = 0;
            engine.rhythmRot = engine.variationRot = engine.legatoRot = engine.accentRot = engine.melodyRot = engine.octaveRot = 0;
        }
    }
    // Note: Poly DNA windows (Len/Off/Rot) are now handled exclusively by 
    // DeepStraitsSands expanders in Monsoon::process to avoid ID collisions.
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
