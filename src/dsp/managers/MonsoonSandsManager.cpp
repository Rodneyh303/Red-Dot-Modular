#include "MonsoonSandsManager.hpp"
#include "MonsoonLockManager.hpp"
#include "../VoiceResolver.hpp"   // kMonoSlot — the mono mix-in slice
#include "../SpreadInterp.hpp"
#include "../SpreadResolver.hpp"   // step 3b: single authority for effective spread amount
#include "../../Monsoon.hpp"
#include "../../MonsoonSandsVisualExpander.hpp"
//#include "../../MonsoonSandsExpander.hpp"
#include "../../StraitsSandsMacroVisual.hpp"
#include "MonsoonExpanderManager.hpp"
#include "../../MonsoonContainment.hpp"             // the publish block below dereferences core.graph — complete type required (ExpanderManager.hpp only forward-declares)
#include "../../MonsoonChangeAlleyV2.hpp"           // V2 shares the pin contract
#include "../SandsTopology.hpp"   // step 3b: readStrand owner migration
#include "../../MonsoonStraitsExpander.hpp"
#include "../../StraitsEastSandsVisual.hpp"
#include "../../ui/ModuleLookup.hpp"   // THE lookup authority: lookupMonsoon (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START)
#include <cassert>
#include <cstring>   // memcmp/memcpy — the control-tick redundancy gates (perf round 2)

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
    if (expanderManager.cachedChangeAlleyV2) {
        // Change Alley V2 owns the same pin contract; push its board the same way.
        auto* v2 = expanderManager.cachedChangeAlleyV2;
        bool identity = true;
        for (int v = 0; v < 16; ++v) {
            engine.pe.caRhythmSrc[v] = v2->rhythmSrc[v];   // stage current pins (applied by remap below)
            engine.pe.caMelodySrc[v] = v2->melodySrc[v];
            engine.pe.caQmixSrc[v]   = v2->qmixSrc[v];     // q-mix green plane (parity; blend threshold + slewedQmix route)
            if (v2->rhythmSrc[v] != v || v2->melodySrc[v] != v || v2->qmixSrc[v] != v) identity = false;
        }
        // Pins LATCH (LOCK_SEMANTICS §9: correlation config, shapes draws PRE-spread). Under lock,
        // HOLD the pre-lock remapped slewed buffers: skip the recompute+remap AND the sig capture, so
        // a pin edit under lock is neither applied now NOR marked seen -> it's detected and committed
        // at unlock (remapSigChanged_ fires then). This closes a real leak: when a CA is attached but
        // NO Sands visual (sandsActive=false), PatternEngine copies slewed->random every sample
        // UNGATED, so a pin move would otherwise reach output under lock. (When Sands IS present the
        // spread step is already lock-gated, but this covers the no-Sands case too.) The staged
        // caRhythmSrc/caMelodySrc above hold the current pins, so the unlock remap applies correctly.
        // No prime needed: the played random[] arrays are persisted, so a locked-load is already right.
        // Pins scope (LOCK_SCOPE_MENU): PER-AXIS. pinR remaps the rhythm-family buffers, pinM the
        // melody-family buffers (remapSlewedByPins(doR,doM) — the families use separate src arrays +
        // buffers, so the split is exact). A frozen axis is skipped, holding its pre-lock pinned values.
        const bool pinR = dotModular::LockManager::liveNow(dotModular::Control::Pins, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/false);
        const bool pinM = dotModular::LockManager::liveNow(dotModular::Control::Pins, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/true);
        // q-mix pins ride their OWN axis (SB_CA_Q, green plane), independent of melody. Pins is LATCH,
        // so !locked OR the q-mix CA bit opted live. Evaluated on the bit directly (third axis).
        const bool pinQ = !engine.locked
                        || (engine.scopeLiveMask & (1u << 12)) != 0;   // == dotModular::SB_CA_Q
        if (pinR || pinM || pinQ) {
            if (!identity) {
                // FLICKER FIX (SANDS_SCATTER_FLICKER_DIAGNOSIS): only re-derive+remap when the pins or
                // the underlying slewed content actually changed. Running it every block rewrote the
                // slewed buffers in place under the UI reader -> REST/ACCENT flicker. When unchanged,
                // last cycle's remapped buffers are already correct, so skip the churn. (The combined
                // sig decides WHEN to recompute; pinR/pinM/pinQ decide WHICH families — a frozen axis's
                // src can't change under lock, so the combined sig firing on a live axis is harmless.)
                if (remapSigChanged_(v2->rhythmSrc, v2->melodySrc, v2->qmixSrc, /*identity=*/false)) {
                    engine.pe.forceRecomputeSlewed();
                    engine.pe.remapSlewedByPins(pinR, pinM, pinQ);
                    captureRemapSig_(v2->rhythmSrc, v2->melodySrc, v2->qmixSrc, /*identity=*/false);
                }
            } else {
                // Pins returned to identity: mark it so the next non-identity is treated as a change.
                captureRemapSig_(v2->rhythmSrc, v2->melodySrc, v2->qmixSrc, /*identity=*/true);
            }
        }
    } else {
        for (int v = 0; v < 16; ++v) { engine.pe.caRhythmSrc[v] = (uint8_t)v; engine.pe.caMelodySrc[v] = (uint8_t)v; engine.pe.caQmixSrc[v] = (uint8_t)v; }
    }
    // ── Containment PUBLISH (the CA seam mirrored — the build plan's step-4 "engine-side publish").
    // PatternEngine owns the authoritative array (the decision manager's future read); the
    // Containment module is the editor/driver. Staged EVERY block like the CA pins: the graph is
    // HELD between boundaries, so the staging is idempotent within a phrase and always current at
    // a boundary. ABSENT module => the R2 all-disconnected default (all zeros = full equality),
    // the same absent-module reset the CA block above performs with its identity diagonal.
    if (expanderManager.cachedContainment) {
        const auto* ctm = expanderManager.cachedContainment;
        for (int v = 0; v < 16; ++v)
            engine.pe.containmentParents[v] = ctm->core.graph.parents[v];
    } else {
        for (int v = 0; v < 16; ++v) engine.pe.containmentParents[v] = 0;
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
    // MVC step 1c: Macro's GLOBAL scope now lives in the store, not in Macro's params.
    // Resolved once here; the lambdas below capture it by reference. (Macro's widget
    // step() dual-writes params -> store, so this reads the same values the params hold.)
    // Resolve the Monsoon store owner from any attached Sands expander (Macro OR Mono) —
    // they hang off the SAME Monsoon. Was macroVis-only, which left gMon null when Macro was
    // absent and broke Mono store reads (attens). Macro-global reads below stay guarded by
    // hasMacro/macroVis, so broadening is safe.
    //
    // LOOKUP AUTHORITY (CC_PROMPT_MODULE_LOOKUP_AUTHORITY_START): the host resolve is
    // the memoized lookupMonsoon — event-driven (onExpanderChange bumps the topology
    // epoch), ONE walk per module per generation on this thread instead of the old
    // hand-rolled once-per-call memos (perf round 2) which still re-walked every
    // control tick on a static rack. Same pointers (the same segment-rule walk),
    // BIT-IDENTICAL to the per-site walks both replaced.
    Monsoon* const macroHost = macroVis ? redDot::lookupMonsoon(macroVis) : nullptr;
    Monsoon* const monoHost  = monoVis  ? redDot::lookupMonsoon(monoVis)  : nullptr;
    Monsoon* const eastHost  = expanderManager.cachedEastSandsVisual
                             ? redDot::lookupMonsoon(expanderManager.cachedEastSandsVisual) : nullptr;
    Monsoon* gMon = nullptr;
    if (macroVis)      gMon = macroHost;
    else if (monoVis)  gMon = monoHost;
    // HARDENING (mmOwn precedent — delegation-bug round): the chain walk can return null
    // at a rack boundary even with the expander attached; the old code then filled
    // ownership via the !hasMacro lambda = ALL-EAST, and warp followed East with
    // Macro+East both attached (the gMon-null state also zeroes every other store read —
    // the documented spread/QMIX-arc cluster). expanderManager.owner is the back-pointer
    // Monsoon::process sets before this call — same store, no walk. Walk first (keeps
    // shared-expander semantics where the walk works), back-pointer as null-walk rescue.
    if (!gMon && expanderManager.owner) gMon = expanderManager.owner;

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
    // CC_PROMPT_07 (the warp voice/slot + Macro→East leak): presence alone is NOT enough.
    // Without the owner rows, topo.owner() answers the Inputs STRUCT DEFAULTS —
    // eastV1Owner all-true (V1 never delegated) and eastPolyOwner all-false (EVERY poly
    // (v,lane) Macro-owned) — so warpDelegated_ below went all-true for V2+ on EVERY
    // lane: Macro's global curve reached even East-owned cells (the "non-delegated
    // leak"), East's own-slot warp was bypassed for V2+ ("East warp has no effect"),
    // and V1 never tracked Macro ("no effect on voice 1"). Fill the rows from the store
    // (voiceSlot rows: v=0 → V1, 1..15 → V2..V16; no Macro ⇒ East is the sole owner).
    // Layer-2 (ZAI_PROMPT_delegation_decision.md §2B): the ONE shared fill is now
    // Inputs::fillOwnersFromStore. The lambda's `gMon &&` guard moves to the caller —
    // gMon is the store owner resolved above; where it is null the store is unreadable
    // and the fallback reproduces the old guard value EXACTLY (!hasMacro per cell — the
    // lambda's short-circuit: a CONSTANT, no store read, so not the drift class the
    // one-fill rule targets; it also keeps this file's B3 pin in test_warp_voice_isolation
    // true). The helper takes the boolean store directly (editor.eastOwns) — the
    // boolean-model migration (Rodney, 2026-10-22) removed the float `> 0.5f`; there is
    // no threshold anywhere in the ownership pipeline now.
    // GATE 1 (perf round 2): skip the ownership fill + topology build + the warpDelegated_
    // mirror rescan when the fingerprint is unchanged. BIT-IDENTICAL ARGUMENT: the fill is
    // a pure function of (eastPresent, macroPresent, gMon-null, eastOwns[112],
    // polyVoiceCount); SandsTopology::build is a pure function of the filled Inputs; the
    // mirror scan below writes only cells whose owner() flipped (and rebuilds only then).
    // Fingerprint unchanged ⇒ fill output unchanged ⇒ owner() answers identically ⇒ the
    // scan would find ZERO flips ⇒ warpTopoChanged stays false and no rebuild runs —
    // exactly what skipping produces. warpDelegated_ has no writer outside that scan
    // (grep: the only setter path), so an unchanged fingerprint also means the mirror is
    // already in sync. The WARPDIAG log below stays UNGATED — it keeps its own 1/2048-tick
    // rate limit and reads live state, so its output is unchanged (chg=0, the observed
    // steady state).
    const bool topoUnchanged = lastTopoSig_.valid
        && lastTopoSig_.eastPresent == topoIn.eastPresent
        && lastTopoSig_.macroPresent == topoIn.macroPresent
        && lastTopoSig_.polyBaseActive == topoIn.polyBaseActive
        && lastTopoSig_.polyVoiceCount == topoIn.polyVoiceCount
        && lastTopoSig_.gMonNull == (gMon == nullptr)
        && (gMon == nullptr
            || 0 == std::memcmp(lastTopoSig_.own, gMon->editor.eastOwns, sizeof lastTopoSig_.own));
    if (!topoUnchanged) {
        if (gMon) {
            topoIn.fillOwnersFromStore(gMon->editor.eastOwns,
                                       /*eastPresent=*/expanderManager.cachedEastSandsVisual != nullptr,
                                       /*macroPresent=*/hasMacro);
        } else {
            topoIn.fillOwners(/*eastPresent=*/expanderManager.cachedEastSandsVisual != nullptr,
                              [hasMacro](int, int) { return !hasMacro; });
        }
        cachedTopo_ = dotModular::SandsTopology::build(topoIn);
        lastTopoSig_.valid = true;
        lastTopoSig_.eastPresent = topoIn.eastPresent;
        lastTopoSig_.macroPresent = topoIn.macroPresent;
        lastTopoSig_.polyBaseActive = topoIn.polyBaseActive;
        lastTopoSig_.polyVoiceCount = topoIn.polyVoiceCount;
        lastTopoSig_.gMonNull = (gMon == nullptr);
        if (gMon) std::memcpy(lastTopoSig_.own, gMon->editor.eastOwns, sizeof lastTopoSig_.own);
        else      std::memset(lastTopoSig_.own, 0, sizeof lastTopoSig_.own);
    }
    if (monoVis) {
        // MVC step 1d: owner is STORE-BACKED (editor.monoOwner via getMonoOwner). Was params[ownerDispId].
        // SANDS CONSOLIDATION Step 7: monoV1Owner removed — V1 ownership now via eastV1Owner only.
    }
    // (was: build a fresh topology every tick; now the gate reuses the cached one —
    // a const REF, so every topo.owner() read below is the identical value the fresh
    // build would have produced.)
    const dotModular::SandsTopology& topo = cachedTopo_;
    const bool macroDrivesOutput = hasMacro && polyBaseActive;
    // Warp mirror delegation (CC_PROMPT_03 STEP 3): mirror the topology's Macro-owned
    // (v,lane) matrix onto the engine (the spreadTargetMode pattern), and rebuild the
    // mirror when it flips — a delegation change moves the EFFECTIVE warp for that
    // (v,lane) even though no bank slot did. Full scope: owner flips are rare user
    // actions. Inert while no bank is wired (the rebuilds no-op).
    {
        bool warpTopoChanged = false;
        if (!topoUnchanged) {   // GATE 1: unchanged fingerprint ⇒ owner() unchanged ⇒ no flips possible
            for (int v = 0; v < 16; ++v)
                for (int l = 0; l < 7; ++l) {
                    const bool d = (topo.owner(v, l) == dotModular::SandsTopology::Role::MACRO);
                    if (engine.pe.warpDelegated_[v][l] != d) {
                        engine.pe.warpDelegated_[v][l] = d;
                        warpTopoChanged = true;
                    }
                }
            if (warpTopoChanged) engine.pe.rebuildWarpMirrorAll();
        }
        // ── DIAGNOSTIC (delegation-bug hunt; REVERT after fix) ──────────────
        // Throttled authority-state burst (~1 line / 1.4 s at the 1500 Hz control
        // rate). If warp follows East with Macro attached, one of these fields
        // explains it:
        //   gMon=ok/NULL + the fallback above — old code filled ALL-EAST on NULL;
        //   own=hex(7 lanes; bit l set = store says EAST owns) for V1/V2;
        //   ndel=hex(7 lanes; bit l set = NOT delegated → own-slot/East-side warp);
        //   chg = topology flips this burst; rb = total mirror rebuilds (a per-tick
        //   climb here = rebuild storm = CPU).
        // Statics shared across Monsoon instances — fine for the single-Monsoon repro.
        {
            static unsigned diagTick = 0, rbCnt = 0;
            if (warpTopoChanged) ++rbCnt;
            if ((diagTick++ & 0x7FF) == 0) {
                auto packs = [&](int v, unsigned& own, unsigned& ndel) {
                    own = ndel = 0;
                    for (int l = 0; l < 7; ++l) {
                        if (gMon && gMon->getOwnerEast(v, l)) own |= (1u << l);
                        if (!engine.pe.warpDelegated_[v][l])  ndel |= (1u << l);
                    }
                };
                unsigned o0, d0, o1, d1;
                packs(0, o0, d0);
                packs(1, o1, d1);
                INFO("[WARPDIAG] gMon=%s macro=%d east=%d poly=%d | V1 own=%02x ndel=%02x V2 own=%02x ndel=%02x | chg=%d rb=%u",
                     gMon ? "ok" : "NULL", hasMacro ? 1 : 0,
                     expanderManager.cachedEastSandsVisual ? 1 : 0, polyBaseActive ? 1 : 0,
                     o0, d0, o1, d1, (int)warpTopoChanged, rbCnt);
            }
        }
    }
    // sandsActive must be true whenever ANYTHING writes the spread-applied final arrays,
    // or PatternEngine's slew stage copies the raw slewed draws back over them every
    // sample. Macro STANDALONE (hasMacro, no base expander) now applies its global spread
    // to the voices (see the else-branch below), so include it here too — otherwise that
    // work was silently overwritten and the spread knob looked dead.
    engine.pe.setSandsActive(hasMonoVisual || macroDrivesOutput || (hasEastVisual && polyBaseActive) || hasMacro);
    // Hint assignment + HINT-RISE REFILL (V16-flicker round-2, finding 2): the
    // load-time redraw runs BEFORE the first processDNA sets the hint (hint=0 there),
    // so the poly voices' slewed*/pubs stay ALL-ZERO until the first roll — every poly
    // bar flat after load (masked as "stable"). When the hint RISES, recompute +
    // publish all three families so the new voices have real data immediately. One
    // full recompute per hint change (load attach / voice-count turn) — roll-priced.
    // (Known edge, NOT fixed here: with CA pins set, the pin remap's signature doesn't
    // include the hint, so a hint-rise under non-identity pins leaves the new voices
    // un-remapped until the next roll.)
    if (effPolyVoices > engine.pe.numPolyVoicesHint) {
        engine.pe.numPolyVoicesHint = effPolyVoices;   // set BEFORE the recompute: fills v=1..eff
        engine.pe.forceRecomputeSlewed();              // recompute + publish, all three families
    } else {
        engine.pe.numPolyVoicesHint = effPolyVoices;   // gated + output-bounded: display matches audio
    }

    // ── Helper: apply mono CV offset at read site ─────────────────────────
    auto applyMonoCV = [&](float base, int lane, int param, float lo, float hi) -> float {
        if (!monoVis || !monoVis->inputs[Mono::cvId(lane, param)].isConnected()) return base;
        float cv  = monoVis->inputs[Mono::cvId(lane, param)].getVoltage() / 10.f;
        // MVC step 1d: atten is STORE-BACKED (editor.monoAtten[lane,param], lane=editor).
        // Was params[attenId(lane,param)]; gMon now resolves via monoVis too, so CV depth works
        // even without Macro attached.
        float att = gMon ? gMon->getMonoAtten(lane, param) : 0.f;
        return clamp(base + cv * att * (hi - lo), lo, hi);
    };

    // (applyMacroCV — the macro CV-offset read-site helper — moved VERBATIM into
    //  publishGlobalProduce_ with its only caller, the RATE MANAGER S4 extraction.)
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
        // editor lane l. Editor lane == engine strand (identity since the
        // lane-index unification; the old MONO_LANE_TO_STRAND table is deleted),
        // so this stays a loop over lane constants rather than a hand-ordered
        // call list that could drift. (DNA-expander base path is retired;
        // base always comes from the visual params. cvRow == editor lane.)
        auto readStrand = [&](int l) {
            // MVC step 1d: LOR base is STORE-BACKED (editor.lorBase[kMonoSlot, bank, c]). Was
            // params[lenId/offId/rotId(l)] (removed). l is the EDITOR lane; bank = engine lane for
            // poly (0..4), self for VAR/LEG (5,6) — East's lorBank. gMon resolves via monoVis so
            // it's valid without Macro. Defaults (16/0/0) match the identity init if store is empty.
            const int bLor = dotModular::lorStoreBank(l);   // single canonical editor→lorBase bank (covers VAR/LEG)
            float baseLen = gMon ? gMon->getLorBase(dotModular::VoiceResolver::kMonoSlot, bLor, 0) : 16.f;
            float baseOff = gMon ? gMon->getLorBase(dotModular::VoiceResolver::kMonoSlot, bLor, 1) : 0.f;
            float baseRot = gMon ? gMon->getLorBase(dotModular::VoiceResolver::kMonoSlot, bLor, 2) : 0.f;

            // V1 ownership (poly lanes only: editor 0..4 = MEL/OCT/QMIX/REST/ACC). When
            // Mono CEDES a lane (ownerDispId == 0) and Macro is attached, V1's base
            // comes from Macro's GLOBAL base for that lane instead of Mono's own LOR
            // edit — mirroring how poly voices switch base by ownerId. LEG/VAR (l>=4)
            // are mono-only and always Mono-owned. (macroBase is published later in
            // this same block → one control-block lag, same as the macroMix delta.)
            // POLY_LANE_COUNT (5) not 4: editor lanes 0..4 = MEL/OCT/QMIX/REST/ACC are ALL poly
            // lanes and can be delegated to Macro; the old < 4 dropped ACCENT (editor lane 4).
            if (l < dotModular::POLY_LANE_COUNT && hasMacro && macroVis) {
                // STEP 3b: delegated ⟺ topo.owner(0,l) == MACRO.
                const bool delegated = (topo.owner(0, l) == dotModular::SandsTopology::Role::MACRO);
                if (delegated) {
                    // Delegated → track Macro's base + the CEDING VOICE's tapped CV delta
                    // (P9b per-voice: the ceding voice here is the mono strand, slot 0 —
                    // its own macroTap lerps between the published endpoints). PRE (tap=0)
                    // = raw CV even when the left atten is 0; POST (tap=1) = CV × atten.
                    // (Was the Macro-GLOBAL tap baked into one macroSendDelta — every
                    // receiving voice forced onto the same pre/post choice.)
                    auto* mmDeleg = macroHost;   // PERF: once-per-call walk memo (see top of processDNA)
                    const float tapL = (mmDeleg ? mmDeleg->getMacroTap(dotModular::VoiceResolver::kMonoSlot, l, 0) : 1.f);
                    baseLen = macroVis->macroBase[l][0] + macroVis->tappedDelta(tapL, l, 0);
                    baseOff = macroVis->macroBase[l][1] + macroVis->tappedDelta(tapL, l, 1);
                    baseRot = macroVis->macroBase[l][2] + macroVis->tappedDelta(tapL, l, 2);
                }
            }

            // SANDS CONSOLIDATION Step 7: V1 base CV applies only to lanes the V1 editor OWNS.
            // A delegated lane tracks Macro exclusively — V1's CV must not additionally modulate it.
            // V1 editor is EAST (or MACRO_SOLE). The applyMonoCV lambda is now a no-op (Mono visual
            // killed in Step 6 — monoVis is always nullptr), but the guard stays for future East CV.
            const bool v1OwnsLane = (topo.owner(0, l) == dotModular::SandsTopology::Role::EAST);
            if (v1OwnsLane) {
                baseLen = applyMonoCV(baseLen, l, 0, 1.f, 16.f);
                baseOff = applyMonoCV(baseOff, l, 1, 0.f, 15.f);
                baseRot = applyMonoCV(baseRot, l, 2, 0.f, 15.f);
            }

            // INTERPRETATION Y — voice-1 mix-in. The mono master strand is voice 1 of
            // the ensemble; like every poly voice it can receive the East per-lane CV
            // and the Macro global-CV mix-in, summed on top of the mono base. We use
            // SLOT 0 of the East/Macro per-voice banks — which is now voice 1 / mono under
            // the voice-number (N→N) indexing (slot v = voice v+1 — i.e. EXACTLY
            // VoiceResolver::voiceSlot: V1→0, V2→1, …, V16→15; the ONE slot convention,
            // the same one fillOwners/warpDelegated_/the warp banks use). Poly voices read
            // slot v+1, so they no longer collide with this mono slot.
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
                float att = (eastHost ? eastHost->getMacroAtten(dotModular::VoiceResolver::kMonoSlot, engLane*4 + item) : 0.f);   // slot 0 = mono — PERF: walk memo (was TWO walks per call)
                float cv  = eastVis->inputs[East::cvId(engLane,item)].getVoltage(0) / 10.f; // ch0
                return cv * att * (hi - lo);
            };
            // Macro global CV distributed to voice 1 (voice-0 send × macroCVDelta).
            // The send bank AND macroCVDelta/macroSendDelta are indexed by the UNIFIED
            // poly lane (publishGlobal's loop) — editor l IS that lane (identity), so
            // pass l directly.
            // NOTE: macroCVDelta is published by the Macro block LATER in this same
            // processDNA (after the mono strand), so voice 1 reads the PREVIOUS control
            // block's delta — a one-block (sub-ms, control-rate) lag. Imperceptible.
            // Returns the UNCLAMPED Macro send delta (0 when no Macro). Summed with East's, clamped once.
            auto macroDelta = [&](int engLane, int item)->float {
                if (!hasMacro || !macroVis) return 0.f;
                auto* mm = macroHost;   // slot 0 = mono (V1) — PERF: walk memo
                float send = (mm ? mm->getMacroSend(dotModular::VoiceResolver::kMonoSlot, engLane, item) : 0.f);
                // P9b: the PER-VOICE tapped delta — V1's own macroTap selects between the
                // published pre/post endpoints (was the Macro-global tap, one choice for all).
                const float tap = (mm ? mm->getMacroTap(dotModular::VoiceResolver::kMonoSlot, engLane, (item == 3 ? 1 : 0)) : 1.f);
                return macroVis->tappedDelta(tap, engLane, item) * send;
            };
            // East CV exists only for the 4 poly lanes (MEL/OCT/REST/ACC); VAR/LEG
            // (editor l>=4) are mono-only with no East jack → skip the eastMix there.
            // A DELEGATED lane tracks Macro's value exclusively (set above) — it takes
            // neither East's CV nor the Macro SEND on top (that would double-count Macro's
            // modulation, already in the delegated base). Only OWNED lanes receive these.
            if (l < dotModular::SandsGrid::POLY_LANES && v1OwnsLane) {
                // Sum ALL mods (East CV + Macro send) onto the base, then clamp the END RESULT once.
                baseLen = math::clamp(baseLen + eastDelta(l, 0, 1.f, 16.f) + macroDelta(l, 0),  1.f, 16.f);
                baseOff = math::clamp(baseOff + eastDelta(l, 1, 0.f, 15.f) + macroDelta(l, 1),  0.f, 15.f);
                baseRot = math::clamp(baseRot + eastDelta(l, 2, 0.f, 15.f) + macroDelta(l, 2),  0.f, 15.f);
            }

            // LOR is LATCH (LOCK_SEMANTICS §9): under lock, do NOT re-push the strand window --
            // the engine's LOR state persists, so skipping the write holds the pre-lock resolved
            // value (base + latched CV). This also latches OWNER for free: owner selects which base
            // feeds baseLen above, and freezing the apply freezes that selection's effect too.
            // SCOPE (LOCK_SCOPE_MENU): per-strand R/M — MELODY/OCTAVE strands are the melody axis,
            // the rest (RHYTHM/VAR/LEG/ACCENT) rhythm. strand is the engine strand for this lane.
            // q-mix is its OWN Sands axis (SB_SANDS_Q), not rhythm (the old lorMelodyAxis=false
            // default put STRAND_QMIX on the rhythm bit — split-brain vs its melody-axis spread).
            const bool lorMelodyAxis = (l == dotModular::STRAND_MELODY || l == dotModular::STRAND_OCTAVE);
            const bool lorLive = (l == dotModular::STRAND_QMIX)
                ? (!engine.locked || (engine.scopeLiveMask & (1u << 13)) != 0)   // == dotModular::SB_SANDS_Q
                : dotModular::LockManager::liveNow(dotModular::Control::Lor, engine.locked, engine.scopeLiveMask, lorMelodyAxis);
            if (lorLive) {
                engine.setStrand(StrandWriter::MONO, l,
                                 (int)std::round(baseLen),
                                 (int)std::round(baseOff),
                                 (int)std::round(baseRot));
            }
        };

        // Spread: base trimpot + per-lane spread CV. Lane index is the UNIFIED order
        // (editor == engine == PL_*: 0=MEL, 1=OCT, 2=QMIX, 3=REST, 4=ACC, 5=VAR, 6=LEG).
       // if (hasMonoVisual) 
       // {
            // REST/MEL/OCT/ACCENT spread (engine/spread lanes 0..3). Accent is now
            // wired like the others (was previously skipped: loop l<3 + spreadEffective[3]=0,
            // so Mono accent spread did nothing). sprId(3)=accent base; East spread CV
            // jack cvId(3,3); Macro send/delta lane 3.
            for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
                // Delegation: if Mono CEDES this poly spread lane to Macro, the spread
                // tracks Macro's global spread (base + tapped send delta) exclusively —
                // no Mono base, no Mono/East CV (parallel to the LOR delegation above).
                // l is the UNIFIED poly lane (editor == engine — identity); the owner
                // store and monoMacroOwnsEngineLane both speak the same unified order.
                // Effective spread amount via the single authority (SpreadResolver). All inputs are
                // read from exactly the same places the previous inline arithmetic did; the resolver
                // replicates the manager's step-wise clamping, so this is behaviour-inert. Topology
                // decides delegation (owner()==MACRO, via monoMacroOwnsEngineLane); the resolver only
                // consumes that boolean. l is the unified poly lane.
                auto* eastVisS = expanderManager.cachedEastSandsVisual;
                // MVC step 1d: spread base + atten are now STORE-BACKED (editor.spread /
                // editor.monoAtten), not params. gMon is the store owner; was NOT in scope here.
                Monsoon* gMon = monoHost;   // PERF: walk memo (was a per-lane re-walk)
                redDot::SpreadResolver::Inputs sin;
                sin.delegated    = hasMacro && macroVis &&
                                   SandsMonoVisualIds::monoMacroOwnsEngineLane(monoVis, l);
                sin.macroPresent = (hasMacro && macroVis);
                // base: editor.spread[kMonoSlot, l] (l = unified poly lane). Was params[sprId(l)].
                sin.base         = gMon ? gMon->getSpread(dotModular::VoiceResolver::kMonoSlot, l) : 0.f;
                // Own Mono spread CV (sprCvId jack + sprAttenId), unit-scaled /10.
                if (monoVis->inputs[Mono::sprCvId(l)].isConnected()) {
                    sin.ownCv.connected   = true;
                    sin.ownCv.unitVoltage = monoVis->inputs[Mono::sprCvId(l)].getVoltage() / 10.f;
                    // atten: editor.monoAtten is lane-indexed (col 3 = spread atten) — identity,
                    // the SAME lane index the widget's StoreKnob writes. Was params[sprAttenId(l)].
                    sin.ownCv.atten       = gMon ? gMon->getMonoAtten(l, 3) : 0.f;   // identity: spread lane == editor lane
                }
                // East V1 spread CV add (cvId(engineLane,3) jack + attenId(mono slot,l,3)).
                if (eastVisS && eastVisS->inputs[East::cvId(l,3)].isConnected()) {
                    sin.eastCv.connected   = true;
                    sin.eastCv.unitVoltage = eastVisS->inputs[East::cvId(l,3)].getVoltage(0) / 10.f;
                    sin.eastCv.atten       = (eastHost ? eastHost->getMacroAtten(dotModular::VoiceResolver::kMonoSlot, l*4 + 3) : 0.f);   // PERF: walk memo (was TWO walks)
                }
                if (sin.macroPresent) {
                    sin.macroBase      = macroVis->macroBase[l][3];
                    // P9b: both endpoints + the ceding voice's (mono slot) tap — the
                    // resolver lerps (SpreadResolver::effective → tappedDelta).
                    sin.macroPreDelta  = macroVis->macroPreDelta[l][3];
                    sin.macroPostDelta = macroVis->macroPostDelta[l][3];
                    sin.macroTap       = (macroHost ? macroHost->getMacroTap(dotModular::VoiceResolver::kMonoSlot, l, 1) : 1.f);    // PERF: walk memo (was TWO walks)
                    sin.macroSend      = (macroHost ? macroHost->getMacroSend(dotModular::VoiceResolver::kMonoSlot, l, 3) : 0.f);  // PERF: walk memo (was TWO walks)
                }
                engine.spreadERef(0, l) = redDot::SpreadResolver::effective(sin);   // slot 0 = mono/V1
            }
            // engine.spread[slot][editorLane] is the engine's own spread state (was monoVis->
            // spreadEffective — MVC: the model no longer reaches into the visual). Accessed
            // via spreadE/spreadERef in the UNIFIED order (0=MEL,1=OCT,2=QMIX,3=REST,4=ACCENT,
            // 5=VAR,6=LEG — editor == engine). SANDS
            // CONSOLIDATION Step 3: lanes 5/6 (VAR/LEG) are now spread-active (the loop above runs
            // l < POLY_LANES=7); the old 'lanes 4/5 unused (LEG/VAR mono-only)' note was stale.

            // ── Sands spread→final (Option W, Model 1) ───────────────────────
            // Mono owns the MONO final arrays: read the SLEWED draw, apply
            // per-lane spread for REST/MEL/OCT (converge toward the poly-incl-
            // mono average), pass LEG/ACC/VAR raw, write the FINAL arrays the
            // sequencer reads. setSandsActive(true) stops slew copying the
            // un-spread draw over the top.
            // When LOCKED, leave the final arrays frozen (skip the rewrite) so
            // lock freezes the audible output — spread CV won't leak through.
            engine.pe.setSandsActive(true);
            // Spread scope (LOCK_SCOPE_MENU): PER-AXIS. sprR gates the rhythm-axis arrays (rhythm,
            // legato, accent, variation), sprM the melody-axis arrays (melody, octave). A frozen axis
            // skips its writes so slew leaves the pre-lock spread-applied values in place (setSandsActive
            // stays true, so the frozen arrays are NOT overwritten by the raw draw either).
            const bool sprR = dotModular::LockManager::liveNow(dotModular::Control::Spread, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/false);
            const bool sprM = dotModular::LockManager::liveNow(dotModular::Control::Spread, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/true);
            // QMIX is its OWN axis (SB_SANDS_Q), not melody. Spread is LATCH: !locked OR the q-mix bit.
            const bool sprQ = !engine.locked || (engine.scopeLiveMask & (1u << 13)) != 0;   // == dotModular::SB_SANDS_Q
            if (sprR || sprM || sprQ) {
            // GATE 2 (perf round 2): the 16×N applyMono pass below is a pure function of
            // (per-lane spread values, spreadTargetMode, the mono slewed + pre-remap rows)
            // — and it writes random_[0], so the sig also carries the row AS LAST WRITTEN:
            // ANY other writer of the row (pin-remap promote, rotates, resets) breaks the
            // match and the pass re-runs, preserving the unconditional version's every-tick
            // healing exactly. Inputs unchanged AND row still ours ⇒ the pass would rewrite
            // bit-identical cells ⇒ skip is value-identical. (The assert path's pe.ca*Src
            // reads are compiled out wherever this TU ships — REDDOT_SPREAD_ASSERT is 0 by
            // default and the test rig does not compile this file.)
            const float sigSpM[7] = {
                engine.spreadE(0, SequencerEngine::PL_MELODY),
                engine.spreadE(0, SequencerEngine::PL_OCTAVE),
                engine.spreadE(0, SequencerEngine::PL_QMIX),
                engine.spreadE(0, SequencerEngine::PL_REST),
                engine.spreadE(0, SequencerEngine::PL_ACCENT),
                engine.spreadE(0, SequencerEngine::PL_VARIATION),
                engine.spreadE(0, SequencerEngine::PL_LEGATO),
            };
            if (v1SpreadDirty_(sprR, sprM, sprQ, sigSpM)) {
            for (int i = 0; i < 16; ++i) {
                if (sprR) {
                    // LANE UNIFICATION (Phase 2): the spread-value stores are ALL unified
                    // (menu EDITOR_LONG, East binds, the pipeline loop above) — so the
                    // spreadE fetch must use PL_* too. The old raw literals (0/1/2/3/4/5/6
                    // under the dead spread order) read the WRONG lane's knob.
                    engine.pe.rhythmRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_REST, i,
                        engine.spreadE(0, SequencerEngine::PL_REST));
                    // VAR/LEG spread: applyMono, not the raw slewed copy. Under
                    // anchor-V1 this is a self-target no-op, but under follow-CA (V1 pinned) it
                    // interpolates own(pre-remap) vs target(post-remap) — meaningful. Either way
                    // the spread value is now read/published instead of dropped on the floor.
                    engine.pe.variationRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_VARIATION, i,
                        engine.spreadE(0, SequencerEngine::PL_VARIATION));
                    engine.pe.legatoRandom[i]    = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_LEGATO, i,
                        engine.spreadE(0, SequencerEngine::PL_LEGATO));
                    engine.pe.accentRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_ACCENT, i,
                        engine.spreadE(0, SequencerEngine::PL_ACCENT));
                }
                if (sprM) {
                    engine.pe.melodyRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_MELODY, i,
                        engine.spreadE(0, SequencerEngine::PL_MELODY));
                    engine.pe.octaveRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_OCTAVE, i,
                        engine.spreadE(0, SequencerEngine::PL_OCTAVE));
                }
                if (sprQ) {
                    // QMIX spread on its OWN axis (SB_SANDS_Q): applyMono lane = PL_QMIX.
                    engine.pe.qmixRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_QMIX, i,
                        engine.spreadE(0, SequencerEngine::PL_QMIX));
                }
            }
            v1SpreadCapture_(sprR, sprM, sprQ, sigSpM);
            }  // end GATE 2 dirty check (perf round 2)
            }  // end if(sprR || sprM || sprQ)
        //}
        // Drive all 6 mono strands via the single-source-of-truth lane map.
        for (int l = 0; l < dotModular::SandsGrid::MONO_LANES; ++l) readStrand(l);

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
            Monsoon* mmV1 = eastHost;   // PERF: walk memo (eastV1 == cachedEastSandsVisual — the memo's argument)
            const bool macroHere = hasMacro && (macroVis != nullptr);
            auto monoOwnedByMacro = [&](int lane)->bool {
                // MVC step 1d: owner is STORE-BACKED (editor.macroOwn via getMacroOwn at kMonoMacroOwnRow).
                // ==0 vs <=0.5: identical on the writable domain {0.0, 1.0} (all writers are 0/1; see ZAI_PROMPT_ownership_boolean.md §3)
                return macroHere && !(mmV1 && mmV1->ownerIsEast(Monsoon::kMonoMacroOwnRow, lane));
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
                    // SCOPE (LOCK_SCOPE_MENU): MELODY/OCTAVE strands = melody axis; else rhythm.
                    // q-mix is its OWN axis (SB_SANDS_Q), not rhythm (fixes the split-brain vs spread).
                    const bool lorMelAxis = (el == dotModular::STRAND_MELODY || el == dotModular::STRAND_OCTAVE);
                    const bool lorLive = (el == dotModular::STRAND_QMIX)
                        ? (!engine.locked || (engine.scopeLiveMask & (1u << 13)) != 0)   // == dotModular::SB_SANDS_Q
                        : dotModular::LockManager::liveNow(dotModular::Control::Lor, engine.locked, engine.scopeLiveMask, lorMelAxis);
                    if (monoOwnedByMacro(el)) {
                        if (lorLive)   // LOR LATCH: skip re-push under lock
                        engine.setStrand(StrandWriter::EAST, el,
                            (int)std::round(macroVis->macroBase[el][0] + macroVis->macroCVDelta[el][0]),
                            (int)std::round(macroVis->macroBase[el][1] + macroVis->macroCVDelta[el][1]),
                            (int)std::round(macroVis->macroBase[el][2] + macroVis->macroCVDelta[el][2]));
                        continue;
                    }
                    const int b0 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, el, 0)) : 16;
                    const int b1 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, el, 1)) : 0;
                    const int b2 = mmV1 ? (int)std::round(mmV1->getLorBase(kMono, el, 2)) : 0;
                    const float len = (float)std::max(1, b0);
                    const float off = (float)(((b1 % 16) + 16) % 16);
                    const float rot = (float)(((b2 % 16) + 16) % 16);
                    auto sendBlend = [&](int item)->float {
                        if (!macroVis) return 0.f;   // match the widget's guard exactly
                        float send = mmV1 ? mmV1->getMacroSend(kMono, el, item) : 0.f;
                        // P9b: V1's own tap selects between the published endpoints.
                        const float tap = (mmV1 ? mmV1->getMacroTap(kMono, el, (item == 3 ? 1 : 0)) : 1.f);
                        return macroVis->tappedDelta(tap, el, item) * send;
                    };
                    auto addCV = [&](float base, int item, float lo, float hi)->float {
                        if (eastV1->inputs[East::cvId(el,item)].isConnected()) {
                            float att = mmV1 ? mmV1->getMacroAtten(kMono, el*4 + item) : 0.f;
                            float cv  = eastV1->inputs[East::cvId(el,item)].getPolyVoltage(0) / 10.f;  // ch0 = V1
                            base += cv * att * (hi - lo);
                        }
                        return rack::math::clamp(base + sendBlend(item), lo, hi);
                    };
                    if (lorLive)   // LOR LATCH: skip re-push under lock (q-mix on SB_SANDS_Q)
                    engine.setStrand(StrandWriter::EAST, el,
                        (int)std::round(addCV(len, 0, 1.f, 16.f)),
                        (int)std::round(addCV(off, 1, 0.f, 15.f)),
                        (int)std::round(addCV(rot, 2, 0.f, 15.f)));
                }
                // VAR/LEG (editor 5/6) now flow through the SAME core loop above (POLY_LANES=7),
                // via cvId(5/6,item) + getMacroAtten(kMono, 5/6*4+item) + getLorBase(kMono, 5/6,·).
                // The separate varleg V1 LOR block is RETIRED (sands consolidation Step 3): one
                // CV/atten system, no varlegCvId/varlegAtten. VAR/LEG are never Macro-owned
                // (monoOwnedByMacro returns false for lanes 5/6), so the core loop's Macro branch
                // is skipped and the base+CV path runs — matching the old block's behaviour exactly
                // (sendBlend is 0 for VAR/LEG, so the core loop's clamp(base+sendBlend) == clamp(base)).
            }

            // ── SPREAD: lock-gated (frozen pattern must not be re-spread). LOR above already ran.
            // Scope (LOCK_SCOPE_MENU): PER-AXIS. axR gates rhythm arrays (rhythm/accent/legato/var),
            // axM the melody arrays (melody/octave). Note spR/spM/spO/spA below are spread VALUES per
            // lane (not axis flags) — distinct from these axis bools.
            const bool axR = dotModular::LockManager::liveNow(dotModular::Control::Spread, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/false);
            const bool axM = dotModular::LockManager::liveNow(dotModular::Control::Spread, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/true);
            const bool axQ = !engine.locked || (engine.scopeLiveMask & (1u << 13)) != 0;   // == dotModular::SB_SANDS_Q (q-mix own axis)
            if (axR || axM || axQ) {
            auto sprForLane = [&](int lane)->float {
                // CC_PROMPT_UI_TO_RESOLVER_SPEC step 1: the delegation rides the ONE
                // skeleton — SpreadResolver (SandsModifierResolver<SpreadPolicy>) — the
                // same authority combineSpread (poly) and publishGlobal (Macro-own) use,
                // and the resolved amount is PUBLISHED to the engine spread store
                // (spreadERef(0, lane)): the ONE owner-resolved V1 spread, read by BOTH
                // the audio pass below and SequencerEngine::effectiveProbability's
                // Owner lens (the display/out twin) — no second resolution anywhere.
                // BIT-IDENTICAL to the old inline shape by expression order (the
                // combineSpread note applies verbatim; the delegated branch NEUTRALIZES
                // the tap — the old delegated delta was the POST macroCVDelta directly).
                const bool delegated = monoOwnedByMacro(lane);
                redDot::SpreadResolver::Inputs sin;
                sin.delegated    = delegated;
                sin.macroPresent = macroHere;
                if (delegated) {
                    sin.macroBase      = macroVis->macroBase[lane][3];
                    sin.macroPreDelta  = macroVis->macroCVDelta[lane][3];   // NEUTRALIZE the tap: pre == post
                    sin.macroPostDelta = macroVis->macroCVDelta[lane][3];   // → the lerp yields macroCVDelta bit-exactly
                } else {
                    sin.base = mmV1 ? mmV1->getSpread(dotModular::VoiceResolver::kMonoSlot, lane)
                                    : 0.f;   // MVC step 1d: SPREAD_* param gone; no Monsoon → no spread
                    if (eastV1->inputs[East::cvId(lane,3)].isConnected()) {
                        sin.ownCv.connected   = true;
                        sin.ownCv.unitVoltage = eastV1->inputs[East::cvId(lane,3)].getPolyVoltage(0) / 10.f;  // ch0 = V1
                        sin.ownCv.atten       = (mmV1 ? mmV1->getMacroAtten(dotModular::VoiceResolver::kMonoSlot, lane*4 + 3) : 0.f);
                    }
                    // Macro send blend on East-owned V1 spread (mirrors poly combineSpread):
                    // the per-voice tapped delta (V1's own tap, P9b) × mono-slot send.
                    if (macroHere) {
                        auto* mm = macroHost;   // PERF: walk memo (was a per-lane re-walk inside sprForLane)
                        sin.macroSend      = (mm ? mm->getMacroSend(dotModular::VoiceResolver::kMonoSlot, lane, 3) : 0.f);
                        sin.macroPreDelta  = macroVis->macroPreDelta[lane][3];
                        sin.macroPostDelta = macroVis->macroPostDelta[lane][3];
                        sin.macroTap       = (mm ? mm->getMacroTap(dotModular::VoiceResolver::kMonoSlot, lane, 1) : 1.f);
                    }
                }
                const float sp = redDot::SpreadResolver::effective(sin);
                engine.spreadERef(0, lane) = sp;   // publish: the ONE owner-resolved V1 amount
                return sp;
            };
            // LANE UNIFICATION (Phase 2): sprForLane fetches UNIFIED stores (editor.spread,
            // cvId jacks, atten/send banks) — pass PL_* so each axis reads its own lane.
            // (The old raw 0..6 literals read the dead spread order: rhythm got MEL's knob.)
            const float spR = sprForLane(SequencerEngine::PL_REST);       // rhythm axis
            const float spM = sprForLane(SequencerEngine::PL_MELODY);     // melody axis
            const float spO = sprForLane(SequencerEngine::PL_OCTAVE);     // melody axis
            const float spA = sprForLane(SequencerEngine::PL_ACCENT);     // rhythm axis
            const float spQ = sprForLane(SequencerEngine::PL_QMIX);       // own axis (SB_SANDS_Q)
            const float spV = sprForLane(SequencerEngine::PL_VARIATION);  // rhythm axis
            const float spL = sprForLane(SequencerEngine::PL_LEGATO);     // rhythm axis
            engine.pe.setSandsActive(true);
            // V1/mono spread: applyMono reads the mode from pe.spreadTargetMode[lane] and
            // handles both original + target selection internally. No Monsoon pointer needed.
            // GATE 2 (perf round 2): the Mono-branch gate's East twin — see the sig comment
            // in MonsoonSandsManager.hpp (inputs + row-as-last-written; ONE shared sig, so a
            // Mono↔East branch switch is just an input change). Unified PL order.
            const float sigSpE[7] = { spM, spO, spQ, spR, spA, spV, spL };
            if (v1SpreadDirty_(axR, axM, axQ, sigSpE)) {
            for (int i = 0; i < 16; ++i) {
                if (axR) {
                    engine.pe.rhythmRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_REST, i, spR);
                    engine.pe.accentRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_ACCENT, i, spA);
                    // VAR/LEG spread — applyMono, not the raw slewed copy (the raw copy
                    // dropped the spread value entirely; under follow-CA this is meaningful, and it
                    // keeps VAR/LEG on the same path as the 5 core lanes).
                    engine.pe.variationRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_VARIATION, i, spV);
                    engine.pe.legatoRandom[i]    = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_LEGATO, i, spL);
                }
                if (axM) {
                    engine.pe.melodyRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_MELODY, i, spM);
                    engine.pe.octaveRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_OCTAVE, i, spO);
                }
                if (axQ) {
                    // QMIX spread on its OWN axis (SB_SANDS_Q).
                    engine.pe.qmixRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_QMIX, i, spQ);
                }
            }
            v1SpreadCapture_(axR, axM, axQ, sigSpE);
            }   // end GATE 2 dirty check (perf round 2)
            }   // end if (axR || axM) — spread only; V1 LOR above runs under lock too
        }
    }

    // ── Macro global LOR with CV (poly: per-lane, SAME for every voice) ────
    // Macro publishes its OWN display data (macroBase / macroCVDelta / macroPre/PostDelta /
    // spreadEffective) whenever a Macro is present — this is what Sands Helix renders, and it must
    // work standalone (no Straits / no poly output). The output-application (blend into the poly
    // voices) is separately gated on macroDrivesOutput below. (Regression from the Straits refactor:
    // publishGlobal used to be gated on macroDrivesOutput, so with no Straits attached Sands Helix's
    // macroBase stayed zero and the module looked completely inert.)
    // RATE MANAGER (S4): the publish itself — the publishGlobal lambda + its POLY_LANES
    // loop — is now a Control-rate PRODUCER (publishGlobalProduce_ below, declared via
    // declareRateProducers), dispatched at the sync position with a skip-on-unchanged
    // input+produced-cells fingerprint. It runs BEFORE the sync-population producer
    // (declaration order), which reads the macroBase it writes; every earlier reader in
    // this function already read LAST tick's published values, so nothing changes for
    // them. The standalone output-application block below stays INLINE (it is output
    // application, not publish — and entangled with the warp-mirror refresh ordering).
    if (hasMacro && macroVis) {
        // Output application: only when Macro actually drives real output voices (Straits attached
        // with poly active). When macroDrivesOutput, East's per-voice sync (runs after) combines
        // Macro's base+delta into each voice via the owner/blend send. When NOT — standalone Macro
        // (no Straits) — apply Macro's GLOBAL spread to the drawn voices here so its bars aren't dead.
        if (!macroDrivesOutput) {
        // Spread scope (LOCK_SCOPE_MENU): PER-AXIS. mSpR gates rhythm arrays, mSpM the melody arrays,
        // for BOTH the mono final arrays and the per-voice poly arrays below.
        const bool mSpR = dotModular::LockManager::liveNow(dotModular::Control::Spread, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/false);
        const bool mSpM = dotModular::LockManager::liveNow(dotModular::Control::Spread, engine.locked, engine.scopeLiveMask, /*melodyAxis=*/true);
        if (mSpR || mSpM) {
            const int nPoly = rack::math::clamp(engine.numPolyVoices, 0, 15);
            // Global spread level per lane (knob + CV), unified PL_* order.
            float spv[dotModular::SandsGrid::POLY_LANES];
            for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
                // Standalone Macro global spread — own CV only (same as publishGlobal), via the resolver.
                redDot::SpreadResolver::Inputs ssin;
                ssin.base = (gMon ? gMon->getGlobalSpread(lane) : 0.f);
                if (macroVis->inputs[Macro::macroCvId(lane,3)].isConnected()) {
                    ssin.ownCv.connected   = true;
                    ssin.ownCv.unitVoltage = macroVis->inputs[Macro::macroCvId(lane,3)].getVoltage() / 10.f;
                    ssin.ownCv.atten       = (gMon ? gMon->getGlobalAtten(lane, 3) : 0.f);
                }
                float sp = redDot::SpreadResolver::effective(ssin);
                macroVis->spreadEffective[lane] = sp;
                spv[lane] = sp;
                // UI-to-resolver step 1: standalone Macro owns EVERYTHING (no East) —
                // publish the owner-resolved V1 amount to the engine spread store so
                // effectiveProbability's Owner lens reads what this pass applies.
                engine.spreadERef(0, lane) = sp;
            }
            engine.pe.setSandsActive(true);
            // V1 (mono final arrays): converge the mono draw toward the ensemble.
            for (int i = 0; i < 16; ++i) {
                if (mSpR) {
                    // LANE UNIFICATION (Phase 2): spv is indexed by the unified PL_* order
                    // (the fetch loop reads getGlobalSpread from the unified store) — each
                    // axis reads its OWN lane's value.
                    engine.pe.rhythmRandom[i]    = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_REST, i, spv[SequencerEngine::PL_REST]);
                    engine.pe.accentRandom[i]    = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_ACCENT, i, spv[SequencerEngine::PL_ACCENT]);
                    // VAR/LEG spread: applyMono, not the raw slewed copy.
                    engine.pe.variationRandom[i] = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_VARIATION, i, spv[SequencerEngine::PL_VARIATION]);
                    engine.pe.legatoRandom[i]    = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_LEGATO, i, spv[SequencerEngine::PL_LEGATO]);
                }
                if (mSpM) {
                    engine.pe.melodyRandom[i]    = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_MELODY, i, spv[SequencerEngine::PL_MELODY]);
                    engine.pe.octaveRandom[i]    = redDot::SpreadInterp::applyMono(
                        engine.pe, SequencerEngine::PL_OCTAVE, i, spv[SequencerEngine::PL_OCTAVE]);
                }
            }
            // Stage 2a: cache poly spread values for step-rate refresh (was: full-field
            // applyPoly loop nPoly x 5 lanes x 16 steps). Mono arrays above are applyMono
            // (16 cells, cheap) — left as-is.
            engine.pe.cachedSpreadLiveR = mSpR;
            engine.pe.cachedSpreadLiveM = mSpM;
            engine.pe.cachedSpreadLiveQ = false;   // standalone Macro: no QMIX axis here (no East)
            engine.pe.cachedSpreadInitialized = true;   // Stage 2a: enable step-rate refresh
            for (int v = 1; v <= nPoly; ++v) {
                engine.pe.cachedPolySpread[v][SequencerEngine::PL_REST]   = spv[SequencerEngine::PL_REST];
                engine.pe.cachedPolySpread[v][SequencerEngine::PL_ACCENT] = spv[SequencerEngine::PL_ACCENT];
                engine.pe.cachedPolySpread[v][SequencerEngine::PL_MELODY] = spv[SequencerEngine::PL_MELODY];
                engine.pe.cachedPolySpread[v][SequencerEngine::PL_OCTAVE] = spv[SequencerEngine::PL_OCTAVE];
            }
        }   // if (mSpR || mSpM)
        }   // if (!macroDrivesOutput)
    }       // if (hasMacro && macroVis)

    // Warp mirror (CC_PROMPT_03): the spread passes above rewrote the MONO row of
    // random_ — refresh that row's mirror cells (voice-scoped, 112 cells ≈ 0.004ms,
    // per control block). Poly rows are step-rate refreshed via refreshPolyRandomCell
    // (which carries its own cell-scope rebuild).
    // GATE 3 (perf round 2): the mono-row rebuild is a pure function of
    // (random_[0], warpDelegated_[0], warpBank_) — warpMixCell reads exactly those
    // three. Fingerprint them; when bit-identical to the last REBUILD's inputs, the
    // rebuild would rewrite identical cells, so skip. Every other path that changes
    // them carries its own rebuild (rolls/pin remaps → rebuildWarpMirrorAll inside
    // remapSlewedByPins; bank re-wire → setWarpBank's rebuildAll; curve/w edits → the
    // scoped dirty-mask rebuilds; delegation flips → GATE 1's rebuildWarpMirrorAll) —
    // and any writer the map missed still trips this fingerprint, because the
    // comparison is against LIVE state, not a schedule.
    {
        auto& pe = engine.pe;
        if (!lastMirrorSig_.valid
            || pe.warpBank_ != lastMirrorSig_.bank
            || 0 != std::memcmp(lastMirrorSig_.row, pe.random_[0], sizeof lastMirrorSig_.row)
            || 0 != std::memcmp(lastMirrorSig_.deleg, pe.warpDelegated_[0], sizeof lastMirrorSig_.deleg)) {
            pe.rebuildWarpMirrorVoice(0);
            lastMirrorSig_.valid = true;
            lastMirrorSig_.bank = pe.warpBank_;
            std::memcpy(lastMirrorSig_.row, pe.random_[0], sizeof lastMirrorSig_.row);
            std::memcpy(lastMirrorSig_.deleg, pe.warpDelegated_[0], sizeof lastMirrorSig_.deleg);
        }
    }

    // Note: Poly DNA windows handled in Monsoon::process controlDivider block.
}

