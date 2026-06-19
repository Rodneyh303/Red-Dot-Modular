#pragma once
/**
 * PhaseEngine.hpp — Mode E clock source: an external PHASE RAMP drives playhead
 * position instead of a clock generating it. CV1 carries the phase in Mode E,
 * analogous to gate1in carrying gates in Mode B.
 *
 * Phase 0→1 maps to one bar = 16 sixteenths = 16 * pulsesPer16th pulses
 * (24 PPQN → 96 positions, 48 → 192, 96 → 384) — the same pulse grid the gate
 * engine uses, so all timing is integer pulses with no continuous-time component.
 *
 * Mirrors ClockEngine's OUTPUT CONTRACT (pulseEdge / sixteenthEdge / quarterEdge +
 * bpm + sixteenthSec) so the existing execute path is reused unchanged. ADDS a
 * `reverse` flag: when the phase ramp moves backward, edges fire in reverse and the
 * sequencer steps its playhead backward (handled in SequencerEngine).
 *
 * STEP 1 SCOPE (this file): forward + within-phrase reverse. No PRNG, no jumps, no
 * draw-counter. A jump/scrub (large discontinuity) is detected and, for now, simply
 * RESYNCS position without emitting a burst of edges (so step 1 never tries to
 * integrate across a teleport — that's a later step). Continuous motion (the normal
 * fwd/rev case) emits one edge per crossed pulse.
 *
 * Header-only, no Rack dependency beyond math clamp (kept local to avoid coupling).
 */

#include <cstdint>
#include <cmath>

namespace redDot {

struct PhaseEngine {
    // ── Config ────────────────────────────────────────────────────────────────
    int   ppqn          = 24;       // master grid (24/48/96); pulsesPer16th = ppqn/4
    float phaseInMax    = 10.f;     // input voltage that maps to phase = 1.0 (one bar)

    // ── State ─────────────────────────────────────────────────────────────────
    bool   havePrev     = false;    // false until first sample seen
    double prevPhase    = 0.0;      // last normalised phase in [0,1) (unwrapped frac)
    long   prevPulse    = 0;        // last absolute pulse index (can be negative)
    double prevContPhase= 0.0;      // last *continuous* (unwrapped) phase, for velocity
    double phaseVel     = 0.0;      // measured d(phase)/d(sample), for bpm + warp

    // ── Outputs (valid after each process()) ────────────────────────────────────
    bool  pulseEdge     = false;    // one grid pulse elapsed (either direction)
    bool  sixteenthEdge = false;    // one 1/16 elapsed
    bool  quarterEdge   = false;    // one quarter elapsed
    bool  reverse       = false;    // true if motion this sample was backward
    bool  jumped        = false;    // true if a discontinuity (jump/scrub) was seen
    long  pulsePos      = 0;        // current absolute pulse index
    float bpm           = 120.f;    // derived from measured phase velocity (for display/fallback)
    float sixteenthSec  = 0.125f;   // derived 1/16 length in seconds (for any seconds consumer)

    static int pulsesPer16th(int p) { return (p > 0 ? p : 24) / 4; }

    void reset() {
        havePrev = false;
        prevPhase = prevContPhase = 0.0; prevPulse = 0; phaseVel = 0.0;
        pulseEdge = sixteenthEdge = quarterEdge = reverse = jumped = false;
        pulsePos = 0; bpm = 120.f; sixteenthSec = 0.125f;
    }

    // Map a raw phase voltage to normalised phase [0,1). Wrapping is expected: the
    // ramp typically sweeps 0→max repeatedly; we treat it modulo one bar.
    double normPhase(float phaseVolt) const {
        double p = (double)phaseVolt / (double)(phaseInMax > 1e-6f ? phaseInMax : 10.f);
        p = p - std::floor(p);           // wrap into [0,1)
        return p;
    }

