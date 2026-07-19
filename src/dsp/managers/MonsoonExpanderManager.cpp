#include "MonsoonExpanderManager.hpp"
#include "../SpreadInterp.hpp"
#include "../VoiceResolver.hpp"   // read-only shadow (step 1)
#include "../SandsTopology.hpp"   // step 3: solo/none write-guard migration
#include "../../Monsoon.hpp"
#include <cassert>
//#include "../../MonsoonDeepStraitsSands.hpp"
#include "../../StraitsEastSandsVisual.hpp"
#include "../../ui/VisualExpanderHelpers.hpp"   // redDot::findMonsoonEitherSide (LANE_DIR field reads)
#include "../../StraitsSandsMacroVisual.hpp"
#include "../../MonsoonSandsVisualExpander.hpp"   // SandsMonoVisualIds::dirDispId for mono direction push

using namespace rack;

// Who owns V1/mono direction on `lane`? The ONE place that decides — see the header.
// Precedence:
//   Mono present and owns the lane  -> Mono's cell   (Mono always owns VAR/LEG)
//   otherwise Macro present         -> Macro's cell  (lanes 0..3 only; Macro has no VAR/LEG)
//   otherwise East present          -> East's V1 slot (East is the mono editor)
//   otherwise                       -> nothing (caller leaves the lane Forward)
MonsoonExpanderManager::MonoDirSrc MonsoonExpanderManager::monoDirAuthority(int lane) const {
    MonoDirSrc r;
    if (lane < 0 || lane >= dotModular::NUM_STRANDS) return r;
    auto* monoVis  = cachedSandsVisualExpander;
    auto* macroVis = cachedMacroSandsVisual;
    auto* eastVis  = cachedEastSandsVisual;
    const bool varleg = (lane >= 4);          // VARIATION / LEGATO: Macro has no such lane

    if (monoVis) {
        // Mono always owns VAR/LEG; for 0..3 it owns only when its owner cell says so.
        const bool monoOwns = varleg
            || (monoVis->params[SandsMonoVisualIds::ownerDispId(lane)].getValue() > 0.5f);
        if (monoOwns) { r.mod = monoVis; r.paramId = SandsMonoVisualIds::dirDispId(lane); return r; }
        if (macroVis) { r.mod = macroVis; r.paramId = StraitsMacroVisualIds::dirDispId(lane); return r; }
        return r;                              // Mono present but doesn't own, no Macro -> nobody
    }
    if (macroVis && !varleg) { r.mod = macroVis; r.paramId = StraitsMacroVisualIds::dirDispId(lane); return r; }
    // No Mono, and either no Macro or a VAR/LEG lane Macro cannot own -> East's V1 slot.
    // This is the arm that was missing: with Macro attached, V1's VAR/LEG had NO source, so
    // those lanes were pinned Forward and East's DirCell for them did nothing.
    if (eastVis) { r.mod = eastVis; r.eastMonoLane = lane; return r; }   // field-backed (Monsoon::editor.laneDir V1 slot)
    return r;
}

