#pragma once
#include <rack.hpp>
#include <cmath>
#include <cassert>

// Spread contract assertion (SPREAD_TARGET_MODES.md POST-MORTEM): under follow-CA with a voice
// actually pinned, own (pre-remap) must differ from target (post-remap). OFF by default because
// there is a legitimate transient where they coincide for a block: a dice roll recomputes the
// slewed (target) buffers but does NOT re-run remapSlewedByPins (that is gated on PIN changes,
// not dice), so for one control cycle the freshly-recomputed target can momentarily equal the
// stale pre-remap own before the next remap re-applies the pin. That transient is not an output
// bug, but it WOULD trip a hot-path assert and crash Rack. So the contract is checked in TESTS
// (where the remap is driven deterministically), not on the live audio thread. Define
// REDDOT_SPREAD_CONTRACT_ASSERT=1 in a test to enable it.
#ifndef REDDOT_SPREAD_CONTRACT_ASSERT
#define REDDOT_SPREAD_CONTRACT_ASSERT 0
#endif
#if REDDOT_SPREAD_CONTRACT_ASSERT
#define REDDOT_SPREAD_ASSERT(cond, msg) assert((cond) && (msg))
#else
#define REDDOT_SPREAD_ASSERT(cond, msg) ((void)0)
#endif
#include "engines/PatternEngine.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// SpreadInterp — the SINGLE definition of spread interpolation, shared by the
// sequencer path (MonsoonSandsManager mono/macro + MonsoonExpanderManager East
// per-voice) and the visual display (SpreadManager). Previously these were three
// independent implementations that disagreed (different target buffers, mono
// in/out of the average, and the mode toggle was ignored by the sequencer). This
// is now the one place the behaviour is defined.
//
// Canonical definition (agreed with project owner):
//   • Operates on the PRE-spread slewed draws (slewedRhythm / slewedPolyRhythm,
//     i.e. A + mix·(B−A)). Spread and mix are both affine, so applying spread to
//     the post-mix slewed draw is equivalent to mixing spread-applied A/B.
//   • Lane index here is the SPREAD lane: 0=REST(rhythm) 1=MELODY 2=OCTAVE.
//   • Target modes:
//   TARGET: always the mono (voice-1) slewed draw. A voice targeting itself is a
//   no-op, so the mono strand is a fixed anchor — the desired behaviour.
//   (The former AVERAGE_POLY target was REMOVED: the per-step mean of N iid uniform
//   draws concentrates at 0.5, so full spread collapsed every voice AND every step to
//   ~0.5 — mush. Full spread toward mono gives UNISON, a real musical destination.
//   Voice-1 primacy already holds elsewhere: VAR/LEG borrow mono. See §2a.)
//   • Bipolar spread: amount>0 → toward target; amount<0 → toward (1−target) by
//     |amount|; ==0 → unchanged. Result clamped 0..1.
// ─────────────────────────────────────────────────────────────────────────────

namespace redDot {

struct SpreadInterp {
    // Pointers to the lane's slewed buffers for the engine. Set per lane by the
    // caller so the same code serves rhythm/melody/octave.
    // Per-lane accessor into the PatternEngine slewed draws.
    // Lane index is the UNIFIED PL_*/strand order (MEL=0, OCT=1, QMIX=2, REST=3, ACC=4,
    // VAR=5, LEG=6 — PatternEngine::PolyLane; editor==engine==strand since the lane-index
    // unification). NOTE: these switches USED to speak the old spread order (0=REST, 1=MEL,
    // 2=OCT, 3=ACC, 4=QMIX) while unified-PL_* callers passed the new order — the lane-
    // convention split caught by test_voice_conventions [C]; unified here 2026-10-16.
    static float monoSlewed(const PatternEngine& pe, int lane, int step) {
        // COLLAPSE: slewed*[16][16] — V1 at [0]. monoSlewed reads voiceSlot 0.
        switch (lane) {
            case PatternEngine::PL_MELODY:    return pe.slewedMelody[0][step];
            case PatternEngine::PL_OCTAVE:    return pe.slewedOctave[0][step];
            case PatternEngine::PL_QMIX:      return pe.slewedQmix[0][step];
            case PatternEngine::PL_REST:      return pe.slewedRhythm[0][step];
            case PatternEngine::PL_ACCENT:    return pe.slewedAccent[0][step];
            case PatternEngine::PL_VARIATION: return pe.slewedVariation[0][step];
            case PatternEngine::PL_LEGATO:    return pe.slewedLegato[0][step];
            default:                          return pe.slewedRhythm[0][step];   // invalid lane → rhythm fallback
        }
    }
    static float polySlewed(const PatternEngine& pe, int lane, int voice, int step) {
        // voice is voiceSlot (0=V1, 1=V2, ..., 15=V16). slewed*[voice] directly.
        switch (lane) {
            case PatternEngine::PL_MELODY:    return pe.slewedMelody[voice][step];
            case PatternEngine::PL_OCTAVE:    return pe.slewedOctave[voice][step];
            case PatternEngine::PL_QMIX:      return pe.slewedQmix[voice][step];
            case PatternEngine::PL_REST:      return pe.slewedRhythm[voice][step];
            case PatternEngine::PL_ACCENT:    return pe.slewedAccent[voice][step];
            case PatternEngine::PL_VARIATION: return pe.slewedVariation[voice][step];
            case PatternEngine::PL_LEGATO:    return pe.slewedLegato[voice][step];
            default:                          return pe.slewedRhythm[voice][step];   // invalid lane → rhythm fallback
        }
    }

