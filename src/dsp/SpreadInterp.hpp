#pragma once
#include <rack.hpp>
#include <cmath>
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

    // Convenience: full pipeline for one value (mono/V1 path) — ANCHOR V1 ONLY.
    // Target is always monoSlewed (V1's own draw = self-target no-op for positive spread).
    // Do NOT use this for Follow-CA mode — use applyMono() instead, which resolves V1's
    // CA source correctly. The old comment claiming "Follow CA: V1's post-remap == monoSlewed"
    // was wrong when src[0] != 0 and led to a silent no-op (target == original → self-target guard).
    static float apply(const PatternEngine& pe, int lane, int step,
                       float original, float spreadAmount) {
        return interpolate(original, target(pe, lane, step), spreadAmount);
    }

    // V1's CA source row for a spread lane (0=REST, 1=MEL, 2=OCT, 3=ACC, 4=QMIX).
    // Maps the lane to the appropriate CA pin plane (rhythm/melody/qmix) and returns src[0].
    static int v1caSrc(const PatternEngine& pe, int lane) {
        switch (lane) {
            case 0: case 3: return pe.caRhythmSrc[0];  // REST, ACC → rhythm plane
            case 1: case 2: return pe.caMelodySrc[0];  // MEL, OCT → melody plane
            case 4:         return pe.caQmixSrc[0];    // QMIX → qmix plane
            default:        return 0;
        }
    }

    // Mono/V1 path with mode awareness. Under Follow-CA the target resolves from V1's CA
    // source (src[0]): src==0 → V1 targets itself (no-op); src==k>0 → poly voice k-1's draw
    // (poly arrays hold V2–V16 at indices 0–14, so src k maps to poly index k-1). This does
    // NOT rely on the remap having run — it reads the source voice's buffer directly, making
    // it robust against remap caching. The caller supplies `original` via monoOwn (pre-remap
    // for follow-CA, slewed for anchor-V1).
    static float applyMono(const PatternEngine& pe, int lane, int step,
                           float original, float spreadAmount, bool followCA) {
        float t;
        if (followCA) {
            int src = v1caSrc(pe, lane);
            t = (src == 0) ? monoSlewed(pe, lane, step)
                           : polySlewed(pe, lane, src - 1, step);
        } else {
            t = monoSlewed(pe, lane, step);  // anchor V1: self-target
        }
        return interpolate(original, t, spreadAmount);
    }

    // Poly path: the target depends on the mode.
    //   Anchor V1:  target = V1's draw (monoSlewed) — voices anchor to V1.
    //   Follow CA:  target = the voice's OWN post-remap draw (polySlewed) — which IS
    //               src[v]'s material, because CA's pin remap already put it there.
    // The caller selects `original` (own) via polyOwn: pre-remap (follow-CA) or post-remap
    // (anchor V1). Without the polySlewed target here, follow-CA voices would incorrectly
    // target V1's remapped draw instead of their own src[v]'s material.
    static float applyPoly(const PatternEngine& pe, int lane, int voice, int step,
                           float original, float spreadAmount, bool followCA) {
        float t = followCA ? polySlewed(pe, lane, voice, step)
                           : monoSlewed(pe, lane, step);
        return interpolate(original, t, spreadAmount);
    }
};

} // namespace redDot
