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
//       AVERAGE_POLY : (mono + Σ active-poly) / (1 + nPoly)  — mono IS included.
//       MONO_DRAW    : the raw mono slewed draw. (A voice targeting itself in
//                      MONO_DRAW is a no-op, so the mono strand stays a fixed
//                      anchor — the desired behaviour.)
//   • Bipolar spread: amount>0 → toward target; amount<0 → toward (1−target) by
//     |amount|; ==0 → unchanged. Result clamped 0..1.
// ─────────────────────────────────────────────────────────────────────────────

namespace redDot {

struct SpreadInterp {
    enum Target { AVERAGE_POLY, MONO_DRAW };

    // Pointers to the lane's slewed buffers for the engine. Set per lane by the
    // caller so the same code serves rhythm/melody/octave.
    static const float* monoBuf(const rack::Module* /*unused*/) { return nullptr; }

    // Per-lane accessor into the PatternEngine slewed draws.
    static float monoSlewed(const PatternEngine& pe, int lane, int step) {
        switch (lane) {
            case 0:  return pe.slewedRhythm[step];
            case 1:  return pe.slewedMelody[step];
            case 3:  return pe.slewedAccent[step];
            default: return pe.slewedOctave[step];
        }
    }
    static float polySlewed(const PatternEngine& pe, int lane, int voice, int step) {
        switch (lane) {
            case 0:  return pe.slewedPolyRhythm[voice][step];
            case 1:  return pe.slewedPolyMelody[voice][step];
            case 3:  return pe.slewedPolyAccent[voice][step];
            default: return pe.slewedPolyOctave[voice][step];
        }
    }

    // The interpolation target for a lane/step under the chosen mode.
    // nPoly = number of active poly voices included in the average.
    static float target(const PatternEngine& pe, Target mode, int lane, int step, int nPoly) {
        if (mode == MONO_DRAW) return monoSlewed(pe, lane, step);
        // AVERAGE_POLY: mono + all active poly, over (1 + nPoly).
        float s = monoSlewed(pe, lane, step);
        for (int v = 0; v < nPoly; ++v) s += polySlewed(pe, lane, v, step);
        return s / (1.f + (float)nPoly);
    }

    // The shared bipolar interpolation + clamp.
    static float interpolate(float original, float targetValue, float spreadAmount) {
        float result;
        // Self-target handling depends on SIGN:
        //  • spread >= 0 → converge TOWARD the target. When target == original there's
        //    nothing to converge to → no-op (correct; a lone positive spread does nothing).
        //  • spread <  0 → move toward the INVERSION (1 − target). This is meaningful even
        //    when target == original: it inverts the draw toward (1 − d). V1 in voice-1-
        //    target (MONO_DRAW) mode must respond to negative spread this way. (The earlier
        //    blanket 'target==original → no-op' guard killed this; it only belongs on the
        //    positive branch.)
        if (spreadAmount == 0.0f) result = original;
        else if (spreadAmount > 0.0f) {
            if (targetValue == original) result = original;                       // converge to self = no-op
            else result = original + (targetValue - original) * spreadAmount;
        }
        else result = original + ((1.0f - targetValue) - original) * std::fabs(spreadAmount);  // invert toward (1−target)
        return rack::math::clamp(result, 0.0f, 1.0f);
    }

    // Convenience: full pipeline for one value.
    //   original     = the voice's own slewed draw (mono path: the mono draw)
    //   spreadAmount = the (possibly modulated) spread for this voice/lane
    static float apply(const PatternEngine& pe, Target mode, int lane, int step,
                       int nPoly, float original, float spreadAmount) {
        return interpolate(original, target(pe, mode, lane, step, nPoly), spreadAmount);
    }
};

} // namespace redDot
