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
    static const float* monoBuf(const rack::Module* /*unused*/) { return nullptr; }

    // Per-lane accessor into the PatternEngine slewed draws.
    // Lane index is the SPREAD/poly-engine lane: 0=REST 1=MELODY 2=OCTAVE 3=ACCENT 4=QMIX
    // (== SequencerEngine::PL_ order). QMIX is a melody-family value lane; it reads its own
    // slewedQmix / slewedPolyQmix twin buffers (present as of Task 4b).
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

    // The interpolation target for a lane/step: always the mono (voice-1) draw.
    static float target(const PatternEngine& pe, int lane, int step) {
        return monoSlewed(pe, lane, step);
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

    // Convenience: full pipeline for one value.
    //   original     = the voice's own slewed draw (mono path: the mono draw)
    //   spreadAmount = the (possibly modulated) spread for this voice/lane
    static float apply(const PatternEngine& pe, int lane, int step,
                       float original, float spreadAmount) {
        return interpolate(original, target(pe, lane, step), spreadAmount);
    }
};

} // namespace redDot
