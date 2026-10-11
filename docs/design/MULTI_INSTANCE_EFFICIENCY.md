# Multiple Monsoons realistic — polyrhythm + CA sharing efficiency (Rodney)

GOAL: enough efficiency that MULTIPLE Monsoons in one patch is realistic — because polyrhythm and
CA-sharing are MULTI-INSTANCE features (independent timebases can't come from one instance; the gate
drives the single master step). So per-instance cost must be low enough for 3-5+ instances.

16x faster draws (4x batching + 4x SIMD) is the foundation. On top, TWO cost levers that COMPOSE:

## Lever 1 — SHARED COUNTER -> SHARED DRAW (polyrhythm case)
Instances on the SAME counter position read the SAME multivariate draw. The draw is all-voices-all-steps
by construction, so ONE draw at the shared counter serves ALL instances there — each reads its own voice
slots from the one computed draw. Compute the draw ONCE, not per-instance.
(Same material, same position, DIFFERENT RATE = polyrhythm of correlated material.)

## Lever 2 — VOICE OPTIMIZATION (both cases, esp. crab/canon)
Each instance processes only its ACTIVE voices (active-voices-only, Phase B). A multi-instance patch uses
FEW voices per instance, so total voice cost = SUM of active voices across instances, not N x 16.
- CRAB/CANON case: DIFFERENT counters, OWN PRNG (no draw sharing — independent material/position). Cost
  controlled by voices only (e.g. 4 each).

## They COMPOSE (the win)
SHARED-COUNTER polyrhythm gets BOTH levers. Example: 2 instances, shared counter, 4 voices each = 8 total:
  cost ~ ONE shared draw + 8 voices  (vs naive 2 standalone = 2 draws + 2x16 voices).
4 instances x 4 voices on a shared counter ~ 1 shared draw + 16 voices = about ONE Monsoon's cost for a
FOUR-instance correlated polyrhythm.
Benefit profile:
- SHARED counter (polyrhythm): shared draw AND voice-opt -> cheapest.
- INDEPENDENT counter (crab/canon): voice-opt only -> still much cheaper than full instances.
Net: N instances cost like ~1-2, not N.

## CONCURRENCY — mutual lock (poly-change PERP counter-change)
A poly-voice-count change and a counter change BOTH trigger buffer regen. Each is CHEAP provided the
OTHER input is held still:
- Change voices while counter FIXED -> cheap rebuild (voice delta at the stable current position).
- Change counter while voices FIXED -> cheap (bidirectional buffer extends; voice set stable).
- BOTH at once -> expensive/racy (rebuilding against two moving inputs).
So LOCK them mutually exclusive: one operation's regen settles before the other starts. This is both a
correctness rule and a PERFORMANCE rule (each rebuild is cheap because the other is still). Doable NOW
because the 16x makes the delta regen (new voices at current counter, or new counter positions at current
voice set) fast enough to do on-change without a glitch.

## Engineering notes
- Shared draw across instances needs a SHARED draw cache keyed by (position, key) — linked instances
  (same seed/key) hit it automatically; independent instances (different key) don't (correct). The
  seed/CA-link connection is the natural channel to carry the shared-cache pointer (VCV modules are
  isolated; a cross-module shared cache needs a deliberate mechanism via the link).
- Active-voices-only is the Phase-B downstream optimization (already specced).
- Pairs with the counter/offset-scrub integration (slew/mix/dice) and the bidirectional buffer.