// ── RATE MANAGER (S4): publishGlobal — the SECOND Control-rate producer ───────
// The VERBATIM extraction of the publish loop (same writes, same order); the guard
// is the reachability recomputed (hasMacro && macroVis). Declared BEFORE the
// sync-population producer (Monsoon's ctor order) — dispatch is in-order and the
// population reads the macroBase this publishes.
void MonsoonSandsManager::publishGlobalProduce_(Monsoon& ownerMon) {
    auto& expanderManager = ownerMon.expanderManager;
    auto* macroVis = expanderManager.cachedMacroSandsVisual;
    if (!macroVis) return;   // the reachability guard (hasMacro && macroVis)
    // gMon — the store owner, recomputed exactly as processDNA's head resolves it
    // (the memoized walk first, the owner back-pointer as the null-walk rescue; the
    // produce only runs with macroVis present).
    Monsoon* gMon = redDot::lookupMonsoon(macroVis);
    if (!gMon && expanderManager.owner) gMon = expanderManager.owner;
    // (applyMacroCV copied VERBATIM from processDNA's local helper — same reads: the
    //  Macro CV jacks + gMon's global attens. Both are fingerprint fields below.)
    auto applyMacroCV = [&](float base, int lane, int param, float lo, float hi) -> float {
        if (!macroVis || !macroVis->inputs[Macro::macroCvId(lane,param)].isConnected()) return base;
        float cv  = macroVis->inputs[Macro::macroCvId(lane,param)].getVoltage() / 10.f;
        float att = (gMon ? gMon->getGlobalAtten(lane, param) : 0.f);
        return clamp(base + cv * att * (hi - lo), lo, hi);
    };
    auto publishGlobal = [&](int lane) {
        // bases (knob, no CV)
        float baseLen = gMon ? gMon->getGlobalLor(lane,0) : 0.f;
        float baseOff = gMon ? gMon->getGlobalLor(lane,1) : 0.f;
        float baseRot = gMon ? gMon->getGlobalLor(lane,2) : 0.f;
        float baseSpr = gMon ? gMon->getGlobalSpread(lane) : 0.f;
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
            msin.ownCv.atten       = (gMon ? gMon->getGlobalAtten(lane, 3) : 0.f);
        }
        float cvSpr = redDot::SpreadResolver::effective(msin);
        // UI-to-resolver step 1: publish Macro's OWN effective spread to the ENGINE
        // store — the MacroOwn lens's amount for SequencerEngine::
        // effectiveProbability. Bit-identical to the module's old publish
        // (clamp(macroBase[l][3] + macroPostDelta[l][3]): cvSpr IS that resolver
        // output — base + (cv·att)·2, end-clamped, same expression order). The
        // module's /32-gated publish is deleted with its sweep (step 3).
        engine.macroOwnSpread[lane] = cvSpr;
        // P9b PER-VOICE send-tap: the publish now carries BOTH endpoints of the pre/post
        // lerp; each receiving VOICE selects via its own macroTap (the send is per-voice,
        // so the tap is too — the old single globally-tapped macroSendDelta forced every
        // voice onto one tap). pre = raw cv·(hi-lo) (tap=0); post = cv·att·(hi-lo)
        // (tap=1, == the old tappedDelta at POST). macroSendDelta (still written below)
        // dies once every reader migrates.
        auto endpointDeltas = [&](int item, float lo, float hi, float& pre, float& post) {
            pre = post = 0.f;
            if (!macroVis || !macroVis->inputs[Macro::macroCvId(lane,item)].isConnected()) return;
            const float cv  = macroVis->inputs[Macro::macroCvId(lane,item)].getVoltage() / 10.f;
            const float att = (gMon ? gMon->getGlobalAtten(lane, item) : 0.f);
            pre  = cv * (hi - lo);
            post = cv * att * (hi - lo);
        };
        float preD[4], postD[4];
        endpointDeltas(0, 1.f, 16.f, preD[0], postD[0]);
        endpointDeltas(1, 0.f, 15.f, preD[1], postD[1]);
        endpointDeltas(2, 0.f, 15.f, preD[2], postD[2]);
        endpointDeltas(3, -1.f, 1.f,  preD[3], postD[3]);
        for (int item = 0; item < 4; ++item) {
            macroVis->macroPreDelta[lane][item]  = preD[item];
            macroVis->macroPostDelta[lane][item] = postD[item];
        }
        // macroCVDelta = true POST delta (Macro's own display). The per-voice
        // endpoints (preD/postD above) are what the sends carry now; the old single
        // globally-tapped macroSendDelta is DELETED (every reader migrated).
        // item 0=LEN 1=OFF 2=ROT 3=SPR.
        macroVis->macroBase[lane][0] = baseLen;  macroVis->macroCVDelta[lane][0] = cvLen - baseLen;
        macroVis->macroBase[lane][1] = baseOff;  macroVis->macroCVDelta[lane][1] = cvOff - baseOff;
        macroVis->macroBase[lane][2] = baseRot;  macroVis->macroCVDelta[lane][2] = cvRot - baseRot;
        macroVis->macroBase[lane][3] = baseSpr;  macroVis->macroCVDelta[lane][3] = cvSpr - baseSpr;
        // spread display reads the CV-applied global spread (base+CV, no blend)
        macroVis->spreadEffective[lane] = cvSpr;
    };
    // Publish EVERY poly lane's global LOR+spread, incl. QMIX (PL_QMIX=4). The manual unroll
    // stopped at 3 (accent), so q-mix's macroBase[4] stayed zero → Macro's q-mix LOR knob and
    // spread had no effect on the observed probability. Loop over POLY_LANES so it can't drop a
    // lane again (matches the OUTPUT-application loops below at PL_LANES).
    for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane)
        publishGlobal(lane);   // unified PL_* order (0=MEL, 1=OCT, 2=QMIX, 3=REST, 4=ACC, 5=VAR, 6=LEG)
}

