# CC PROMPT  Scale mask arbiter: guard the 12-tone assumption (REVISED: no width change)

REVISED after confirming the scope. Authority: src/dsp/ScaleMaskArbiter.hpp, MonsoonScaleManager.*, and the callers.

## FACTS (verified)
- resolveScaleMask / rotateMask12 are 12-bit. Their callers are Monsoon (authored and factory masks), the Shophouse
  expander (override layer), and MonsoonScaleManager (the wrapper).
- Monsoon and Shophouse can only be set to a 12-tone tuning. The 24-degree tuning exists only in ColonnadesDuo, which
  does not call the arbiter.
- Colonnades (12) and ColonnadesDuo (24) get their scale state from MicroTuningModule's tuning table, not from the mask.

So there is NO truncation today. The arbiter is correct for its domain. The remaining risk is that the 12-tone assumption
silently breaks if a future change routes a 24-degree tuning into Monsoon or Shophouse.

## TASK 1  Guard the assumption
- Add a compile-time and/or runtime guard: the arbiter's width is a named constant (kArbiterWidth = 12). Any caller passing
  a tuning with degree count != kArbiterWidth fails loudly (static_assert where the width is known at compile time, a
  debug assert or a logged rejection at runtime otherwise). It must not silently truncate.
- Document the constraint in the arbiter header: "12-EDO pitch-class gate. Microtonal degrees (ColonnadesDuo, N = 24)
  do not pass through this arbiter."

## TASK 2  Tests (test/test_ScaleMaskArbiter.cpp)
- Priority: override beats authored beats factory beats all; each valid flag respected.
- Width guard: a 24-degree tuning presented to the arbiter is REJECTED (not truncated), with the test asserting rejection.
- Rotation: rotateMask12 by k in [-12..12] wraps modulo 12; rotate then inverse is identity.

## GATES
- run_all green on both SDK legs.
- Behaviour bit-identical for every existing caller (all 12-tone): no re-pins.
- ColonnadesDuo path untouched.

## DO NOT
- Widen the arbiter. That is a separate decision, and only if Monsoon or Shophouse ever needs 24-tone masks.
- Change the priority order or Shophouse override semantics.
- Touch the feat/microtonal branch's doc and panel commits.

## REOPEN CONDITION
Revisit the width only if a future feature routes a microtonal tuning with more than 12 degrees into Monsoon or Shophouse.
