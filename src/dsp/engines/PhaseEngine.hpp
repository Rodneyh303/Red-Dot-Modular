#pragma once
/**
 * PhaseEngine.hpp — Mode E clock source: an external PHASE RAMP drives playhead
 * position instead of a clock generating it. CV1 carries the phase in Mode E,
 * analogous to gate1in carrying gates in Mode B.
 *
 * Phase 0→1 maps to the ACTIVE PATTERN = patternLength STEPS =
 * patternLength * PULSES_PER_STEP pulses (CC_PROMPT_PPQN_48: a STEP is 6 pulses,
 * FIXED — the span is step-indexed at ANY PPQN; PHASE_TIER Stage 1: the span is
 * the pattern length, not a hardcoded 16 — one bar at length 16/24 PPQN, two bars
 * at 32; at 48 PPQN length 16 → 96 pulses = HALF a bar — the ring twice around is
 * one bar at 48, two bars at 24). The MUSICAL 1/16 edges (sixteenthEdge, every
 * PPQN/4 pulses) and the quarter feed keep their musical meaning; the BAR modulo
 * for the quarter feed stays 16 sixteenths. The same pulse grid the gate engine
 * uses, so all timing is integer pulses with no continuous-time component.
 *
 * Mirrors ClockEngine's EDGE OUTPUT CONTRACT (pulseEdge / stepEdge / sixteenthEdge /
 * quarterEdge) so the existing execute path is reused unchanged. The phase path is
 * PULSE-QUANTISED and TEMPO-INDEPENDENT (CC_FIX_PHASE_DERIVED_BPM): it reads positions
 * from the ramp, not time — the velocity-to-tempo derivation this file carried (the
 * derived BPM / sixteenth-seconds / velocity) had no reader and is gone; the internal
 * clock keeps its own BPM readout, and the distribution warp composes per step from
 * the step content, never from a tempo. ADDS a reverse flag: when the phase ramp
 * moves backward, edges fire in reverse and the sequencer steps its playhead
 * backward (handled in SequencerEngine).
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
#include "../NoteValues.hpp"   // PULSES_PER_STEP — the step grid (CC_PROMPT_PPQN_48)

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
    double prevContPhase= 0.0;      // last *continuous* (unwrapped) phase — THE transport
                                    // accumulator (position truth; not a tempo quantity)

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
    bool  stepEdge      = false;    // ONE STEP elapsed (PULSES_PER_STEP grid pulses — the
                                    // pattern's advance edge at any PPQN; at 24 it coincides
                                    // with sixteenthEdge bit-for-bit)
    bool  sixteenthEdge = false;    // one 1/16 elapsed (MUSICAL — every PPQN/4 pulses)
    bool  quarterEdge   = false;    // one quarter elapsed
    bool  reverse       = false;    // true if motion this sample was backward
    bool  jumped        = false;    // true if a discontinuity (jump/scrub) was seen
    int   jumpSteps     = 0;        // signed # of STEPS to replay on a jump (±) — was
                                    // jumpSixteenths (the same number at 24 PPQN, where a
                                    // step IS a 1/16; CC_PROMPT_PPQN_48 renamed it: the
                                    // replay unit is the STEP, 6 pulses at any PPQN)
    long  pulsePos      = 0;        // current absolute pulse index
    bool  pulseChanged  = false;    // THE PULSE-CHANGE GATE signal (CC_FIX_PHASE_RUN_AT_PPQN,
                                    // docs tip 4cb1a749): true ONLY on the samples where the
                                    // quantised pulse position moved (targetPulse != prevPulse).
                                    // The expensive tail — jump detection and the pulse walk —
                                    // runs on those samples; between them the cheap path
                                    // just warms the trackers and ACCUMULATES prevContPhase (a
                                    // running sum across skipped samples — never reset, the
                                    // doc's DO-NOT). Monsoon gates its Mode-E snap/drift path to
                                    // this flag. The probe is the tail's OWN target expression
                                    // (llround(prevContPhase·PPB) / tierPulseFor), so firing is
                                    // bit-identical to the old per-sample step != 0.
    // Stage 2 outputs: each tier's OWN per-sample motion sign (+1 / -1 / 0 flat) — the
    // raw material for the SIGN-PRODUCT direction rule (dirT · dirS, neutral = no flip).
    // `reverse` stays the COMPOSED motion sign (the position machinery's truth); the
    // semantic direction is composed at the caller (Monsoon) from these two.
    int8_t dirT = 0;                // transport tier motion sign this sample
    int8_t dirS = 0;                // subphase tier motion sign this sample

    static constexpr int pulsesPer16th(int p) { return (p > 0 ? p : 24) / 4; }

    void reset() {
        havePrev = false;
        prevPhase = prevContPhase = 0.0; prevPulse = 0;
        pulseEdge = stepEdge = sixteenthEdge = quarterEdge = reverse = jumped = false; jumpSteps = 0;
        pulsePos = 0;
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
        pulseEdge = stepEdge = sixteenthEdge = quarterEdge = reverse = jumped = false; jumpSteps = 0;
        pulseChanged = false;
        ppqn = (ppqnSetting > 0 ? ppqnSetting : 24);
        const int p16   = pulsesPer16th(ppqn);
        // PHASE TIER Stage 1 + CC_PROMPT_PPQN_48: pulses per FULL SPAN (phase 0→1) =
        // the ACTIVE pattern length in STEPS — patternLength × PULSES_PER_STEP (a step
        // is 6 pulses, FIXED, at any PPQN). Was p16 * plen (the span in sixteenths) —
        // the same number at 24 PPQN (6·plen, bit-identical); at 48 the span is 96
        // pulses for a 16-step pattern (HALF a bar — the ring twice around is one bar
        // at 48, two bars at 24). The MUSICAL sixteenth edge keeps its own p16 cadence.
        const long plen = (long)(patternLength > 0 ? patternLength : 16);
        const long PPB  = (long)PULSES_PER_STEP * plen;   // pulses per full span (phase 0→1)

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
        // PULSE-QUANTISED, TEMPO-INDEPENDENT (CC_FIX_PHASE_DERIVED_BPM): the ramp's
        // speed is not a position quantity — the velocity-to-tempo derivation that
        // lived here had no reader (the dead Monsoon sync removed with it). The
        // sampleTime parameter stays for call-compat: the Mirrors-ClockEngine output
        // contract and every caller are untouched.

        long step = targetPulse - pulsePos;
        long mag  = step < 0 ? -step : step;

        // Discontinuity (jump/scrub): a single-sample move of more than one STEP is
        // not continuous play (the replay unit is the STEP — 6 pulses at any PPQN;
        // at 24 this is the old >one-1/16 threshold, bit-identical). Per the spec we
        // REPLAY the event chain over the crossed steps with frozen modulation — and
        // because the whole replay happens inside ONE process() call, modulation is
        // already frozen (re-sampled only next block), so the freeze is automatic
        // (§8b time-scale separation).
        //
        // We DON'T emit the pulse-by-pulse burst here; instead we report jumpSteps
        // (signed count of STEPS to replay) and let the dispatch run the verified
        // executeModeE path that many times. WITHIN-DRAW SCOPE: the replay is bounded
        // to the current phrase; a jump that would cross the phrase boundary is clamped
        // to the boundary (cross-draw regeneration is a later refinement).
        const long JUMP_PULSES = PULSES_PER_STEP;    // > one STEP in a sample = jump
        if (mag > JUMP_PULSES) {
            jumped  = true;
            reverse = (step < 0);
            // How many *whole STEPS* the jump spans (the unit the sequencer
            // replays). Round to nearest step so we land on the step grid.
            long stepsMoved = (mag + PULSES_PER_STEP/2) / PULSES_PER_STEP;
            jumpSteps = (int)((step < 0) ? -stepsMoved : stepsMoved);
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
        // (usually 0 or 1), so this loop is short. The STEP edge fires every
        // PULSES_PER_STEP-th pulse (the pattern's advance at any PPQN); the MUSICAL
        // sixteenth/quarter edges are derived from the absolute pulse index parity
        // (every p16 = PPQN/4 pulses) so they stay phase-aligned regardless of
        // direction.
        long dir = reverse ? -1 : +1;
        for (long i = 0; i < mag; ++i) {
            pulsePos += dir;
            pulseEdge = true;
            // A STEP boundary is crossed when pulsePos is a multiple of
            // PULSES_PER_STEP. Going forward the edge belongs to the pulse we just
            // ENTERED; going back, to the pulse we just LEFT — both reduce to
            // "pulsePos % PULSES_PER_STEP == 0".
            if (((pulsePos % PULSES_PER_STEP) + PULSES_PER_STEP) % PULSES_PER_STEP == 0)
                stepEdge = true;
            // A 1/16 boundary is crossed when pulsePos is a multiple of p16 (the
            // MUSICAL sixteenth — PPQN/4 pulses).
            long mod = ((pulsePos % p16) + p16) % p16;
            if (mod == 0) {
                sixteenthEdge = true;
                // THE BAR MODULO STAYS 16 (PHASE TIER Stage 1): a bar is always 16
                // MUSICAL sixteenths; the pattern can be 16 or 32 steps. step16 = the
                // sixteenth WITHIN the current bar — the quarter feed and any
                // bar-local consumer keep their meaning at 32.
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
    // TEMPO-INDEPENDENT (CC_FIX_PHASE_DERIVED_BPM): neither tier is a clock — the
    // composed position is read from the ramp VALUES alone, never from their speed
    // (the subphase is a position dial, not a clock).
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
        pulseEdge = stepEdge = sixteenthEdge = quarterEdge = reverse = jumped = false; jumpSteps = 0;
        dirT = dirS = 0;
        pulseChanged = false;
        ppqn = (ppqnSetting > 0 ? ppqnSetting : 24);
        const int p16   = pulsesPer16th(ppqn);
        const long plen = (long)(patternLength > 0 ? patternLength : 16);
        const long PPB  = (long)PULSES_PER_STEP * plen;   // pulses per full span (the STEP grid — CC_PROMPT_PPQN_48)
        // The region as a SPAN fraction: the region is MUSICAL (regionSixteenths ·
        // p16 grid pulses — the calibration's bar/half/quarter/eighth/16th), the span
        // is STEP-based (PPB = PPS·plen). At 24 this reduces to regionSixteenths/plen
        // (the old form, bit-identical: exact-integer operands, one correctly-rounded
        // division of the same real quotient); at 48 a quarter region covers twice
        // the span fraction — its musical width is preserved.
        const double rF = regionSixteenths * (double)p16 / (double)PPB;

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
        // PULSE-QUANTISED, TEMPO-INDEPENDENT (CC_FIX_PHASE_DERIVED_BPM): no velocity,
        // no derived tempo — the composed position follows the ramp values alone.

        long step = targetPulse - pulsePos;
        long mag  = step < 0 ? -step : step;

        // The SAME jump contract as process(): a single-sample move over one STEP is a
        // discontinuity — report jumpSteps, let the dispatch replay the crossed
        // steps (the arrival rule at region landings) and snap. A region crossing moves
        // exactly regionSixteenths·p16 pulses ≥ 4·p16 > PULSES_PER_STEP — always detected
        // (at 24 p16 = PULSES_PER_STEP — the old threshold, bit-identical).
        const long JUMP_PULSES = PULSES_PER_STEP;
        if (mag > JUMP_PULSES) {
            jumped  = true;
            reverse = (step < 0);
            long stepsMoved = (mag + PULSES_PER_STEP/2) / PULSES_PER_STEP;
            jumpSteps = (int)((step < 0) ? -stepsMoved : stepsMoved);
            pulsePos = targetPulse;
            prevPulse = targetPulse;
            return;
        }

        // (The old step == 0 early-out is unreachable past the gate — the same
        // prevPulse == pulsePos invariant as process(): targetPulse != prevPulse
        // ⟹ step != 0.)

        reverse = (step < 0);

        // The continuous walk — one pulseEdge per crossed pulse, the STEP edge every
        // PULSES_PER_STEP pulses, the MUSICAL sixteenth/quarter edges from the absolute
        // index parity, THE BAR MODULO STAYS 16 (Stage 1).
        long dir = reverse ? -1 : +1;
        for (long i = 0; i < mag; ++i) {
            pulsePos += dir;
            pulseEdge = true;
            if (((pulsePos % PULSES_PER_STEP) + PULSES_PER_STEP) % PULSES_PER_STEP == 0)
                stepEdge = true;
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