    // process(): call once per sample with the current phase voltage.
    //  - connected=false → emit nothing (caller falls back to internal clock).
    //  - JUMP_PULSES: a single-sample motion larger than this many pulses is treated
    //    as a discontinuity (jump/scrub), not continuous motion. Step 1 resyncs
    //    without emitting an edge burst.
    void process(float phaseVolt, bool connected, float sampleTime, int ppqnSetting) {
        pulseEdge = sixteenthEdge = quarterEdge = reverse = jumped = false;
        ppqn = (ppqnSetting > 0 ? ppqnSetting : 24);
        const int p16   = pulsesPer16th(ppqn);
        const long PPB  = (long)p16 * 16;          // pulses per bar (phase 0→1)

        if (!connected) { havePrev = false; return; }

        double phase = normPhase(phaseVolt);
        // Absolute (unwrapped) pulse index within the current bar. We keep pulsePos
        // absolute by tracking bar wraps via the shortest-path delta below.
        long pulseInBar = (long)std::floor(phase * (double)PPB);

        if (!havePrev) {
            havePrev = true;
            prevPhase = phase; prevContPhase = phase;
            prevPulse = pulseInBar; pulsePos = pulseInBar;
            return;                                  // no edge on first sample
        }

        // Shortest-path phase delta in [-0.5, 0.5] so a wrap (…0.98 → 0.02) reads as
        // a small forward step, not a huge backward jump.
        double d = phase - prevPhase;
        if (d >  0.5) d -= 1.0;
        if (d < -0.5) d += 1.0;

        // Velocity (for bpm/warp): phase units per sample → bars/sec → bpm.
        if (sampleTime > 0.f) {
            phaseVel = d / (double)sampleTime;       // bars per second (signed)
            double barsPerSec = std::fabs(phaseVel);
            float newBpm = (float)(barsPerSec * 4.0 * 60.0);
            if (newBpm > 1.f && newBpm < 1000.f) {
                bpm = newBpm;
                sixteenthSec = 15.f / bpm;
            }
        }

        // Advance the continuous (unwrapped) phase by the shortest-path delta, and
        // derive the absolute target pulse from it. Deriving from the absolute phase
        // (not from accumulated rounded deltas) is what lets sub-pulse per-sample
        // motion accumulate correctly until a pulse boundary is actually crossed.
        prevContPhase += d;
        long targetPulse = (long)std::llround(prevContPhase * (double)PPB);

        long step = targetPulse - pulsePos;
        long mag  = step < 0 ? -step : step;

        // Discontinuity guard (step 1): a single-sample move of more than a 1/16 is
        // a jump/scrub, not continuous play. Resync silently (no edge burst) — jump
        // integration is a later step.
        const long JUMP_PULSES = p16;                // > one 1/16 in a sample = jump
        if (mag > JUMP_PULSES) {
            jumped = true;
            pulsePos = targetPulse;
            prevPhase = phase;
            prevPulse = targetPulse;
            return;
        }

        if (step == 0) { prevPhase = phase; return; }

        reverse = (step < 0);

        // Emit one pulseEdge per crossed pulse. For continuous motion mag is small
        // (usually 0 or 1), so this loop is short. sixteenth/quarter edges are
        // derived from the absolute pulse index parity so they stay phase-aligned
        // regardless of direction.
        long dir = reverse ? -1 : +1;
        for (long i = 0; i < mag; ++i) {
            pulsePos += dir;
            pulseEdge = true;
            // A 1/16 boundary is crossed when pulsePos is a multiple of p16. Going
            // forward the edge belongs to the pulse we just ENTERED; going back, to
            // the pulse we just LEFT — both reduce to "pulsePos % p16 == 0".
            long mod = ((pulsePos % p16) + p16) % p16;
            if (mod == 0) {
                sixteenthEdge = true;
                long step16 = ((pulsePos / p16) % 16 + 16) % 16;
                if (step16 % 4 == 0) quarterEdge = true;
            }
        }

        prevPhase = phase;
        prevPulse = pulsePos;
    }
};

} // namespace redDot
