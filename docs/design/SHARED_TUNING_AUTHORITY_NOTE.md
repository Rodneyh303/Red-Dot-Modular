# Sharing a tuning authority across Monsoons — decision note (Rodney)

STATUS: DECISION NOTE / [OPEN]. Question: should Sikit, Colonnades, Colonnades Duo, and Shophouse
(+ Shophouse Micro) be allowed to serve MULTIPLE Monsoons? Scopes both Colonnades variants and both
Shophouse variants.

## What a tuning authority actually does
Publishes `cents[]` + `weight[]` + `maskAuthored` into the host's TuningTable (`engine.pe.tuning`, one per
Monsoon). Root cents locked at 0 (Scalar rule). Model A delegation: the Monsoon reads its scale mask from
the claimant Micro. Today one Monsoon arbitrates Sikit vs Colonnades vs Shophouse for its SINGLE tuning
claim (Sikit preferred; loser greys).

## KEY DISTINCTION — this is BROADCAST (write-fan-out), not shared-read like CA
CA sharing = many Monsoons READ one correlation state (shared mutable state → needs a primary).
Tuning sharing = one authority WRITES the same scale into several Monsoons' OWN TuningTables. There is no
shared cell — N private tables receive identical broadcast values. So:
- **No primary needed.** Fails the primary predicate (no shared-state mutation, no read-back). It's the
  SEEDER pattern: one source, many independent consumers, asymmetry-free.

## PROS
- Musically coherent / expected: a shared scale across a polymeter or crab-canon rig. Canon partners on
  one maqam without hand-syncing two authorities.
- Composes with the shared-determinism story: shared seed (same material) + shared tuning (same scale) +
  shared CV (same shaping) = "these Monsoons are one instrument", all broadcast, all primary-free.
- Scala/.scl import once → whole rig retuned, vs importing the same file into each authority.

## CONS / constraints
- **Capacity must match.** A Micro's degree count must equal the host's (the 12/24 mode check; Shophouse
  Micro capacity must match the bound Micro). Sharing is only valid across Monsoons of matching N —
  enforce it, and surface WHY binding fails when N differs, or it's a confusing silent non-bind.
- **Two-dimensional contention.** Today: which authority wins on one Monsoon. With sharing: which authority
  wins on each Monsoon AND which Monsoons each authority serves. More claim state to reason about.
- **Per-Micro CV mod fans out.** The Interchange→weight[] path means a shared Colonnades' weight
  modulation reaches ALL served Monsoons. May or may not be wanted.
- **Mask enforcement becomes uniform.** `maskAuthored` / scale-guide enforcement is per-Monsoon engine
  state; a shared authority enforces identically everywhere, losing "same tuning, different per-Monsoon
  mask enforcement" as a degree of freedom.

## Shophouse both variants — the sharpest case
Shophouse Micro DRIVES its bound Micro's cents knobs + enabledState (scene-front fold) — it's ACTIVE/
animating, not a static publish. A shared Shophouse's scene changes would re-tune the whole ensemble
LIVE with one gesture. That's the MOST musical version of sharing and the MOST stateful simultaneously.

## RECOMMENDATION — check the broadcast answer first (same as shared-Sands)
Ask: do you need one SHARED authority, or N authorities producing the SAME scale?
- **Static scale** (a mode, a .scl import, a preset): duplicated authorities each holding the same scale
  gives shared tuning with NO shared module — no capacity-match constraint across the share, no
  two-dimensional contention. Prefer this; it's the seeder-style dissolve that made shared-Sands
  unnecessary. Only build actual sharing if duplicated-same-scale proves too clumsy in practice.
- **Live scene-driving** (Shophouse Micro animating tuning across the ensemble): broadcast of a STATIC
  scale can't express a shared live GESTURE. Either share the module, or (per the canon finding) send the
  same trigger/CV to duplicated Shophouse authorities. THIS is the case that genuinely justifies sharing.

So: the motivating case is live Shophouse scene-driving across an ensemble; the static-scale case is
better served by duplicated authorities + same scale (no sharing). If sharing IS built, it's broadcast/
no-primary, gated on capacity match, with the CV-fan-out and uniform-enforcement consequences made
explicit. [OPEN — decide after q-mix / CA output side; not on the critical path.]