// The input + produced-cells image (the S0b pattern): every input the publish reads
// (the global LOR/spread/atten stores on the owner, the Macro CV jacks, presence) plus
// the published cells (macroBase/CVDelta/preD/postD/spreadEffective on the visual +
// macroOwnSpread on the engine) — self-healing against foreign writers (UI store edits
// on the visual land in the image and re-publish).
size_t MonsoonSandsManager::publishGlobalFingerprint_(Monsoon& ownerMon, void* dst) {
    auto& expanderManager = ownerMon.expanderManager;
    auto* macroVis = expanderManager.cachedMacroSandsVisual;
    Monsoon* gMon = macroVis ? redDot::lookupMonsoon(macroVis) : nullptr;
    if (!gMon && macroVis && expanderManager.owner) gMon = expanderManager.owner;
    struct __attribute__((packed)) Image {
        uint64_t macroVis, gMon;
        float    lor[7][3];        // getGlobalLor(lane, 0..2) — the LOR knobs
        float    spr[7];           // getGlobalSpread(lane)
        float    atten[7][4];      // getGlobalAtten(lane, item) — the CV depths
        uint8_t  cvConnected[7][4];
        float    cvVoltage[7][4];  // macroCvId(lane,item): connected + voltage
        // produced cells (self-healing)
        float    macroBase[7][4];
        float    macroCVDelta[7][4];
        float    macroPreDelta[7][4];
        float    macroPostDelta[7][4];
        float    spreadEffective[7];
        float    macroOwnSpread[7];
    };
    Image img{};
    img.macroVis = (uint64_t)(uintptr_t)macroVis;
    img.gMon     = (uint64_t)(uintptr_t)gMon;
    if (gMon)
        for (int l = 0; l < 7; ++l) {
            for (int i = 0; i < 3; ++i) img.lor[l][i] = gMon->getGlobalLor(l, i);
            img.spr[l] = gMon->getGlobalSpread(l);
            for (int i = 0; i < 4; ++i) img.atten[l][i] = gMon->getGlobalAtten(l, i);
        }
    if (macroVis) {
        for (int l = 0; l < 7; ++l)
            for (int i = 0; i < 4; ++i) {
                auto& in = macroVis->inputs[Macro::macroCvId(l, i)];   // Port's readers are non-const
                img.cvConnected[l][i] = in.isConnected() ? 1 : 0;
                img.cvVoltage[l][i]   = in.isConnected() ? in.getVoltage() : 0.f;
            }
        std::memcpy(img.macroBase,      macroVis->macroBase,      sizeof img.macroBase);
        std::memcpy(img.macroCVDelta,   macroVis->macroCVDelta,   sizeof img.macroCVDelta);
        std::memcpy(img.macroPreDelta,  macroVis->macroPreDelta,  sizeof img.macroPreDelta);
        std::memcpy(img.macroPostDelta, macroVis->macroPostDelta, sizeof img.macroPostDelta);
        std::memcpy(img.spreadEffective, macroVis->spreadEffective, sizeof img.spreadEffective);
    }
    std::memcpy(img.macroOwnSpread, engine.macroOwnSpread, sizeof img.macroOwnSpread);
    std::memcpy(dst, &img, sizeof img);
    return sizeof img;
}

