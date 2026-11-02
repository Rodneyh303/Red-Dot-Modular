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

════════════════════════════════════════════════════════════════════════════════
## SEQUENCING — poly advance FIRST (its tests de-risk the counter removal) (Rodney)
Supersedes "cleanup first". The order is:
1. POLY STEP-ADVANCE first, via the offset-on-LOR BRIDGE (advanceOffset[voice] on the current counter),
   WITH a thorough PER-VOICE-DIVERGENCE unit-test suite (voice 1 advances while voice 3 holds → they read
   these positions; backward; wrap at phrase; global-locked == bit-identical to mono).
2. COUNTER REMOVAL (readers → UAS, one at a time, bit-identical) — proven by BOTH the existing goldens
   (synchronized behaviour) AND the step-1 divergence tests (per-voice behaviour).
3. 32 STEPS — nearly free under the clean model (length-agnostic position mapping).

### WHY THIS ORDER (the testing argument — Rodney)
The counter removal's WHOLE PURPOSE is to enable per-voice DIVERGENCE, but the existing goldens only test
SYNCHRONIZED (mono) behaviour. Remove the counter first and you prove "synchronized unchanged" while the
case that matters (voices at different positions) has NO coverage. Build poly advance FIRST, with divergence
tests, and those tests become the coverage that proves the counter removal preserves DIVERGENT behaviour,
not just synchronized. You do the scary refactor WITH coverage of the scary case.

### "BUILD TWICE" is NOT a problem — the TESTS are the durable asset
The bridge implementation (advanceOffset on the counter) gets REWORKED when the counter dies — but the
TESTS do not. The tests pin BEHAVIOUR ("these voices read these positions"), which must hold identically
before and after. So: feature built once (reworked onto the clean model), test suite written once and
SURVIVES the rework, VALIDATING it. The bridge is cheap/throwaway; the divergence test suite is the valuable
durable thing, and it only exists because poly advance was built first. The rework is SAFE precisely because
those tests prove it.

### BONUS
Ships the cool feature (per-voice stuttering — nearly dumped) NOW, and it does double duty as the safety net
for the counter cleanup. Musical payoff + de-risked refactor from one piece of work.

════════════════════════════════════════════════════════════════════════════════
## READER INVENTORY (refreshed post-UAS, HEAD bd8f0b1) — the counter is ALREADY mostly a pure driver
Grepped every totalStepsElapsed site. Most of the 23 hits are COMMENTS explaining the counter. The ACTUAL
code sites are few and cleanly classified — the removal is SMALL, because UAS already made nearly every
read go through driverTickAt. Classification:

### WRITE sites (where the counter is SET — these define "current position", the irreducible state)
- :148   reset()           → totalStepsElapsed = 0        (zero on construct/reset)
- :384-385 advancePlayhead → ±1 per advance              (THE increment — the only mutation in play)
- :785   phase-jump snap   → = absStep + 1               (Mode E: snap to a phase-derived position)
- Monsoon.cpp:562 hard reset→ = 0                          (Beat-1 sync)
These are the "current global step" the position model needs. They STAY (as the single current-position
value, advanced on the clock event) — they are NOT the redundancy. The redundancy is the RECOMPUTE-FROM-IT.

### READ sites (consume the counter to compute positions) — ALL already go through driverTickAt
- :417, :419  advancePlayhead recompute → driverTickAt(totalStepsElapsed [± dir], …)    [through UAS]
- :662        per-voice effective clock → totalStepsElapsed + advanceOffset_[v]          [through UAS]
- :731, :733  recomputeLaneTicks       → driverTickAt(totalStepsElapsed [± dir], …)      [through UAS]
- :861        step-light brightness    → driverTickAt(totalStepsElapsed, …) - 1 - …      [through UAS]
So EVERY position read is ALREADY driverTickAt(totalStepsElapsed, …) — the UAS mapping. There are NO direct
raw-counter reads that bypass the mapping (the DNA_LCM kill + UAS already did that cleanup). 

### DIRECT (int) reads — the only non-mapping consumers
- :1928, :1984  DEBUG/trace printf → (long long)totalStepsElapsed   (diagnostic logging only; not logic)
No (int) casts into logic remain (the DNA_LCM-era int-cast sites are gone).

## WHAT THIS MEANS — [CORRECTED] the inventory proves the SMELL, it does NOT dissolve it
RETRACTION: an earlier version of this section concluded "nothing to kill, UAS already fixed it". THAT WAS
WRONG and contradicted the brainstorm at the top of this doc. The brainstorm's point is NOT "a redundant
PARALLEL counter alongside UAS" — it is that POSITION-BEING-A-FUNCTION-OF-ONE-GLOBAL-COUNTER is itself the
smell, however clean that function is. The inventory shows EVERY read is driverTickAt(totalStepsElapsed, …)
— i.e. every position is f(the one global counter). That is EXACTLY the thing to kill: positions should be
OWNED, LOCAL, per-(voice,lane) holders (pos = wrap(pos ± dir) on the holder's own advance), NOT recomputed
from a shared global counter. A tidy f(counter) is still "everything is a function of the global counter" —
the artificial unification of voice/lane/position the brainstorm rejects.
So the inventory is the SITE LIST for the migration, not a reason to skip it:
- WRITE sites (:148 reset, :384-5 advance ±1, :785 phase-snap, Monsoon:562 hard-reset) — the global counter
  mutations; under position-holders these become the global clock EVENT that TICKS the holders (each holder
  advances itself), not a counter everything is recomputed from.
- READ sites (:417/419/662/731/733/861 via driverTickAt) — these are the recompute-from-counter reads to
  REPLACE with "read the holder's own owned position".
- :1928/1984 debug printf — diagnostic, drop or adapt.

## PLAN (restored) — migrate to position-holders; THEN 32 steps is free
Per the brainstorm/staging above: migrate laneTick_/laneTickV_ from "cache recomputed from totalStepsElapsed
via LOR" to "per-(voice,lane) holder updated locally on its advance event". STAGING: bit-identical first
(holders reproduce the current counter-derived positions exactly — golden-proven), THEN 32 steps + further
independence ride free. The global clock event still exists (the transport tick) — it TICKS the holders; it
is no longer the thing positions are a FUNCTION of. Philox unchanged.
32 STEPS comes AFTER (position-holders are length-agnostic → each wraps at its own length → no global-modulus
/ &0x0F audit; 32 nearly free). STEP_COUNT_DECISION.md + LIGHT_RING_32_STEPS.md.