void MonsoonExpanderManager::sync(SequencerEngine& engine, bool spreadInterpMono) {
    // Poly-lane constants (engine order REST=0,MEL=1,OCT=2,ACC=3). Named here so
    // the renumber tracks automatically and the parallel index conventions below
    // (polyLen[v][lane] / combineLOR(lane,..) / combineSpread(lane,..) /
    // SpreadInterp::apply(..,lane,..)) all move together.
    using PL = SequencerEngine;  // PL::PL_REST etc.
    //auto* deepEast   = cachedDeepStraitsSandsEastExpander;
    //auto* deepWest   = cachedDeepStraitsSandsWestExpander;
    auto* straits = cachedPolyVoiceExpander;
    auto* eastVisual  = cachedEastSandsVisual;

    auto* eastLOR   = eastVisual;// ? static_cast<rack::Module*>(eastVisual);
    auto* eastInterp = eastVisual;// ? static_cast<rack::Module*>(eastVisual)

    // West retired: one Straits module now handles all poly voices (2..16 = 15 voices).
    // The poly cables are 16ch; the cap is simply "Straits present → up to 15 poly voices".
    const bool polyBaseActive = (straits != nullptr) && (engine.numPolyVoices >= 1);
    const int  polyOutCap = polyBaseActive ? 15 : 0;
    const int  effPolyVoices = math::clamp(engine.numPolyVoices, 0, polyOutCap);

    // STEP 3: single-authority config classification (presence-only — enough for the
    // solo/none write guards; per-lane ownership inputs are filled in later steps when
    // the combination guards migrate). See docs/design/SANDS_TOPOLOGY_RESOLVER_PLAN.md.
    dotModular::SandsTopology::Inputs topoIn;
    fillPresence(topoIn, engine.numPolyVoices);   // single presence authority
    // STEP 5b: populate per-voice East ownership so the topology can drive the POLY write
    // guards (not just presence/V1). Source is East's persistent ownerId (engine-ordered)
    // and monoOwnerId; converted to editor lane so topo speaks editor lane (decision 1).
    if (cachedEastSandsVisual) {
        for (int el = 0; el < 4; ++el) {
            const int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
            topoIn.eastV1Owner[el] = cachedEastSandsVisual->params[StraitsEastVisualIds::monoOwnerId(eng)].getValue() > 0.5f;
            for (int pv = 0; pv < 15; ++pv)
                topoIn.eastPolyOwner[pv][el] = cachedEastSandsVisual->params[StraitsEastVisualIds::ownerId(pv, eng)].getValue() > 0.5f;
        }
    }
    const dotModular::SandsTopology topo = dotModular::SandsTopology::build(topoIn);

    // Spread interpolation is now centralised in redDot::SpreadInterp (see
    // src/dsp/SpreadInterp.hpp) — the single source of truth shared with the
    // mono/macro path and the visual display.

    // VAR/LEG delegation fallback (§4d): default every voice to follow-mono BEFORE the
    // East-gated block. If East is absent or detached, the block below is skipped and this
    // stands — so "no East ⇒ follow mono" holds even after East is removed mid-session (no
    // stale Local-East state). When East IS present, the block re-asserts Local East per param.
    for (int dv = 0; dv < 15; ++dv) { engine.setVarlegLocalEast(dv, 0, false); engine.setVarlegLocalEast(dv, 1, false); }
    // Step 3 (plans/lane_direction_homes.md): poly direction is reset-then-pushed exactly like
    // delegation above. The reset is the half that was missing while the engine was direction's
    // home: nothing cleared laneDirV_, so pulling East out of the rack left poly direction
    // applied forever. Now no East => Forward, same as no East => no delegation.
    for (int dv = 0; dv < 15; ++dv)
        for (int l = 0; l < dotModular::NUM_STRANDS; ++l)
            engine.laneDirVPending_[dv][l] = SequencerEngine::LaneDir::Forward;
    // Step 4: mono direction reset — same pattern. No expander => Forward.
    for (int l = 0; l < dotModular::NUM_STRANDS; ++l)
        engine.laneDirPending_[l] = SequencerEngine::LaneDir::Forward;
    // Push mono direction from the owning expander.
    // Lanes 0..3: if Mono present and owns → Mono's dirDispId; if Macro owns → Macro's dirDispId.
    // Lanes 4..5 (VAR/LEG): Mono always owns → Mono's dirDispId.
    // If Mono absent but Macro present → Macro's dirDispId (lanes 0..3 only).
    // If neither Mono nor Macro → East's monoDirId (V1 slot). If nothing → Forward (reset).
    //
    // ALSO push Macro's own direction to macroLaneDir_ (always, regardless of ownership)
    // so the engine advances macroLaneTick_ independently for Macro's playhead display.
    {
        // Read through the single authority helper — the same one East's V1 gate-mod writes
        // through, so the mod can never target a store the manager isn't reading.
        for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
            MonoDirSrc src = monoDirAuthority(l);
            if (!src.valid()) continue;   // nothing owns this lane -> leave Forward from reset
            if (src.isField()) {
                // East V1 direction lives in Monsoon::editor.laneDir (NUM_PARAMS_MIGRATION.md).
                // Reach it through the East expander's Monsoon pointer.
                if (auto* ev = dynamic_cast<StraitsEastSandsVisual*>(src.mod))
                    if (auto* mm = redDot::findMonsoonEitherSide(ev))
                        engine.laneDirPending_[l] = (SequencerEngine::LaneDir)(int)std::lround(
                            math::clamp(mm->getMonoLaneDir(src.eastMonoLane), 0.f, 3.f));
            } else {
                engine.laneDirPending_[l] = (SequencerEngine::LaneDir)(int)std::lround(
                    math::clamp(src.mod->params[src.paramId].getValue(), 0.f, 3.f));
            }
        }
        auto* macroVis = cachedMacroSandsVisual;
        // Push Macro's direction + LOR length to macroLaneDir_/macroLOR_ (all 4 lanes, always)
        if (macroVis) {
            auto* mmod = dynamic_cast<StraitsSandsMacroVisual*>(macroVis);
            for (int el = 0; el < 4; ++el) {
                // el = editor lane (MEL=0, OCT=1, REST=2, ACC=3) = strand index
                engine.macroLaneDir_[el] = (SequencerEngine::LaneDir)(int)std::lround(
                    math::clamp(macroVis->params[StraitsMacroVisualIds::dirDispId(el)].getValue(), 0.f, 3.f));
                // Push Macro's own LOR length for bounce detection.
                // macroBase is indexed by ENGINE lane (REST=0, MEL=1, OCT=2, ACC=3);
                // convert editor lane → engine lane via EDITOR_TO_ENGINE_LANE.
                if (mmod) {
                    int engLane = dotModular::EDITOR_TO_ENGINE_LANE[el];
                    engine.macroLOR_[el] = std::max(1, (int)std::lround(mmod->macroBase[engLane][0] + mmod->macroCVDelta[engLane][0]));
                }
            }
        } else {
            for (int el = 0; el < 4; ++el) {
                engine.macroLaneDir_[el] = SequencerEngine::LaneDir::Forward;
                engine.macroLOR_[el] = 16;
            }
        }
    }

    if (eastLOR && (straits || eastVisual) && polyBaseActive) {
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
                // STEP 5b: poly write ownership via the resolver. Was:
                //   ownerEast = ownerId(v, lane) > 0.5f
                // topo voice arg is 1-based for poly (0 = mono/V1); `lane` here is
                // engine-ordered → convert to editor lane. Cross-check in debug.
                const int elc = dotModular::ENGINE_LANE_TO_EDITOR[lane];
                const bool ownerEast = (topo.owner(v + 1, elc) == dotModular::SandsTopology::Role::EAST);
                // (step 5+6 cross-check assert removed — the resolver is now the single
                // source of truth; the old ownerId-match check fired on load-time transients
                // during attach/teardown. Same strip as the other 7 topology cross-checks.)
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
                // STEP 5b: same poly write ownership via the resolver (spread path).
                const int els = dotModular::ENGINE_LANE_TO_EDITOR[lane];
                const bool ownerEast = (topo.owner(v + 1, els) == dotModular::SandsTopology::Role::EAST);
                // (step 5+6 cross-check assert removed — see the poly-write path above.)
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
            engine.polyLenERef(v, PL::PL_REST) = combineLOR(PL::PL_REST, 0, rhythmBase,     PL::PL_REST, 0, 1.f, 16.f);
            engine.polyOffERef(v, PL::PL_REST) = combineLOR(PL::PL_REST, 1, rhythmBase + 1, PL::PL_REST, 1, 0.f, 15.f);
            engine.polyRotERef(v, PL::PL_REST) = combineLOR(PL::PL_REST, 2, rhythmBase + 2, PL::PL_REST, 2, 0.f, 15.f);

            // ── EAST_EXTRA_LANES: per-voice VARIATION / LEGATO LOR ────────────────────────────
            // These are mono STRANDS, not PolyLanes, so they must use the EDITOR-order accessor
            // (polyLORRef masks & 7). polyLenERef would mask & 3 and silently alias VAR -> REST.
            // No Macro blend and no spread: Macro cannot own these lanes (an owned lane drives all
            // voices identically, which annihilates the per-voice divergence that IS the feature),
            // and there is nothing to spread (the probability array stays mono; only the reading
            // position differs). Straight base params, clamped to the same ranges as the poly lanes.
            if (eastLOR) {
                using SE = SequencerEngine;
                // VARLEG deleg + atten migrated to Monsoon::editor (NUM_PARAMS_MIGRATION.md);
                // read them via this Monsoon pointer (LANE_DIR below uses the same lookup).
                Monsoon* mmE = redDot::findMonsoonEitherSide(eastLOR);
                const int varBase = MonsoonIds::POLY_VARIATION_VOICE_1_LEN + v * 3;
                const int legBase = MonsoonIds::POLY_LEGATO_VOICE_1_LEN    + v * 3;
                // East's own base + per-voice poly CV for a VAR/LEG L/O/R item. Mirrors eastLorVal
                // but uses the VAR/LEG CV jacks (varlegCvId) and per-voice depth (varlegAttId).
                // No Macro blend: VAR/LEG are never Macro-owned (an owned lane would annihilate
                // per-voice divergence). vl = 0 (VAR) or 1 (LEG); col c = 0/1/2 (LEN/OFF/ROT).
                // Poly cable ch(v) → poly voice v (V(v+2)); the mono/V1 ch0 mix-in is applied in
                // the East widget's v1Editable strand write, not here.
                auto varlegLorVal = [&](int paramIdx, int vl, int c, float lo, float hi)->int {
                    float base = eastLOR->params[paramIdx].getValue();
                    if (eastVisual->inputs[StraitsEastVisualIds::varlegCvId(vl,c)].isConnected()) {
                        float att = mmE ? mmE->getVarlegAtten(slot, vl, c) : 0.f;
                        float cv  = eastVisual->inputs[StraitsEastVisualIds::varlegCvId(vl,c)]
                                        .getPolyVoltage(v) / 10.f;
                        base = math::clamp(base + cv * att * (hi - lo), lo, hi);
                    }
                    return (int)std::lround(base);
                };
                engine.polyLORRef(v, SE::EDITOR_LANE_VARIATION, SE::LOR_LEN) = varlegLorVal(varBase + 0, 0, 0, 1.f, 16.f);
                engine.polyLORRef(v, SE::EDITOR_LANE_VARIATION, SE::LOR_OFF) = varlegLorVal(varBase + 1, 0, 1, 0.f, 15.f);
                engine.polyLORRef(v, SE::EDITOR_LANE_VARIATION, SE::LOR_ROT) = varlegLorVal(varBase + 2, 0, 2, 0.f, 15.f);
                engine.polyLORRef(v, SE::EDITOR_LANE_LEGATO,    SE::LOR_LEN) = varlegLorVal(legBase + 0, 1, 0, 1.f, 16.f);
                engine.polyLORRef(v, SE::EDITOR_LANE_LEGATO,    SE::LOR_OFF) = varlegLorVal(legBase + 1, 1, 1, 0.f, 15.f);
                engine.polyLORRef(v, SE::EDITOR_LANE_LEGATO,    SE::LOR_ROT) = varlegLorVal(legBase + 2, 1, 2, 0.f, 15.f);

                // Delegation toggles (§4d): 0 = follow mono (default, silent), 1 = Local East.
                // When Local East, the LOR pushed above is read; when delegating, the engine
                // ignores it and reads mono's VAR/LEG position instead. lane 0 = VAR, 1 = LEG.
                engine.setVarlegLocalEast(v, 0, mmE && mmE->getVarlegDeleg(v, 0) > 0.5f);
                engine.setVarlegLocalEast(v, 1, mmE && mmE->getVarlegDeleg(v, 1) > 0.5f);

                // Per-voice LANE DIRECTION, from East's bank — same shape as the delegation
                // push above. This is what frees direction from the widget: the editor owns the
                // datum, the manager pushes it module-side, the engine derives.
                //
                // Push ONLY laneDirVPending_, never laneSignV_/laneSignVPending_: advancePlayhead
                // derives the sign for Forward/Reverse and OWNS it for Pendulum/PingPong (flipping
                // at the LOR endpoint). Pushing a sign here would overwrite the bounce-induced flip
                // with laneDirSign(Pendulum)=+1 every pass and the lane would never turn around.
                if (auto* mm = redDot::findMonsoonEitherSide(eastLOR)) {
                    for (int l = 0; l < dotModular::NUM_STRANDS; ++l) {
                        int dv = (int)std::lround(math::clamp(mm->getLaneDir(v, l), 0.f, 3.f));
                        engine.laneDirVPending_[v][l] = (SequencerEngine::LaneDir)dv;
                    }
                }
#if RULE2_DEBUG
                {
                    // Throttled: ~15-line burst (covers all voices once) every ~131k iters.
                    static unsigned long r2c = 0;
                    if ((r2c++ & 0x1FFFF) < 15)
                        INFO("[R2 push ] v=%2d VARp=%.2f LEGp=%.2f legLOR=(%d,%d,%d)",
                            v,
                            (mmE ? mmE->getVarlegDeleg(v, 0) : 0.f),
                            (mmE ? mmE->getVarlegDeleg(v, 1) : 0.f),
                            (int)std::lround(math::clamp(eastLOR->params[legBase + 0].getValue(), 1.f, 16.f)),
                            (int)std::lround(math::clamp(eastLOR->params[legBase + 1].getValue(), 0.f, 15.f)),
                            (int)std::lround(math::clamp(eastLOR->params[legBase + 2].getValue(), 0.f, 15.f)));
                }
#endif
            }

            float restInterp = eastInterp->params[MonsoonIds::POLY_REST_INTERP_1 + v].getValue();
            if (eastVisual && eastVisual->inputs[cvId(PL::PL_REST,3)].isConnected()) {
                float att = eastLOR->params[attenId(slot,PL::PL_REST,3)].getValue();   // PER-VOICE depth
                float cv  = eastVisual->inputs[cvId(PL::PL_REST,3)].getPolyVoltage(v) / 10.f;
                restInterp += cv * att * 2.f;   // ×2 = ±1 span. UNCLAMPED: summed with the Macro blend in
                    // combineSpread, which end-clamps the TOTAL once (per-term clamping
                    // discarded headroom and made East-then-Macro != Macro-then-East).
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
                    engine.pe.polyRandom(v, PL::PL_REST)[j] = redDot::SpreadInterp::apply(
                        engine.pe, mode, PL::PL_REST, j, nPoly, engine.pe.slewedPolyRhythm[v][j], restInterp);
                }
            }
            
            int melodyBase = MonsoonIds::POLY_MELODY_VOICE_1_LEN + v * 3;
            float melodyInterp = eastInterp->params[MonsoonIds::POLY_MELODY_INTERP_1 + v].getValue();
            engine.polyLenERef(v, PL::PL_MELODY) = combineLOR(PL::PL_MELODY, 0, melodyBase,     PL::PL_MELODY, 0, 1.f, 16.f);
            engine.polyOffERef(v, PL::PL_MELODY) = combineLOR(PL::PL_MELODY, 1, melodyBase + 1, PL::PL_MELODY, 1, 0.f, 15.f);
            engine.polyRotERef(v, PL::PL_MELODY) = combineLOR(PL::PL_MELODY, 2, melodyBase + 2, PL::PL_MELODY, 2, 0.f, 15.f);
            if (eastVisual && eastVisual->inputs[cvId(PL::PL_MELODY,3)].isConnected()) {
                float att = eastLOR->params[attenId(slot,PL::PL_MELODY,3)].getValue();   // PER-VOICE depth
                float cv  = eastVisual->inputs[cvId(PL::PL_MELODY,3)].getPolyVoltage(v) / 10.f;
                melodyInterp += cv * att * 2.f;   // ×2 = ±1 span. UNCLAMPED: summed with the Macro blend in
                    // combineSpread, which end-clamps the TOTAL once (per-term clamping
                    // discarded headroom and made East-then-Macro != Macro-then-East).
            }
            melodyInterp = combineSpread(PL::PL_MELODY, melodyInterp);   // owner + Macro-CV blend (spread)
            if (eastVisual) eastVisual->polySpreadEffective[v][PL::PL_MELODY] = melodyInterp;
            
            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const redDot::SpreadInterp::Target mode = spreadInterpMono
                    ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                for (int j = 0; j < 16; j++) {
                    engine.pe.polyRandom(v, PL::PL_MELODY)[j] = redDot::SpreadInterp::apply(
                        engine.pe, mode, PL::PL_MELODY, j, nPoly, engine.pe.slewedPolyMelody[v][j], melodyInterp);
                }
            }
            
            // if (deepEast) {
            //     engine.polyLenERef(v, 1) = (int)deepEast->params[melodyBase].getValue();
            //     engine.polyOffERef(v, 1) = (int)deepEast->params[melodyBase + 1].getValue();
            //     engine.polyRotERef(v, 1) = (int)deepEast->params[melodyBase + 2].getValue();
            // }
            
            int octaveBase = MonsoonIds::POLY_OCTAVE_VOICE_1_LEN + v * 3;
            engine.polyLenERef(v, PL::PL_OCTAVE) = combineLOR(PL::PL_OCTAVE, 0, octaveBase,     PL::PL_OCTAVE, 0, 1.f, 16.f);
            engine.polyOffERef(v, PL::PL_OCTAVE) = combineLOR(PL::PL_OCTAVE, 1, octaveBase + 1, PL::PL_OCTAVE, 1, 0.f, 15.f);
            engine.polyRotERef(v, PL::PL_OCTAVE) = combineLOR(PL::PL_OCTAVE, 2, octaveBase + 2, PL::PL_OCTAVE, 2, 0.f, 15.f);

            float octaveInterp = eastInterp->params[MonsoonIds::POLY_OCTAVE_INTERP_1 + v].getValue();
            if (eastVisual && eastVisual->inputs[cvId(PL::PL_OCTAVE,3)].isConnected()) {
                float att = eastLOR->params[attenId(slot,PL::PL_OCTAVE,3)].getValue();   // PER-VOICE depth
                float cv  = eastVisual->inputs[cvId(PL::PL_OCTAVE,3)].getPolyVoltage(v) / 10.f;
                octaveInterp += cv * att * 2.f;   // ×2 = ±1 span. UNCLAMPED: summed with the Macro blend in
                    // combineSpread, which end-clamps the TOTAL once (per-term clamping
                    // discarded headroom and made East-then-Macro != Macro-then-East).
            }
            octaveInterp = combineSpread(PL::PL_OCTAVE, octaveInterp);   // owner + Macro-CV blend (spread)
            if (eastVisual) eastVisual->polySpreadEffective[v][PL::PL_OCTAVE] = octaveInterp;
            
            if (!engine.locked) {
                const int nPoly = effPolyVoices;
                const redDot::SpreadInterp::Target mode = spreadInterpMono
                    ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                for (int j = 0; j < 16; j++) {
                    engine.pe.polyRandom(v, PL::PL_OCTAVE)[j] = redDot::SpreadInterp::apply(
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
                engine.polyLenERef(v, PL::PL_ACCENT) = combineLOR(PL::PL_ACCENT, 0, accentBase,     PL::PL_ACCENT, 0, 1.f, 16.f);
                engine.polyOffERef(v, PL::PL_ACCENT) = combineLOR(PL::PL_ACCENT, 1, accentBase + 1, PL::PL_ACCENT, 1, 0.f, 15.f);
                engine.polyRotERef(v, PL::PL_ACCENT) = combineLOR(PL::PL_ACCENT, 2, accentBase + 2, PL::PL_ACCENT, 2, 0.f, 15.f);
                float accentInterp = math::clamp(eastInterp->params[MonsoonIds::POLY_ACCENT_INTERP_1 + v].getValue(), -1.f, 1.f);
                // Accent spread CV (cvId(PL_ACCENT,3)) — was missing, so accent spread
                // only responded to the manual knob, never to modulation. Mirror the
                // REST/MEL/OCT path: add CV×atten, then combineSpread (owner + Macro blend).
                if (eastVisual && eastVisual->inputs[cvId(PL::PL_ACCENT,3)].isConnected()) {
                    float att = eastLOR->params[attenId(slot, PL::PL_ACCENT, 3)].getValue();   // PER-VOICE depth
                    float cv  = eastVisual->inputs[cvId(PL::PL_ACCENT,3)].getPolyVoltage(v) / 10.f;
                    accentInterp += cv * att * 2.f;   // ×2 = ±1 span. UNCLAMPED: summed with the Macro blend in
                    // combineSpread, which end-clamps the TOTAL once (per-term clamping
                    // discarded headroom and made East-then-Macro != Macro-then-East).
                }
                accentInterp = combineSpread(PL::PL_ACCENT, accentInterp);   // owner + Macro-CV blend (spread)
                if (eastVisual) eastVisual->polySpreadEffective[v][PL::PL_ACCENT] = accentInterp;   // accent spread → editor display
                if (!engine.locked) {
                    const int nPoly = effPolyVoices;
                    const redDot::SpreadInterp::Target mode = spreadInterpMono
                        ? redDot::SpreadInterp::MONO_DRAW : redDot::SpreadInterp::AVERAGE_POLY;
                    for (int j = 0; j < 16; j++) {
                        engine.pe.polyRandom(v, PL::PL_ACCENT)[j] = redDot::SpreadInterp::apply(
                            engine.pe, mode, PL::PL_ACCENT, j, nPoly, engine.pe.slewedPolyAccent[v][j], accentInterp);
                    }
                }
            }

            // if (deepEast) {
            //     engine.polyLenERef(v, 2) = (int)deepEast->params[octaveBase].getValue();
            //     engine.polyOffERef(v, 2) = (int)deepEast->params[octaveBase + 1].getValue();
            //     engine.polyRotERef(v, 2) = (int)deepEast->params[octaveBase + 2].getValue();
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
            // is its `else if`), so "no Mono" here means exactly MACRO_SOLE.
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
                    engine.setStrand(StrandWriter::MACRO, STRND[lane],
                                     (int)std::round(macroVis->macroBase[lane][0]),
                                     (int)std::round(macroVis->macroBase[lane][1]),
                                     (int)std::round(macroVis->macroBase[lane][2]));
                }
            }
            for (int v = 0; v < effPolyVoices; ++v) {
                // Global LOR per lane (Macro owns → no per-voice/ownership branch).
                // macroBase[lane][item]: item 0=LEN 1=OFF 2=ROT, engine-lane indexed.
                for (int lane = 0; lane < PL::PL_LANES; ++lane) {
                    engine.polyLenERef(v, lane) = (int)math::clamp(std::round(macroVis->macroBase[lane][0]), 1.f, 16.f);
                    engine.polyOffERef(v, lane) = ((int)std::round(macroVis->macroBase[lane][1]) % 16 + 16) % 16;
                    engine.polyRotERef(v, lane) = ((int)std::round(macroVis->macroBase[lane][2]) % 16 + 16) % 16;
                }
                // Global spread per lane → spread-applied final arrays for this voice.
                const float spR = math::clamp(macroVis->macroBase[PL::PL_REST][3]   + macroVis->macroSendDelta[PL::PL_REST][3],   -1.f, 1.f);
                const float spM = math::clamp(macroVis->macroBase[PL::PL_MELODY][3] + macroVis->macroSendDelta[PL::PL_MELODY][3], -1.f, 1.f);
                const float spO = math::clamp(macroVis->macroBase[PL::PL_OCTAVE][3] + macroVis->macroSendDelta[PL::PL_OCTAVE][3], -1.f, 1.f);
                const float spA = math::clamp(macroVis->macroBase[PL::PL_ACCENT][3] + macroVis->macroSendDelta[PL::PL_ACCENT][3], -1.f, 1.f);
                for (int j = 0; j < 16; ++j) {
                    engine.pe.polyRandom(v, PL::PL_REST)[j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_REST,   j, nPoly, engine.pe.slewedPolyRhythm[v][j], spR);
                    engine.pe.polyRandom(v, PL::PL_MELODY)[j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_MELODY, j, nPoly, engine.pe.slewedPolyMelody[v][j], spM);
                    engine.pe.polyRandom(v, PL::PL_OCTAVE)[j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_OCTAVE, j, nPoly, engine.pe.slewedPolyOctave[v][j], spO);
                    engine.pe.polyRandom(v, PL::PL_ACCENT)[j] = redDot::SpreadInterp::apply(engine.pe, mode, PL::PL_ACCENT, j, nPoly, engine.pe.slewedPolyAccent[v][j], spA);
                }
            }
        }
    }
}