    // Published-snapshot twins (DISPLAY-ONLY). The UI thread must never read the live
    // slewed*: the audio thread rewrites them for ~116µs per recomputeEffective*, and a
    // video-rate read mid-rewrite is a torn value. This was the V16 flicker: the bars'
    // own-draw came from pubSlewed* but the spread TARGET leaked through to the live
    // array — so every voice at spread != 0 (V16 was the spread-up voice in the repro)
    // tracked a churning/torn target while spread-0 voices looked stable. The audio
    // path (applyMono/applyPoly — engine thread, same thread as the writer) keeps the
    // LIVE reads above, which are correct there.
    static float monoSlewedPub(const PatternEngine& pe, int lane, int step) {
        switch (lane) {
            case PatternEngine::PL_MELODY:    return pe.pubSlewedPolyMelody[0][step];
            case PatternEngine::PL_OCTAVE:    return pe.pubSlewedPolyOctave[0][step];
            case PatternEngine::PL_QMIX:      return pe.pubSlewedPolyQmix[0][step];
            case PatternEngine::PL_REST:      return pe.pubSlewedPolyRhythm[0][step];
            case PatternEngine::PL_ACCENT:    return pe.pubSlewedPolyAccent[0][step];
            case PatternEngine::PL_VARIATION: return pe.pubSlewedPolyVariation[0][step];
            case PatternEngine::PL_LEGATO:    return pe.pubSlewedPolyLegato[0][step];
            default:                          return pe.pubSlewedPolyRhythm[0][step];
        }
    }
    static float polySlewedPub(const PatternEngine& pe, int lane, int voice, int step) {
        // voice is voiceSlot (0=V1, 1=V2, ..., 15=V16).
        switch (lane) {
            case PatternEngine::PL_MELODY:    return pe.pubSlewedPolyMelody[voice][step];
            case PatternEngine::PL_OCTAVE:    return pe.pubSlewedPolyOctave[voice][step];
            case PatternEngine::PL_QMIX:      return pe.pubSlewedPolyQmix[voice][step];
            case PatternEngine::PL_REST:      return pe.pubSlewedPolyRhythm[voice][step];
            case PatternEngine::PL_ACCENT:    return pe.pubSlewedPolyAccent[voice][step];
            case PatternEngine::PL_VARIATION: return pe.pubSlewedPolyVariation[voice][step];
            case PatternEngine::PL_LEGATO:    return pe.pubSlewedPolyLegato[voice][step];
            default:                          return pe.pubSlewedPolyRhythm[voice][step];
        }
    }

