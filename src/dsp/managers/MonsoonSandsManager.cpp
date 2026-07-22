#include "MonsoonSandsManager.hpp"
#include "../VoiceResolver.hpp"   // kMonoSlot — the mono mix-in slice
#include "../SpreadInterp.hpp"
#include "../SpreadResolver.hpp"   // step 3b: single authority for effective spread amount
#include "../../Monsoon.hpp"
#include "../../MonsoonSandsVisualExpander.hpp"
//#include "../../MonsoonSandsExpander.hpp"
#include "../../StraitsSandsMacroVisual.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonChangeAlleyExpander.hpp"   // full type: processDNA reads ca->rhythmSrc/melodySrc
#include "../SandsTopology.hpp"   // step 3b: readStrand owner migration
#include "../../MonsoonStraitsExpander.hpp"
#include "../../StraitsEastSandsVisual.hpp"
#include "../../ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide (MACRO field reads)
#include <cassert>

using namespace rack;
using namespace MonsoonIds;
// NOTE: NOT using namespace SandsMonoVisualIds or StraitsMacroVisualIds
// to avoid ambiguous cvId/attenId/sprId calls — qualify explicitly below.
namespace Mono  = SandsMonoVisualIds;
namespace Macro = StraitsMacroVisualIds;
namespace East  = StraitsEastVisualIds;   // for the voice-1 mix-in (interp. Y)

