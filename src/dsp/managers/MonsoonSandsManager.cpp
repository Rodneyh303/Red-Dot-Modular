#include "MonsoonSandsManager.hpp"
#include "../SpreadInterp.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonSandsVisualExpander.hpp"
//#include "../../MonsoonSandsExpander.hpp"
#include "../../StraitsSandsMacroVisual.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonStraitsEastExpander.hpp"

using namespace rack;
using namespace MonsoonIds;
// NOTE: NOT using namespace SandsMonoVisualIds or StraitsMacroVisualIds
// to avoid ambiguous cvId/attenId/sprId calls — qualify explicitly below.
namespace Mono  = SandsMonoVisualIds;
namespace Macro = StraitsMacroVisualIds;
namespace East  = StraitsEastVisualIds;   // for the voice-1 mix-in (interp. Y)

void MonsoonSandsManager::processDNA(const MonsoonExpanderManager& expanderManager, bool spreadInterpMono) {
    const bool hasVisual = (expanderManager.cachedSandsVisualExpander != nullptr);
    //const bool hasKnobs  = (expanderManager.cachedDnaExpander          != nullptr);
    const bool hasMacro  = (expanderManager.cachedMacroSandsVisual     != nullptr);

    auto* monoVis  = expanderManager.cachedSandsVisualExpander;
    auto* macroVis = expanderManager.cachedMacroSandsVisual;

    // Sands final-ownership lifecycle (Option W): default each cycle to whether
    // any Sands visual stage will own final (Mono, Macro, or East poly). When
    // none is present this is false and slew copies slewedDraw → final.
    const bool hasEastVisual = (expanderManager.cachedEastSandsVisual != nullptr);
    // Poly base active = Straits base poly output expander present AND at least
    // one poly voice requested. The poly Sands editors (Macro here, East in
    // Monsoon.cpp) are no-op unless this holds.
    const bool polyBaseActive = (expanderManager.cachedPolyVoiceExpander != nullptr)
                                && (engine.numPolyVoices >= 1);
    // Voice OUTPUT topology bounds the spread ensemble: East base expander
    // outputs poly voices 2..8 (engine.voices[0..6], i.e. up to 7), West adds
    // voices 9..15 (up to 8 more). Voices with no output path must NOT be
    // averaged into spread, or the ensemble converges toward phantom voices.
    // So the effective poly count exceeds 7 only when West is also present.
    const bool hasWest = (expanderManager.cachedStraitWestExpander != nullptr);
    const int  polyOutCap = polyBaseActive ? (hasWest ? 15 : 7) : 0;
    const int  effPolyVoices = clamp(engine.numPolyVoices, 0, polyOutCap);
    const bool macroActive = hasMacro && polyBaseActive;
    engine.pe.setSandsActive(hasVisual || macroActive || (hasEastVisual && polyBaseActive));
    engine.pe.numPolyVoicesHint = effPolyVoices;  // gated + output-bounded: display matches audio

    // ── Helper: apply mono CV offset at read site ─────────────────────────
    auto applyMonoCV = [&](float base, int lane, int param, float lo, float hi) -> float {
        if (!monoVis || !monoVis->inputs[Mono::cvId(lane, param)].isConnected()) return base;
        float cv  = monoVis->inputs[Mono::cvId(lane, param)].getVoltage() / 10.f;
        float att = monoVis->params[Mono::attenId(lane, param)].getValue();
        return clamp(base + cv * att * (hi - lo), lo, hi);
    };

    // ── Helper: apply macro CV offset at read site ────────────────────────
    auto applyMacroCV = [&](float base, int lane, int param, float lo, float hi) -> float {
        if (!macroVis || !macroVis->inputs[Macro::macroCvId(lane,param)].isConnected()) return base;
        float cv  = macroVis->inputs[Macro::macroCvId(lane,param)].getVoltage() / 10.f;
        float att = macroVis->params[Macro::macroAttenId(lane,param)].getValue();
        return clamp(base + cv * att * (hi - lo), lo, hi);
    };
    // Macro SPREAD CV: bipolar, unit-scaled (cv*att over ±1) to match Mono's
    // applyMonoSprCV. NOT routed through applyMacroCV because that scales by
    // (hi-lo), which for a ±1 range would double the CV. param index 3 = SPR.
    auto applyMacroSprCV = [&](float base, int lane) -> float {
        if (!macroVis || !macroVis->inputs[Macro::macroCvId(lane,3)].isConnected()) return base;
        float cv  = macroVis->inputs[Macro::macroCvId(lane,3)].getVoltage() / 10.f;
        float att = macroVis->params[Macro::macroAttenId(lane,3)].getValue();
        return clamp(base + cv * att, -1.f, 1.f);
    };

    // ── Helper: apply mono spread CV (REST/MEL/OCT only, own jack/atten) ──
    auto applyMonoSprCV = [&](float base, int sprLane) -> float {
        if (!monoVis || !monoVis->inputs[Mono::sprCvId(sprLane)].isConnected()) return base;
        float cv  = monoVis->inputs[Mono::sprCvId(sprLane)].getVoltage() / 10.f;
        float att = monoVis->params[Mono::sprAttenId(sprLane)].getValue();
        // Spread is BIPOLAR (param range -1..1; negative inverts the interp
        // target). Clamp to [-1,1], not [0,1] — the old [0,1] clamp floored any
        // negative spread to 0, so negative modulation only worked when the base
        // happened to be positive ([C]).
        return clamp(base + cv * att, -1.f, 1.f);
    };

    // Spread interpolation is now centralised in redDot::SpreadInterp (see
    // src/dsp/SpreadInterp.hpp) — the single source of truth.

    if (hasVisual) 
    {
        // readStrand: read a mono strand's base L/O/R from the visual params,
        // apply mono CV, and write the clamped result to the engine strand for
        // editor lane `l`. The editor-lane → engine-strand permutation lives in
        // dotModular::MONO_LANE_TO_STRAND (dsp/LaneMapping.hpp) — the single
        // source of truth — so this is driven by a loop rather than a hand-
        // ordered call list that could drift. (DNA-expander base path is retired;
        // base always comes from the visual params. cvRow == editor lane.)
        auto readStrand = [&](int l) {
            int strand = dotModular::MONO_LANE_TO_STRAND[l];
            float baseLen = monoVis->params[Mono::lenId(l)].getValue();
            float baseOff = monoVis->params[Mono::offId(l)].getValue();
            float baseRot = monoVis->params[Mono::rotId(l)].getValue();

            baseLen = applyMonoCV(baseLen, l, 0, 1.f, 16.f);
            baseOff = applyMonoCV(baseOff, l, 1, 0.f, 15.f);
            baseRot = applyMonoCV(baseRot, l, 2, 0.f, 15.f);

            // INTERPRETATION Y — voice-1 mix-in. The mono master strand is voice 1 of
            // the ensemble; like every poly voice it can receive the East per-lane CV
            // and the Macro global-CV mix-in, summed on top of the mono base. We use
            // VOICE 0's slice of the East/Macro per-voice banks (tab 1 = voice 1 = mono).
            // Only when the respective module is attached. Lane/item→East(r,c) mirrors
            // the poly combineLOR mapping. (Base stays mono's — this is modulation only.)
            auto* eastVis = expanderManager.cachedEastSandsVisual;
            // East per-lane CV folded onto voice 1 (voice-0 depth × East CV ch0).
            auto eastMix = [&](float base, int r, int c, float lo, float hi)->float {
                if (!eastVis || !eastVis->inputs[East::cvId(r,c)].isConnected()) return base;
                float att = eastVis->params[East::attenId(0, r, c)].getValue();   // voice 0
                float cv  = eastVis->inputs[East::cvId(r,c)].getVoltage(0) / 10.f; // ch0
                return math::clamp(base + cv * att * (hi - lo), lo, hi);
            };
            // Macro global CV distributed to voice 1 (voice-0 send × macroCVDelta).
            // NOTE: macroCVDelta is published by the Macro block LATER in this same
            // processDNA (after the mono strand), so voice 1 reads the PREVIOUS control
            // block's delta — a one-block (sub-ms, control-rate) lag. Imperceptible for a
            // modulation value; if it ever matters, hoist the macroActive publishGlobal
            // block above the hasVisual mono block (it has no dependency on mono).
            auto macroMix = [&](float base, int item, float lo, float hi)->float {
                if (!hasMacro || !macroVis) return base;
                float send = macroVis->params[Macro::sendId(0, l, item)].getValue();   // voice 0
                return math::clamp(base + macroVis->macroCVDelta[l][item] * send, lo, hi);
            };
            // REST/MEL/OCT (l=0/1/2) East (r,c): Len (l*2,0) Off (l*2,1) Rot (l*2+1,0).
            baseLen = eastMix(baseLen, l*2,   0, 1.f, 16.f);  baseLen = macroMix(baseLen, 0, 1.f, 16.f);
            baseOff = eastMix(baseOff, l*2,   1, 0.f, 15.f);  baseOff = macroMix(baseOff, 1, 0.f, 15.f);
            baseRot = eastMix(baseRot, l*2+1, 0, 0.f, 15.f);  baseRot = macroMix(baseRot, 2, 0.f, 15.f);

            engine.strandLenRef(strand) = clamp((int)std::round(baseLen), 1, 16);
            engine.strandOffRef(strand) = ((int)std::round(baseOff) % 16 + 16) % 16;
            engine.strandRotRef(strand) = ((int)std::round(baseRot) % 16 + 16) % 16;
        };

        // Spread (REST/MEL/OCT only): base trimpot + per-lane spread CV.
        // sprId/sprCvId lane index: 0=REST, 1=MELODY, 2=OCTAVE (editor order).
       // if (hasVisual) 
       // {
            for (int l = 0; l < 3; ++l) {
                float baseSpr = monoVis->params[Mono::sprId(l)].getValue();
                float sp = applyMonoSprCV(baseSpr, l);
                // INTERP Y — voice-1 spread mix-in (voice 0 slice), bipolar [-1,1].
                auto* eastVisS = expanderManager.cachedEastSandsVisual;
                static const int sprRow[3] = { 1, 3, 5 };   // East spread CV at cvId(row,1)
                if (eastVisS && eastVisS->inputs[East::cvId(sprRow[l],1)].isConnected()) {
                    float att = eastVisS->params[East::attenId(0, sprRow[l], 1)].getValue();
                    float cv  = eastVisS->inputs[East::cvId(sprRow[l],1)].getVoltage(0) / 10.f;
                    sp = rack::math::clamp(sp + cv * att * 2.f, -1.f, 1.f);   // span [-1,1]=2
                }
                if (hasMacro && macroVis) {
                    float send = macroVis->params[Macro::sendId(0, l, 3)].getValue();
                    sp = rack::math::clamp(sp + macroVis->macroCVDelta[l][3] * send, -1.f, 1.f);
                }
                monoVis->spreadEffective[l] = sp;
            }
            // LEG(3) and VAR(5) are mono-only — no spread. ACCENT(4) is now a poly lane
            // with its own spread (set by the mono Sands spread control / its own CV path),
            // so it is NOT zeroed here.
            monoVis->spreadEffective[3] = 0.f;
            monoVis->spreadEffective[5] = 0.f;

            // ── Sands spread→final (Option W, Model 1) ───────────────────────
            // Mono owns the MONO final arrays: read the SLEWED draw, apply
            // per-lane spread for REST/MEL/OCT (converge toward the poly-incl-
            // mono average), pass LEG/ACC/VAR raw, write the FINAL arrays the
            // sequencer reads. setSandsActive(true) stops slew copying the
            // un-spread draw over the top.
            // When LOCKED, leave the final arrays frozen (skip the rewrite) so
            // lock freezes the audible output — spread CV won't leak through.
            engine.pe.setSandsActive(true);
            if (!engine.locked) {
            // Ensemble = mono + active poly voices. Average over (1 + nPoly).
            // Ensemble poly count is bounded by the voice OUTPUT topology
            // (effPolyVoices): only voices with an actual output path (East ≤7,
            // +West for 8..15) are averaged. With no base expander it is 0, so
            // Mono spread degenerates to a no-op (average == mono draw itself).
            const int nPoly = effPolyVoices;
            const redDot::SpreadInterp::Target mode = spreadInterpMono
                ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
            for (int i = 0; i < 16; ++i) {
                // Single source of truth: target (mode-aware, mono-inclusive avg)
                // + bipolar interpolate, over the pre-spread slewed draws.
                engine.pe.rhythmRandom[i] = redDot::SpreadInterp::apply(
                    engine.pe, mode, 0, i, nPoly, engine.pe.slewedRhythm[i], monoVis->spreadEffective[0]);
                engine.pe.melodyRandom[i] = redDot::SpreadInterp::apply(
                    engine.pe, mode, 1, i, nPoly, engine.pe.slewedMelody[i], monoVis->spreadEffective[1]);
                engine.pe.octaveRandom[i] = redDot::SpreadInterp::apply(
                    engine.pe, mode, 2, i, nPoly, engine.pe.slewedOctave[i], monoVis->spreadEffective[2]);
                engine.pe.legatoRandom[i]    = engine.pe.slewedLegato[i];     // mono-only, raw
                engine.pe.accentRandom[i]    = engine.pe.slewedAccent[i];
                engine.pe.variationRandom[i] = engine.pe.slewedVariation[i];
            }
            }  // end if(!engine.locked)
        //}
        // Drive all 6 mono strands via the single-source-of-truth lane map.
        for (int l = 0; l < 6; ++l) readStrand(l);

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

    // ── Macro global LOR with CV (poly: per-lane, SAME for every voice) ────
    if (macroActive) {
        // Macro publishes its per-(lane,item) base + CV-delta split; East's sync
        // (runs after, always present when macroActive) combines them per voice
        // via the owner switch + blend send. Macro no longer writes engine.polyLen
        // directly — East owns the final write so the blend equation is applied in
        // one place.
        auto publishGlobal = [&](int lane) {
            // bases (knob, no CV)
            float baseLen = macroVis->params[Macro::lorId(lane,0)].getValue();
            float baseOff = macroVis->params[Macro::lorId(lane,1)].getValue();
            float baseRot = macroVis->params[Macro::lorId(lane,2)].getValue();
            float baseSpr = macroVis->params[Macro::sprId(lane)].getValue();
            // CV-applied
            float cvLen = applyMacroCV(baseLen, lane, 0, 1.f, 16.f);
            float cvOff = applyMacroCV(baseOff, lane, 1, 0.f, 15.f);
            float cvRot = applyMacroCV(baseRot, lane, 2, 0.f, 15.f);
            float cvSpr = applyMacroSprCV(baseSpr, lane);
            // publish base + CV-only delta (item 0=LEN 1=OFF 2=ROT 3=SPR)
            macroVis->macroBase[lane][0] = baseLen;  macroVis->macroCVDelta[lane][0] = cvLen - baseLen;
            macroVis->macroBase[lane][1] = baseOff;  macroVis->macroCVDelta[lane][1] = cvOff - baseOff;
            macroVis->macroBase[lane][2] = baseRot;  macroVis->macroCVDelta[lane][2] = cvRot - baseRot;
            macroVis->macroBase[lane][3] = baseSpr;  macroVis->macroCVDelta[lane][3] = cvSpr - baseSpr;
            // spread display reads the CV-applied global spread (base+CV, no blend)
            macroVis->spreadEffective[lane] = cvSpr;
        };
        publishGlobal(0);
        publishGlobal(1);
        publishGlobal(2);
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
