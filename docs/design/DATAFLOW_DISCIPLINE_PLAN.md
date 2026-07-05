# Dataflow discipline — killing the recurring "value doesn't reach the consumer" bug class

Status: **plan, not actioned.** Written after a single debugging session surfaced **six**
bugs of one shape. Consolidates them with the pattern already named in
`CODEBASE_REFACTOR_REVIEW.md` §A ("same bug shape as the Sands clobber") and the
single-writer work in `SANDS_TOPOLOGY_RESOLVER_PLAN.md` (steps 1, 5b). This doc is the
*unifying diagnosis + prioritised execution path*; those two are the Sands-specific slices.

Companion: `DISPLAY_STORE_ENGINE_SEPARATION.md` (the store/enforcement split that already
works for the scale faders — the pattern this plan generalises).

---

## 1. The pattern, stated precisely

### 1.0 The deeper cause: mutable state crossing rate boundaries

The bugs below all share a root the "push vs pull" framing only half-captures: **this is a
multi-rate system with shared mutable state, and the defects are rate-boundary mismatches.**
Three rates coexist:

- **Sample rate** — the engine's per-sample decisions (`voices[].restProb` read in the hot
  loop; `wrapped` set deep in per-step advance).
- **Control rate** — block-level work (modulation math, expander sync, mask compute) — often
  gated behind `shouldExecute` or run once per `process()`.
- **Screen-refresh rate** — widget `draw()` at ~60 Hz, fully async from audio (arcs, fader
  dimming, scale-name text).