void MonsoonSandsManager::declareRateProducers(redDot::RateManager& rm, Monsoon& ownerMon) {
    rm.declareProducer(
        redDot::Rate::Control,
        [this, &ownerMon](void* dst) { return publishGlobalFingerprint_(ownerMon, dst); },
        [this, &ownerMon]()           { publishGlobalProduce_(ownerMon); });
}

// ──── Scramble Operations ────────────────────────────────────────────────

// ──── Reset Operations ───────────────────────────────────────────────────

// ──── Helper ─────────────────────────────────────────────────────────────

// --- CA pin-remap change guard (flicker fix) ---------------------------------------------------
// The remapped slewed buffers are a pure function of (pins, draw counters, mix, slew). Re-run the
// forceRecomputeSlewed()+remapSlewedByPins() pair only when that signature changes; otherwise the
// buffers from last cycle are already correct and re-running just churns them under the UI reader.
//
// SLEW IS INTENTIONALLY EXCLUDED from this check (phrase-boundary slew consumption). Slew is a
// distributional parameter that must be constant across a phrase; latchSlew latches it every
// control-rate cycle (unconditionally — never behind the ABMix gate), but the slewed buffers
// are only re-derived at the ROLL (phrase boundary),
// where redrawRhythm/Melody/Qmix advance the draw counter. The draw-counter checks below detect
// that roll and re-fire the remap with the new slewed content. Mid-phrase slew changes do NOT
// re-derive the slewed buffers (latchSlew + refreshVisualCache skip recompute on slew-only change),
// so checking slew here would fire forceRecomputeSlewed() at control rate on every slew modulation
// tick — the CPU spike. The counter advance at the roll is the correct and sufficient trigger.
bool MonsoonSandsManager::remapSigChanged_(const uint8_t* rSrc, const uint8_t* mSrc, const uint8_t* qSrc, bool identity) const {
    auto& pe = engine.pe;
    if (identity != lastRemap_.wasIdentity)               return true;
    if (pe.rhythmDrawCtr != lastRemap_.rCtr)              return true;
    if (pe.melodyDrawCtr != lastRemap_.mCtr)              return true;
    if (pe.qmixDrawCtr   != lastRemap_.qCtr)              return true;
    // MIX is NOT checked (phrase-boundary consumption — same as slew). The draw-counter
    // checks above detect when the slewed buffers are re-derived at the roll (phrase
    // boundary), which is when the remap should re-fire. Mid-phrase mix changes do NOT
    // re-derive the slewed buffers (latchMix + refreshVisualCache skip recompute on mix
    // change), so checking mix here would fire forceRecomputeSlewed() at control rate on
    // every mix modulation tick — the CPU spike.
    for (int v = 0; v < 16; ++v)
        if (rSrc[v] != lastRemap_.rSrc[v] || mSrc[v] != lastRemap_.mSrc[v] || qSrc[v] != lastRemap_.qSrc[v]) return true;
    return false;
}

