# BUILD PLAN — Containment module (the articulation-envelope routing)

Authority chain: `CC_PROMPT_CANTONMENT_MODULE_SPEC.md` (the spec) <- `ARTICULATION_ENVELOPE_VS_CORRELATION.md`
(the design lineage). AMENDED IN THIS PLAN by Rodney's rulings (2026-10-26):
(1) MULTI-PARENT DAG IS OK — containment = the INTERSECTION of the parent containments.
(2) EXPAND uses MEMORY — follow the CA precedent (trajectory restore).
Status: EXECUTED — steps 1-4 landed on feat/containment-module and CI-green (through ae88a36b); step 6 (PR into lane-expander-refactor) is Rodney's call. FILED in-repo on Rodney's instruction (2026-10-26).

## WHAT THE RULINGS CHANGE (recorded, not re-litigated)

- SINGLE-PARENT FOREST -> MULTI-PARENT ACYCLIC DAG. Supersedes the spec's "single-parent + ACYCLIC"
  invariant and the authority doc's "DELIBERATELY EXCLUDED — multi-parent containment" section
  (the deferred niche is claimed; the structural-uniformity-with-CA-planes argument is consciously
  traded for the intersection capability — decided, on record).
- The cycle-FATAL reasoning is UNCHANGED and still load-bearing: containment COMPOSES derived gates
  (needs evaluation order + base case); correlation READS pre-existing draws (order-free). The DAG
  keeps well-foundedness: a topological order exists iff acyclic. Cycles remain forbidden forever.
- INVERT becomes TRIVIALLY CLEAN: the dual (full edge reversal) of any DAG is a DAG — a cycle in the
  reversed edges would imply a cycle in the forward edges. With single-parent gone there is no
  branching conflict, no canonical rule needed. The spec's safe-transform list gets STRONGER.
- INTERSECTION is the semantics that justifies multi-parent: contained in A AND B = sound only where
  BOTH resolved gates open. This is the one containment meaning correlation LACKS (copula cannot
  reproduce a support intersection) — Rodney claimed it deliberately.

## THE STATE — a multi-parent acyclic DAG over the 16 voices

- `uint16_t parents[16]` — bit u of parents[v] set = "voice v is contained in voice u".
  Capacity 15 parents/voice; no self-bit (bit v of parents[v] always 0, rejected at edit).
- DEFAULT: all zeros = UNCONTAINED (the full-equality case; "mono not special" — V1 is one
  containable voice among 16, nothing privileged, nothing auto-nested).
- AUTHORITATIVE HOME: engine-side, mirroring the CA seam (PatternEngine.hpp:274 `caRhythmSrc[16]`
  identity-default; the CA module is the store-backed EDITOR/DRIVER, not the owner). The containment
  module edits + drives transforms; the future decision manager CONSUMES. Serialization = the raw
  uint16 array (trivial autosave round-trip; the WARPDIAG lesson: stores must round-trip identically).
- INVARIANT (enforced at every edit AND every transform): ACYCLIC, no self-bits. Multi-parent is the
  only relaxation vs the spec; acyclicity is NOT relaxed.

## EDITS — pin edits must keep the graph a DAG

- addParent(v, u): REJECT u == v (self-loop). REJECT if v is in the ANCESTOR CLOSURE of u (adding
  v->u would close a cycle v->u->...->v). Ancestor check = bitmask wavefront: frontier = parents[u];
  repeat frontier |= OR of parents[w] for all w in frontier, <= 16 rounds; reject iff bit v appears.
  Trivial cost (16x16-bit ORs), runs at EDIT rate only — never per-sample. Same O-order class as the
  spec's O(depth) walk, generalized to fan-in.
