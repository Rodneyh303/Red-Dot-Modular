// StepAdvance.hpp
// ──────────────
// THE STEP-ADVANCE DECISION (CC_PROMPT_GATE_STEP_ADVANCE_SPEC, Stages 1-3) — pure, rack-free,
// testable in isolation like RhythmDecisionCore (the spec's §"RELATION TO THE RDM" law: keep it
// a clean function decideAdvance(p, u) so it is testable like the rhythm predicates).
//
// In GATE MODE (Mode B), Variation is repurposed to STEP-ADVANCE PROBABILITY: on a gate rise,
// p(advance the playhead) vs p(repeat the current step). Rest controls whether you HEAR a step;
// advance controls whether you VISIT one — a SEPARATE decision at the advance site, not one of
// the RDM's five rhythm predicates.
//
// THE MAPPING (the spec's concrete table):
//   p >= 0 : FORWARD  iff u < p,  else REPEAT        (p = +1 → always forward = today's identity)
//   p <  0 : BACKWARD iff u < -p, else REPEAT        (STAGE 2 — bipolar, reserved: every source
//                                                     this build is [0,1], so p < 0 is unreachable;
//                                                     the branch is implemented + tested so Stage 2
//                                                     is a sourcing change, not a logic change)
//   p == 0 : always REPEAT (stutter).
//
// REPEAT semantics (Rodney, spec correction 91b100c — there is NO held-vs-redraw choice; it is a
// FALSE distinction): the random data is STEP-ADDRESSED (the Philox counter / strand index for
// that step). If the playhead does not advance, the step is unchanged → the SAME counter position
// → the SAME random data → the SAME result, BY CONSTRUCTION. "Repeat" = re-output the same step's
// data because you are reading the same address. The engine implements REPEAT by simply NOT
// calling advancePlayhead — no re-draw branch exists or may exist.
//
// ONE PLAYHEAD (the spec's constraint): main + subgates share ONE playhead; each gate rise that
// passes its level's check moves the shared playhead. Direction (BACKWARD) is Stage-2 sourcing;
// with unipolar sources the playhead only ever moves forward this build.

#pragma once

namespace dotModular {

struct StepAdvance {
    enum Decision : int { BACKWARD = -1, REPEAT = 0, FORWARD = 1 };

    // Pure decision from (resolved probability, uniform draw). u ∈ [0,1) (the house Philox
    // uniform). Deterministic, no state, no Rack types.
    static constexpr Decision decide(float p, float u) {
        if (p < 0.f) return (u < -p) ? BACKWARD : REPEAT;   // STAGE 2 (reserved — unreachable this build)
        return (u < p) ? FORWARD : REPEAT;                  // STAGE 1/3 unipolar advance
    }
};

} // namespace dotModular