void MonsoonSandsManager::captureRemapSig_(const uint8_t* rSrc, const uint8_t* mSrc, const uint8_t* qSrc, bool identity) {
    auto& pe = engine.pe;
    for (int v = 0; v < 16; ++v) { lastRemap_.rSrc[v] = rSrc[v]; lastRemap_.mSrc[v] = mSrc[v]; lastRemap_.qSrc[v] = qSrc[v]; }
    lastRemap_.rCtr = pe.rhythmDrawCtr;  lastRemap_.mCtr = pe.melodyDrawCtr;  lastRemap_.qCtr = pe.qmixDrawCtr;
    // Mix + Slew are NOT captured (phrase-boundary consumption — see remapSigChanged_ comment).
    lastRemap_.wasIdentity = identity;
}

// ── GATE 2 helpers (perf round 2 — the V1 spread-pass fingerprint; see the sig
// comment in MonsoonSandsManager.hpp) ──────────────────────────────────────────────
// The dirty check compares against LIVE engine state (not a schedule), so any writer
// the header comment didn't enumerate still trips it — the skip is only ever taken
// when the pass would rewrite bit-identical cells.
bool MonsoonSandsManager::v1SpreadDirty_(bool axR, bool axM, bool axQ, const float sp[7]) {
    const auto& s = lastV1Spread_;
    const auto& pe = engine.pe;
    if (!s.valid) return true;
    if (s.axR != axR || s.axM != axM || s.axQ != axQ) return true;
    if (std::memcmp(s.sp, sp, sizeof s.sp) != 0) return true;
    if (std::memcmp(s.mode, pe.spreadTargetMode, sizeof s.mode) != 0) return true;
    // applyMono's pe inputs: the mono (V1) row of each strand family's slewed and
    // pre-remap buffers. (The REDDOT_SPREAD_ASSERT path also reads pe.ca*Src, but it is
    // compiled out wherever this TU ships — the macro defaults 0 and the test rig does
    // not compile MonsoonSandsManager.)
    if (std::memcmp(s.swRhythm,    pe.slewedRhythm[0],        sizeof s.swRhythm)    != 0
     || std::memcmp(s.swVariation, pe.slewedVariation[0],     sizeof s.swVariation) != 0
     || std::memcmp(s.swLegato,    pe.slewedLegato[0],        sizeof s.swLegato)    != 0
     || std::memcmp(s.swAccent,    pe.slewedAccent[0],        sizeof s.swAccent)    != 0
     || std::memcmp(s.swMelody,    pe.slewedMelody[0],        sizeof s.swMelody)    != 0
     || std::memcmp(s.swOctave,    pe.slewedOctave[0],        sizeof s.swOctave)    != 0
     || std::memcmp(s.swQmix,      pe.slewedQmix[0],          sizeof s.swQmix)      != 0) return true;
    if (std::memcmp(s.prRhythm,    pe.preRemapSlewedRhythm[0],    sizeof s.prRhythm)    != 0
     || std::memcmp(s.prVariation, pe.preRemapSlewedVariation[0], sizeof s.prVariation) != 0
     || std::memcmp(s.prLegato,    pe.preRemapSlewedLegato[0],    sizeof s.prLegato)    != 0
     || std::memcmp(s.prAccent,    pe.preRemapSlewedAccent[0],    sizeof s.prAccent)    != 0
     || std::memcmp(s.prMelody,    pe.preRemapSlewedMelody[0],    sizeof s.prMelody)    != 0
     || std::memcmp(s.prOctave,    pe.preRemapSlewedOctave[0],    sizeof s.prOctave)    != 0
     || std::memcmp(s.prQmix,      pe.preRemapSlewedQmix[0],      sizeof s.prQmix)      != 0) return true;
    // The row as last written: an external writer (pin-remap promote, rotate, reset)
    // breaks this and the pass re-runs — the healing property of the unconditional
    // every-tick rewrite is preserved exactly.
    if (std::memcmp(s.row, pe.random_[0], sizeof s.row) != 0) return true;
    return false;
}

