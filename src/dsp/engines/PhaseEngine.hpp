#pragma once
/**
 * PhaseEngine.hpp — Mode E clock source: an external PHASE RAMP drives playhead
 * position instead of a clock generating it. CV1 carries the phase in Mode E,
 * analogous to gate1in carrying gates in Mode B.
 *
 * Phase 0→1 maps to the ACTIVE PATTERN = patternLength sixteenths =
 * patternLength * pulsesPer16th pulses (PHASE_TIER Stage 1: the span is the pattern
 * length, not a hardcoded 16 — one bar at length 16, two bars at 32; at 24 PPQN
 * length 16 → 96 positions, length 32 → 192). The BAR itself stays 16 steps — the
 * sixteenth-edge feed below still mods by 16 (a bar is always 16 steps; the pattern
 * can be 16 or 32). The same pulse grid the gate engine uses, so all timing is
 * integer pulses with no continuous-time component.
 *
 * Mirrors ClockEngine's OUTPUT CONTRACT (pulseEdge / sixteenthEdge / quarterEdge +
 * bpm + sixteenthSec) so the existing execute path is reused unchanged. ADDS a
 * reverse flag: when the phase ramp moves backward, edges fire in reverse and the
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
    float phaseInMax    = 10.f;     // input voltage that maps to phase = 1.0 (one full pattern span)
    int   patternLength = 16;       // PHASE TIER Stage 1: the ACTIVE pattern length (1..kPatternLengthCap)
                                    // — phase 0→1 spans patternLength steps. Default 16 = today's
                                    // bar exactly (bit-identical); Monsoon feeds engine.cachedLength
                                    // per block so a 32-step pattern gets the full 32-step sweep.

    // ── State ─────────────────────────────────────────────────────────────────
    bool   havePrev     = false;    // false until first sample seen
    double prevPhase    = 0.0;      // last normalised phase in [0,1) (unwrapped frac)
    long   prevPulse    = 0;        // last absolute pulse index (can be negative)
    double prevContPhase= 0.0;      // last *continuous* (unwrapped) phase, for velocity
    double phaseVel     = 0.0;      // measured d(phase)/d(sample), for bpm + warp

    // ── Stage 2 state (the subphase tier — PHASE_TIER, PHASE_TIER_SYSTEM_IDEA.md IDEA 2) ──
    bool   haveTier     = false;    // false until the tier path's first sample (a fresh CV3 cable reseeds)
    double prevSub      = 0.0;      // last wrapped subphase in [0,1)
    // NOTE: there is deliberately NO composed-phase accumulator. The composed position is
    // recomputed FROM THE FORMULA every sample (see processTiers) — an accumulator driven
    // by shortest-path deltas diverges from the formula at every subphase ramp wrap by
    // ~rF (the wrap reads forward, the formula value drops back into the region), and the
    // within-region guarantee dies. prevContPhase (shared with process()) is the ONLY
    // accumulator — the UNWRAPPED transport, whose own wrap is genuinely forward.

    // ── Outputs (valid after each process()) ────────────────────────────────────
    bool  pulseEdge     = false;    // one grid pulse elapsed (either direction)
    bool  sixteenthEdge = false;    // one 1/16 elapsed
    bool  quarterEdge   = false;    // one quarter elapsed
    bool  reverse       = false;    // true if motion this sample was backward
    bool  jumped        = false;    // true if a discontinuity (jump/scrub) was seen
    int   jumpSixteenths= 0;        // signed # of 1/16 steps to replay on a jump (±)
    long  pulsePos      = 0;        // current absolute pulse index
    bool  pulseChanged  = false;    // THE PULSE-CHANGE GATE signal (CC_FIX_PHASE_RUN_AT_PPQN,
                                    // docs tip 4cb1a749): true ONLY on the samples where the
                                    // quantised pulse position moved (targetPulse != prevPulse).
                                    // The expensive tail — velocity, jump detection, the pulse
                                    // walk — runs on those samples; between them the cheap path
                                    // just warms the trackers and ACCUMULATES prevContPhase (a
                                    // running sum across skipped samples — never reset, the
                                    // doc's DO-NOT). Monsoon gates its Mode-E snap/drift path to
                                    // this flag. The probe is the tail's OWN target expression
                                    // (llround(prevContPhase·PPB) / tierPulseFor), so firing is
                                    // bit-identical to the old per-sample step != 0.
    float bpm           = 120.f;    // derived from measured phase velocity (for display/fallback)
    float sixteenthSec  = 0.125f;   // derived 1/16 length in seconds (for any seconds consumer)
    // Stage 2 outputs: each tier's OWN per-sample motion sign (+1 / -1 / 0 flat) — the
    // raw material for the SIGN-PRODUCT direction rule (dirT · dirS, neutral = no flip).
    // `reverse` stays the COMPOSED motion sign (the position machinery's truth); the
    // semantic direction is composed at the caller (Monsoon) from these two.
    int8_t dirT = 0;                // transport tier motion sign this sample
    int8_t dirS = 0;                // subphase tier motion sign this sample

    static int pulsesPer16th(int p) { return (p > 0 ? p : 24) / 4; }

    void reset() {
        havePrev = false;
        prevPhase = prevContPhase = 0.0; prevPulse = 0; phaseVel = 0.0;
        pulseEdge = sixteenthEdge = quarterEdge = reverse = jumped = false; jumpSixteenths = 0;
        pulsePos = 0; bpm = 120.f; sixteenthSec = 0.125f;
        haveTier = false; prevSub = 0.0;
        dirT = dirS = 0;
        pulseChanged = false;
    }

    // Map a raw phase voltage to normalised phase [0,1). Wrapping is expected: the
    // ramp typically sweeps 0→max repeatedly; we treat it modulo one bar.
    double normPhase(float phaseVolt) const {
        double p = (double)phaseVolt / (double)(phaseInMax > 1e-6f ? phaseInMax : 10.f);
        p = p - std::floor(p);           // wrap into [0,1)
        return p;
    }

    // PHASE TIER Stage 2 — the composed span fraction → pulse, via FLOOR (not llround).
    // The transport's continuous path llrounds because the SPAN ring is cyclic — its
    // edge leak (pulse 192 at phase 0.9999) IS the wrap. A REGION is not cyclic in the
    // span (the ≤1-bar cap), so llround would leak one pulse across the region boundary
    // at s → 1⁻ (composed 0.4995 of a 0.5 region llrounds to pulse 96 = the NEXT
    // region's first pulse); floor keeps the subphase STRICTLY inside its region at
    // every s < 1 and matches the transport path's own SEED convention (pulseInBar is
    // floored there too). The composed value itself is the quotient/remainder formula
    // evaluated fresh (never accumulated — see the state-block note).
    static long tierPulseFor(double contT, double s, double rF, long ppb) {
        double composed = (double)std::floor(contT / rF) * rF + s * rF;
        return (long)std::floor(composed * (double)ppb);
    }

    // process(): call once per sample with the current phase voltage.
    //  - connected=false → emit nothing (caller falls back to internal clock).
    //  - JUMP_PULSES: a single-sample motion larger than this many pulses is treated
    //    as a discontinuity (jump/scrub), not continuous motion. Step 1 resyncs
    //    without emitting an edge burst.
    void process(float phaseVolt, bool connected, float sampleTime, int ppqnSetting) {
        pulseEdge = sixteenthEdge = quarterEdge = reverse = jumped = false; jumpSixteenths = 0;
        pulseChanged = false;
        ppqn = (ppqnSetting > 0 ? ppqnSetting : 24);
        const int p16   = pulsesPer16th(ppqn);
        // PHASE TIER Stage 1: pulses per FULL SPAN (phase 0→1) = the ACTIVE pattern length.
        // Was p16 * 16 (one hardcoded 16-step bar). At patternLength 16 this is the same
        // number — bit-identical; at 32 the phase sweep covers both bars (the D9 answer:
        // option A, the full-span mapping). The BAR modulo stays 16 (see step16 below).
        const long plen = (long)(patternLength > 0 ? patternLength : 16);
        const long PPB  = (long)p16 * plen;        // pulses per full span (phase 0→1)

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

        // ── THE CHEAP PATH (every sample — CC_FIX_PHASE_RUN_AT_PPQN step 1) ────────
        // Shortest-path phase delta in [-0.5, 0.5] so a wrap (…0.98 → 0.02) reads as
        // a small forward step, not a huge backward jump.
        double d = phase - prevPhase;
        if (d >  0.5) d -= 1.0;
        if (d < -0.5) d += 1.0;
        prevPhase = phase;      // the tracker warms EVERY sample (the accumulator's input)

        // Advance the continuous (unwrapped) phase by the shortest-path delta, and
        // derive the absolute target pulse from it. Deriving from the absolute phase
        // (not from accumulated rounded deltas) is what lets sub-pulse per-sample
        // motion accumulate correctly until a pulse boundary is actually crossed.
        // The accumulator is a RUNNING SUM carried across skipped samples — never
        // reset, never recomputed (the fix doc's DO-NOT) — so the unwrapped position
        // is correct whenever the next pulse computation runs.
        prevContPhase += d;

        // ── THE PULSE-CHANGE GATE (step 2) ──────────────────────────────────────
        // Rodney: phase only needs to run at PPQN. The quantised PULSE POSITION —
        // here the continuous path's own target llround(prevContPhase·PPB) — moves
        // only at pulse boundaries, and nothing downstream reads an in-between value
        // (Monsoon reads the quantised step). The probe is the SAME expression the
        // tail lands on, compared against prevPulse (the last absolute pulse index —
        // the doc's "pulseInBar != prevPulse" against the engine's own tracker), so
        // the gate fires EXACTLY when the old per-sample body's step != 0 would
        // have: bit-identical firing samples, and the settle call after the floored
        // seed (the resting llround landing the span probes pin) still fires.
        long targetPulse = (long)std::llround(prevContPhase * (double)PPB);
        pulseChanged = (targetPulse != prevPulse);
        if (!pulseChanged) return;   // edges held low (reset at entry), position unchanged

        // ── THE TAIL (runs ONLY when the quantised pulse position moved) ─────────
        // Velocity (for bpm/warp): phase units per sample → bars/sec → bpm. Sampled
        // at pulse rate now — a derived display/fallback value consumed at the
        // edges; nothing reads it between pulses (nothing continuous to preserve).
        if (sampleTime > 0.f) {
            phaseVel = d / (double)sampleTime;       // bars per second (signed)
            double barsPerSec = std::fabs(phaseVel);
            float newBpm = (float)(barsPerSec * 4.0 * 60.0);
            if (newBpm > 1.f && newBpm < 1000.f) {
                bpm = newBpm;
                sixteenthSec = 15.f / bpm;
            }
        }

        long step = targetPulse - pulsePos;
        long mag  = step < 0 ? -step : step;

        // Discontinuity (jump/scrub): a single-sample move of more than one 1/16 is
        // not continuous play. Per the spec we REPLAY the event chain over the crossed
        // 1/16 steps with frozen modulation — and because the whole replay happens
        // inside ONE process() call, modulation is already frozen (re-sampled only
        // next block), so the freeze is automatic (§8b time-scale separation).
        //
        // We DON'T emit the pulse-by-pulse burst here; instead we report jumpSixteenths
        // (signed count of 1/16 steps to replay) and let the dispatch run the verified
        // executeModeE path that many times. WITHIN-DRAW SCOPE: the replay is bounded
        // to the current phrase; a jump that would cross the phrase boundary is clamped
        // to the boundary (cross-draw regeneration is a later refinement).
        const long JUMP_PULSES = p16;                // > one 1/16 in a sample = jump
        if (mag > JUMP_PULSES) {
            jumped  = true;
            reverse = (step < 0);
            // How many *whole 1/16 steps* the jump spans (the unit the sequencer
            // replays). Round to nearest 1/16 so we land on a step grid.
            long sixteenthsMoved = (mag + p16/2) / p16;
            jumpSixteenths = (int)((step < 0) ? -sixteenthsMoved : sixteenthsMoved);
            pulsePos = targetPulse;
            prevPulse = targetPulse;
            return;
        }

        // (The old step == 0 early-out is unreachable past the gate: prevPulse ==
        // pulsePos between tails — the seed sets both, every tail ends with
        // prevPulse = pulsePos — so targetPulse != prevPulse ⟹ step != 0. The gate
        // IS the old no-motion return, and the tracker update lives in the cheap
        // path now.)

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
                // THE BAR MODULO STAYS 16 (PHASE TIER Stage 1): a bar is always 16 steps;
                // the pattern can be 16 or 32. step16 = the step WITHIN the current bar —
                // the quarter feed and any bar-local consumer keep their meaning at 32.
                long step16 = ((pulsePos / p16) % 16 + 16) % 16;
                if (step16 % 4 == 0) quarterEdge = true;
            }
        }

        prevPulse = pulsePos;
    }

    // ── PHASE TIER Stage 2: the SUBPHASE TIER (the quotient/remainder) ─────────────
    // processTiers(): call once per sample with the transport phase voltage AND, when a
    // subphase cable is present, its voltage. Design authority:
    // docs/design/PHASE_TIER_SYSTEM_IDEA.md IDEA 2 (decided); build spec
    // CC_PROMPT_PHASE_TIER.md Stage 2; implementation map PHASE_TIER_PLAN.md.
    //
    // THE MODEL (decided):
    //   rF       = regionSixteenths / patternLength      (the region as a span fraction)
    //   coarse   = floor(contT / rF)                     (the UNWRAPPED region address)
    //   composed = coarse·rF + s·rF                      (s = the subphase, WRAPPED [0,1))
    //   position = composed → pulses → the EXISTING grid (the same jump/replay/snap/drift
    //              machinery the transport-only path uses — "not a new jump class, the
    //              existing one applied twice").
    //
    // WHY THE COMPOSITION LIVES HERE, IN THE UNWRAPPED DOMAIN (adjudication, plan §Stage 2):
    // composing the two WRAPPED phases into a composed VOLTAGE aliases at the transport
    // cycle wrap — the composed position's mod-1 delta is ambiguous (0.99 → 0.49 reads
    // −0.5 where the true motion is +0.5 ring-forward, and the shortest-path rescue only
    // fires strictly beyond ±0.5). Tracking the transport unwrapped (prevContPhase — the
    // tracker process() already keeps) makes the region address monotone with the
    // transport, so a cycle wrap is a REGION CROSSING into the next cycle's region and the
    // composed motion continues ring-forward. The subphase stays WRAPPED — it can only
    // address WITHIN the selected region, so "no jumps between bars" (regionSize = 1 bar)
    // holds BY CONSTRUCTION and the ≤1-bar cap is structural, not a rule.
    //
    // Within a region the coarse term is FROZEN — the transport's own fine motion is
    // QUANTISED AWAY (that is the model: transport selects the region, subphase positions
    // within it). The composed value is RECOMPUTED FROM THE FORMULA every sample (no
    // accumulator — see the state-block note: an accumulator diverges from the formula at
    // every subphase ramp wrap); jump detection and `reverse` read the VALUE delta
    // (targetPulse - pulsePos), so a transport cycle wrap is a forward region landing
    // (the coarse term stepped) and a subphase ramp wrap is a backward jump that stays
    // inside the region (the decided "its jumps stay inside one bar").
    //
    // DIRECTION: `reverse` here stays the COMPOSED motion sign (the position machinery's
    // truth — the replay and the snap follow the physically crossed steps). The SEMANTIC
    // direction is the SIGN PRODUCT (dirT · dirS, flat = neutral = no flip — "flat input
    // carries no direction; no memory; decided"), composed by the caller. dirT/dirS are
    // this sample's per-tier shortest-path motion signs, derived exactly how `reverse`
    // itself is derived for the transport-only path.
    //
    // BPM follows the TRANSPORT ramp's velocity (the tempo carrier), not the composed
    // motion — the subphase is a position dial, not a clock.
    void processTiers(float tVolt, float sVolt, bool subPatched, double regionSixteenths,
                      float sampleTime, int ppqnSetting) {
        if (!subPatched) {
            // UNPATCHED = Stage 1 EXACTLY (the bit-identical gate): the plain path, and
            // the tier trackers dropped so a later cable reseeds cleanly. Neutral dirs —
            // the caller's sign product degenerates to phase.reverse.
            haveTier = false;
            dirT = dirS = 0;
            process(tVolt, true, sampleTime, ppqnSetting);
            return;
        }
        pulseEdge = sixteenthEdge = quarterEdge = reverse = jumped = false; jumpSixteenths = 0;
        dirT = dirS = 0;
        pulseChanged = false;
        ppqn = (ppqnSetting > 0 ? ppqnSetting : 24);
        const int p16   = pulsesPer16th(ppqn);
        const long plen = (long)(patternLength > 0 ? patternLength : 16);
        const long PPB  = (long)p16 * plen;
        const double rF = regionSixteenths / (double)plen;   // the region as a span fraction

        double t = normPhase(tVolt);
        double s = normPhase(sVolt);

        if (!haveTier) {
            // First tier sample: seed everything from the CURRENT voltages, no edges
            // (the same first-sample contract process() keeps).
            haveTier   = true;
            prevSub    = s;
            prevPhase  = t;                                   // the transport trackers stay warm
            prevContPhase = t;                                // (so an unpatch re-enters seamlessly)
            pulsePos   = tierPulseFor(t, s, rF, PPB);
            prevPulse  = pulsePos;
            return;
        }

        // ── THE CHEAP PATH (every sample — CC_FIX_PHASE_RUN_AT_PPQN step 1) ────────
        // The transport delta (shortest-path — the ramp wrap reads forward, the same
        // convention process() uses) advances the UNWRAPPED transport.
        double dT = t - prevPhase;
        if (dT >  0.5) dT -= 1.0;
        if (dT < -0.5) dT += 1.0;
        dirT = (dT > 0.0) ? 1 : (dT < 0.0 ? -1 : 0);
        prevPhase = t;
        prevContPhase += dT;

        // The subphase delta (shortest-path) — the SIGN-PRODUCT material only. The
        // POSITION never integrates these deltas (see the state-block note above).
        // Both dirs stay PER-SAMPLE (cheap, and the sign product reads them on still
        // samples too — test_phase_tier §10/§12) — only the EXPENSIVE tail is gated.
        double dS = s - prevSub;
        if (dS >  0.5) dS -= 1.0;
        if (dS < -0.5) dS += 1.0;
        dirS = (dS > 0.0) ? 1 : (dS < 0.0 ? -1 : 0);
        prevSub = s;

        // ── THE PULSE-CHANGE GATE (step 2) ──────────────────────────────────────
        // THE COMPOSITION — the quotient/remainder, recomputed from the formula EVERY
        // sample (never accumulated): the coarse term from the UNWRAPPED transport (so a
        // transport cycle wrap is a region crossing into the next cycle's region — the
        // composed motion continues ring-forward), the fine term from the WRAPPED
        // subphase (so the position can only ever sit WITHIN the selected region — the
        // ≤1-bar cap is structural). A subphase RAMP WRAP (s: 0.99 → 0.01) therefore
        // reads, through the value delta below, as a BACKWARD JUMP that stays inside the
        // region — the decided text's own words ("its jumps stay inside one bar"),
        // handled by the same replay + snap as any other jump (the arrival rule).
        // Adjudication recorded in PHASE_TIER_PLAN.md §Stage 2. The composed value and
        // its pulse mapping live in tierPulseFor (above) — evaluated fresh, floored.
        //
        // THE PROBE IS THE COMPOSITION ITSELF (the same tierPulseFor the tail lands
        // on, compared against prevPulse): the composed position moves with the
        // SUBPHASE between transport pulses (the coarse term is frozen within a
        // region — that IS the model), so a transport-only probe would freeze a
        // subphase sweep with a stalled transport and the within-region model dies
        // (test_phase_tier §11/§14 are exactly that sweep). The gate fires when the
        // composed target moved — EXACTLY when the old per-sample body's step != 0
        // would have (bit-identical firing samples).
        long targetPulse = tierPulseFor(prevContPhase, s, rF, PPB);
        pulseChanged = (targetPulse != prevPulse);
        if (!pulseChanged) return;   // edges held low (reset at entry), position unchanged

        // ── THE TAIL (runs ONLY when the quantised pulse position moved) ─────────
        // BPM follows the TRANSPORT ramp's velocity (the tempo carrier) — sampled at
        // pulse rate now; consumed at the edges, never between pulses.
        if (sampleTime > 0.f) {
            phaseVel = dT / (double)sampleTime;
            double barsPerSec = std::fabs(phaseVel);
            float newBpm = (float)(barsPerSec * 4.0 * 60.0);
            if (newBpm > 1.f && newBpm < 1000.f) {
                bpm = newBpm;
                sixteenthSec = 15.f / bpm;
            }
        }

        long step = targetPulse - pulsePos;
        long mag  = step < 0 ? -step : step;

        // The SAME jump contract as process(): a single-sample move over one 1/16 is a
        // discontinuity — report jumpSixteenths, let the dispatch replay the crossed
        // steps (the arrival rule at region landings) and snap. A region crossing moves
        // exactly regionSixteenths·p16 pulses ≥ 4·p16 > p16 — always detected.
        const long JUMP_PULSES = p16;
        if (mag > JUMP_PULSES) {
            jumped  = true;
            reverse = (step < 0);
            long sixteenthsMoved = (mag + p16/2) / p16;
            jumpSixteenths = (int)((step < 0) ? -sixteenthsMoved : sixteenthsMoved);
            pulsePos = targetPulse;
            prevPulse = targetPulse;
            return;
        }

        // (The old step == 0 early-out is unreachable past the gate — the same
        // prevPulse == pulsePos invariant as process(): targetPulse != prevPulse
        // ⟹ step != 0.)

        reverse = (step < 0);

        // The continuous walk — one pulseEdge per crossed pulse, sixteenth/quarter edges
        // from the absolute index parity, THE BAR MODULO STAYS 16 (Stage 1).
        long dir = reverse ? -1 : +1;
        for (long i = 0; i < mag; ++i) {
            pulsePos += dir;
            pulseEdge = true;
            long mod = ((pulsePos % p16) + p16) % p16;
            if (mod == 0) {
                sixteenthEdge = true;
                long step16 = ((pulsePos / p16) % 16 + 16) % 16;
                if (step16 % 4 == 0) quarterEdge = true;
            }
        }
        prevPulse = pulsePos;
    }
};

} // namespace redDot