    // The interpolation target for a lane/step.
    // Anchor V1: the mono (voice-1) draw (today's behaviour).
    // Follow CA: the post-remap draw — which IS src[v]'s material, because CA's pin remap
    // already put it there. So the "target" is the same slewed buffer the caller passes as
    // original in follow-CA mode; the caller selects own (pre-remap) vs post-remap.
    static float target(const PatternEngine& pe, int lane, int step) {
        return monoSlewed(pe, lane, step);
    }
    // In Follow CA mode the "own" endpoint is the voice's PRE-REMAP draw (before CA replaced
    // it), and the "leader" is the post-remap value (src[v]'s material). This helper returns
    // the pre-remap mono draw for the mono/V1 path (Follow CA on V1 is a no-op by construction
    // — V1's pre-remap == post-remap — but the poly path needs the pre-remap poly buffers).
    static float monoPreRemap(const PatternEngine& pe, int lane, int step) {
        // COLLAPSE: preRemapSlewed*[16][16] — V1 at [0].
        switch (lane) {
            case PatternEngine::PL_MELODY:    return pe.preRemapSlewedMelody[0][step];
            case PatternEngine::PL_OCTAVE:    return pe.preRemapSlewedOctave[0][step];
            case PatternEngine::PL_QMIX:      return pe.preRemapSlewedQmix[0][step];
            case PatternEngine::PL_REST:      return pe.preRemapSlewedRhythm[0][step];
            case PatternEngine::PL_ACCENT:    return pe.preRemapSlewedAccent[0][step];
            case PatternEngine::PL_VARIATION: return pe.preRemapSlewedVariation[0][step];
            case PatternEngine::PL_LEGATO:    return pe.preRemapSlewedLegato[0][step];
            default:                          return pe.preRemapSlewedRhythm[0][step];
        }
    }
    static float polyPreRemap(const PatternEngine& pe, int lane, int voice, int step) {
        // voice is voiceSlot (0=V1, 1=V2, ..., 15=V16).
        switch (lane) {
            case PatternEngine::PL_MELODY:    return pe.preRemapSlewedMelody[voice][step];
            case PatternEngine::PL_OCTAVE:    return pe.preRemapSlewedOctave[voice][step];
            case PatternEngine::PL_QMIX:      return pe.preRemapSlewedQmix[voice][step];
            case PatternEngine::PL_REST:      return pe.preRemapSlewedRhythm[voice][step];
            case PatternEngine::PL_ACCENT:    return pe.preRemapSlewedAccent[voice][step];
            case PatternEngine::PL_VARIATION: return pe.preRemapSlewedVariation[voice][step];
            case PatternEngine::PL_LEGATO:    return pe.preRemapSlewedLegato[voice][step];
            default:                          return pe.preRemapSlewedRhythm[voice][step];
        }
    }

    // Phase 3: copula mix2 — the knob IS rho (correlation) directly, not a linear blend
    // coefficient. mix2 preserves the uniform marginal (the whole point of the rework) and
    // the 1-p mirror special case disappears (rho = -1 → exactly 1 - targetValue via mix2's
    // own special case).
    //
    // Self-target guard preserved: V1 (the anchor) targets itself — own == leader. Positive
    // spread toward yourself is a no-op (you're already there); mix2(own, own, rho>0) would
    // CHANGE the value (blending a value with itself in normal space concentrates it), which
    // is wrong. Negative self-target spread inverts toward (1 - own) — mix2 handles this
    // correctly (rho < 0 → toward complement).
    //
    // spreadAmount == 0 → return original exactly (bit-identity at spread 0).
    static float interpolate(float original, float targetValue, float spreadAmount) {
        if (spreadAmount == 0.0f) return original;
        if (spreadAmount > 0.0f && targetValue == original) return original;  // V1 self-target no-op
        return (float)redDot::copula::mix2((double)original, (double)targetValue, (double)spreadAmount);
    }

    // DISPLAY-ONLY entry point (anchor-V1 target, caller supplies original). Used by
    // SpreadManager and macroOwnProbability for visual interpolation. Named explicitly
    // to prevent accidental use on the audio path where Follow-CA mode requires
    // applyMono/applyPoly (which resolve the target from the CA source).
    static float applyAnchorV1Only(const PatternEngine& pe, int lane, int step,
                                   float original, float spreadAmount) {
        // DISPLAY-ONLY: the target comes from the PUBLISHED snapshot, not the live
        // slewed* (the live read here was the V16 flicker — see monoSlewedPub).
        return interpolate(original, monoSlewedPub(pe, lane, step), spreadAmount);
    }