void MonsoonSandsManager::v1SpreadCapture_(bool axR, bool axM, bool axQ, const float sp[7]) {
    auto& s = lastV1Spread_;
    const auto& pe = engine.pe;
    s.valid = true;
    s.axR = axR; s.axM = axM; s.axQ = axQ;
    std::memcpy(s.sp, sp, sizeof s.sp);
    std::memcpy(s.mode, pe.spreadTargetMode, sizeof s.mode);
    std::memcpy(s.swRhythm,    pe.slewedRhythm[0],        sizeof s.swRhythm);
    std::memcpy(s.swVariation, pe.slewedVariation[0],     sizeof s.swVariation);
    std::memcpy(s.swLegato,    pe.slewedLegato[0],        sizeof s.swLegato);
    std::memcpy(s.swAccent,    pe.slewedAccent[0],        sizeof s.swAccent);
    std::memcpy(s.swMelody,    pe.slewedMelody[0],        sizeof s.swMelody);
    std::memcpy(s.swOctave,    pe.slewedOctave[0],        sizeof s.swOctave);
    std::memcpy(s.swQmix,      pe.slewedQmix[0],          sizeof s.swQmix);
    std::memcpy(s.prRhythm,    pe.preRemapSlewedRhythm[0],    sizeof s.prRhythm);
    std::memcpy(s.prVariation, pe.preRemapSlewedVariation[0], sizeof s.prVariation);
    std::memcpy(s.prLegato,    pe.preRemapSlewedLegato[0],    sizeof s.prLegato);
    std::memcpy(s.prAccent,    pe.preRemapSlewedAccent[0],    sizeof s.prAccent);
    std::memcpy(s.prMelody,    pe.preRemapSlewedMelody[0],    sizeof s.prMelody);
    std::memcpy(s.prOctave,    pe.preRemapSlewedOctave[0],    sizeof s.prOctave);
    std::memcpy(s.prQmix,      pe.preRemapSlewedQmix[0],      sizeof s.prQmix);
    std::memcpy(s.row, pe.random_[0], sizeof s.row);
}
