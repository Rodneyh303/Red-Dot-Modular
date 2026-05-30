#include "MonsoonSandsManager.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonSandsVisualExpander.hpp"
#include "../../MonsoonSandsExpander.hpp"
#include "../../StraitsSandsMacroVisual.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonStraitsEastExpander.hpp"

using namespace rack;
using namespace MonsoonIds;
using namespace SandsMonoVisualIds;
using namespace StraitsMacroVisualIds;

void MonsoonSandsManager::processDNA(const MonsoonExpanderManager& expanderManager) {
    const bool hasVisual = (expanderManager.cachedSandsVisualExpander != nullptr);
    const bool hasKnobs  = (expanderManager.cachedDnaExpander          != nullptr);
    const bool hasMacro  = (expanderManager.cachedMacroSandsVisual     != nullptr);

    auto* monoVis  = expanderManager.cachedSandsVisualExpander;
    auto* macroVis = expanderManager.cachedMacroSandsVisual;

    // ── Helper: apply mono CV offset at read site ─────────────────────────
    // Mono: cvId(lane, param) where param 0=LEN,1=OFF,2=ROT,3=SPR
    // base + cv * atten * scale, clamped.
    auto applyMonoCV = [&](float base, int lane, int param, float lo, float hi) -> float {
        if (!monoVis || !monoVis->inputs[cvId(lane, param)].isConnected()) return base;
        float cv  = monoVis->inputs[cvId(lane, param)].getVoltage() / 10.f;
        float att = monoVis->params[attenId(lane, param)].getValue();
        return clamp(base + cv * att * (hi - lo), lo, hi);
    };

    // ── Helper: apply macro CV offset at read site ────────────────────────
    // Macro: StraitsMacroVisualIds::cvId(lane, param), param 0=LEN,1=OFF,2=ROT,3=SPR
    auto applyMacroCV = [&](float base, int lane, int param, float lo, float hi) -> float {
        if (!macroVis || !macroVis->inputs[StraitsMacroVisualIds::cvId(lane,param)].isConnected()) return base;
        float cv  = macroVis->inputs[StraitsMacroVisualIds::cvId(lane,param)].getVoltage() / 10.f;
        float att = macroVis->params[StraitsMacroVisualIds::attenId(lane,param)].getValue();
        return clamp(base + cv * att * (hi - lo), lo, hi);
    };

    if (hasVisual || hasKnobs) {
        // L/O/R source: visual expander if present (reads SandsMonoVisualIds slots),
        // else knob expander (reads DNA_* slots). CV applied at read site.
        auto readStrand = [&](
                int monoLenId, int monoOffId, int monoRotId,
                int kbLenParam, int kbLenInput, int kbOffParam, int kbOffInput, int kbRotParam,
                int cvRow,   // row index for Mono CV (0-5), -1 if none
                int& tLen, int& tOff, int& tRot)
        {
            float baseLen, baseOff, baseRot;
            if (hasVisual) {
                baseLen = monoVis->params[monoLenId].getValue();
                baseOff = monoVis->params[monoOffId].getValue();
                baseRot = monoVis->params[monoRotId].getValue();
            } else {
                auto* kb = expanderManager.cachedDnaExpander;
                baseLen = kb->params[kbLenParam].getValue()
                        + kb->inputs[kbLenInput].getNormalVoltage(0.f) * 1.6f;
                baseOff = kb->params[kbOffParam].getValue()
                        + kb->inputs[kbOffInput].getNormalVoltage(0.f) * 1.5f;
                baseRot = kb->params[kbRotParam].getValue();
            }

            // Apply Mono visual CV if connected (cvRow = lane index 0-5)
            if (cvRow >= 0 && hasVisual) {
                baseLen = applyMonoCV(baseLen, cvRow, 0, 1.f, 16.f);
                baseOff = applyMonoCV(baseOff, cvRow, 1, 0.f, 15.f);
                baseRot = applyMonoCV(baseRot, cvRow, 2, 0.f, 15.f);
                // Spread: compute effective, write to spreadEffective[lane].
                // Widget reads spreadEffective[] → SpreadManager.setSpread().
                // No write-back to sprId param — base is always preserved.
                float baseSpr = monoVis->params[sprId(cvRow)].getValue();
                monoVis->spreadEffective[cvRow] = applyMonoCV(baseSpr, cvRow, 3, 0.f, 1.f);
            }

            tLen = clamp((int)std::round(baseLen), 1, 16);
            tOff = ((int)std::round(baseOff) % 16 + 16) % 16;
            tRot = ((int)std::round(baseRot) % 16 + 16) % 16;
        };

        // Mono cvRow = lane index (0-5), one row per lane, 4 params per row
        // Lane 0=REST(rhythm), 1=VAR(variation), 2=LEG(legato),
        //      3=ACC(accent),   4=MEL(melody),    5=OCT(octave)
        // Initialise spreadEffective from base params (CV overrides below)
        if (hasVisual) {
            for (int l = 0; l < 6; ++l)
                monoVis->spreadEffective[l] = monoVis->params[sprId(l)].getValue();
        }
        readStrand(lenId(0),offId(0),rotId(0), DNA_R_LEN_PARAM,DNA_R_LEN_INPUT,DNA_R_OFF_PARAM,DNA_R_OFF_INPUT,DNA_R_ROT_PARAM, 0, engine.rhythmLen,    engine.rhythmOff,    engine.rhythmRot);
        readStrand(lenId(1),offId(1),rotId(1), DNA_V_LEN_PARAM,DNA_V_LEN_INPUT,DNA_V_OFF_PARAM,DNA_V_OFF_INPUT,DNA_V_ROT_PARAM, 1, engine.variationLen, engine.variationOff, engine.variationRot);
        readStrand(lenId(2),offId(2),rotId(2), DNA_L_LEN_PARAM,DNA_L_LEN_INPUT,DNA_L_OFF_PARAM,DNA_L_OFF_INPUT,DNA_L_ROT_PARAM, 2, engine.legatoLen,    engine.legatoOff,    engine.legatoRot);
        readStrand(lenId(3),offId(3),rotId(3), DNA_A_LEN_PARAM,DNA_A_LEN_INPUT,DNA_A_OFF_PARAM,DNA_A_OFF_INPUT,DNA_A_ROT_PARAM, 3, engine.accentLen,    engine.accentOff,    engine.accentRot);
        readStrand(lenId(4),offId(4),rotId(4), DNA_M_LEN_PARAM,DNA_M_LEN_INPUT,DNA_M_OFF_PARAM,DNA_M_OFF_INPUT,DNA_M_ROT_PARAM, 4, engine.melodyLen,    engine.melodyOff,    engine.melodyRot);
        readStrand(lenId(5),offId(5),rotId(5), DNA_O_LEN_PARAM,DNA_O_LEN_INPUT,DNA_O_OFF_PARAM,DNA_O_OFF_INPUT,DNA_O_ROT_PARAM, 5, engine.octaveLen,    engine.octaveOff,    engine.octaveRot);

        if (hasKnobs) {
            auto dnaExp = expanderManager.cachedDnaExpander;
            #define CHECK_DNA_ACT(suffix, func) \
                if (scrambleParamTrig_##suffix.process(dnaExp->params[DNA_SCRAMBLE_##suffix##_PARAM].getValue())) scramble##func(); \
                if (scrambleInputTrig_##suffix.process(dnaExp->inputs[DNA_SCRAMBLE_##suffix##_INPUT].getVoltage())) scramble##func(); \
                if (resetParamTrig_##suffix.process(dnaExp->params[DNA_RESET_##suffix##_PARAM].getValue())) reset##func(); \
                if (resetInputTrig_##suffix.process(dnaExp->inputs[DNA_RESET_##suffix##_INPUT].getVoltage())) reset##func();
            CHECK_DNA_ACT(ALL, All); CHECK_DNA_ACT(R, RhythmGroup);
            CHECK_DNA_ACT(M, MelodyGroup); CHECK_DNA_ACT(V, Variation);
            CHECK_DNA_ACT(L, Legato); CHECK_DNA_ACT(A, Accent); CHECK_DNA_ACT(O, Octave);
            #undef CHECK_DNA_ACT
        }
    } else {
        if (engine.rhythmLen != 16) {
            engine.rhythmLen = engine.variationLen = engine.legatoLen =
            engine.accentLen = engine.melodyLen = engine.octaveLen = 16;
            engine.rhythmOff = engine.variationOff = engine.legatoOff =
            engine.accentOff = engine.melodyOff = engine.octaveOff = 0;
            engine.rhythmRot = engine.variationRot = engine.legatoRot =
            engine.accentRot = engine.melodyRot = engine.octaveRot = 0;
        }
    }

    // ── Macro global LOR with CV ──────────────────────────────────────────
    // Applied on top of existing voice patterns when Macro is connected.
    // Macro row mapping: REST=rows 0-1, MEL=rows 2-3, OCT=rows 4-5
    if (hasMacro) {
        auto applyGlobal = [&](int lane) {
            float bLen = macroVis->params[StraitsMacroVisualIds::lorId(lane,0)].getValue();
            float bOff = macroVis->params[StraitsMacroVisualIds::lorId(lane,1)].getValue();
            float bRot = macroVis->params[StraitsMacroVisualIds::lorId(lane,2)].getValue();
            bLen = applyMacroCV(bLen, lane, 0, 1.f, 16.f);
            bOff = applyMacroCV(bOff, lane, 1, 0.f, 15.f);
            bRot = applyMacroCV(bRot, lane, 2, 0.f, 15.f);
            // Spread CV (param 3): effective value written to SPREAD_REST/MEL/OCT
            // which the Macro widget reads for SpreadManager.setSpread()
            float bSpr = macroVis->params[StraitsMacroVisualIds::sprId(lane)].getValue();
            bSpr = applyMacroCV(bSpr, lane, 3, 0.f, 1.f);
            macroVis->params[StraitsMacroVisualIds::sprId(lane)].setValue(bSpr);
            return std::make_tuple(
                clamp((int)std::round(bLen), 1, 16),
                ((int)std::round(bOff) % 16 + 16) % 16,
                ((int)std::round(bRot) % 16 + 16) % 16);
        };
        auto [rLen,rOff,rRot] = applyGlobal(0);
        engine.rhythmLen = rLen; engine.rhythmOff = rOff; engine.rhythmRot = rRot;
        auto [mLen,mOff,mRot] = applyGlobal(1);
        engine.melodyLen = mLen; engine.melodyOff = mOff; engine.melodyRot = mRot;
        auto [oLen,oOff,oRot] = applyGlobal(2);
        engine.octaveLen = oLen; engine.octaveOff = oOff; engine.octaveRot = oRot;
    }

    // Note: Poly DNA windows handled in Monsoon::process controlDivider block.
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