void MonsoonSandsManager::processDNA(const MonsoonExpanderManager& expanderManager) {
    // ── Change Alley pin remap (PRE-SPREAD) ──────────────────────────────────
    // Push the current pins into pe, then remap the SLEWED buffers by pin BEFORE any
    // spread runs below. A pinned voice's borrowed draw is then spread with the CONSUMER's
    // own reference (== pinning the A/B samples, the agreed design). Identity = no-op.
    // IDEMPOTENCE: slewed persists across cycles (recompute is mix-latch-gated, not
    // per-cycle), so remapping in place would compound. Regenerate slewed fresh from the
    // pristine A/B buffers FIRST (recompute is a cheap re-derivation, MixApplied-idempotent),
    // then remap the clean result. This makes the per-cycle remap correct and non-walking.
    if (expanderManager.cachedChangeAlleyExpander) {
        auto* ca = expanderManager.cachedChangeAlleyExpander;
        bool identity = true;
        for (int v = 0; v < 16; ++v) {
            engine.pe.caRhythmSrc[v] = ca->rhythmSrc[v];
            engine.pe.caMelodySrc[v] = ca->melodySrc[v];
            if (ca->rhythmSrc[v] != v || ca->melodySrc[v] != v) identity = false;
        }
        if (!identity) {
            engine.pe.forceRecomputeSlewed();   // fresh slewed = bl(A,B), pristine
            engine.pe.remapSlewedByPins();       // then remap the clean buffers
        }
    } else {
        for (int v = 0; v < 16; ++v) { engine.pe.caRhythmSrc[v] = (uint8_t)v; engine.pe.caMelodySrc[v] = (uint8_t)v; }
    }
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
    expanderManager.fillPresence(topoIn, engine.numPolyVoices);   // single presence authority
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
    // (Macro spread CV is now handled inside redDot::SpreadResolver — the former applyMacroSprCV
    // lambda was folded into SpreadResolver::CvTerm and removed, like applyMonoSprCV before it.)

    // (Mono spread CV is now handled inside redDot::SpreadResolver — the former applyMonoSprCV
    // lambda was folded into SpreadResolver::CvTerm and removed, as was Macro's applyMacroSprCV.)

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
            // Returns the UNCLAMPED East CV delta (0 when unpatched). Mods are summed first and the
            // TOTAL is clamped once by the caller — clamping each term as it lands discards headroom
            // and makes the result order-dependent (East-then-Macro != Macro-then-East).
            auto eastDelta = [&](int engLane, int item, float lo, float hi)->float {
                if (!eastVis || !eastVis->inputs[East::cvId(engLane,item)].isConnected()) return 0.f;
                float att = (redDot::findMonsoonEitherSide(eastVis) ? redDot::findMonsoonEitherSide(eastVis)->getMacroAtten(dotModular::VoiceResolver::kMonoSlot, engLane*4 + item) : 0.f);   // slot 0 = mono
                float cv  = eastVis->inputs[East::cvId(engLane,item)].getVoltage(0) / 10.f; // ch0
                return cv * att * (hi - lo);
            };
            // Macro global CV distributed to voice 1 (voice-0 send × macroCVDelta).
            // sendId AND macroCVDelta are ENGINE-lane indexed (REST=0,MEL=1,OCT=2,
            // ACC=3) — same as the poly path — so pass the engine lane, not editor l.
            // NOTE: macroCVDelta is published by the Macro block LATER in this same
            // processDNA (after the mono strand), so voice 1 reads the PREVIOUS control
            // block's delta — a one-block (sub-ms, control-rate) lag. Imperceptible.
            // Returns the UNCLAMPED Macro send delta (0 when no Macro). Summed with East's, clamped once.
            auto macroDelta = [&](int engLane, int item)->float {
                if (!hasMacro || !macroVis) return 0.f;
                float send = (redDot::findMonsoonEitherSide(macroVis) ? redDot::findMonsoonEitherSide(macroVis)->getMacroSend(dotModular::VoiceResolver::kMonoSlot, engLane, item) : 0.f);   // slot 0 = mono
                return macroVis->macroSendDelta[engLane][item] * send;  // P9: tapped send delta
            };
            // East CV exists only for the 4 poly lanes (MEL/OCT/REST/ACC); VAR/LEG
            // (editor l>=4) are mono-only with no East jack → skip the eastMix there.
            // A DELEGATED lane tracks Macro's value exclusively (set above) — it takes
            // neither East's CV nor the Macro SEND on top (that would double-count Macro's
            // modulation, already in the delegated base). Only OWNED lanes receive these.
            if (l < 4 && monoOwnsLane) {
                int eng = dotModular::EDITOR_TO_ENGINE_LANE[l];   // editor → engine lane (East CV jack)
                // Sum ALL mods (East CV + Macro send) onto the base, then clamp the END RESULT once.
                baseLen = math::clamp(baseLen + eastDelta(eng, 0, 1.f, 16.f) + macroDelta(eng, 0),  1.f, 16.f);
                baseOff = math::clamp(baseOff + eastDelta(eng, 1, 0.f, 15.f) + macroDelta(eng, 1),  0.f, 15.f);
                baseRot = math::clamp(baseRot + eastDelta(eng, 2, 0.f, 15.f) + macroDelta(eng, 2),  0.f, 15.f);
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
                // Effective spread amount via the single authority (SpreadResolver). All inputs are
                // read from exactly the same places the previous inline arithmetic did; the resolver
                // replicates the manager's step-wise clamping, so this is behaviour-inert. Topology
                // decides delegation (owner()==MACRO, via monoMacroOwnsEngineLane); the resolver only
                // consumes that boolean. l is the SPREAD/engine lane (0=REST,1=MEL,2=OCT,3=ACC).
                auto* eastVisS = expanderManager.cachedEastSandsVisual;
                redDot::SpreadResolver::Inputs sin;
                sin.delegated    = hasMacro && macroVis &&
                                   SandsMonoVisualIds::monoMacroOwnsEngineLane(monoVis, l);
                sin.macroPresent = (hasMacro && macroVis);
                sin.base         = monoVis->params[Mono::sprId(l)].getValue();
                // Own Mono spread CV (sprCvId jack + sprAttenId), unit-scaled /10.
                if (monoVis->inputs[Mono::sprCvId(l)].isConnected()) {
                    sin.ownCv.connected   = true;
                    sin.ownCv.unitVoltage = monoVis->inputs[Mono::sprCvId(l)].getVoltage() / 10.f;
                    sin.ownCv.atten       = monoVis->params[Mono::sprAttenId(l)].getValue();
                }
                // East V1 spread CV add (cvId(engineLane,3) jack + attenId(mono slot,l,3)).
                if (eastVisS && eastVisS->inputs[East::cvId(l,3)].isConnected()) {
                    sin.eastCv.connected   = true;
                    sin.eastCv.unitVoltage = eastVisS->inputs[East::cvId(l,3)].getVoltage(0) / 10.f;
                    sin.eastCv.atten       = (redDot::findMonsoonEitherSide(eastVisS) ? redDot::findMonsoonEitherSide(eastVisS)->getMacroAtten(dotModular::VoiceResolver::kMonoSlot, l*4 + 3) : 0.f);
                }
                if (sin.macroPresent) {
                    sin.macroBase      = macroVis->macroBase[l][3];
                    sin.macroSendDelta = macroVis->macroSendDelta[l][3];
                    sin.macroSend      = (redDot::findMonsoonEitherSide(macroVis) ? redDot::findMonsoonEitherSide(macroVis)->getMacroSend(dotModular::VoiceResolver::kMonoSlot, l, 3) : 0.f);
                }
                engine.spreadERef(0, l) = redDot::SpreadResolver::effective(sin);   // slot 0 = mono/V1
            }
            // engine.spread[slot][editorLane] is the engine's own spread state (was monoVis->
            // spreadEffective — MVC: the model no longer reaches into the visual). Accessed engine-
            // order via spreadE/spreadERef (0=REST,1=MEL,2=OCT,3=ACCENT); lanes 4/5 unused (LEG/VAR
            // are mono-only, no spread).

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
            for (int i = 0; i < 16; ++i) {
                // Single source of truth: target (mode-aware, mono-inclusive avg)
                // + bipolar interpolate, over the pre-spread slewed draws.
                engine.pe.rhythmRandom[i] = redDot::SpreadInterp::apply(
                    engine.pe, 0, i, engine.pe.slewedRhythm[i], engine.spreadE(0, 0));
                engine.pe.melodyRandom[i] = redDot::SpreadInterp::apply(
                    engine.pe, 1, i, engine.pe.slewedMelody[i], engine.spreadE(0, 1));
                engine.pe.octaveRandom[i] = redDot::SpreadInterp::apply(
                    engine.pe, 2, i, engine.pe.slewedOctave[i], engine.spreadE(0, 2));
                engine.pe.legatoRandom[i]    = engine.pe.slewedLegato[i];     // mono-only, raw
                engine.pe.accentRandom[i] = redDot::SpreadInterp::apply(
                    engine.pe, 3, i, engine.pe.slewedAccent[i], engine.spreadE(0, 3));
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
        if (!hasEastV1 && engine.strandLen(dotModular::STRAND_RHYTHM) != 16) {
            // Reset all mono-strand LOR to defaults (Length 16, Offset 0, Rotation 0).
            for (int s = 0; s < dotModular::NUM_STRANDS; ++s) {
                engine.strandLenRef(s) = 16;
                engine.strandOffRef(s) = 0;
                engine.strandRotRef(s) = 0;
            }
        }
        if (hasEastV1) {
            // V1 LOR (below) runs unconditionally, matching poly combineLOR — the LOR window is
            // derived even under lock; only SPREAD is lock-gated (wrapped in if(!locked) further down).
            // Apply V1 SPREAD to the mono final arrays, PER LANE:
            //   • lane owned by East   → East's V1 spread base (Model: spread[kMonoSlot]) + CV.
            //   • lane delegated Macro → Macro's GLOBAL spread (macroBase[lane][3] + send).
            // (spread[0]/macroBase are spread/engine-indexed 0=REST..3=ACC.)
            // Macro-owned test mirrors the widget: East ownerDispId(lane) <= 0.5 == Macro.
            // Stage 3a: the BASE is read from the unified store (Model), not SPREAD_* (View).
            // saveSlot(kMonoSlot) mirrors SPREAD_*→spread[0] every UI frame, so this is
            // value-preserving; the View is now only edited, never read for the engine.
            Monsoon* mmV1 = redDot::findMonsoonEitherSide(eastV1);
            const bool macroHere = hasMacro && (macroVis != nullptr);
            auto monoOwnedByMacro = [&](int lane)->bool {
                return macroHere && !(eastV1->params[East::ownerDispId(lane)].getValue() > 0.5f);
            };

            // Stage 3b: V1 LOR derivation — moved here from the East widget's v1Editable
            // mono-strand write. The manager now derives V1's windows from the Model
            // (lorBase[kMonoSlot]) exactly as it derives poly windows, so V1 is no longer a
            // special widget-side path and the strands are written on the audio thread (no
            // UI/audio race). Value-preserving: lorBase[0] == the old currentState via the
            // widget's saveSlot(kMonoSlot) mirror (Stage 2); delegation reuses monoOwnedByMacro
            // (== the old v1Topo.owner(0,el)==MACRO for the no-Mono/v1Editable case).
            {
                const int kMono = dotModular::VoiceResolver::kMonoSlot;
                // Lanes 0..3 (REST/MEL/OCT/ACC): Macro base when delegated, else base+CV+send blend.
                for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
                    const int engLane = dotModular::EDITOR_TO_ENGINE_LANE[el];
                    const int strand  = dotModular::MONO_LANE_TO_STRAND[el];
                    if (monoOwnedByMacro(engLane)) {
                        engine.setStrand(StrandWriter::EAST, strand,
                            (int)std::round(macroVis->macroBase[engLane][0] + macroVis->macroCVDelta[engLane][0]),
                            (int)std::round(macroVis->macroBase[engLane][1] + macroVis->macroCVDelta[engLane][1]),
                            (int)std::round(macroVis->macroBase[engLane][2] + macroVis->macroCVDelta[engLane][2]));
                        continue;
                    }
                    const int b0 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, engLane, 0)) : 16;
                    const int b1 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, engLane, 1)) : 0;
                    const int b2 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, engLane, 2)) : 0;
                    const float len = (float)std::max(1, b0);
                    const float off = (float)(((b1 % 16) + 16) % 16);
                    const float rot = (float)(((b2 % 16) + 16) % 16);
                    auto sendBlend = [&](int item)->float {
                        if (!macroVis) return 0.f;   // match the widget's guard exactly
                        float send = mmV1 ? mmV1->getMacroSend(kMono, engLane, item) : 0.f;
                        return macroVis->macroSendDelta[engLane][item] * send;
                    };
                    auto addCV = [&](float base, int item, float lo, float hi)->float {
                        if (eastV1->inputs[East::cvId(engLane,item)].isConnected()) {
                            float att = mmV1 ? mmV1->getMacroAtten(kMono, engLane*4 + item) : 0.f;
                            float cv  = eastV1->inputs[East::cvId(engLane,item)].getPolyVoltage(0) / 10.f;  // ch0 = V1
                            base += cv * att * (hi - lo);
                        }
                        return rack::math::clamp(base + sendBlend(item), lo, hi);
                    };
                    engine.setStrand(StrandWriter::EAST, strand,
                        (int)std::round(addCV(len, 0, 1.f, 16.f)),
                        (int)std::round(addCV(off, 1, 0.f, 15.f)),
                        (int)std::round(addCV(rot, 2, 0.f, 15.f)));
                }
                // VAR/LEG (4/5): East-owned, never Macro-delegated, no send blend. strand == el.
                for (int el = dotModular::SandsGrid::POLY_LANES; el < dotModular::SandsGrid::EAST_LANES; ++el) {
                    const int vl = el - dotModular::SandsGrid::POLY_LANES;   // 0=VAR, 1=LEG
                    const int b0 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, el, 0)) : 16;
                    const int b1 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, el, 1)) : 0;
                    const int b2 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, el, 2)) : 0;
                    auto addCV = [&](float base, int item, float lo, float hi)->float {
                        if (eastV1->inputs[East::varlegCvId(vl,item)].isConnected()) {
                            float att = mmV1 ? mmV1->getVarlegAtten(kMono, vl, item) : 0.f;
                            float cv  = eastV1->inputs[East::varlegCvId(vl,item)].getPolyVoltage(0) / 10.f;  // ch0 = V1
                            base += cv * att * (hi - lo);
                        }
                        return rack::math::clamp(base, lo, hi);
                    };
                    engine.setStrand(StrandWriter::EAST, el,
                        (int)std::round(addCV((float)std::max(1, b0), 0, 1.f, 16.f)),
                        (int)std::round(addCV((float)(((b1 % 16) + 16) % 16), 1, 0.f, 15.f)),
                        (int)std::round(addCV((float)(((b2 % 16) + 16) % 16), 2, 0.f, 15.f)));
                }
            }

            // ── SPREAD: lock-gated (frozen pattern must not be re-spread). LOR above already ran.
            if (!engine.locked) {
            auto sprForLane = [&](int lane)->float {
                if (monoOwnedByMacro(lane))
                    return rack::math::clamp(macroVis->macroBase[lane][3] + macroVis->macroCVDelta[lane][3], -1.f, 1.f);
                float sp = mmV1 ? mmV1->getSpread(dotModular::VoiceResolver::kMonoSlot, lane)
                                : eastV1->params[East::SPREAD_R + lane].getValue();   // R/M/O/A contiguous
                if (eastV1->inputs[East::cvId(lane,3)].isConnected()) {
                    float att = (mmV1 ? mmV1->getMacroAtten(dotModular::VoiceResolver::kMonoSlot, lane*4 + 3) : 0.f);
                    float cv  = eastV1->inputs[East::cvId(lane,3)].getPolyVoltage(0) / 10.f;  // ch0 = V1
                    sp = sp + cv * att * 2.f;
                }
                // Macro send blend on East-owned V1 spread (mirrors poly combineSpread):
                // macroSendDelta[lane][3] * mono-slot send.
                if (macroHere) {
                    float send = (redDot::findMonsoonEitherSide(macroVis) ? redDot::findMonsoonEitherSide(macroVis)->getMacroSend(dotModular::VoiceResolver::kMonoSlot, lane, 3) : 0.f);
                    sp += macroVis->macroSendDelta[lane][3] * send;
                }
                return rack::math::clamp(sp, -1.f, 1.f);
            };
            const float spR = sprForLane(0);
            const float spM = sprForLane(1);
            const float spO = sprForLane(2);
            const float spA = sprForLane(3);
            const int nPoly = effPolyVoices;
            engine.pe.setSandsActive(true);
            for (int i = 0; i < 16; ++i) {
                engine.pe.rhythmRandom[i] = redDot::SpreadInterp::apply(engine.pe, 0, i, engine.pe.slewedRhythm[i], spR);
                engine.pe.melodyRandom[i] = redDot::SpreadInterp::apply(engine.pe, 1, i, engine.pe.slewedMelody[i], spM);
                engine.pe.octaveRandom[i] = redDot::SpreadInterp::apply(engine.pe, 2, i, engine.pe.slewedOctave[i], spO);
                engine.pe.accentRandom[i] = redDot::SpreadInterp::apply(engine.pe, 3, i, engine.pe.slewedAccent[i], spA);
                engine.pe.legatoRandom[i]    = engine.pe.slewedLegato[i];
                engine.pe.variationRandom[i] = engine.pe.slewedVariation[i];
            }
            }   // end if (!engine.locked) — spread only; V1 LOR above runs under lock too
        }
    }

    // ── Macro global LOR with CV (poly: per-lane, SAME for every voice) ────
    // Macro publishes its OWN display data (macroBase / macroCVDelta / macroSendDelta /
    // spreadEffective) whenever a Macro is present — this is what Sands Helix renders, and it must
    // work standalone (no Straits / no poly output). The output-application (blend into the poly
    // voices) is separately gated on macroDrivesOutput below. (Regression from the Straits refactor:
    // publishGlobal used to be gated on macroDrivesOutput, so with no Straits attached Sands Helix's
    // macroBase stayed zero and the module looked completely inert.)
    if (hasMacro && macroVis) {
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
            // Effective macro global spread via the single authority. Macro publishing its own global
            // has only its own CV (no East, no delegation, no macro-send-to-self), so only ownCv is
            // set — SpreadResolver then yields clamp(base + ownCv·2), identical to applyMacroSprCV.
            redDot::SpreadResolver::Inputs msin;
            msin.base = baseSpr;
            if (macroVis->inputs[Macro::macroCvId(lane,3)].isConnected()) {
                msin.ownCv.connected   = true;
                msin.ownCv.unitVoltage = macroVis->inputs[Macro::macroCvId(lane,3)].getVoltage() / 10.f;
                msin.ownCv.atten       = macroVis->params[Macro::macroAttenId(lane,3)].getValue();
            }
            float cvSpr = redDot::SpreadResolver::effective(msin);
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

        // Output application: only when Macro actually drives real output voices (Straits attached
        // with poly active). When macroDrivesOutput, East's per-voice sync (runs after) combines
        // Macro's base+delta into each voice via the owner/blend send. When NOT — standalone Macro
        // (no Straits) — apply Macro's GLOBAL spread to the drawn voices here so its bars aren't dead.
        if (!macroDrivesOutput) {
        if (!engine.locked) {
            const int nPoly = rack::math::clamp(engine.numPolyVoices, 0, 15);
            // Global spread level per lane (knob + CV), spread/engine-indexed 0..3.
            float spv[4];
            for (int lane = 0; lane < 4; ++lane) {
                // Standalone Macro global spread — own CV only (same as publishGlobal), via the resolver.
                redDot::SpreadResolver::Inputs ssin;
                ssin.base = macroVis->params[Macro::sprId(lane)].getValue();
                if (macroVis->inputs[Macro::macroCvId(lane,3)].isConnected()) {
                    ssin.ownCv.connected   = true;
                    ssin.ownCv.unitVoltage = macroVis->inputs[Macro::macroCvId(lane,3)].getVoltage() / 10.f;
                    ssin.ownCv.atten       = macroVis->params[Macro::macroAttenId(lane,3)].getValue();
                }
                float sp = redDot::SpreadResolver::effective(ssin);
                macroVis->spreadEffective[lane] = sp;
                spv[lane] = sp;
            }
            engine.pe.setSandsActive(true);
            // V1 (mono final arrays): converge the mono draw toward the ensemble.
            for (int i = 0; i < 16; ++i) {
                engine.pe.rhythmRandom[i]    = redDot::SpreadInterp::apply(engine.pe, 0, i, engine.pe.slewedRhythm[i], spv[0]);
                engine.pe.melodyRandom[i]    = redDot::SpreadInterp::apply(engine.pe, 1, i, engine.pe.slewedMelody[i], spv[1]);
                engine.pe.octaveRandom[i]    = redDot::SpreadInterp::apply(engine.pe, 2, i, engine.pe.slewedOctave[i], spv[2]);
                engine.pe.accentRandom[i]    = redDot::SpreadInterp::apply(engine.pe, 3, i, engine.pe.slewedAccent[i], spv[3]);
                engine.pe.legatoRandom[i]    = engine.pe.slewedLegato[i];
                engine.pe.variationRandom[i] = engine.pe.slewedVariation[i];
            }
            // Each poly voice's final arrays: same global spread level applied per voice.
            // apply() computes the lane's ensemble target internally; pass each voice's
            // own slewed draw as the value to interpolate (same call shape as East's loop).
            for (int v = 0; v < nPoly; ++v) {
                for (int i = 0; i < 16; ++i) {
                    engine.pe.polyRandom(v, SequencerEngine::PL_REST)[i] = redDot::SpreadInterp::apply(engine.pe, 0, i, engine.pe.slewedPolyRhythm[v][i], spv[0]);
                    engine.pe.polyRandom(v, SequencerEngine::PL_MELODY)[i] = redDot::SpreadInterp::apply(engine.pe, 1, i, engine.pe.slewedPolyMelody[v][i], spv[1]);
                    engine.pe.polyRandom(v, SequencerEngine::PL_OCTAVE)[i] = redDot::SpreadInterp::apply(engine.pe, 2, i, engine.pe.slewedPolyOctave[v][i], spv[2]);
                    engine.pe.polyRandom(v, SequencerEngine::PL_ACCENT)[i] = redDot::SpreadInterp::apply(engine.pe, 3, i, engine.pe.slewedPolyAccent[v][i], spv[3]);
                }
            }
        }   // if (!engine.locked)
        }   // if (!macroDrivesOutput)
    }       // if (hasMacro && macroVis)

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
