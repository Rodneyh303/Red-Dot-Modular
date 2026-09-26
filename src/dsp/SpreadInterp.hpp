#pragma once
#include <rack.hpp>
#include <cmath>
#include <cassert>
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
    // Lane index is the SPREAD/poly-engine lane: 0=REST 1=MELODY 2=OCTAVE 3=ACCENT 4=QMIX
    static float monoSlewed(const PatternEngine& pe, int lane, int step) {
        switch (lane) {
            case 0:  return pe.slewedRhythm[step];
            case 1:  return pe.slewedMelody[step];
            case 3:  return pe.slewedAccent[step];
            case 4:  return pe.slewedQmix[step];
            default: return pe.slewedOctave[step];
        }
    }
    static float polySlewed(const PatternEngine& pe, int lane, int voice, int step) {
        switch (lane) {
            case 0:  return pe.slewedPolyRhythm[voice][step];
            case 1:  return pe.slewedPolyMelody[voice][step];
            case 3:  return pe.slewedPolyAccent[voice][step];
            case 4:  return pe.slewedPolyQmix[voice][step];
            default: return pe.slewedPolyOctave[voice][step];
        }
    }

    // The interpolation target for a lane/step.
    // Anchor V1: the mono (voice-1) draw (today's behaviour).
    // Follow CA: the post-remap draw — which IS src[v]'s material, because CA's pin remap
    // already put it there. So the "target" is the same slewed buffer the caller passes as
    // `original` in follow-CA mode; the caller selects `own` (pre-remap) vs post-remap.
    static float target(const PatternEngine& pe, int lane, int step) {
        return monoSlewed(pe, lane, step);
    }
    // In Follow CA mode the "own" endpoint is the voice's PRE-REMAP draw (before CA replaced
    // it), and the "leader" is the post-remap value (src[v]'s material). This helper returns
    // the pre-remap mono draw for the mono/V1 path (Follow CA on V1 is a no-op by construction
    // — V1's pre-remap == post-remap — but the poly path needs the pre-remap poly buffers).
    static float monoPreRemap(const PatternEngine& pe, int lane, int step) {
        switch (lane) {
            case 0:  return pe.preRemapSlewedRhythm[step];
            case 1:  return pe.preRemapSlewedMelody[step];
            case 3:  return pe.preRemapSlewedAccent[step];
            case 4:  return pe.preRemapSlewedQmix[step];
            default: return pe.preRemapSlewedOctave[step];
        }
    }
    static float polyPreRemap(const PatternEngine& pe, int lane, int voice, int step) {
        switch (lane) {
            case 0:  return pe.preRemapSlewedPolyRhythm[voice][step];
            case 1:  return pe.preRemapSlewedPolyMelody[voice][step];
            case 3:  return pe.preRemapSlewedPolyAccent[voice][step];
            case 4:  return pe.preRemapSlewedPolyQmix[voice][step];
            default: return pe.preRemapSlewedPolyOctave[voice][step];
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
        return interpolate(original, target(pe, lane, step), spreadAmount);
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
            case 0: case 3: return pe.caRhythmSrc[0];  // REST, ACC → rhythm plane
            case 1: case 2: return pe.caMelodySrc[0];  // MEL, OCT → melody plane
            case 4:         return pe.caQmixSrc[0];    // QMIX → qmix plane
            default:        return 0;
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
        assert(!(followCA && spreadAmount != 0.0f && v1caSrc(pe, lane) != 0 && own == t)
               && "follow-CA mono spread: own==target (pre-remap collapsed) — knob would be a silent no-op");
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
        // SPREAD CONTRACT (SPREAD_TARGET_MODES.md POST-MORTEM): under follow-CA with this voice
        // actually pinned (src != self), own (pre-remap) MUST differ from t (post-remap = its
        // leader's material). Same silent-no-op collapse the mono path hit — assert on BOTH paths.
        assert(!(followCA && spreadAmount != 0.0f
                 && (int)pe.caSrcRow(voice + 1, laneToStrand(lane)) != voice + 1 && own == t)
               && "follow-CA poly spread: own==target (pre-remap collapsed) — knob would be a silent no-op");
        return interpolate(own, t, spreadAmount);
    }

    // Spread lane (0=REST,1=MEL,2=OCT,3=ACC,4=QMIX) → engine strand, for the poly assertion's
    // src lookup (caSrcRow takes a strand, not a spread lane).
    static int laneToStrand(int lane) {
        switch (lane) {
            case 0: return dotModular::STRAND_RHYTHM;
            case 1: return dotModular::STRAND_MELODY;
            case 2: return dotModular::STRAND_OCTAVE;
            case 3: return dotModular::STRAND_ACCENT;
            case 4: return dotModular::STRAND_QMIX;
            default: return dotModular::STRAND_RHYTHM;
        }
    }
};

} // namespace redDot
