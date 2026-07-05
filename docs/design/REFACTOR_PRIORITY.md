# Refactor priority — reconciling the dataflow plan and the codebase review

Purpose: one ordering. `CODEBASE_REFACTOR_REVIEW.md` (inventory of specific instances +
hygiene) and `DATAFLOW_DISCIPLINE_PLAN.md` (root-cause pattern + preventive machinery) are
**not competing** — this doc merges them so there is a single priority list, not two.

## Is the codebase review still relevant? Yes — validated, and one item re-rated up.

Spot-checked the top items against `integration/suite-12` (this session's date):

| Item | Review rating | Current state | Verdict |
|---|---|---|---|
| A1 strand→field 6× switch | HIGH / LOW | still 6 hand-written switches | **open, still valid** |
| A3 dead `SpreadInterpolation.hpp` | MED / TRIVIAL | still present in built tree | **open, still valid** |
| C2 `polyBaseActive` scattered | ~~MED / TRIVIAL~~ → **HIGH** | **3 different defs across 5 sites** (`ptr&&voices>=1` vs `ptr only` vs hardcoded `true`) | **open — RE-RATED UP: this is the root of this session's Sands-Helix bug and a suspect for the live StrandLedger conflict** |

Nothing in the review was made obsolete by the transit/poly refactors. If anything those
refactors are *where this session's bugs came from*, so the review got **more** relevant.

## How the two docs relate

- Dataflow plan = **why** the review's §A items keep biting (push/copy/gate/stale-derived) +
  the **preventive machinery** (write-conflict detector).
- Codebase review = **which specific instances** to fix (A1, C2, A3, …) + adjacent hygiene
  (dead code, test gaps, RT-safety F1–F9).

They interlock: C2 (`polyBaseActive` five ways) **is** an instance of the dataflow plan's D4
("one resolver per derived concept"). A1 (strand→field 6×) is D4 for the strand mapping. The
dataflow detector (Step 1) is what makes the review's risky items *safe to attempt*.

---

## Field finding (Tier 1 fix): the StrandLedger conflict was a SIX-source topology bug

Fixing the live `MACRO-then-EAST` conflict (`fix/strandledger-macro-east-conflict`) surfaced the
scale of the multiple-sources problem. **`SandsTopology::build()` is called at 6 independent sites**,
each populating `eastPresent`/`macroPresent` from a *different* source that can disagree:

| Site | eastPresent from | macroPresent from |
|---|---|---|
| East widget `topologyFor` (549) | `true` (self) | cache |
| East widget V1 (923) | `true` (self) | `macroAttached()` |
| Macro widget (227) | cache | `true` (self) |
| Mono-visual widget (214) | cache | cache |
| ExpanderManager (55) | cache | cache |
| SandsManager (70) | cache | cache |

The widgets **hardcode their own presence** (`true`) while the managers **read the cache** — they
diverge exactly when the scan hasn't cached a widget that is nonetheless self-asserting present. That
divergence *is* the conflict. The Tier-1 guard (defer East write unless Monsoon's cache recognises it)
fixes the one write-collision, but the six-way disagreement will keep manifesting (ownership, lock,
editability) until there is **one** topology authority everyone pulls from.

**This is direct evidence to raise the Sands-topology-resolver work (`SANDS_TOPOLOGY_RESOLVER_PLAN.md`)
in priority** — it is the same class as C2 (`polyBaseActive`, 3 defs / 5 sites) and A1 (strand→field,
6×). All three are "one derived fact, recomputed independently at N sites." The resolver is the D4 cure
for the whole family, not just Sands.

## Deferred: Sands vs East+Macro — Macro global-LOR editability on V1 (minor, note only)

Recorded to not lose it; fix later. **Macro's editing of its OWN global LOR must never be gated by
mono/east lane ownership** — ownership is about whose values DRIVE the engine, not who may edit their
own globals.
- **East+Macro (correct):** Macro edits its global LOR on any tab incl. V1, whether or not East has
  delegated the lane.
- **Sands-mono+Macro (bug):** Macro's global-LOR editing is BLOCKED on the V1 tab when Mono owns the
  lane. The Sands path conflates "who wins" (drive) with "who can edit" (Macro's own globals).
- Fix direction: the Macro global-LOR edit gate should depend only on Macro presence, not on
  `owner()`/mono-owns-lane. Compare the East V1 editability path (which doesn't block Macro's globals)
  and mirror it on the Sands path. Low risk, isolated to the Macro global-LOR editability predicate.
- Best done *after* the probability-modifier unification lands, since that touches the same edit paths.

## Unified priority order

Principle: **detector first (makes everything else verifiable) → the live bug → the
under-rated root predicate → cheap hygiene → durable tests → the bigger consolidations →
opportunistic.** Value/risk, not doc-of-origin, decides order.

### Tier 0 — do first, unblocks the rest
1. **Dataflow Step 1 — generalise the write-conflict detector.** Debug-only, near-zero risk,
   catches the whole shape-A class automatically, and is the guard rail for every item below.
   *(Dataflow plan §4.1)*

### Tier 1 — live bug + the root predicate (highest value)
2. **Dataflow Step 2 — fix the firing `StrandLedger MACRO-then-EAST` conflict.** A real bug
   emitting every block now. *(Dataflow §4.2)*
3. **Review C2 — collapse `polyBaseActive` to ONE definition** (absorb into the topology
   build / one accessor). Re-rated HIGH: it caused bug #3 and likely underlies #2 above. Fixing
   it may *resolve* item 2 — do them together. *(Review C2 = Dataflow D4)*

### Tier 2 — cheap hygiene (minutes, pure win, do while warm)
4. **Review A3** (delete dead `SpreadInterpolation.hpp`) + **B2** (`.gitattributes` CRLF) +
   **B0/B1** (verified dead code, deprecated dir). Trivial, removes noise that hides real
   drift. *(Review §A3, §B)*

### Tier 3 — the other D4 instance + durable safety net
5. **Review A1 — strand→field mapping once** (single table, replace the 6 switches) + its
   round-trip test. Complements the detector; removes 36 hand-maps. *(Review A1 = Dataflow D4)*
6. **Review D1 — manager / Sands-combination tests.** Highest-value durable investment; the
   orchestration layer is where every session's bugs lived and it's untested. Build on
   `test_SandsTopology.cpp`. Also gives Dataflow Steps 3–4 their regression guardrail. *(Review D1)*

### Tier 4 — bigger consolidations (reuse the topology work)
7. **Dataflow Step 3 — pull cold derived values** (e.g. `activeScaleMask` →
   `getActiveScaleMask()`), one at a time. *(Dataflow §4.3)*
8. **Review C1 / Dataflow Step 4 — single-writer on hot caches + spread-value resolution into
   topology** (topology step 5b). Highest risk, last, detector active, each field a deliberate
   isolated task with a reproduced-bug regression test first. *(Review C1 = Dataflow §4.4)*

### Tier 5 — opportunistic only
9. **Review A2, D2, E*, F4–F9** — ID-helper hygiene, persistence audit, magic-number
   cleanup, RNG/sample-rate/denormal checks. As convenient; none block anything.

### Not now / separate track
- **RT-safety F1–F3** (expander-cast type safety, audio-thread allocation, UI↔audio
  thread-safety) — HIGH impact *if they fire*, but a distinct audit, not part of the dataflow
  cleanup. Schedule separately.
- **Shophouse facade** (`SHOPHOUSE_FACADE_NOTES.md`) — creative track, deliberately not
  interleaved with this (interleaving is how the drift bugs breed).

---

## One-line summary

The review is fully relevant; this session promoted **C2 (`polyBaseActive`)** from a minor
tidy to a Tier-1 root-cause fix, and the dataflow plan's **detector (Tier 0)** is the
precondition that makes the review's risky items (A1, C1, D-series) safe to land.
