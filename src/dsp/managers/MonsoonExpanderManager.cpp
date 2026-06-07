#include "MonsoonExpanderManager.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonDeepStraitsSands.hpp"
#include "../../StraitsEastSandsVisual.hpp"
#include "../../StraitsWestSandsVisual.hpp"

using namespace rack;

void MonsoonExpanderManager::sync(SequencerEngine& engine) {
    auto* deepEast   = cachedDeepStraitsSandsEastExpander;
    //auto* deepWest   = cachedDeepStraitsSandsWestExpander;
    auto* straitEast = cachedPolyVoiceExpander;
    auto* straitWest = cachedStraitWestExpander;
    auto* eastVisual  = cachedEastSandsVisual;
    //auto* westVisual  = cachedWestSandsVisual;

    auto* eastLOR   = eastVisual ? static_cast<rack::Module*>(eastVisual)
                                 : static_cast<rack::Module*>(deepEast);
    auto* eastInterp = eastVisual ? static_cast<rack::Module*>(eastVisual)
                                  : static_cast<rack::Module*>(deepEast);

    const bool polyBaseActive = (straitEast != nullptr) && (engine.numPolyVoices >= 1);
    const int  polyOutCap = polyBaseActive ? (straitWest ? 15 : 7) : 0;
    const int  effPolyVoices = math::clamp(engine.numPolyVoices, 0, polyOutCap);

    if (eastLOR && (straitEast || eastVisual) && polyBaseActive) {
        using namespace DeepStraitsSandsEastIds;
        using namespace StraitsEastVisualIds;
        
        for (int v = 0; v < 15; v++) {
            int rhythmBase = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
            
            auto applyLorCV = [&](int paramIdx, int r, int c, float lo, float hi)-> int {
                float base = eastLOR->params[paramIdx].getValue();
                if (eastVisual && eastVisual->inputs[cvId(r,c)].isConnected()) {
                    float att = eastVisual->params[attenId(r,c)].getValue();
                    float cv  = eastVisual->inputs[cvId(r,c)].getVoltage(v) / 10.f;
                    base = math::clamp(base + cv * att * (hi - lo), lo, hi);
                }
                return (int)base;
            };
            engine.polyLen[v][0] = applyLorCV(rhythmBase,     0, 0, 1.f, 16.f);
            engine.polyOff[v][0] = applyLorCV(rhythmBase + 1, 0, 1, 0.f, 15.f);
            engine.polyRot[v][0] = applyLorCV(rhythmBase + 2, 1, 0, 0.f, 15.f);

            float restInterp = eastInterp->params[MonsoonIds::POLY_REST_INTERP_1 + v].getValue();
            if (eastVisual && eastVisual->inputs[cvId(1,1)].isConnected()) {
                float att = eastVisual->params[attenId(1,1)].getValue();
                float cv  = eastVisual->inputs[cvId(1,1)].getVoltage(v) / 10.f;
                restInterp = math::clamp(restInterp + cv * att, 0.f, 1.f);
            }
            
            if (deepEast) {
                engine.voices[v].restProb = deepEast->params[MonsoonIds::POLY_REST_PARAM_1 + v].getValue();
            }

            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const float denom = 1.f + (float)nPoly;
                float avgRhythmRandom[16] = {};
                for (int j = 0; j < 16; j++) {
                    float s = engine.pe.slewedRhythm[j];
                    for (int i = 0; i < nPoly; i++) s += engine.pe.slewedPolyRhythm[i][j];
                    avgRhythmRandom[j] = s / denom;
                }
                for (int j = 0; j < 16; j++) {
                    float voiceVal = engine.pe.slewedPolyRhythm[v][j];
                    engine.pe.polyRhythmRandom[v][j] = voiceVal + restInterp * (avgRhythmRandom[j] - voiceVal);
                }
            }
            
            int melodyBase = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
            float melodyInterp = eastInterp->params[MonsoonIds::POLY_MELODY_INTERP_1 + v].getValue();
            engine.polyLen[v][1] = applyLorCV(melodyBase,     2, 0, 1.f, 16.f);
            engine.polyOff[v][1] = applyLorCV(melodyBase + 1, 2, 1, 0.f, 15.f);
            engine.polyRot[v][1] = applyLorCV(melodyBase + 2, 3, 0, 0.f, 15.f);
            if (eastVisual && eastVisual->inputs[cvId(3,1)].isConnected()) {
                float att = eastVisual->params[attenId(3,1)].getValue();
                float cv  = eastVisual->inputs[cvId(3,1)].getVoltage(v) / 10.f;
                melodyInterp = math::clamp(melodyInterp + cv * att, 0.f, 1.f);
            }
            
            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const float denom = 1.f + (float)nPoly;
                float avgMelodyRandom[16] = {};
                for (int j = 0; j < 16; j++) {
                    float s = engine.pe.slewedMelody[j];
                    for (int i = 0; i < nPoly; i++) s += engine.pe.slewedPolyMelody[i][j];
                    avgMelodyRandom[j] = s / denom;
                }
                for (int j = 0; j < 16; j++) {
                    float voiceVal = engine.pe.slewedPolyMelody[v][j];
                    engine.pe.polyMelodyRandom[v][j] = voiceVal + melodyInterp * (avgMelodyRandom[j] - voiceVal);
                }
            }
            
            if (deepEast) {
                engine.polyLen[v][1] = (int)deepEast->params[melodyBase].getValue();
                engine.polyOff[v][1] = (int)deepEast->params[melodyBase + 1].getValue();
                engine.polyRot[v][1] = (int)deepEast->params[melodyBase + 2].getValue();
            }
            
            int octaveBase = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
            engine.polyLen[v][2] = applyLorCV(octaveBase,     4, 0, 1.f, 16.f);
            engine.polyOff[v][2] = applyLorCV(octaveBase + 1, 4, 1, 0.f, 15.f);
            engine.polyRot[v][2] = applyLorCV(octaveBase + 2, 5, 0, 0.f, 15.f);

            float octaveInterp = eastInterp->params[MonsoonIds::POLY_OCTAVE_INTERP_1 + v].getValue();
            if (eastVisual && eastVisual->inputs[cvId(5,1)].isConnected()) {
                float att = eastVisual->params[attenId(5,1)].getValue();
                float cv  = eastVisual->inputs[cvId(5,1)].getVoltage(v) / 10.f;
                octaveInterp = math::clamp(octaveInterp + cv * att, 0.f, 1.f);
            }
            
            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const float denom = 1.f + (float)nPoly;
                float avgOctaveRandom[16] = {};
                for (int j = 0; j < 16; j++) {
                    float s = engine.pe.slewedOctave[j];
                    for (int i = 0; i < nPoly; i++) s += engine.pe.slewedPolyOctave[i][j];
                    avgOctaveRandom[j] = s / denom;
                }
                for (int j = 0; j < 16; j++) {
                    float voiceVal = engine.pe.slewedPolyOctave[v][j];
                    engine.pe.polyOctaveRandom[v][j] = voiceVal + octaveInterp * (avgOctaveRandom[j] - voiceVal);
                }
            }
            
            if (deepEast) {
                engine.polyLen[v][2] = (int)deepEast->params[octaveBase].getValue();
                engine.polyOff[v][2] = (int)deepEast->params[octaveBase + 1].getValue();
                engine.polyRot[v][2] = (int)deepEast->params[octaveBase + 2].getValue();
            }
        }
        
        if (deepEast) {
            bool scrambleAll = deepEast->params[SCRAMBLE_ALL_PARAM].getValue() > 0.5f ||
                              deepEast->inputs[SCRAMBLE_ALL_INPUT].getVoltage() > 1.f;
            if (scrambleAll) {
                for (int v = 0; v < 7; v++) {
                    int b = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
                    deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
                    deepEast->params[b+1].setValue(random::uniform() * 15.f);
                    b = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
                    deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
                    deepEast->params[b+1].setValue(random::uniform() * 15.f);
                    b = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
                    deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
                    deepEast->params[b+1].setValue(random::uniform() * 15.f);
                }
            } else {
                for (int v = 0; v < 7; v++) {
                    if (deepEast->params[SCRAMBLE_VOICE_1 + v].getValue() > 0.5f ||
                        deepEast->inputs[SCRAMBLE_VOICE_1_INPUT + v].getVoltage() > 1.f) {
                        int b = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
                        deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
                        deepEast->params[b+1].setValue(random::uniform() * 15.f);
                        b = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
                        deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
                        deepEast->params[b+1].setValue(random::uniform() * 15.f);
                        b = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
                        deepEast->params[b].setValue(random::uniform() * 15.f + 1.f);
                        deepEast->params[b+1].setValue(random::uniform() * 15.f);
                    }
                }
            }
            
            bool resetAllE = deepEast->params[RESET_ALL_PARAM].getValue() > 0.5f ||
                            deepEast->inputs[RESET_ALL_INPUT].getVoltage() > 1.f;
            if (resetAllE) {
                for (int v = 0; v < 7; v++) {
                    int b = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
                    deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
                    b = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
                    deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
                    b = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
                    deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
                }
            } else {
                for (int v = 0; v < 7; v++) {
                    if (deepEast->params[RESET_VOICE_1 + v].getValue() > 0.5f ||
                        deepEast->inputs[RESET_VOICE_1_INPUT + v].getVoltage() > 1.f) {
                        int b = MonsoonIds::POLY_DNA_VOICE_1_LEN + v * 3;
                        deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
                        b = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
                        deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
                        b = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
                        deepEast->params[b].setValue(16.f); deepEast->params[b+1].setValue(0.f); deepEast->params[b+2].setValue(0.f);
                    }
                }
            }
        }
    }
}