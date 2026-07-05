#include "MonsoonSandsManager.hpp"
#include "../VoiceResolver.hpp"   // kMonoSlot — the mono mix-in slice
#include "../SpreadInterp.hpp"
#include "../../Monsoon.hpp"
#include "../../MonsoonSandsVisualExpander.hpp"
//#include "../../MonsoonSandsExpander.hpp"
#include "../../StraitsSandsMacroVisual.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../SandsTopology.hpp"   // step 3b: readStrand owner migration
#include "../../MonsoonStraitsExpander.hpp"
#include "../../StraitsEastSandsVisual.hpp"
#include <cassert>

using namespace rack;
using namespace MonsoonIds;
// NOTE: NOT using namespace SandsMonoVisualIds or StraitsMacroVisualIds
// to avoid ambiguous cvId/attenId/sprId calls — qualify explicitly below.
namespace Mono  = SandsMonoVisualIds;
namespace Macro = StraitsMacroVisualIds;
namespace East  = StraitsEastVisualIds;   // for the voice-1 mix-in (interp. Y)

void MonsoonSandsManager::processDNA(const MonsoonExpanderManager& expanderManager, bool spreadInterpMono) {
    // ── Expander-presence / activity flags (names chosen to avoid the old trap) ──
    //   hasMonoVisual    : the SANDS MONO editor is attached (NOT "any visual"; NOT Macro).
    //   hasMacro         : the MACRO visual is attached (standalone counts — the right flag
    //                      for Macro's DISPLAY/spread, which works without an output path).
    //   hasEastVisual    : the East Sands visual is attached.
    //   polyBaseActive   : a Straits BASE OUTPUT expander (East) present AND >=1 poly voice —
    //                      i.e. there is an actual audio output path for poly voices.
    //   macroDrivesOutput: Macro attached AND polyBaseActive — Macro modulation reaches real
    //                      OUTPUT voices. NOT "Macro is on": standalone Macro still drives its
    //                      own display, just no output.
    const bool hasMonoVisual = (expanderManager.cachedSandsVisualExpander != nullptr);
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
    // Voice OUTPUT topology bounds the spread ensemble: the Straits base expander
    // outputs poly voices 2..16 (engine.voices[0..14], up to 15) on 16ch poly cables.
    // Voices with no output path must NOT be averaged into spread, or the ensemble
    // converges toward phantom voices. West retired (Straits redesign): one Straits
    // module now covers all poly voices, so the cap is simply "poly base present → 15".
    const int  polyOutCap = polyBaseActive ? 15 : 0;
    const int  effPolyVoices = clamp(engine.numPolyVoices, 0, polyOutCap);

    // STEP 3b: build the ownership authority for this block, with the per-lane Mono V1
    // ownership populated (step 3a only needed presence). readStrand's owner decisions
    // migrate onto topo.owner(0, l). See docs/design/SANDS_TOPOLOGY_RESOLVER_PLAN.md.
    dotModular::SandsTopology::Inputs topoIn;
    topoIn.monoPresent    = hasMonoVisual;
    topoIn.eastPresent    = hasEastVisual;
    topoIn.macroPresent   = hasMacro;
    topoIn.polyBaseActive = polyBaseActive;
    topoIn.polyVoiceCount = engine.numPolyVoices;
    if (monoVis) {
        for (int l = 0; l < 4; ++l)
            topoIn.monoV1Owner[l] = monoVis->params[Mono::ownerDispId(l)].getValue() > 0.5f;
    }
    const dotModular::SandsTopology topo = dotModular::SandsTopology::build(topoIn);
    const bool macroDrivesOutput = hasMacro && polyBaseActive;
    // sandsActive must be true whenever ANYTHING writes the spread-applied final arrays,
    // or PatternEngine's slew stage copies the raw slewed draws back over them every
    // sample. Macro STANDALONE (hasMacro, no base expander) now applies its global spread
    // to the voices (see the else-branch below), so include it here too — otherwise that
    // work was silently overwritten and the spread knob looked dead.
    engine.pe.setSandsActive(hasMonoVisual || macroDrivesOutput || (hasEastVisual && polyBaseActive) || hasMacro);
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
    // applyMonoSprCV. The spread param is BIPOLAR ±1 → span (hi-lo) = 2. To make a
    // full-depth CV reach the full range from a centered base, multiply by the span
    // (×2), same as the LOR path's ×(hi-lo). (The earlier code dropped this ×2 on the
    // mistaken belief it would "double" the CV — that left ±5V reaching only HALF the
    // ±1 range. East V1 spread already used ×2 correctly; this aligns Mono/Macro.)
    // param index 3 = SPR.
    auto applyMacroSprCV = [&](float base, int lane) -> float {
        if (!macroVis || !macroVis->inputs[Macro::macroCvId(lane,3)].isConnected()) return base;
        float cv  = macroVis->inputs[Macro::macroCvId(lane,3)].getVoltage() / 10.f;
        float att = macroVis->params[Macro::macroAttenId(lane,3)].getValue();
        return clamp(base + cv * att * 2.f, -1.f, 1.f);   // ×2 = ±1 span
    };

    // ── Helper: apply mono spread CV (REST/MEL/OCT only, own jack/atten) ──
    auto applyMonoSprCV = [&](float base, int sprLane) -> float {
        if (!monoVis || !monoVis->inputs[Mono::sprCvId(sprLane)].isConnected()) return base;
        float cv  = monoVis->inputs[Mono::sprCvId(sprLane)].getVoltage() / 10.f;
        float att = monoVis->params[Mono::sprAttenId(sprLane)].getValue();
        // Spread is BIPOLAR (param range -1..1; negative inverts the interp target).
        // ×2 spans the full ±1 range at full depth; clamp to [-1,1] (not [0,1], which
        // would floor negative spread to 0).
        return clamp(base + cv * att * 2.f, -1.f, 1.f);   // ×2 = ±1 span
    };

    // Spread interpolation is now centralised in redDot::SpreadInterp (see
    // src/dsp/SpreadInterp.hpp) — the single source of truth.

    if (hasMonoVisual) 
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
            // l is the EDITOR lane. The Mono LOR params are now ALSO editor-ordered
            // (LEN_MELODY=0, OCT, REST, ACC, VAR, LEG), so lenId(l) reads directly —
            // no editor→param-bank remap any more.
            float baseLen = monoVis->params[Mono::lenId(l)].getValue();
            float baseOff = monoVis->params[Mono::offId(l)].getValue();
            float baseRot = monoVis->params[Mono::rotId(l)].getValue();

            // V1 ownership (poly lanes only: editor 0..3 = MEL/OCT/REST/ACC). When
            // Mono CEDES a lane (ownerDispId == 0) and Macro is attached, V1's base
            // comes from Macro's GLOBAL base for that lane instead of Mono's own LOR
            // edit — mirroring how poly voices switch base by ownerId. LEG/VAR (l>=4)
            // are mono-only and always Mono-owned. (macroBase is published later in
            // this same block → one control-block lag, same as the macroMix delta.)
            if (l < 4 && hasMacro && macroVis) {
                // STEP 3b: delegated ⟺ topo.owner(0,l) == MACRO.
                const bool delegated = (topo.owner(0, l) == dotModular::SandsTopology::Role::MACRO);
                if (delegated) {
                    int el = dotModular::EDITOR_TO_ENGINE_LANE[l];   // editor → poly engine lane
                    // Delegated → track Macro's base + the TAPPED CV delta (macroSendDelta),
                    // so the per-lane PRE/POST send tap controls how Macro's modulation
                    // reaches the delegated lane: PRE (tap=0) = raw CV even when the left
                    // atten is 0; POST (tap=1) = CV × left atten. (Was macroCVDelta = always
                    // POST, so the tap did nothing and att=0 killed all modulation here.)
                    baseLen = macroVis->macroBase[el][0] + macroVis->macroSendDelta[el][0];
                    baseOff = macroVis->macroBase[el][1] + macroVis->macroSendDelta[el][1];
                    baseRot = macroVis->macroBase[el][2] + macroVis->macroSendDelta[el][2];
                }
            }

            // Mono's own CV applies only to lanes Mono OWNS. A delegated lane tracks
            // Macro exclusively (G5) — Mono's CV must not additionally modulate it.
            // Mono owns ⟺ owner(0,l)==MONO; lanes >=4 (VAR/LEG) are always MONO when present.
            const bool monoOwnsLane = (topo.owner(0, l) == dotModular::SandsTopology::Role::MONO);
            if (monoOwnsLane) {
                baseLen = applyMonoCV(baseLen, l, 0, 1.f, 16.f);
                baseOff = applyMonoCV(baseOff, l, 1, 0.f, 15.f);
                baseRot = applyMonoCV(baseRot, l, 2, 0.f, 15.f);
            }

            // INTERPRETATION Y — voice-1 mix-in. The mono master strand is voice 1 of
            // the ensemble; like every poly voice it can receive the East per-lane CV
            // and the Macro global-CV mix-in, summed on top of the mono base. We use
            // SLOT 0 of the East/Macro per-voice banks — which is now voice 1 / mono under
            // the voice-number (N→N) indexing (slot v = voice v+1). Poly voices read slot v+1,
            // so they no longer collide with this mono slot.
            // Only when the respective module is attached. Lane/item→East(r,c) mirrors
            // the poly combineLOR mapping. (Base stays mono's — this is modulation only.)
            auto* eastVis = expanderManager.cachedEastSandsVisual;
            // East per-lane CV folded onto voice 1 (voice-0 depth × East CV ch0).
            // ADDITIVE: East CV is summed on top of Mono's base (base stays Mono's;
            // this is V1 modulation only — combo 7/8 "East CV into V1 adds to V1 mod").
            // (engLane, item) addresses East's CV jack exactly as the gen binds it
            // (cvId(engineLane, item)); previously this used a wrong l*2 2-rows-per-lane
            // scheme — the same class as the poly CV crosstalk bug.
            auto eastMix = [&](float base, int engLane, int item, float lo, float hi)->float {
                if (!eastVis || !eastVis->inputs[East::cvId(engLane,item)].isConnected()) return base;
                float att = eastVis->params[East::attenId(dotModular::VoiceResolver::kMonoSlot, engLane, item)].getValue();   // slot 0 = mono
                float cv  = eastVis->inputs[East::cvId(engLane,item)].getVoltage(0) / 10.f; // ch0
                return math::clamp(base + cv * att * (hi - lo), lo, hi);
            };
            // Macro global CV distributed to voice 1 (voice-0 send × macroCVDelta).
            // sendId AND macroCVDelta are ENGINE-lane indexed (REST=0,MEL=1,OCT=2,
            // ACC=3) — same as the poly path — so pass the engine lane, not editor l.
            // NOTE: macroCVDelta is published by the Macro block LATER in this same
            // processDNA (after the mono strand), so voice 1 reads the PREVIOUS control
            // block's delta — a one-block (sub-ms, control-rate) lag. Imperceptible.
            auto macroMix = [&](float base, int engLane, int item, float lo, float hi)->float {
                if (!hasMacro || !macroVis) return base;
                float send = macroVis->params[Macro::sendId(dotModular::VoiceResolver::kMonoSlot, engLane, item)].getValue();   // slot 0 = mono
                return math::clamp(base + macroVis->macroSendDelta[engLane][item] * send, lo, hi);  // P9: tapped send delta
            };
            // East CV exists only for the 4 poly lanes (MEL/OCT/REST/ACC); VAR/LEG
            // (editor l>=4) are mono-only with no East jack → skip the eastMix there.
            // A DELEGATED lane tracks Macro's value exclusively (set above) — it takes
            // neither East's CV nor the Macro SEND on top (that would double-count Macro's
            // modulation, already in the delegated base). Only OWNED lanes receive these.
            if (l < 4 && monoOwnsLane) {
                int eng = dotModular::EDITOR_TO_ENGINE_LANE[l];   // editor → engine lane (East CV jack)
                baseLen = eastMix(baseLen, eng, 0, 1.f, 16.f);
                baseOff = eastMix(baseOff, eng, 1, 0.f, 15.f);
                baseRot = eastMix(baseRot, eng, 2, 0.f, 15.f);
                baseLen = macroMix(baseLen, eng, 0, 1.f, 16.f);
                baseOff = macroMix(baseOff, eng, 1, 0.f, 15.f);
                baseRot = macroMix(baseRot, eng, 2, 0.f, 15.f);
            }

            engine.setStrand(StrandWriter::MONO, strand,
                             (int)std::round(baseLen),
                             (int)std::round(baseOff),
                             (int)std::round(baseRot));
        };

        // Spread (REST/MEL/OCT only): base trimpot + per-lane spread CV.
        // sprId/sprCvId lane index: 0=REST, 1=MELODY, 2=OCTAVE (editor order).
       // if (hasMonoVisual) 
       // {
            // REST/MEL/OCT/ACCENT spread (engine/spread lanes 0..3). Accent is now
            // wired like the others (was previously skipped: loop l<3 + spreadEffective[3]=0,
            // so Mono accent spread did nothing). sprId(3)=accent base; East spread CV
            // jack cvId(3,3); Macro send/delta lane 3.
            for (int l = 0; l < 4; ++l) {
                // Delegation: if Mono CEDES this poly spread lane to Macro, the spread
                // tracks Macro's global spread (base + tapped send delta) exclusively —
                // no Mono base, no Mono/East CV (parallel to the LOR delegation above).
                // l is the SPREAD/engine lane (0=REST,1=MEL,2=OCT,3=ACC); ownerDispId is
                // EDITOR-ordered (0=MEL,1=OCT,2=REST,3=ACC), so convert — otherwise the
                // owner of lane N+1 gates lane N's delegation (octave owner ↔ melody
                // spread, etc.).
                // l is the SPREAD/engine lane (0=REST,1=MEL,2=OCT,3=ACC). Use the helper
                // that bakes the engine→editor conversion (Mono's ownerDispId is editor-
                // ordered) so we can't regress the owner-of-N+1-gates-N off-by-one.
                bool sprDelegated = hasMacro && macroVis &&
                                    SandsMonoVisualIds::monoMacroOwnsEngineLane(monoVis, l);
                if (sprDelegated) {
                    float sp = rack::math::clamp(macroVis->macroBase[l][3] + macroVis->macroSendDelta[l][3], -1.f, 1.f);
                    monoVis->spreadEffective[l] = sp;
                    continue;
                }
                float baseSpr = monoVis->params[Mono::sprId(l)].getValue();
                float sp = applyMonoSprCV(baseSpr, l);
                // INTERP Y — voice-1 spread mix-in (voice 0 slice), bipolar [-1,1].
                // l is the spread/engine lane (0=REST,1=MEL,2=OCT,3=ACC). East's spread
                // CV jack for that lane is cvId(engineLane, 3) (col 3 = SPR). East CV
                // ADDS to the mono spread (combo 7/8 V1 mod).
                auto* eastVisS = expanderManager.cachedEastSandsVisual;
                if (eastVisS && eastVisS->inputs[East::cvId(l,3)].isConnected()) {
                    float att = eastVisS->params[East::attenId(dotModular::VoiceResolver::kMonoSlot, l, 3)].getValue();  // slot 0 = mono
                    float cv  = eastVisS->inputs[East::cvId(l,3)].getVoltage(0) / 10.f;
                    sp = rack::math::clamp(sp + cv * att * 2.f, -1.f, 1.f);   // span [-1,1]=2
                }
                if (hasMacro && macroVis) {
                    float send = macroVis->params[Macro::sendId(dotModular::VoiceResolver::kMonoSlot, l, 3)].getValue();
                    sp = rack::math::clamp(sp + macroVis->macroSendDelta[l][3] * send, -1.f, 1.f);  // P9: tapped
                }
                monoVis->spreadEffective[l] = sp;
            }
            // spreadEffective[] is ENGINE/spread-indexed: 0=REST,1=MEL,2=OCT,3=ACCENT.
            // Indices 4/5 are unused (LEG/VAR are mono-only and have no spread).

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
                engine.pe.accentRandom[i] = redDot::SpreadInterp::apply(
                    engine.pe, mode, 3, i, nPoly, engine.pe.slewedAccent[i], monoVis->spreadEffective[3]);
                engine.pe.variationRandom[i] = engine.pe.slewedVariation[i];
            }
            }  // end if(!engine.locked)
        //}
        // Drive all 6 mono strands via the single-source-of-truth lane map.
        for (int l = 0; l < 6; ++l) readStrand(l);

    } else {
        // No Mono visual. If East is present and acting as the V1 editor (combo 3:
        // East, no Mono), East writes the engine mono strands itself (from its V1
        // editor) — do NOT reset them here, or we'd clobber East's V1 edits every
        // block. Only reset to defaults when truly nothing drives the mono strand.
        auto* eastV1 = expanderManager.cachedEastSandsVisual;
        // East drives V1 whenever East visual is present and no Mono (Mono owns V1). Macro
        // may also be present: V1 is then PER-LANE (each lane owned by East or delegated to
        // Macro). hasEastV1 no longer excludes Macro — the per-lane owner switch handles it.
        const bool hasEastV1 = (eastV1 != nullptr);
        if (!hasEastV1 && engine.rhythmLen != 16) {
            engine.rhythmLen = engine.variationLen = engine.legatoLen =
            engine.accentLen = engine.melodyLen = engine.octaveLen = 16;
            engine.rhythmOff = engine.variationOff = engine.legatoOff =
            engine.accentOff = engine.melodyOff = engine.octaveOff = 0;
            engine.rhythmRot = engine.variationRot = engine.legatoRot =
            engine.accentRot = engine.melodyRot = engine.octaveRot = 0;
        }
        if (hasEastV1 && !engine.locked) {
            // Apply V1 SPREAD to the mono final arrays, PER LANE:
            //   • lane owned by East   → East's V1 spread knob (+ East V1 spread CV).
            //   • lane delegated Macro → Macro's GLOBAL spread (macroBase[lane][3] + send).
            // (East SPREAD_R/M/O/A + macroBase are spread/engine-indexed 0=REST..3=ACC.)
            // Macro-owned test mirrors the widget: East ownerDispId(lane) <= 0.5 == Macro.
            const bool macroHere = hasMacro && (macroVis != nullptr);
            auto monoOwnedByMacro = [&](int lane)->bool {
                return macroHere && !(eastV1->params[East::ownerDispId(lane)].getValue() > 0.5f);
            };
            auto sprForLane = [&](int lane)->float {
                if (monoOwnedByMacro(lane))
                    return rack::math::clamp(macroVis->macroBase[lane][3] + macroVis->macroCVDelta[lane][3], -1.f, 1.f);
                float sp = eastV1->params[East::SPREAD_R + lane].getValue();   // R/M/O/A contiguous
                if (eastV1->inputs[East::cvId(lane,3)].isConnected()) {
                    float att = eastV1->params[East::attenId(dotModular::VoiceResolver::kMonoSlot, lane, 3)].getValue();
                    float cv  = eastV1->inputs[East::cvId(lane,3)].getPolyVoltage(0) / 10.f;  // ch0 = V1
                    sp = sp + cv * att * 2.f;
                }
                // Macro send blend on East-owned V1 spread (mirrors poly combineSpread):
                // macroSendDelta[lane][3] * mono-slot send.
                if (macroHere) {
                    float send = macroVis->params[StraitsMacroVisualIds::sendId(
                        dotModular::VoiceResolver::kMonoSlot, lane, 3)].getValue();
                    sp += macroVis->macroSendDelta[lane][3] * send;
                }
                return rack::math::clamp(sp, -1.f, 1.f);
            };
            const float spR = sprForLane(0);
            const float spM = sprForLane(1);
            const float spO = sprForLane(2);
            const float spA = sprForLane(3);
            const int nPoly = effPolyVoices;
            const redDot::SpreadInterp::Target mode = spreadInterpMono
                ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
            engine.pe.setSandsActive(true);
            for (int i = 0; i < 16; ++i) {
                engine.pe.rhythmRandom[i] = redDot::SpreadInterp::apply(engine.pe, mode, 0, i, nPoly, engine.pe.slewedRhythm[i], spR);
                engine.pe.melodyRandom[i] = redDot::SpreadInterp::apply(engine.pe, mode, 1, i, nPoly, engine.pe.slewedMelody[i], spM);
                engine.pe.octaveRandom[i] = redDot::SpreadInterp::apply(engine.pe, mode, 2, i, nPoly, engine.pe.slewedOctave[i], spO);
                engine.pe.accentRandom[i] = redDot::SpreadInterp::apply(engine.pe, mode, 3, i, nPoly, engine.pe.slewedAccent[i], spA);
                engine.pe.legatoRandom[i]    = engine.pe.slewedLegato[i];
                engine.pe.variationRandom[i] = engine.pe.slewedVariation[i];
            }
        }
    }

    // ── Macro global LOR with CV (poly: per-lane, SAME for every voice) ────
    if (macroDrivesOutput) {
        // Macro publishes its per-(lane,item) base + CV-delta split; East's sync
        // (runs after, always present when macroDrivesOutput) combines them per voice
        // via the owner switch + blend send. Macro no longer writes engine.polyLen
        // directly — East owns the final write so the blend equation is applied in
        // one place.
        auto publishGlobal = [&](int lane) {
            // bases (knob, no CV)
            float baseLen = macroVis->params[Macro::lorId(lane,0)].getValue();
            float baseOff = macroVis->params[Macro::lorId(lane,1)].getValue();
            float baseRot = macroVis->params[Macro::lorId(lane,2)].getValue();
            float baseSpr = macroVis->params[Macro::sprId(lane)].getValue();
            // CV-applied (Macro's OWN value — true POST, drives Macro's own display).
            float cvLen = applyMacroCV(baseLen, lane, 0, 1.f, 16.f);
            float cvOff = applyMacroCV(baseOff, lane, 1, 0.f, 15.f);
            float cvRot = applyMacroCV(baseRot, lane, 2, 0.f, 15.f);
            float cvSpr = applyMacroSprCV(baseSpr, lane);
            // P9 send-tap: the delta the SENDS distribute can be tapped PRE or POST the
            // left attenuverter. POST(tap=1)=cv*att (== the true delta); PRE(tap=0)=raw cv.
            // effective atten = lerp(1, att, tap) = 1 + (att-1)*tap.
            auto tappedDelta = [&](int item, float lo, float hi) -> float {
                if (!macroVis || !macroVis->inputs[Macro::macroCvId(lane,item)].isConnected()) return 0.f;
                float cv  = macroVis->inputs[Macro::macroCvId(lane,item)].getVoltage() / 10.f;
                float att = macroVis->params[Macro::macroAttenId(lane,item)].getValue();
                float tap = macroVis->params[Macro::tapIdForItem(lane,item)].getValue();  // P9b: LOR tap (0-2) or spread tap (3)
                float effAtt = 1.f + (att - 1.f) * tap;
                return cv * effAtt * (hi - lo);
            };
            // macroCVDelta = true POST delta (Macro's own display). macroSendDelta =
            // tapped delta (what the sends carry). item 0=LEN 1=OFF 2=ROT 3=SPR.
            macroVis->macroBase[lane][0] = baseLen;  macroVis->macroCVDelta[lane][0] = cvLen - baseLen;  macroVis->macroSendDelta[lane][0] = tappedDelta(0, 1.f, 16.f);
            macroVis->macroBase[lane][1] = baseOff;  macroVis->macroCVDelta[lane][1] = cvOff - baseOff;  macroVis->macroSendDelta[lane][1] = tappedDelta(1, 0.f, 15.f);
            macroVis->macroBase[lane][2] = baseRot;  macroVis->macroCVDelta[lane][2] = cvRot - baseRot;  macroVis->macroSendDelta[lane][2] = tappedDelta(2, 0.f, 15.f);
            macroVis->macroBase[lane][3] = baseSpr;  macroVis->macroCVDelta[lane][3] = cvSpr - baseSpr;  macroVis->macroSendDelta[lane][3] = tappedDelta(3, -1.f, 1.f);
            // spread display reads the CV-applied global spread (base+CV, no blend)
            macroVis->spreadEffective[lane] = cvSpr;
        };
        publishGlobal(0);
        publishGlobal(1);
        publishGlobal(2);
        publishGlobal(3);   // accent lane (Stage 6)
    } else if (hasMacro && macroVis) {
        // Macro standalone (Macro present, no East base expander). The poly voices still
        // EXIST — Monsoon generates numPolyVoices worth of draws regardless of East — but
        // the per-voice spread application lives in MonsoonExpanderManager, gated on East.
        // So nothing applied Macro's GLOBAL spread to the voices and the bars looked dead.
        // Apply it here, exactly like East/Mono but with one GLOBAL level per lane for
        // every voice. Ensemble = the voices that actually have draws (numPolyVoices) —
        // NOT effPolyVoices, which is 0 here because it tracks East's OUTPUT topology, not
        // the drawn voices we display. Spread converges each voice's draw toward the
        // cross-voice average (or V1 in MONO_DRAW mode), the same definition as East/Mono.
        if (!engine.locked) {
            const int nPoly = rack::math::clamp(engine.numPolyVoices, 0, 15);
            const redDot::SpreadInterp::Target mode = spreadInterpMono
                ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
            // Global spread level per lane (knob + CV), spread/engine-indexed 0..3.
            float spv[4];
            for (int lane = 0; lane < 4; ++lane) {
                float baseSpr = macroVis->params[Macro::sprId(lane)].getValue();
                float sp = baseSpr;
                if (macroVis->inputs[Macro::macroCvId(lane,3)].isConnected()) {
                    float att = macroVis->params[Macro::macroAttenId(lane,3)].getValue();
                    float cv  = macroVis->inputs[Macro::macroCvId(lane,3)].getVoltage() / 10.f;
                    sp = rack::math::clamp(baseSpr + cv * att * 2.f, -1.f, 1.f);   // ×2 = ±1 span
                }
                macroVis->spreadEffective[lane] = sp;
                spv[lane] = sp;
            }
            engine.pe.setSandsActive(true);
            // V1 (mono final arrays): converge the mono draw toward the ensemble.
            for (int i = 0; i < 16; ++i) {
                engine.pe.rhythmRandom[i]    = redDot::SpreadInterp::apply(engine.pe, mode, 0, i, nPoly, engine.pe.slewedRhythm[i], spv[0]);
                engine.pe.melodyRandom[i]    = redDot::SpreadInterp::apply(engine.pe, mode, 1, i, nPoly, engine.pe.slewedMelody[i], spv[1]);
                engine.pe.octaveRandom[i]    = redDot::SpreadInterp::apply(engine.pe, mode, 2, i, nPoly, engine.pe.slewedOctave[i], spv[2]);
                engine.pe.accentRandom[i]    = redDot::SpreadInterp::apply(engine.pe, mode, 3, i, nPoly, engine.pe.slewedAccent[i], spv[3]);
                engine.pe.legatoRandom[i]    = engine.pe.slewedLegato[i];
                engine.pe.variationRandom[i] = engine.pe.slewedVariation[i];
            }
            // Each poly voice's final arrays: same global spread level applied per voice.
            // apply() computes the lane's ensemble target internally; pass each voice's
            // own slewed draw as the value to interpolate (same call shape as East's loop).
            for (int v = 0; v < nPoly; ++v) {
                for (int i = 0; i < 16; ++i) {
                    engine.pe.polyRhythmRandom[v][i] = redDot::SpreadInterp::apply(engine.pe, mode, 0, i, nPoly, engine.pe.slewedPolyRhythm[v][i], spv[0]);
                    engine.pe.polyMelodyRandom[v][i] = redDot::SpreadInterp::apply(engine.pe, mode, 1, i, nPoly, engine.pe.slewedPolyMelody[v][i], spv[1]);
                    engine.pe.polyOctaveRandom[v][i] = redDot::SpreadInterp::apply(engine.pe, mode, 2, i, nPoly, engine.pe.slewedPolyOctave[v][i], spv[2]);
                    engine.pe.polyAccentRandom[v][i] = redDot::SpreadInterp::apply(engine.pe, mode, 3, i, nPoly, engine.pe.slewedPolyAccent[v][i], spv[3]);
                }
            }
        }
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
