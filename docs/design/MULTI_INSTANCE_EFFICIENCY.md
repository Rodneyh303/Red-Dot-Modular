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


## CORRECTION / clarification — SHARED SEED vs PRNG-SLAVE are DIFFERENT things (Rodney)
Earlier framing conflated them. They are distinct and apply to DIFFERENT counter regimes:
- **SHARED SEED = a MUSICAL FEATURE (crab/canon).** Same seed, DIFFERENT counters -> correlated-but-offset
  material. The seed-share IS the correlation (why they sound related); the counter offset is the canon.
  Each instance computes its OWN draws (different counter -> different draws). NOT an optimization — it's
  the feature. Cost lever here = VOICES (few per instance), since draws genuinely differ (nothing to share).
- **PRNG-SLAVE = an OPTIMIZATION (shared-counter polyrhythm).** Same seed AND same counter -> the draw is
  IDENTICAL -> don't compute it twice; the slave READS the master's draw. Same musical result, the draw
  computed ONCE. Only applies where counters COINCIDE (draws provably identical). Where counters differ
  (crab/canon) there is nothing to slave.

So they do NOT obviate each other — they occupy different counter regimes:
| case | seed | counter | material lever | voice lever |
| shared-counter polyrhythm | shared | SAME | PRNG-SLAVE (draw once) | voice-opt |
| crab/canon | shared | OFFSET | none (draws differ, each computes) | voice-opt |
| independent | different | different | none | voice-opt |
- VOICE-OPT applies to ALL cases. SHARED CA is ORTHOGONAL (links arrangement, any case).

(This supersedes the earlier "shared seed -> shared cache keyed by (position,key)" phrasing above: that
cache IS the PRNG-slave implementation for the SAME-counter case; shared-seed-different-counter is the
crab/canon feature where each computes.)

## Computational benefit — TBD, but surely helps the multi-Monsoon case
The exact benefit of PRNG-slave (and of voice-opt across instances) is TO BE ESTABLISHED by measurement
(depends on the post-16x per-instance cost breakdown — draw vs readout vs output). But directionally it
MUST help the multi-Monsoon goal: shared-counter instances stop recomputing identical draws, and every
case pays only for active voices. MEASURE once the single-instance breakdown is known; don't assume the
magnitude.

---

## NEXT PERF PRIORITY — VOICE-COUNT OPTIMIZATION (the biggest win) (Rodney)
Voice-count opt is the biggest/next perf win — bigger AND lower-risk than the SIMD candidates:
- **END-TO-END:** skips inactive voices through the WHOLE per-voice pipeline (draw processing, spread
  apply, readout, slew, quantise, output, display) — not one stage. SIMD-ing one stage (spread apply /
  MA reduction) speeds that stage ~4x for all 16 voices; voice-opt makes inactive voices cost ~0 across
  EVERY stage.
- **MULTIPLIER:** process N not 16 -> ~16/N x on ALL per-voice cost, compounding across stages.
- **ENABLED BY THE 16x:** fast draws make the buffer recompute cheap enough to "just do extra channels"
  on demand (before 16x, adding voices = expensive regen -> risky dynamically; now cheap).
- **THE MULTI-INSTANCE LEVER:** few voices per instance -> N Monsoons realistic (4 inst x 4 voices = 16
  voice-slots, not 64). Directly serves the goal.
- **LOWER-RISK than SIMD:** no determinism tension, no float reordering — just DON'T process inactive
  voices. Biggest win is also the simplest/safest.
Mostly ALREADY SPECCED: it's the active-voices Phase B (downstream gating) + the fast recompute (16x +
bidirectional buffer) + the mutual lock (poly-change PERP counter-change, at a step boundary). Implement:
on voice-count change, at the next STEP boundary, spin up the delta voices (cheap regen), existing voices
untouched.

### Buffer-recompute time ESTIMATE (order of magnitude; measure to confirm)
- **Case A — multivariate draws (our design: each draw is ALL voices):** buffer ALREADY holds all voices
  -> adding channels = just start READING the new voice slots -> **~0 / near-instant, no spike.** The only
  new cost is steady-state processing of the added voices, not a one-time recompute.
- **Case B — if buffer is active-voices-only and must regen the new window:** ~128 positions
  (bidirectional K=64) x per-position draw (cheap post-16x, ~tens of ns). Full-window all-voices regen
  ~= **single-digit microseconds (~2-7us)** = **~0.01-0.04% of a 60Hz frame (16,667us)** -> negligible,
  no glitch. Delta-voices-only is proportionally less.
Either way the recompute is CHEAP-to-FREE -> "just do extra channels" is sound, no glitch. Estimates from
the ~130-250ns copula figure x the 16x; CONFIRM by measuring (add voices mid-play, watch CPU meter, or
time the regen via the debug timer at Monsoon.cpp:1381).


## FINAL SIMPLIFIED FORM — voice-opt = cap the downstream per-voice loops (Rodney)
Since the buffer REDRAW is now cheap (16x), skipping draws for unused voices saves ~nothing -> don't
bother. And the downstream is ALREADY done PER VOICE (per-voice loops). So voice-opt collapses to:
- **Draws: always all 16** (multivariate + cheap — leave them; no conditioning, no buffer resizing).
- **Downstream: cap the per-voice loops at activeCount** (spread apply, readout, slew, quantise, output,
  display). The only change is the LOOP BOUND (0..16 -> 0..activeCount).
- **Voice-add: INSTANT loop-widening** — the draws are already there (all-16), so adding a voice just
  runs more downstream iterations. NO regen, NO history problem, NO draw-side concurrency race. Supersedes
  the earlier "regen the delta voices" framing — there is nothing to regen.
This is the BEST kind of optimization: biggest benefit (end-to-end per-voice cost skipped), SIMPLEST
(change loop bounds), LOWEST-risk (no determinism, no regen, no draw conditioning), INSTANT voice changes.
= the Phase B "downstream active-voices gating" — already partly done (some loops already use
numPolyVoices).

### Implementation = audit the downstream per-voice loops for consistent activeCount bounding
Task: grep every downstream per-voice loop (for (v...)) in the processing stages; confirm EACH caps at
activeCount / numPolyVoices; fix any hardcoded 16 / NUM_VOICES. If spread caps but output doesn't, you
still pay for inactive voices — so it must be CONSISTENT across ALL downstream stages.
EASIER NOW after the poly-voice addressing debt (V1->voice-0) is fixed: voices are uniform (V1 = voice 0,
no mono special path), so "0..activeCount" means the same thing everywhere — no V1-exception to special-
case in the bound. (Pre-unification, a loop might have been 0..15 poly + a separate mono path; now it's
one uniform 0..activeCount.)
