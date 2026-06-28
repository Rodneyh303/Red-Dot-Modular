#include "MonsoonExpanderManager.hpp"
#include "../SpreadInterp.hpp"
#include "../VoiceResolver.hpp"   // read-only shadow (step 1)
#include "../SandsTopology.hpp"   // step 3: solo/none write-guard migration
#include "../../Monsoon.hpp"
#include <cassert>
//#include "../../MonsoonDeepStraitsSands.hpp"
#include "../../StraitsEastSandsVisual.hpp"
//#include "../../StraitsWestSandsVisual.hpp"
#include "../../StraitsSandsMacroVisual.hpp"

using namespace rack;

void MonsoonExpanderManager::sync(SequencerEngine& engine, bool spreadInterpMono) {
    // Poly-lane constants (engine order REST=0,MEL=1,OCT=2,ACC=3). Named here so
    // the renumber tracks automatically and the parallel index conventions below
    // (polyLen[v][lane] / combineLOR(lane,..) / combineSpread(lane,..) /
    // SpreadInterp::apply(..,lane,..)) all move together.
    using PL = SequencerEngine;  // PL::PL_REST etc.
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

    // STEP 3: single-authority config classification (presence-only — enough for the
    // solo/none write guards; per-lane ownership inputs are filled in later steps when
    // the combination guards migrate). See docs/design/SANDS_TOPOLOGY_RESOLVER_PLAN.md.
    dotModular::SandsTopology::Inputs topoIn;
    topoIn.monoPresent     = (cachedSandsVisualExpander != nullptr);
    topoIn.eastPresent     = (cachedEastSandsVisual    != nullptr);
    topoIn.macroPresent    = (cachedMacroSandsVisual   != nullptr);
    topoIn.polyBaseActive  = polyBaseActive;
    topoIn.polyVoiceCount  = engine.numPolyVoices;
    const dotModular::SandsTopology topo = dotModular::SandsTopology::build(topoIn);

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
            // Per-voice send/atten banks are voice-number-indexed (slot 0 = voice 1/mono, slot
            // 1 = poly voice 2, …). Engine poly index v (0..14) is poly voice v+2; derive its
            // 16-wide slot through the resolver so this can't drift from the asserted slot/bank
            // invariant (slot stays off 0, the mono mix-in's slice).
            const int slot = dotModular::VoiceResolver::voiceSlot(v + dotModular::VoiceResolver::kFirstPoly);

            // East's own base+CV for an L/O/R item (paramIdx = East voice param).
            auto eastLorVal = [&](int paramIdx, int r, int c, float lo, float hi)-> float {
                float base = eastLOR->params[paramIdx].getValue();
                if (eastVisual && eastVisual->inputs[cvId(r,c)].isConnected()) {
                    float att = eastLOR->params[attenId(slot,r,c)].getValue();   // PER-VOICE depth
                    float cv  = eastVisual->inputs[cvId(r,c)].getPolyVoltage(v) / 10.f;
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
                // Macro owns → use Macro's CV-APPLIED value (base + its own main-modulator
                // CV delta), not just the base. macroCVDelta is Macro's true POST delta
                // (published in processDNA). Previously this used macroBase only, so a lane
                // delegated to Macro ignored Macro's main LOR modulators on East's display.
                float base = ownerEast ? eastLorVal(paramIdx, r, c, lo, hi)
                                       : (macroPresent ? (macroVis->macroBase[lane][item] + macroVis->macroCVDelta[lane][item])
                                                       : eastLorVal(paramIdx, r, c, lo, hi));
                // Macro-CV blend: only meaningful when EAST owns the lane (when Macro
                // owns, the lane already IS the Macro value — nothing to blend). The
                // send is a PER-VOICE attenuverter on Macro's CV contribution
                // (macroCVDelta), summed with East's own per-voice poly-CV term that
                // eastLorVal already folded into base. Default 0 → opt-in.
                float blend = 0.f;
                if (macroPresent && ownerEast) {
                    float send = macroVis->params[
                        StraitsMacroVisualIds::sendId(slot, lane, item)].getValue();
                    blend = macroVis->macroSendDelta[lane][item] * send;  // P9: tapped send delta
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
                                       : (macroPresent ? (macroVis->macroBase[lane][3] + macroVis->macroCVDelta[lane][3]) : eastInterpVal);
                float blend = 0.f;
                if (macroPresent && ownerEast) {
                    float send = macroVis->params[
                        StraitsMacroVisualIds::sendId(slot, lane, 3)].getValue();
                    blend = macroVis->macroSendDelta[lane][3] * send;  // P9: tapped send delta
                }
                return math::clamp(base + blend, -1.f, 1.f);
            };

            // REST lane (0): owner+blend equation. r/c are East CV jack coords.
            engine.polyLen[v][PL::PL_REST] = combineLOR(PL::PL_REST, 0, rhythmBase,     PL::PL_REST, 0, 1.f, 16.f);
            engine.polyOff[v][PL::PL_REST] = combineLOR(PL::PL_REST, 1, rhythmBase + 1, PL::PL_REST, 1, 0.f, 15.f);
            engine.polyRot[v][PL::PL_REST] = combineLOR(PL::PL_REST, 2, rhythmBase + 2, PL::PL_REST, 2, 0.f, 15.f);

            float restInterp = eastInterp->params[MonsoonIds::POLY_REST_INTERP_1 + v].getValue();
            if (eastVisual && eastVisual->inputs[cvId(PL::PL_REST,3)].isConnected()) {
                float att = eastLOR->params[attenId(slot,PL::PL_REST,3)].getValue();   // PER-VOICE depth
                float cv  = eastVisual->inputs[cvId(PL::PL_REST,3)].getPolyVoltage(v) / 10.f;
                restInterp = math::clamp(restInterp + cv * att * 2.f, -1.f, 1.f);   // ×2 = ±1 span
            }
            restInterp = combineSpread(PL::PL_REST, restInterp);   // owner + Macro-CV blend (spread)
            if (eastVisual) eastVisual->polySpreadEffective[v][PL::PL_REST] = restInterp;
            
            // if (deepEast) {
            //     engine.voices[v].restProb = deepEast->params[MonsoonIds::POLY_REST_PARAM_1 + v].getValue();
            // }

            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const redDot::SpreadInterp::Target mode = spreadInterpMono
                    ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                for (int j = 0; j < 16; j++) {
                    engine.pe.polyRhythmRandom[v][j] = redDot::SpreadInterp::apply(
                        engine.pe, mode, PL::PL_REST, j, nPoly, engine.pe.slewedPolyRhythm[v][j], restInterp);
                }
            }
            
            int melodyBase = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
            float melodyInterp = eastInterp->params[MonsoonIds::POLY_MELODY_INTERP_1 + v].getValue();
            engine.polyLen[v][PL::PL_MELODY] = combineLOR(PL::PL_MELODY, 0, melodyBase,     PL::PL_MELODY, 0, 1.f, 16.f);
            engine.polyOff[v][PL::PL_MELODY] = combineLOR(PL::PL_MELODY, 1, melodyBase + 1, PL::PL_MELODY, 1, 0.f, 15.f);
            engine.polyRot[v][PL::PL_MELODY] = combineLOR(PL::PL_MELODY, 2, melodyBase + 2, PL::PL_MELODY, 2, 0.f, 15.f);
            if (eastVisual && eastVisual->inputs[cvId(PL::PL_MELODY,3)].isConnected()) {
                float att = eastLOR->params[attenId(slot,PL::PL_MELODY,3)].getValue();   // PER-VOICE depth
                float cv  = eastVisual->inputs[cvId(PL::PL_MELODY,3)].getPolyVoltage(v) / 10.f;
                melodyInterp = math::clamp(melodyInterp + cv * att * 2.f, -1.f, 1.f);   // ×2 = ±1 span
            }
            melodyInterp = combineSpread(PL::PL_MELODY, melodyInterp);   // owner + Macro-CV blend (spread)
            if (eastVisual) eastVisual->polySpreadEffective[v][PL::PL_MELODY] = melodyInterp;
            
            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const redDot::SpreadInterp::Target mode = spreadInterpMono
                    ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                for (int j = 0; j < 16; j++) {
                    engine.pe.polyMelodyRandom[v][j] = redDot::SpreadInterp::apply(
                        engine.pe, mode, PL::PL_MELODY, j, nPoly, engine.pe.slewedPolyMelody[v][j], melodyInterp);
                }
            }
            
            // if (deepEast) {
            //     engine.polyLen[v][1] = (int)deepEast->params[melodyBase].getValue();
            //     engine.polyOff[v][1] = (int)deepEast->params[melodyBase + 1].getValue();
            //     engine.polyRot[v][1] = (int)deepEast->params[melodyBase + 2].getValue();
            // }
            
            int octaveBase = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
            engine.polyLen[v][PL::PL_OCTAVE] = combineLOR(PL::PL_OCTAVE, 0, octaveBase,     PL::PL_OCTAVE, 0, 1.f, 16.f);
            engine.polyOff[v][PL::PL_OCTAVE] = combineLOR(PL::PL_OCTAVE, 1, octaveBase + 1, PL::PL_OCTAVE, 1, 0.f, 15.f);
            engine.polyRot[v][PL::PL_OCTAVE] = combineLOR(PL::PL_OCTAVE, 2, octaveBase + 2, PL::PL_OCTAVE, 2, 0.f, 15.f);

            float octaveInterp = eastInterp->params[MonsoonIds::POLY_OCTAVE_INTERP_1 + v].getValue();
            if (eastVisual && eastVisual->inputs[cvId(PL::PL_OCTAVE,3)].isConnected()) {
                float att = eastLOR->params[attenId(slot,PL::PL_OCTAVE,3)].getValue();   // PER-VOICE depth
                float cv  = eastVisual->inputs[cvId(PL::PL_OCTAVE,3)].getPolyVoltage(v) / 10.f;
                octaveInterp = math::clamp(octaveInterp + cv * att * 2.f, -1.f, 1.f);   // ×2 = ±1 span
            }
            octaveInterp = combineSpread(PL::PL_OCTAVE, octaveInterp);   // owner + Macro-CV blend (spread)
            if (eastVisual) eastVisual->polySpreadEffective[v][PL::PL_OCTAVE] = octaveInterp;
            
            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const redDot::SpreadInterp::Target mode = spreadInterpMono
                    ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                for (int j = 0; j < 16; j++) {
                    engine.pe.polyOctaveRandom[v][j] = redDot::SpreadInterp::apply(
                        engine.pe, mode, PL::PL_OCTAVE, j, nPoly, engine.pe.slewedPolyOctave[v][j], octaveInterp);
                }
            }
            
            // ACCENT lane (3): per-voice base L/O/R from POLY_ACCENT_VOICE_* (default identity:
            // LEN 16). Now routed through combineLOR like REST/MEL/OCT so accent gets the owner
            // switch + Macro base + per-voice SEND blend (previously it read its base params
            // directly and bypassed combineLOR, so the accent send did nothing while the other
            // three lanes worked).
            {
                int accentBase = MonsoonIds::POLY_ACCENT_VOICE_1_LEN + v * 3;
                engine.polyLen[v][PL::PL_ACCENT] = combineLOR(PL::PL_ACCENT, 0, accentBase,     PL::PL_ACCENT, 0, 1.f, 16.f);
                engine.polyOff[v][PL::PL_ACCENT] = combineLOR(PL::PL_ACCENT, 1, accentBase + 1, PL::PL_ACCENT, 1, 0.f, 15.f);
                engine.polyRot[v][PL::PL_ACCENT] = combineLOR(PL::PL_ACCENT, 2, accentBase + 2, PL::PL_ACCENT, 2, 0.f, 15.f);
                float accentInterp = math::clamp(eastInterp->params[MonsoonIds::POLY_ACCENT_INTERP_1 + v].getValue(), -1.f, 1.f);
                // Accent spread CV (cvId(PL_ACCENT,3)) — was missing, so accent spread
                // only responded to the manual knob, never to modulation. Mirror the
                // REST/MEL/OCT path: add CV×atten, then combineSpread (owner + Macro blend).
                if (eastVisual && eastVisual->inputs[cvId(PL::PL_ACCENT,3)].isConnected()) {
                    float att = eastLOR->params[attenId(slot, PL::PL_ACCENT, 3)].getValue();   // PER-VOICE depth
                    float cv  = eastVisual->inputs[cvId(PL::PL_ACCENT,3)].getPolyVoltage(v) / 10.f;
                    accentInterp = math::clamp(accentInterp + cv * att * 2.f, -1.f, 1.f);   // ×2 = ±1 span
                }
                accentInterp = combineSpread(PL::PL_ACCENT, accentInterp);   // owner + Macro-CV blend (spread)
                if (eastVisual) eastVisual->polySpreadEffective[v][PL::PL_ACCENT] = accentInterp;   // accent spread → editor display
                if (!engine.locked) {
                    const int nPoly = effPolyVoices;
                    const redDot::SpreadInterp::Target mode = spreadInterpMono
                        ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                    for (int j = 0; j < 16; j++) {
                        engine.pe.polyAccentRandom[v][j] = redDot::SpreadInterp::apply(
                            engine.pe, mode, PL::PL_ACCENT, j, nPoly, engine.pe.slewedPolyAccent[v][j], accentInterp);
                    }
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
    else if (cachedMacroSandsVisual && polyBaseActive) {
        // ── MACRO-ONLY poly path (no East visual) ────────────────────────────
        // When Macro is the sole Sands visual editor, Macro OWNS every lane, so its
        // GLOBAL per-lane values drive all voices (there are no per-voice East params
        // and no ownership split). The East-gated block above is the ONLY other writer
        // of engine.polyLen/Off/Rot and the spread-applied polyRhythmRandom — so without
        // this, Macro-only left polyLen at defaults and polyRhythmRandom un-spread
        // (LOR only appeared to work because the WIDGET draws its window from Macro's own
        // params; the engine never saw it). macroBase/macroSendDelta are published by
        // MonsoonSandsManager::processDNA (macroDrivesOutput is true here).
        using namespace StraitsMacroVisualIds;
        auto* macroVis = cachedMacroSandsVisual;
        const redDot::SpreadInterp::Target mode = spreadInterpMono
            ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
        const int nPoly = effPolyVoices;
        if (!engine.locked) {
            // V1 (mono final arrays + mono strand LOR): Macro owns V1 too when it is the
            // sole visual. The hasMonoVisual block (which normally does this) is skipped
            // with no Mono editor, so apply Macro's global LOR/spread to the mono strand
            // here. (engine strand order: REST/MEL/OCT/ACC = macroBase lanes 0/1/2/3.)
            // BUT: when a Mono visual IS present, Mono owns V1 per-lane and
            // MonsoonSandsManager::readStrand already writes the mono strands/finals from
            // Mono's own params (respecting each lane's owner). Writing them here too would
            // clobber Mono's V1 LOR every frame with Macro's global base — the editor's
            // window then never matched the edit values (uneditable-looking V1 in
            // Mono+Macro). So skip the V1/mono-strand writes when Mono is attached; the
            // poly-voice writes below still run (Mono only owns V1, not V2+).
            // STEP 3 migration: this V1/mono-strand sub-block is the MACRO_SOLE case.
            // At this site East-visual is absent (the East branch above is an `if`, this
            // is its `else if`), so "no Mono" here means exactly MACRO_SOLE. Was:
            //   if (!cachedSandsVisualExpander)
            // Debug cross-check that the topology agrees before relying on it.
            assert((topo.config == dotModular::SandsTopology::Config::MACRO_SOLE)
                   == (!cachedSandsVisualExpander) && "topo MACRO_SOLE must match !mono here");
            if (topo.config == dotModular::SandsTopology::Config::MACRO_SOLE) {
                const float spR = math::clamp(macroVis->macroBase[PL::PL_REST][3]   + macroVis->macroSendDelta[PL::PL_REST][3],   -1.f, 1.f);
                const float spM = math::clamp(macroVis->macroBase[PL::PL_MELODY][3] + macroVis->macroSendDelta[PL::PL_MELODY][3], -1.f, 1.f);
                const float spO = math::clamp(macroVis->macroBase[PL::PL_OCTAVE][3] + macroVis->macroSendDelta[PL::PL_OCTAVE][3], -1.f, 1.f);
                const float spA = math::clamp(macroVis->macroBase[PL::PL_ACCENT][3] + macroVis->macroSendDelta[PL::PL_ACCENT][3], -1.f, 1.f);
                for (int j = 0; j < 16; ++j) {
                    engine.pe.rhythmRandom[j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_REST,   j, nPoly, engine.pe.slewedRhythm[j], spR);
                    engine.pe.melodyRandom[j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_MELODY, j, nPoly, engine.pe.slewedMelody[j], spM);
                    engine.pe.octaveRandom[j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_OCTAVE, j, nPoly, engine.pe.slewedOctave[j], spO);
                    engine.pe.accentRandom[j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_ACCENT, j, nPoly, engine.pe.slewedAccent[j], spA);
                }
                // Mono strand LOR from Macro globals (REST/MEL/OCT/ACC strands).
                static const int STRND[4] = { dotModular::STRAND_RHYTHM, dotModular::STRAND_MELODY,
                                              dotModular::STRAND_OCTAVE, dotModular::STRAND_ACCENT };
                for (int lane = 0; lane < 4; ++lane) {
                    engine.strandLenRef(STRND[lane]) = (int)math::clamp(std::round(macroVis->macroBase[lane][0]), 1.f, 16.f);
                    engine.strandOffRef(STRND[lane]) = ((int)std::round(macroVis->macroBase[lane][1]) % 16 + 16) % 16;
                    engine.strandRotRef(STRND[lane]) = ((int)std::round(macroVis->macroBase[lane][2]) % 16 + 16) % 16;
                }
            }
            for (int v = 0; v < effPolyVoices; ++v) {
                // Global LOR per lane (Macro owns → no per-voice/ownership branch).
                // macroBase[lane][item]: item 0=LEN 1=OFF 2=ROT, engine-lane indexed.
                for (int lane = 0; lane < PL::PL_LANES; ++lane) {
                    engine.polyLen[v][lane] = (int)math::clamp(std::round(macroVis->macroBase[lane][0]), 1.f, 16.f);
                    engine.polyOff[v][lane] = ((int)std::round(macroVis->macroBase[lane][1]) % 16 + 16) % 16;
                    engine.polyRot[v][lane] = ((int)std::round(macroVis->macroBase[lane][2]) % 16 + 16) % 16;
                }
                // Global spread per lane → spread-applied final arrays for this voice.
                const float spR = math::clamp(macroVis->macroBase[PL::PL_REST][3]   + macroVis->macroSendDelta[PL::PL_REST][3],   -1.f, 1.f);
                const float spM = math::clamp(macroVis->macroBase[PL::PL_MELODY][3] + macroVis->macroSendDelta[PL::PL_MELODY][3], -1.f, 1.f);
                const float spO = math::clamp(macroVis->macroBase[PL::PL_OCTAVE][3] + macroVis->macroSendDelta[PL::PL_OCTAVE][3], -1.f, 1.f);
                const float spA = math::clamp(macroVis->macroBase[PL::PL_ACCENT][3] + macroVis->macroSendDelta[PL::PL_ACCENT][3], -1.f, 1.f);
                for (int j = 0; j < 16; ++j) {
                    engine.pe.polyRhythmRandom[v][j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_REST,   j, nPoly, engine.pe.slewedPolyRhythm[v][j], spR);
                    engine.pe.polyMelodyRandom[v][j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_MELODY, j, nPoly, engine.pe.slewedPolyMelody[v][j], spM);
                    engine.pe.polyOctaveRandom[v][j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_OCTAVE, j, nPoly, engine.pe.slewedPolyOctave[v][j], spO);
                    engine.pe.polyAccentRandom[v][j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_ACCENT, j, nPoly, engine.pe.slewedPolyAccent[v][j], spA);
                }
            }
        }
    }
}