#include "MonsoonExpanderManager.hpp"
#include "../SpreadInterp.hpp"
#include "../../Monsoon.hpp"
//#include "../../MonsoonDeepStraitsSands.hpp"
#include "../../StraitsEastSandsVisual.hpp"
//#include "../../StraitsWestSandsVisual.hpp"
#include "../../StraitsSandsMacroVisual.hpp"

using namespace rack;

void MonsoonExpanderManager::sync(SequencerEngine& engine, bool spreadInterpMono) {
    //auto* deepEast   = cachedDeepStraitsSandsEastExpander;
    //auto* deepWest   = cachedDeepStraitsSandsWestExpander;
    auto* straitEast = cachedPolyVoiceExpander;
    auto* straitWest = cachedStraitWestExpander;
    auto* eastVisual  = cachedEastSandsVisual;
    //auto* westVisual  = cachedWestSandsVisual;

    auto* eastLOR   = eastVisual;// ? static_cast<rack::Module*>(eastVisual);
    auto* eastInterp = eastVisual;// ? static_cast<rack::Module*>(eastVisual)

    const bool polyBaseActive = (straitEast != nullptr) && (engine.numPolyVoices >= 1);
    const int  polyOutCap = polyBaseActive ? (straitWest ? 15 : 7) : 0;
    const int  effPolyVoices = math::clamp(engine.numPolyVoices, 0, polyOutCap);

    // Spread interpolation is now centralised in redDot::SpreadInterp (see
    // src/dsp/SpreadInterp.hpp) — the single source of truth shared with the
    // mono/macro path and the visual display.

    if (eastLOR && (straitEast || eastVisual) && polyBaseActive) {
       // using namespace DeepStraitsSandsEastIds;
        using namespace StraitsEastVisualIds;

        // ── Base ownership + Macro-CV blend (per voice, per lane, per item) ───
        // Value equation, applied here (East runs last and owns the final write):
        //   base  = (owner==East) ? eastBase + eastCV : macroBase
        //   value = base + macroCVDelta · blendSend          (blend always added)
        // owner default = Macro; blendSend default = unity. Macro publishes its
        // per-(lane,item) base + CV-delta split (macroBase/macroCVDelta on the
        // visual) in processDNA just before this. When Macro is absent the macro
        // terms are zero and East owns everything (its own base + CV).
        // item index: 0=LEN 1=OFF 2=ROT 3=SPR. (Spr handled in the interp path.)
        auto* macroVis = cachedMacroSandsVisual;
        const bool macroPresent = (macroVis != nullptr);

        for (int v = 0; v < 15; v++) {
            int rhythmBase = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;

            // East's own base+CV for an L/O/R item (paramIdx = East voice param).
            auto eastLorVal = [&](int paramIdx, int r, int c, float lo, float hi)-> float {
                float base = eastLOR->params[paramIdx].getValue();
                if (eastVisual && eastVisual->inputs[cvId(r,c)].isConnected()) {
                    float att = eastVisual->params[attenId(r,c)].getValue();
                    float cv  = eastVisual->inputs[cvId(r,c)].getVoltage(v) / 10.f;
                    base = math::clamp(base + cv * att * (hi - lo), lo, hi);
                }
                return base;
            };
            // Full equation for one L/O/R item → final int window value.
            //   lane: 0/1/2 (REST/MEL/OCT)   item: 0/1/2 (LEN/OFF/ROT)
            //   r,c: East CV jack row/col     paramIdx: East voice LOR param
            auto combineLOR = [&](int lane, int item, int paramIdx, int r, int c,
                                  float lo, float hi)-> int {
                const bool ownerEast = eastLOR->params[
                    StraitsEastVisualIds::ownerId(v, lane)].getValue() > 0.5f;
                float base = ownerEast ? eastLorVal(paramIdx, r, c, lo, hi)
                                       : (macroPresent ? macroVis->macroBase[lane][item] : eastLorVal(paramIdx, r, c, lo, hi));
                float blend = 0.f;
                if (macroPresent) {
                    float send = eastLOR->params[
                        StraitsEastVisualIds::sendId(v, lane, item)].getValue();
                    blend = macroVis->macroCVDelta[lane][item] * send;
                }
                return (int)math::clamp(base + blend, lo, hi);
            };

            // Spread (item 3) counterpart of combineLOR. owner picks East's own
            // per-voice interp vs Macro's global spread base; the Macro spread
            // CV-delta blends in via send item 3. Clamped bipolar [-1,1] because
            // interpolateAndClamp treats negative spread as inverting the target
            // (see the spread-sign fix). eastInterpVal already includes East's own
            // spread CV. NOTE: East's per-voice interp params are 0..1 while the
            // display trimpots + Macro spread are -1..1 — a pre-existing range
            // inconsistency, flagged separately; we clamp the COMBINED value to
            // [-1,1] so Macro ownership/blend can still reach negative spread.
            auto combineSpread = [&](int lane, float eastInterpVal)-> float {
                const bool ownerEast = eastLOR->params[
                    StraitsEastVisualIds::ownerId(v, lane)].getValue() > 0.5f;
                float base = ownerEast ? eastInterpVal
                                       : (macroPresent ? macroVis->macroBase[lane][3] : eastInterpVal);
                float blend = 0.f;
                if (macroPresent) {
                    float send = eastLOR->params[
                        StraitsEastVisualIds::sendId(v, lane, 3)].getValue();
                    blend = macroVis->macroCVDelta[lane][3] * send;
                }
                return math::clamp(base + blend, -1.f, 1.f);
            };

            // REST lane (0): owner+blend equation. r/c are East CV jack coords.
            engine.polyLen[v][0] = combineLOR(0, 0, rhythmBase,     0, 0, 1.f, 16.f);
            engine.polyOff[v][0] = combineLOR(0, 1, rhythmBase + 1, 0, 1, 0.f, 15.f);
            engine.polyRot[v][0] = combineLOR(0, 2, rhythmBase + 2, 1, 0, 0.f, 15.f);

            float restInterp = eastInterp->params[MonsoonIds::POLY_REST_INTERP_1 + v].getValue();
            if (eastVisual && eastVisual->inputs[cvId(1,1)].isConnected()) {
                float att = eastVisual->params[attenId(1,1)].getValue();
                float cv  = eastVisual->inputs[cvId(1,1)].getVoltage(v) / 10.f;
                restInterp = math::clamp(restInterp + cv * att, 0.f, 1.f);
            }
            restInterp = combineSpread(0, restInterp);   // owner + Macro-CV blend (spread)
            
            // if (deepEast) {
            //     engine.voices[v].restProb = deepEast->params[MonsoonIds::POLY_REST_PARAM_1 + v].getValue();
            // }

            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const redDot::SpreadInterp::Target mode = spreadInterpMono
                    ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                for (int j = 0; j < 16; j++) {
                    engine.pe.polyRhythmRandom[v][j] = redDot::SpreadInterp::apply(
                        engine.pe, mode, 0, j, nPoly, engine.pe.slewedPolyRhythm[v][j], restInterp);
                }
            }
            
            int melodyBase = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
            float melodyInterp = eastInterp->params[MonsoonIds::POLY_MELODY_INTERP_1 + v].getValue();
            engine.polyLen[v][1] = combineLOR(1, 0, melodyBase,     2, 0, 1.f, 16.f);
            engine.polyOff[v][1] = combineLOR(1, 1, melodyBase + 1, 2, 1, 0.f, 15.f);
            engine.polyRot[v][1] = combineLOR(1, 2, melodyBase + 2, 3, 0, 0.f, 15.f);
            if (eastVisual && eastVisual->inputs[cvId(3,1)].isConnected()) {
                float att = eastVisual->params[attenId(3,1)].getValue();
                float cv  = eastVisual->inputs[cvId(3,1)].getVoltage(v) / 10.f;
                melodyInterp = math::clamp(melodyInterp + cv * att, 0.f, 1.f);
            }
            melodyInterp = combineSpread(1, melodyInterp);   // owner + Macro-CV blend (spread)
            
            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const redDot::SpreadInterp::Target mode = spreadInterpMono
                    ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                for (int j = 0; j < 16; j++) {
                    engine.pe.polyMelodyRandom[v][j] = redDot::SpreadInterp::apply(
                        engine.pe, mode, 1, j, nPoly, engine.pe.slewedPolyMelody[v][j], melodyInterp);
                }
            }
            
            // if (deepEast) {
            //     engine.polyLen[v][1] = (int)deepEast->params[melodyBase].getValue();
            //     engine.polyOff[v][1] = (int)deepEast->params[melodyBase + 1].getValue();
            //     engine.polyRot[v][1] = (int)deepEast->params[melodyBase + 2].getValue();
            // }
            
            int octaveBase = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
            engine.polyLen[v][2] = combineLOR(2, 0, octaveBase,     4, 0, 1.f, 16.f);
            engine.polyOff[v][2] = combineLOR(2, 1, octaveBase + 1, 4, 1, 0.f, 15.f);
            engine.polyRot[v][2] = combineLOR(2, 2, octaveBase + 2, 5, 0, 0.f, 15.f);

            float octaveInterp = eastInterp->params[MonsoonIds::POLY_OCTAVE_INTERP_1 + v].getValue();
            if (eastVisual && eastVisual->inputs[cvId(5,1)].isConnected()) {
                float att = eastVisual->params[attenId(5,1)].getValue();
                float cv  = eastVisual->inputs[cvId(5,1)].getVoltage(v) / 10.f;
                octaveInterp = math::clamp(octaveInterp + cv * att, 0.f, 1.f);
            }
            octaveInterp = combineSpread(2, octaveInterp);   // owner + Macro-CV blend (spread)
            
            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const redDot::SpreadInterp::Target mode = spreadInterpMono
                    ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                for (int j = 0; j < 16; j++) {
                    engine.pe.polyOctaveRandom[v][j] = redDot::SpreadInterp::apply(
                        engine.pe, mode, 2, j, nPoly, engine.pe.slewedPolyOctave[v][j], octaveInterp);
                }
            }
            
            // if (deepEast) {
            //     engine.polyLen[v][2] = (int)deepEast->params[octaveBase].getValue();
            //     engine.polyOff[v][2] = (int)deepEast->params[octaveBase + 1].getValue();
            //     engine.polyRot[v][2] = (int)deepEast->params[octaveBase + 2].getValue();
            // }
        }
        
        // if (deepEast) {
        //     bool scrambleAll = deepEast->params[SCRAMBLE_ALL_PARAM].getValue() > 0.5f ||
        //                       deepEast->inputs[SCRAMBLE_ALL_INPUT].getVoltage() > 1.f;
        //     if (scrambleAll) {
        //         for (int v = 0; v < 7; v++) {
        //             int b = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
        //             deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
        //             deepEast->params[b+1].setValue(random::uniform() * 15.f);
        //             b = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
        //             deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
        //             deepEast->params[b+1].setValue(random::uniform() * 15.f);
        //             b = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
        //             deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
        //             deepEast->params[b+1].setValue(random::uniform() * 15.f);
        //         }
        //     } else {
        //         for (int v = 0; v < 7; v++) {
        //             if (deepEast->params[SCRAMBLE_VOICE_1 + v].getValue() > 0.5f ||
        //                 deepEast->inputs[SCRAMBLE_VOICE_1_INPUT + v].getVoltage() > 1.f) {
        //                 int b = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
        //                 deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
        //                 deepEast->params[b+1].setValue(random::uniform() * 15.f);
        //                 b = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
        //                 deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
        //                 deepEast->params[b+1].setValue(random::uniform() * 15.f);
        //                 b = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
        //                 deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
        //                 deepEast->params[b+1].setValue(random::uniform() * 15.f);
        //             }
        //         }
        //     }
            
        //     bool resetAllE = deepEast->params[RESET_ALL_PARAM].getValue() > 0.5f ||
        //                     deepEast->inputs[RESET_ALL_INPUT].getVoltage() > 1.f;
        //     if (resetAllE) {
        //         for (int v = 0; v < 7; v++) {
        //             int b = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
        //             deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
        //             b = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
        //             deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
        //             b = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
        //             deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
        //         }
        //     } else {
        //         for (int v = 0; v < 7; v++) {
        //             if (deepEast->params[RESET_VOICE_1 + v].getValue() > 0.5f ||
        //                 deepEast->inputs[RESET_VOICE_1_INPUT + v].getVoltage() > 1.f) {
        //                 int b = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
        //                 deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
        //                 b = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
        //                 deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
        //                 b = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
        //                 deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
        //             }
        //         }
        //     }
        // }
    }
}