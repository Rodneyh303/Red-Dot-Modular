# Rate & Dataflow work -- consolidated entry point (START HERE, for Claude Code)

Five docs cover this ONE area (they overlap by design; this note is the map + the ordered task list).
Rodney flagged EFFECT_TIMING_HIERARCHY + DATAFLOW_DISCIPLINE_PLAN as touching the same area, and yes --
RATE_DISCIPLINE_UNIFICATION already reconciles them. Read in this order:

1. RATE_DISCIPLINE_UNIFICATION.md -- the reconciliation: the 4 canonical rates + how the timing
   hierarchy and dataflow plan are two halves of one discipline. THE FRAME.
2. EFFECT_TIMING_HIERARCHY.md -- the PRINCIPLE: nothing that shapes the pattern is sampled faster than
   the pattern is READ (16th). read-vs-map test decides what quantises to the step vs stays continuous.
3. RATE_TABLE.md -- the "calculation rate by item" summary Rodney means: every modulation input x
   (trigger, consumed-at, writers, correct?). THE AUDIT TARGET -- the last two columns need confirming.
4. DATAFLOW_DISCIPLINE_PLAN.md -- the single-writer / value-doesn't-reach-consumer bug class + the
   prioritised 1-4 plan.
5. PROCESS_RATE_AUDIT.md -- per-sample vs controlDivider vs lightDivider; CPU-creep fixes (display work
   wrongly at sample rate). Optimisation, distinct from correctness.

## What's ALREADY DONE (don't rebuild)
- WriteLedger EXISTS + fully built (SequencerEngine.hpp:71+): debug-only single-writer detector,
  WriteRole/WriteField enums, noteWrite(), CONFLICT warn, compiles out under NDEBUG. Step 1's MECHANISM
  is done. What remains is INSTRUMENTING the write sites + running the audit.
- StrandLedger (the pattern WriteLedger generalises) is live in StraitsEastSandsVisual.cpp.
- PROCESS_RATE_AUDIT: the modViz full-sample-rate display bug already fixed.

## THE ORDERED TASKS (from DATAFLOW plan 1-4, adjusted for what's built)
Cheapest+safest first; each independently shippable; none requires the next. 172-test suite green +
WriteLedger silent = the behaviour-preservation bar.

STEP 1 (do first -- HIGH value, LOW risk): INSTRUMENT the write sites with the existing WriteLedger.
  Add noteWrite(role, field) at every write to the multi-writer fields: restProb, accentProb, wrapped,
  lastSelectedScale, chosen voices[].*. This is debug-only, no release behaviour change. Turns every
  future single-writer violation into a loud debug warning. Highest leverage. The detector is built --
  this is just calling it at each write site + defining the WriteRole per site.

STEP 2 (HIGH value, MED risk): FIX the live StrandLedger conflict already firing. MACRO-then-EAST writes
  all four strands every block persistently (topology misclassification -- MACRO_SOLE classified while an
  East visual is also writing). Trace whether Sands-Helix publish-gating (f7bad1e) widened MACRO writes
  or eastPresent disagrees between sites. Fix: exactly one role writes each strand per block. Ties to
  SANDS_TOPOLOGY_RESOLVER_PLAN.md step 5b. (A concrete bug, not preventative -- Step 1's ledger makes
  the fix verifiable.)

STEP 3 (MED value, LOW-MED risk, incremental): AUDIT the RATE_TABLE last two columns + pull COLD derived
  values to compute-on-read. Confirm each row's consumed-at vs sampling. Specifically:
  - activeScaleMask -> getActiveScaleMask() (kills the stale-derived scale bug class) IF not read
    per-sample (check frequency; if per-block/per-draw, the pull is free).
  - The "?" and "FIX?" rows in RATE_TABLE: mono accentProb (2 write sites -- WriteLedger from Step 1 will
    show if same-block conflict); poly restProb/accentProb expander-write audit; variation rung; scale
    mask compute-on-read candidate.
  - Do NOT batch -- each is a small independent PR with the tests as guardrail.

STEP 4 (MED value, HIGH risk, LAST): single-writer on the HOT per-sample caches. Each hot field one
  writer, one resolver, one cycle point. Poly rest/accent already done (getEffectivePolyRest/Accent sole
  writer). Remaining: semiWeights scale-gate, any voices[].* the WriteLedger flags. Highest risk (hot
  path, tests are the only guardrail) -- reproduce each historical bug as a regression test FIRST.

## Optimisation (separate track, from RATE_TABLE + PROCESS_RATE_AUDIT -- do NOT conflate with correctness)
- PPQN cap at 24 (settled): only gs.tick uses sub-16th; everything else 16th-gated -> PPQN>24 wasted.
  Cap or make opt-in. (RATE_TABLE "PPQN cap + triplet decision".)
- getEffective* reads run every block for 16th-consumed values -- gate behind sixteenthEdge ONLY the
  safe-to-defer ones (the table's consumed-at=16th rows); keep continuous ones (mix/spread/transpose)
  per-block. AUDIT with a profiler in Rack; do NOT assume.

## Guard rails (from DATAFLOW plan 5)
- Land Step 1 (instrumentation) before Steps 3-4 -- the ledger makes the risky refactors verifiable.
- One field / one concept per PR (these bugs multiply on "while I'm here" edits).
- Reproduce each historical bug as a regression test before refactoring its field (six ready cases:
  Causeway CV, accent arcs, Sands Helix standalone, scale-fader-on-shophouse, scale-apply-when-stopped,
  shophouse-CV-wrap).
- Keep the caches -- the lesson is SINGLE-WRITER, not no-cache. Do not over-correct into pure pull.

## Meta-lesson (why this bug class exists -- DATAFLOW 1.0)
The recurring "compiles clean, returns a plausible WRONG value" bug is MUTABLE STATE CROSSING RATE
BOUNDARIES: a value written at one rate, read at another, by a second writer, drifts silently. PULL
sites (compute-on-read) never bug; PUSH sites (cached, multi-writer) do. The discipline: single-writer
for hot caches, compute-on-read for cold derived values, step-quantise what feeds the READ.
