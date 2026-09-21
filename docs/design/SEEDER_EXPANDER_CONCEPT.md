# Seeder expander — concept stub (Rodney)

STATUS: CONCEPT STUB. Not specced. Captures the idea + why it's the right shape.

## What it is
A small expander that emits a SEED (and likely a reseed/restart trigger) which MULTIPLE Monsoons consume,
establishing a shared **determinism domain**: every Monsoon on the same seeder generates identical draws
by construction (the Philox spine is deterministic), with no shared mutable state.

## Why it exists — it replaces sharing/coupling with shared determinism
The recurring need is "two (or N) Monsoons that generate the SAME probabilities." The tempting solutions
were shared Sands or a cross-feed edge (one expander owned by A, feeding B). Both add asymmetric state and
a primary problem. The seeder makes them unnecessary:
- Same seed + deterministic generation ⇒ identical probabilities in each Monsoon, independently computed.
- Each Monsoon keeps its OWN Sands (and own Interchange/Raffles/Junction), reading its own local copy of
  the same numbers, and may modulate them the SAME or DIFFERENTLY.
- Correlation WITHOUT coupling. No shared expander, no primary, no cross-feed edge.

This also names a pattern the code already relies on manually: shared-CA recipes (crab canon) hand-feed
identical seeds to both Monsoons + CA. The seeder makes "these N Monsoons share a determinism domain" an
explicit, visible, one-connection fact instead of a hand-maintained discipline — and may become the
CANONICAL way to set up shared-CA / polymeter / crab-canon rigs.

## Connection model (per CONNECTION_UI_MODEL.md)
- Pure SOURCE: it emits; nothing reads back into it; no shared mutable state.
- Therefore **needs NO primary** (fails the §10 predicate — no asymmetric read-back/mutation).
- It's a one-to-many BROADCASTER (Model-C-like): one seeder → N Monsoons, each an independent consumer.
- The easy, asymmetry-free kind of sharing. Does not reopen the primary/ownership problem.

## What it emits — [OPEN]
- A seed value (how encoded — CV? a typed message on the expander bus?).
- Probably a shared reseed/restart trigger so the whole domain re-rolls together (ties to the per-stream
  reseed-on-restart flags + the CA reseed decision — a seeder restart could be the domain-wide restart).
- [OPEN] Does it carry ONE seed for the whole domain, or a small set (per-stream seeds: rhythm/melody/
  qmix) so streams can be independently anchored across the domain?
  NOTE: canon-by-determinism does NOT need per-stream seeds (see below), so this stays a single-seed
  broadcaster unless another gesture demands the split.

## Shared-CV modulation is a SEPARATE, already-available axis (Rodney)
Two Monsoons can also be modulated by the SAME live CV without any new module, by patching one CV to both.
Two distinct injection points, musically different:
- **Probability-READ side** — same CV to both Monsoons' Interchange / Raffles / Junction inputs (the
  die-action / read-side mod). Both read their (identical, if seeded together) probabilities the same way.
- **Probability-MOD side** — same CV to both Monsoons' Sands (the per-voice probability modulation).
This is ORTHOGONAL to the seeder: the seeder makes the underlying draws identical; shared CV makes the
live modulation of those draws identical (or, patched differently, deliberately divergent). Seeder =
same material; shared CV = same (or different) live shaping. Together they give correlated-but-controllable
behaviour across Monsoons with NO shared mutable state and NO primary — the whole "share generation"
problem dissolved into (a) shared seed and (b) shared CV, both broadcast, both asymmetry-free.

## Canon-by-determinism — a delayed TRIGGER, not a seeder feature (Rodney)
Striking capability that falls out for free: one Monsoon reading the SAME dice sequence a phrase or two
BEHIND the other = canon/round on the generative DECISIONS (not an audio echo). Same seed ⇒ both produce
the identical roll sequence whenever each fires; offset WHEN B advances and B walks A's path later.

Crucially this needs NO seed buffering, NO delayed seed tap, and NO per-stream seeds. The dice roll is
already gated to the phrase boundary, so you only delay the TRIGGER:
- Patch a phrase pulse through a delay (phrase-length shift / a second phrase clock offset by N phrases)
  into B's roll trigger input. Shared seed guarantees "same roll"; the delayed trigger sets "later".
- To offset ONE stream (e.g. melody a phrase behind, rhythm locked): delay only B's MELODY roll trigger,
  leave its rhythm roll trigger aligned. rhythm/melody/qmix already have SEPARATE roll triggers, so the
  per-stream independence is in the TRIGGER ROUTING, not in the seed. This is NOT an argument for
  per-stream seeds.
So: canon = shared seed (broadcast) + offset roll trigger (patched). Two asymmetry-free signals, no new
seeder mechanism, seeder stays single-seed.

## What this does NOT give you
Live COUPLING: if Monsoon A is scattered by a runtime CV and you want B to follow A's *deviation*,
seed-sharing can't express it (each computes independently). That genuinely needs shared state / a feed
edge. Rarer, less musical than lockstep generation; left unbuilt unless a real gesture demands it.

## Next
Decide seed encoding + whether per-stream seeds; then whether the seeder subsumes the manual seed-patching
that shared-CA recipes use today (likely yes → simplifies CA_SHARED_EXPANDER docs).
