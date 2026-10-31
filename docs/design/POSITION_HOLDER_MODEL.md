# POSITION-HOLDER MODEL — kill the global step counter (part of the UAS fix-up, BEFORE 32 steps) (Rodney)

## THE DIAGNOSIS (Rodney) — the global step counter is a dumb construct/dependency
Currently every read position is a function of ONE global counter (totalStepsElapsed): laneTick_/laneTickV_
are CACHES recomputed from it via LOR. This was defended on reasons that DO NOT survive scrutiny:
- "Reproducible randomness needs it" — FALSE. Philox draws are counter-addressed in their OWN space,
  DECOUPLED from step position (dice/dice-scrub traverse the stream forward/back independent of step count).
  Randomness reproducibility comes from Philox, NOT from position-being-a-function-of-a-global-counter.
- "No drift" — defensive, and MOOT: the instrument now INTENTIONALLY desyncs voices (per-voice advance,
  per-voice stutter), so "voices must never desync" is no longer even a goal.
- "Reproducible position" — holds under LOCAL positions too (they're deterministic from decisions+seed).
So the global counter protects nothing essential. Worse, it CONFLATES three separable axes — VOICE, LANE,
POSITION — into one timeline, so every per-voice/per-lane feature (per-voice LOR already desyncs lanes;
per-voice advance desyncs voices; containment) is FIGHTING the architecture, working around the global
counter. It is an artificial UNIFICATION of things that are naturally independent.

## THE NATURAL MODEL — per-(voice,lane) position-holders, NO counters
Position is a per-(VOICE,LANE) value, not a voice property and not a function of a global counter. No
counters (global or per-voice) are needed at all:
- Each (voice, lane) holds a CURRENT read position — one integer it owns.
- On its advance event (the step tick for most lanes; the per-voice step-advance decision for the
  advance-driven case), it updates LOCALLY: pos = wrap(pos ± dir). Hold = unchanged. No accumulation
  history, no global timeline.
- It reads the pattern/probability at pos, and Philox at its own (decoupled) index.
- LOR = the PARAMETERS of its own walk (length = wrap point, offset = start, rotation = initial phase,
  direction). Not a lens onto a shared timeline — the walk's own settings.
- "Voice"/"lane"/"position" are separable: they just name WHICH holder. Not conflated into one index.
This is how most stochastic sequencers work (independent tracks/voices each owning their position). The
global-counter design is the unusual choice — synchronized-by-default — now the wrong shape for an
independence-first instrument.

## WHY BEFORE 32 STEPS (Rodney — sequencing)
- 32 steps on the CURRENT model = audit every `& 0x0F` wrap + every "function of the global counter" site,
  extending a construct we're about to replace = WASTED effort.
- Position-holders are LENGTH-AGNOSTIC by nature: each holder wraps at its OWN LOR length (16, 32, anything).
  Once positions are local, 32 is almost FREE — no global-modulus audit, no wrap-mask hunt; each holder just
  wraps at its own length.
So: fix the architecture (position-holders) FIRST, then 32 steps falls out cheaply. NOT the reverse.

## PLAN
- This is part of the UAS FIX-UP (UAS already moved toward per-lane/voice addressing; this completes it by
  removing the global-counter dependency underneath).
- Migrate laneTick_/laneTickV_ from "cache recomputed from totalStepsElapsed via LOR" to "per-(voice,lane)
  position-holder updated locally per its advance event; LOR = the walk's parameters".
- STAGING: a refactor that is BIT-IDENTICAL first (holders initialised/advanced to reproduce exactly the
  current global-counter-derived positions), proven on the golden battery, THEN the independence (per-voice
  advance, 32 steps) rides on top for free.
- Philox UNCHANGED (already decoupled — this touches POSITION, not randomness).

## STATUS OF THE OFFSET-ON-LOR BRIDGE (PER_VOICE_STEP_ADVANCE_PLAN.md)
The "advanceOffset[voice] on top of LOR(totalStepsElapsed)" approach is a PRAGMATIC BRIDGE that keeps the
global counter. It is a FALLBACK only — if the position-holder migration proves too big to do now. The
TARGET is the position-holder model (no global counter), not the bolt-on. Prefer the migration since it is
part of the UAS fix-up and makes 32 steps cheap; use the bridge only if the migration must be deferred.