Every session bug was a value produced at one rate not correctly handed to a consumer at
another: `wrapped` (sample-rate) not surviving to the control-rate boundary sampler (#6); the
arc's effective value (control-rate) read at screen-rate but written at the wrong control
moment (#2); the scale mask (control-rate) read at screen-rate but never *recomputed* at
control rate (#4). Same rate-crossing hazard, different clothes.

**Why PULL fixes it fundamentally:** a pull accessor computes *at the consumer's rate* from
state owned at a lower rate, so there is no stale hand-off across the boundary.
`getEffectivePolyRest()` works because the screen-rate arc and the control-rate engine both
pull — each gets a fresh value at its own rate from the one source. The single-resolver
pattern isn't just tidier; it **collapses the rate boundary** that the bugs live on. This is
the real design principle behind every fix that stuck.

Corollary for the plan: caches that must remain (hot-loop, §2) are exactly the rate-crossing
points that still need discipline — single writer, produced every control block, so the
sample-rate reader and screen-rate reader never see a half-updated or stale hand-off.


Every bug below is the same shape: **a value is computed correctly, but does not reach a
consumer** — because producer and consumer are coupled through *shared mutable state
written at one time and read at another*. That temporal/spatial gap is the defect surface.

The session's six bugs, by mechanism:

| # | Bug | Mechanism | Sub-shape |
|---|---|---|---|
| 1 | Causeway CV inert | `restProb` written by 2 sites at 2 times; late write clobbered by the early one | **A. wrong copy/time** |
| 2 | Poly accent arcs dead | effective value pushed into `cachedPoly*Effective` copies that drifted from the engine value | **A** |
| 3 | Sands Helix inert | `publishGlobal` (display producer) gated on `macroDrivesOutput`, which needs Straits | **B. gate mismatch** |
| 4 | Scale faders frozen | `updateScaleMask()` never called after the Shophouse wrote the scale | **C. stale derived** |
| 5 | Scale not applied | apply gated inside `if(shouldExecute)` — clock-edge only, so stopped/between-edges never ran | **B** |
| 6 | Shophouse CV dead | `wrapped` set on the *returned* result, not on `engine.lastStepResult` which the consumer reads | **A** |

Three sub-shapes, each with a distinct missing discipline:

- **A. Write-copy drift** (1, 2, 6) — producer writes copy X, consumer reads copy Y; or two
  producers write the same field at different times and the later clobbers. **Root: >1 writer,
  or a copy nobody keeps in sync.**
- **B. Gate mismatch** (3, 5) — a producer of state a consumer reads *every frame* is gated
  behind a condition *narrower* than the consumer's need (an output gate, a clock edge).
  **Root: producer conditionality doesn't match consumer demand.**
- **C. Stale derived** (4) — a value *derived* from a source isn't recomputed when the source
  changes. **Root: manual invalidation that someone forgot.**

### Why PULL sites never bug

The places that **don't** bug all share one trait: the consumer **pulls** — calls an
accessor that computes the value fresh from the one source. `ScaleList::activeEntry()`,
`Monsoon::getEffectivePolyRest()` (added this session), the scale-fader dimming reading
`getSemitoneWeight()`. There is no copy to drift (kills A), no write-time to mis-order
(kills A), no derived value to invalidate (kills C). Gate mismatch (B) is a separate axis —
see §4.

**The missing pattern is: derived values are PULLED, not PUSHED — and where push is
unavoidable (hot loops), it is single-writer, produced unconditionally, from one resolver.**

### The tell: we already built a detector

`SequencerEngine::setStrand` + `beginStrandWriteBlock()` is a **single-writer runtime guard**
for strands — built because this exact class already bit the strand system. It is *currently
firing* (persistent `MACRO then EAST` conflict, see `SANDS_CV_ROUTING_BUGS.md` / open items).
A guard that catches a recurring class is the signal to make the class *unrepresentable*, not
just *detectable* — and to generalise the guard we already trust.

---

## 2. Scope of the hazard (evidence)

Grep survey on `integration/suite-12`:

- **Multi-writer shared fields** (the shape-A surface): `restProb` (3 write sites),
  `accentProb` (4), `wrapped` (3), `lastSelectedScale` (3). Every one of these bugged this
  session. Single-writer fields (`activeScaleMask`, 1) did not drift — but bugged via
  invalidation (shape C).
- **Derived state needing manual recompute**: ~6 sites (masks, effective values, slewed).
- **Direct writes to `engine.voices[].*` / `engine.lastStepResult.*`**: ~17.
- `CODEBASE_REFACTOR_REVIEW.md` §A independently flagged the strand→field maps (36 hand-maps
  across 6 functions) as the same shape — so the codebase already has *documented* instances
  beyond this session's six.

**Not everything can or should be pulled.** Legitimate reasons push exists:
1. **Hot-loop cost** — the engine reads `voices[i].restProb` per-sample per-voice. A resolver
   call per-sample is real overhead. Caching once per block is a *correct* optimisation; the
   bug was two writers, never the cache itself.
2. **Genuine ordering** — the sequencer must advance before `wrapped` is knowable.
3. **Cross-module reads** — arcs on Straits reading Monsoon can't always pull cheaply.

So the target is **not** "pull everything." It is a layered discipline (§3–§4).

---

## 3. The disciplines (what "done" looks like)

**D1 — Single writer per shared engine field.** Every field in `voices[]`, `lastStepResult`,
etc. has exactly one writer at one defined point in the process cycle. `restProb`(3) and
`accentProb`(4) are the violations; the poly resolver already collapsed them to one writer
sourced from `getEffectivePolyRest/Accent`. Extend to the rest.

**D2 — Compute-on-read for non-hot derived values.** Where cost allows, delete the stored
copy and expose an accessor computed from inputs on read. `getActiveScaleMask()` from
`(scaleRoot, lastSelectedScale)` instead of a stored `activeScaleMask` someone must remember
to recompute. Bug 4 *cannot occur* under D2.

**D3 — Publish unconditionally, apply conditionally.** State a consumer reads every frame is
*produced* every frame; only the *effect* is gated. Bugs 3 and 5 were producers trapped
behind output/clock gates. "publishGlobal always when `hasMacro`; blend into voices only when
`macroDrivesOutput`" (the Sands Helix fix) is the template.

**D4 — One resolver per derived concept.** The value exists in exactly one function everyone
pulls from (`ScaleList::activeEntry`, `getEffectivePolyRest`). Caches, if any, are written
*only* from that resolver, at one point. This is D1+D2 unified and is the end state.

---

## 4. Prioritised plan (1–4), by value / risk

Ordered cheapest-and-safest first. Each step is independently shippable; **none requires the
next.** Stop whenever the marginal risk stops being worth it.

### Step 1 — Generalise the write-conflict detector (HIGH value · LOW risk · do first)

Extend the `StrandLedger` idea into a debug-build `WriteLedger` covering the multi-writer
engine fields (`restProb`, `accentProb`, `wrapped`, `lastSelectedScale`, and any
`voices[].*` we choose). Assert single-writer-per-block; `WARN` on violation with
writer-role + field. **This does not change behaviour** — it turns every future shape-A bug
into a loud debug-build warning at the moment of introduction, instead of a silent drift that
ships. It also *documents* the intended single-writer set by construction.

- Effort: low (copy the strand-ledger mechanism; add a `WriteRole` enum + `noteWrite(role,
  field)` at each write site).
- Risk: near-zero (debug-only, no runtime path change in release).
- Payoff: catches shapes A automatically forever. This is the single highest-leverage item.
- Prereq for nothing; makes steps 2–4 *safe to attempt* by catching regressions.

### Step 2 — Fix the live `StrandLedger` conflict already firing (HIGH value · MED risk)

`MACRO then EAST` writes all four strands every block, persistently (not the transient
startup case the comment describes). This is a real topology misclassification — `MACRO_SOLE`
classified while an East visual is present and also writing. Trace whether the Sands-Helix
publish-gating change (`f7bad1e`) widened when MACRO writes, or whether `eastPresent`
disagrees between the two sites. Fix so exactly one role writes each strand per block.
- This is a **concrete bug**, not preventative — arguably belongs before Step 1 if it's
  causing audible/visible wrongness; keep it here only because Step 1's detector makes the fix
  verifiable.
- Ties into `SANDS_TOPOLOGY_RESOLVER_PLAN.md` step 5b (single-writer for poly/spread).

### Step 3 — Pull derived values opportunistically (MED value · LOW–MED risk · incremental)

Convert stored-and-invalidated derived state to compute-on-read **where not hot**, one at a
time, only when already editing that code:
- `activeScaleMask` → `getActiveScaleMask()` (kills bug-4 class permanently). Check: is it read
  per-sample? If only per-block/per-draw, pull is free.
- Other §2 derived sites: assess each for read frequency; pull the cold ones, leave the hot.
- Do **not** batch these; each is a small independent PR with the tests as guardrail.

### Step 4 — Enforce single-writer on the hot caches (MED value · HIGH risk · last)

For the per-sample cached fields that *must* stay pushed (hot loop): make each have exactly
one writer, sourced from one resolver, at one cycle point (D1+D4). The poly rest/accent axis
is **already done** (`getEffectivePolyRest/Accent` + `updatePolyVoiceRest_` as sole writer).
Remaining candidates: `semiWeights` scale-gate (has its own history of this bug), and any
`voices[].*` the WriteLedger (Step 1) flags.
- Highest risk: touches the hot engine path with only the test suite as guardrail. The
  poly-resolver refactor got lucky preserving behaviour; treat each field as a deliberate,
  isolated task with the WriteLedger active and the relevant bug reproduced as a regression
  test **first**.
- Explicitly **out of scope**: converting hot caches to pull (cost). The lesson is
  single-writer, not no-cache.

---

## 5. Guard rails for doing this safely

- **Land Step 1 before Steps 3–4.** The detector is what makes the risky refactors verifiable.
- **One field / one concept per PR.** These bugs multiply exactly when "while I'm here" edits
  touch a second field.
- **Reproduce each historical bug as a test before refactoring its field** (six ready-made
  regression cases: Causeway CV, accent arcs, Sands Helix standalone, scale-fader-on-shophouse,
  scale-apply-when-stopped, shophouse-CV-wrap).
- **Keep the caches.** Do not over-correct into pure pull; the hot loop needs them.
- Behaviour-preservation is the bar: 172-test suite green + the six regression tests + the
  WriteLedger silent.

---

## 6. Explicitly NOT doing

- Rewriting the manager/engine separation — it's sound (`CODEBASE_REFACTOR_REVIEW.md` general
  assessment: "well structured", 29 managers, not a god-object). This is drift-risk cleanup,
  not re-architecture.
- Converting hot per-sample reads to pull.
- Bundling this with the Shophouse facade work (`SHOPHOUSE_FACADE_NOTES.md`) — different axis.
