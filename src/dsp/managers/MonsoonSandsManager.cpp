#include "MonsoonSandsManager.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonSandsVisualExpander.hpp"
#include "../../MonsoonSandsExpander.hpp"
#include "../../StraitsSandsMacroVisual.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonStraitsEastExpander.hpp"

using namespace rack;
using namespace MonsoonIds;
// NOTE: NOT using namespace SandsMonoVisualIds or StraitsMacroVisualIds
// to avoid ambiguous cvId/attenId/sprId calls — qualify explicitly below.
namespace Mono  = SandsMonoVisualIds;
namespace Macro = StraitsMacroVisualIds;

void MonsoonSandsManager::processDNA(const MonsoonExpanderManager& expanderManager) {
    const bool hasVisual = (expanderManager.cachedSandsVisualExpander != nullptr);
    const bool hasKnobs  = (expanderManager.cachedDnaExpander          != nullptr);
    const bool hasMacro  = (expanderManager.cachedMacroSandsVisual     != nullptr);

    auto* monoVis  = expanderManager.cachedSandsVisualExpander;
    auto* macroVis = expanderManager.cachedMacroSandsVisual;

    // ── Helper: apply mono CV offset at read site ─────────────────────────
    auto applyMonoCV = [&](float base, int lane, int param, float lo, float hi) -> float {
        if (!monoVis || !monoVis->inputs[Mono::cvId(lane, param)].isConnected()) return base;
        float cv  = monoVis->inputs[Mono::cvId(lane, param)].getVoltage() / 10.f;
        float att = monoVis->params[Mono::attenId(lane, param)].getValue();
        return clamp(base + cv * att * (hi - lo), lo, hi);
    };

    // ── Helper: apply macro CV offset at read site ────────────────────────
    auto applyMacroCV = [&](float base, int lane, int param, float lo, float hi) -> float {
        if (!macroVis || !macroVis->inputs[Macro::cvId(lane,param)].isConnected()) return base;
        float cv  = macroVis->inputs[Macro::cvId(lane,param)].getVoltage() / 10.f;
        float att = macroVis->params[Macro::attenId(lane,param)].getValue();
        return clamp(base + cv * att * (hi - lo), lo, hi);
    };

    // ── Helper: apply mono spread CV (REST/MEL/OCT only, own jack/atten) ──
    auto applyMonoSprCV = [&](float base, int sprLane) -> float {
        if (!monoVis || !monoVis->inputs[Mono::sprCvId(sprLane)].isConnected()) return base;
        float cv  = monoVis->inputs[Mono::sprCvId(sprLane)].getVoltage() / 10.f;
        float att = monoVis->params[Mono::sprAttenId(sprLane)].getValue();
        return clamp(base + cv * att, 0.f, 1.f);
    };

    if (hasVisual || hasKnobs) {
        auto readStrand = [&](
                int monoLenId, int monoOffId, int monoRotId,
                int kbLenParam, int kbLenInput, int kbOffParam, int kbOffInput, int kbRotParam,
                int cvRow,
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

            if (cvRow >= 0 && hasVisual) {
                baseLen = applyMonoCV(baseLen, cvRow, 0, 1.f, 16.f);
                baseOff = applyMonoCV(baseOff, cvRow, 1, 0.f, 15.f);
                baseRot = applyMonoCV(baseRot, cvRow, 2, 0.f, 15.f);
            }

            tLen = clamp((int)std::round(baseLen), 1, 16);
            tOff = ((int)std::round(baseOff) % 16 + 16) % 16;
            tRot = ((int)std::round(baseRot) % 16 + 16) % 16;
        };

        // Spread (REST/MEL/OCT only): base trimpot + per-lane spread CV.
        // sprId/sprCvId lane index: 0=REST, 1=MELODY, 2=OCTAVE (editor order).
        if (hasVisual) {
            for (int l = 0; l < 3; ++l) {
                float baseSpr = monoVis->params[Mono::sprId(l)].getValue();
                monoVis->spreadEffective[l] = applyMonoSprCV(baseSpr, l);
            }
            // LEG/ACC/VAR have no spread
            for (int l = 3; l < 6; ++l) monoVis->spreadEffective[l] = 0.f;
        }
        readStrand(Mono::lenId(0),Mono::offId(0),Mono::rotId(0), DNA_R_LEN_PARAM,DNA_R_LEN_INPUT,DNA_R_OFF_PARAM,DNA_R_OFF_INPUT,DNA_R_ROT_PARAM, 0, engine.rhythmLen,    engine.rhythmOff,    engine.rhythmRot);
        readStrand(Mono::lenId(1),Mono::offId(1),Mono::rotId(1), DNA_V_LEN_PARAM,DNA_V_LEN_INPUT,DNA_V_OFF_PARAM,DNA_V_OFF_INPUT,DNA_V_ROT_PARAM, 1, engine.variationLen, engine.variationOff, engine.variationRot);
        readStrand(Mono::lenId(2),Mono::offId(2),Mono::rotId(2), DNA_L_LEN_PARAM,DNA_L_LEN_INPUT,DNA_L_OFF_PARAM,DNA_L_OFF_INPUT,DNA_L_ROT_PARAM, 2, engine.legatoLen,    engine.legatoOff,    engine.legatoRot);
        readStrand(Mono::lenId(3),Mono::offId(3),Mono::rotId(3), DNA_A_LEN_PARAM,DNA_A_LEN_INPUT,DNA_A_OFF_PARAM,DNA_A_OFF_INPUT,DNA_A_ROT_PARAM, 3, engine.accentLen,    engine.accentOff,    engine.accentRot);
        readStrand(Mono::lenId(4),Mono::offId(4),Mono::rotId(4), DNA_M_LEN_PARAM,DNA_M_LEN_INPUT,DNA_M_OFF_PARAM,DNA_M_OFF_INPUT,DNA_M_ROT_PARAM, 4, engine.melodyLen,    engine.melodyOff,    engine.melodyRot);
        readStrand(Mono::lenId(5),Mono::offId(5),Mono::rotId(5), DNA_O_LEN_PARAM,DNA_O_LEN_INPUT,DNA_O_OFF_PARAM,DNA_O_OFF_INPUT,DNA_O_ROT_PARAM, 5, engine.octaveLen,    engine.octaveOff,    engine.octaveRot);

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
    if (hasMacro) {
        auto applyGlobal = [&](int lane, int& outLen, int& outOff, int& outRot) {
            float bLen = macroVis->params[Macro::lorId(lane,0)].getValue();
            float bOff = macroVis->params[Macro::lorId(lane,1)].getValue();
            float bRot = macroVis->params[Macro::lorId(lane,2)].getValue();
            bLen = applyMacroCV(bLen, lane, 0, 1.f, 16.f);
            bOff = applyMacroCV(bOff, lane, 1, 0.f, 15.f);
            bRot = applyMacroCV(bRot, lane, 2, 0.f, 15.f);
            float bSpr = macroVis->params[Macro::sprId(lane)].getValue();
            bSpr = applyMacroCV(bSpr, lane, 3, 0.f, 1.f);
            macroVis->params[Macro::sprId(lane)].setValue(bSpr);
            outLen = clamp((int)std::round(bLen), 1, 16);
            outOff = ((int)std::round(bOff) % 16 + 16) % 16;
            outRot = ((int)std::round(bRot) % 16 + 16) % 16;
        };
        // C++11: out-params instead of structured bindings
        applyGlobal(0, engine.rhythmLen, engine.rhythmOff, engine.rhythmRot);
        applyGlobal(1, engine.melodyLen, engine.melodyOff, engine.melodyRot);
        applyGlobal(2, engine.octaveLen, engine.octaveOff, engine.octaveRot);
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