- removeParent(v, u) / clearParents(v): ALWAYS valid (detaching cannot cycle).
- UI SHOWS the rejection (can't pin — would cycle), never silently fails. CA-grid copy: the
  containment plane is a 16x16 cell grid where a row may carry ZERO TO FIFTEEN pins — the CA row-radio
  ("exactly one pin per row") is DROPPED for this plane only. Multi-pin rows ARE the intersection.

## INTERSECTION SEMANTICS — the contract recorded for the decision manager (not built here)

- Evaluation in TOPOLOGICAL ORDER (roots = no parents, first): gate(v) = clamp(own_gate(v),
  gate(p1), ..., gate(pk)) over ALL parents pk — i.e. the intersection of the parents' RESOLVED
  gate windows (min over upper bounds / the support intersection).
- TRANSITIVITY COMPOSES FOR FREE: each parent's window is already clamped by ITS parents when
  evaluated earlier in topo order, so v ⊂ A ⊂ B yields v ∩ A ∩ B without extra machinery.
- The DAG guarantees the recursion is well-founded (base case: roots; finite; topo order exists).
  That recursion is the DECISION MANAGER's job (MONO_SPECIAL_IN_LOGIC_AUDIT.md). This module only
  PRODUCES the graph; consumption is out of scope here.

## THE TRANSFORM SET — DAG-closed endomorphisms, no/few-param triggered gestures (Rodney's split)

Gesture-vs-edit split UNCHANGED from the spec: selection-heavy ops (specific swaps/grafts) are PIN
EDITING (which now also covers deliberate multi-parenting), never triggered transforms. All verbs
below are pure functions over parents[16] (header-only, rack-free, `dotModular::ct` namespace,
mirroring `dotModular::ca`), applied to the CURRENT graph (compose per trigger — the CA march):

- ROTATE ROLES (<=1 param: step, default 1) — conjugation by the cyclic shift pi over the ACTIVE
  pool: new_parents[pi(v)] = pi_bits(parents[v]) (bit u -> bit pi(u)). Rows >= activeCount untouched
  (manual pins persist beyond the live pool — CA convention). Closed: an isomorphism preserves
  acyclicity and self-bit-freedom. Shape identical; voices rotate through it.
- REFLECT ROLES (0 params) — conjugation by the mirror permutation v -> activeCount-1-v. Closed: same
  isomorphism argument. The similitude mirror.
- INVERT (0 params) — FULL EDGE REVERSAL: new_parents = transpose of the 16x16 bit matrix.
  Closed PROVEN: the dual of a DAG is a DAG. All containment edges flip ("turn the nesting inside
  out"); the formerly-most-contained voice(s) become root(s). Clean BECAUSE multi-parent (Rodney's
  ruling did this — no canonical rule needed).
- COLLAPSE (0 params) — skip one nesting level: new_parents[v] = UNION of parents[u] for all
  u in parents[v] (grandparent re-link, set union over the bitmask — multi-parent makes this total,
  no lowest-wins tie-break needed). Closure proof: every new edge v->g was a length-2 old path, so
  new reachability ⊆ old reachability ⊆ acyclic. Composes per trigger (the longest path shortens by
  >= 1 per trigger — repeat to flatten fully). COLLAPSE-ALL is the composed limit (deterministic).
- EXPAND (0 params) — MEMORY: restore the level the last COLLAPSE removed, via the CA TRUE-REVERSE
  precedent (CATrajectoryBuffer-style ring of COMMITTED graph states, pushed at each phrase-boundary
  commit; EXPAND pops one). NOT a synthesized inverse (collapse is lossy — many graphs collapse to
  the same graph). Rodney ruled: follow the CA precedent. The trajectory seam is reserved in step 3
  even if the ring itself lands in a follow-up.

PROPERTY TEST IS THE GUARANTEE (spec principle, upgraded to DAGs): seeded-Philox random-DAG
generator (random edge sets built incrementally under the same edit checks) x every verb -> assert
acyclic + no self-bits + active-pool discipline. The test is the closure proof in CI, every run.

## TRANSFORM APPLICATION — at PHRASE BOUNDARY (copy CA exactly)

- armed[] per verb + stamp[] monotonic counter, stamped on unarmed->armed; REUSE
  `dsp/CAPendingOrder.hpp` AS-IS (it is generic over rows/armed/stamp — redDot::, rack-free).
- Apply at the phrase boundary driven from Monsoon::process (the CA pattern — CA's
  applyPendingTransforms runs on the audio thread at control rate, called by Monsoon; the module
  never owns a clock). RateManager: `Rate::Phrase` is the first-class tier (ui/RateManager.hpp,
  S1-S9); NO ClockDivider/gridPulse members (the R7 grep-gate forbids them in engines; the module
  abides by the same discipline by design).
- Owner guard (only ONE containment instance mutates the graph — CA's owner-guard precedent);
  live-under-lock axisMask semantics (out-of-axis armed rows stay pending, keep their stamps,
  commit later — the reset-when-empty rule).
- Between boundaries the graph is HELD (the stable skeleton; correlation reconfigures within it).

## MODULE SHAPE

- Files: `src/dsp/ContainmentGraph.hpp` (state + edit ops + predicates, pure), 
  `src/dsp/ContainmentTransforms.hpp` (the verbs, pure), `src/MonsoonContainment.hpp` (+ 4-line .cpp
  instantiation, the CA layout). Plugin registration: plugin.json entry #28 + plugin.cpp addModel +
  Model* extern + panel asset (plugin.json currently 27 entries / 21 active addModel calls —
  reconcile count on landing).
- UI: ONE routing plane (containment), CA-grid copy, multi-pin rows, cycle-reject affordance,
  pending lamps per verb, connect-mark. Zero param slots by design (DAW_PARAM_AUDIT) — transform
  step config via context menu, not param slots.
- Undo: store-backed pin edits (StoreEditAction, house standard); one committed phrase-boundary
  batch = one undo step (CA precedent).

## TESTS (register in test/run_all.sh TESTS; current battery 83 suites)

- test_containment_edits — self reject, ancestor-reject (wavefront), valid accept, remove/clear,
  autosave round-trip identity, default all-zeros.
- test_containment_transforms — known-answer (spec chain example inverted: full reversal;
  rotate/reflect conjugation; collapse union; expand restore), per-verb closure property test over
  seeded random DAGs, active-pool discipline (rows >= activeCount untouched), composition marches.
- test_containment_pending — chronological order, row-index tie-break, out-of-axis persistence,
  reset-when-empty, phrase-boundary timing via the mock flow.
- test_containment_module — mock-SDK module-level: edit->pending->boundary commit->publish cycle,
  serialization, owner guard.
- Gates: run_all.sh full battery + UNIFORMITY_STRICT + anchor/bind audit (new panel) + warning-clean.

## THE STEPS (branch protocol per CC_PROMPTS_INDEX.md — own branch, PR into lane-expander-refactor,
green before next)

| Step | Content | Tests |
|---|---|---|
| 0 | Branch `containment-module` off tip; scaffold files; registration; browser-visible empty module | compile clean |
| 1 | Graph state + edits + cycle-reject + serialization | test_containment_edits |
| 2 | Pure verbs + property harness | test_containment_transforms |
| 3 | Pending + phrase-boundary apply (CAPendingOrder reuse; Rate::Phrase; owner guard) | test_containment_pending |
| 4 | Module integration + one-plane multi-pin UI + rejection affordance + store/undo + engine-side publish | anchor/bind + module test |
| 5 | Full gates | battery green |
| 6 | PR into lane-expander-refactor | CI green |

## OUT OF SCOPE (unchanged from the spec)

- The gate-clamp recursion / decision manager (consumes the graph; separate work — recently
  re-justified and simplified, 0494ab1f, so its momentum is real).
- Dynamic containment via non-DAG modulation — the priority-order escape hatch stays the flagged
  future route (a total order can never cycle); the DAG-closed transforms ARE the safe dynamism.
- TRUE-REVERSE full ring integration: deferred follow-up behind the EXPAND trajectory seam (the
  involutions — invert, reflect, even-step rotates — self-reverse meanwhile).

## SPEC AMENDMENTS TO LAND WITH THE BRANCH'S FIRST DOCS COMMIT

Spec + authority doc edits recording: multi-parent DAG (intersection) supersedes single-parent
forest everywhere, including the authority doc's "DELIBERATELY EXCLUDED" section; row-radio dropped
for the containment plane; INVERT upgraded to full reversal; EXPAND = trajectory memory; the
intersection/topo-order contract above. (Needs a PAT-rotated push — see open items.)

## OPEN ITEMS (Rodney)

- Module NAME / SLUG (Singapore-theme; "Containment" is the spec's working placeholder).
- GO for the step-0 scaffold on branch `containment-module`.
- PAT rotation (current key at 7 uses) before the branch's first push.