    // Mono/V1 path — reads the mode from pe.spreadTargetMode[lane], so the caller
    // doesn't need a Monsoon pointer. Handles both original + target selection:
    //   Anchor V1:  own = monoSlewed (V1's draw), target = monoSlewed (self-target no-op).
    //   Follow CA:  own = monoPreRemap (V1's pre-remap draw — its OWN material before CA
    //               overwrote it), target = monoSlewed (V1's post-remap draw = the pinned
    //               voice's material, because the remap put it there). The spread knob
    //               interpolates between these two endpoints.
    // The key: own (pre-remap) != target (post-remap) when V1 is actually pinned. If they're
    // equal (identity pins or no CA), the self-target guard at interpolate() makes it a no-op,
    // which is the correct behaviour (V1 targeting itself = nothing to follow).
    // V1's CA source row for a spread lane (0=REST, 1=MEL, 2=OCT, 3=ACC, 4=QMIX).
    // Maps the lane to the appropriate CA pin plane (rhythm/melody/qmix) and returns src[0].
    // Used only by the debug assertion to know when V1 is actually pinned (src != self).
    static int v1caSrc(const PatternEngine& pe, int lane) {
        switch (lane) {
            case PatternEngine::PL_MELODY: case PatternEngine::PL_OCTAVE:
                return pe.caMelodySrc[0];    // MEL, OCT → melody plane
            case PatternEngine::PL_QMIX:
                return pe.caQmixSrc[0];      // QMIX → qmix plane
            case PatternEngine::PL_REST: case PatternEngine::PL_ACCENT:
            case PatternEngine::PL_VARIATION: case PatternEngine::PL_LEGATO:
                return pe.caRhythmSrc[0];    // REST, ACC, VAR, LEG → rhythm plane
            default:
                return 0;
        }
    }

    static float applyMono(const PatternEngine& pe, int lane, int step, float spreadAmount) {
        bool followCA = (pe.spreadTargetMode[lane] == 1);
        float own = followCA ? monoPreRemap(pe, lane, step) : monoSlewed(pe, lane, step);
        float t = monoSlewed(pe, lane, step);
        // SPREAD CONTRACT (SPREAD_TARGET_MODES.md POST-MORTEM): under follow-CA with V1
        // actually pinned (src != self), own (pre-remap) MUST differ from t (post-remap =
        // leader's material). If they collapse, interpolate()'s self-target guard makes the
        // knob a silent no-op — the bug that bit three times. Assert loudly in debug builds.
        REDDOT_SPREAD_ASSERT(!(followCA && spreadAmount != 0.0f && v1caSrc(pe, lane) != 0 && own == t),
               "follow-CA mono spread: own==target (pre-remap collapsed) — knob would be a silent no-op");
        return interpolate(own, t, spreadAmount);
    }

    // Poly path — reads the mode from pe.spreadTargetMode[lane], so the caller
    // doesn't need a Monsoon pointer. Handles both original + target selection:
    //   Anchor V1:  own = polySlewed (voice's draw), target = monoSlewed (V1's draw).
    //   Follow CA:  own = polyPreRemap (voice's pre-remap draw), target = polySlewed
    //               (voice's post-remap = src[v]'s material, already in the poly buffer).
    static float applyPoly(const PatternEngine& pe, int lane, int voice, int step, float spreadAmount) {
        bool followCA = (pe.spreadTargetMode[lane] == 1);
        float own = followCA ? polyPreRemap(pe, lane, voice, step) : polySlewed(pe, lane, voice, step);
        float t = followCA ? polySlewed(pe, lane, voice, step) : monoSlewed(pe, lane, step);
        REDDOT_SPREAD_ASSERT(!(followCA && spreadAmount != 0.0f
                 && (int)pe.caSrcRow(voice, laneToStrand(lane)) != voice && own == t),
               "follow-CA poly spread: own==target (pre-remap collapsed) — knob would be a silent no-op");
        return interpolate(own, t, spreadAmount);
    }

    // Spread lane (0=REST,1=MEL,2=OCT,3=ACC,4=QMIX) → engine strand, for the poly assertion's
    // src lookup (caSrcRow takes a strand, not a spread lane).
    // Lane-index unification: lane == strand (PL_* reordered to match STRAND_*).
    // laneToStrand is now IDENTITY — lane IS the strand index.
    static int laneToStrand(int lane) {
        return (lane >= 0 && lane < dotModular::NUM_STRANDS) ? lane : dotModular::STRAND_RHYTHM;
    }
};

} // namespace redDot
