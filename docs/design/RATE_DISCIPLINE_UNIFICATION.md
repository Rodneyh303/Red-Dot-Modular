# Rate discipline -- unifying the timing hierarchy and the dataflow plan

Status: RECONCILIATION NOTE. Ties together EFFECT_TIMING_HIERARCHY.md and
DATAFLOW_DISCIPLINE_PLAN.md, which Rodney flagged as overlapping. They are two halves of ONE
discipline about a multi-rate system; neither supersedes the other. This note states how they
compose so future work applies both, not one.

## The four rates (canonical list)
Fastest to slowest, the rates a value can live at or move between:
1. SAMPLE rate (~48kHz)  per-sample engine decisions (voices[].restProb read in the hot loop;
   wrapped set deep in per-step advance; the continuous audio/CV output itself).
2. CONTROL rate (~kHz, audio-block)  block-level modulation math, expander sync, mask compute;
   often gated behind shouldExecute or once-per-process().
3. 1/16 STEP boundary  the rate the pattern ADVANCES and CONSUMES its shaping inputs.
4. PHRASE boundary  bar/loop granularity; queued events (dice, scatter, scene advance) fire here.
   (Plus a non-rate coarser gate: UNLOCK -- latched config under lock mode, released on user action.)
And async to all of them:
5. SCREEN-REFRESH rate (~60Hz)  widget draw(), fully decoupled from audio.

## The two questions -- and which doc owns each
Every value in the engine has to answer TWO independent questions. Getting one right and the other
wrong still produces a bug. The two docs each own one question:

- **WHICH rate should this value be SAMPLED at?**  -> EFFECT_TIMING_HIERARCHY.md
  The temporal-quantisation question. Sample a pattern-shaping control faster than the pattern is
  READ and you get waste (discarded precision) or JITTER (playhead reacts to a value the sequencer
  hasn't consumed). The ladder (control / 1-16-step / phrase / unlock) picks the correct rate. Test:
  "does it feed the pattern READ (-> quantise to the step) or the continuous OUTPUT (-> leave at
  control rate)?"

- **Does the value REACH its consumer across a rate boundary intact?** -> DATAFLOW_DISCIPLINE_PLAN.md
  The hand-off-integrity question. A value produced at one rate and read at another drifts (two
  writers / stale copy = shape A), is gated too narrowly (producer gated tighter than the consumer
  reads = shape B), or goes stale (derived value not recomputed = shape C). The disciplines
  (single-writer D1, compute-on-read D2, publish-unconditionally-apply-conditionally D3, one
  resolver D4) make the hand-off safe. PULL is the deep fix because it computes at the consumer's
  rate from one lower-rate source -> no cross-rate hand-off to corrupt.

## Why they overlap (same substrate)
Both are about **mutable state crossing rate boundaries** -- DATAFLOW says so explicitly ("the
defects are rate-boundary mismatches"), and TIMING's whole ladder is a map of those boundaries. They
are orthogonal axes over the SAME multi-rate state:

  TIMING  = at which boundary is this value QUANTISED / sampled?   (choose the rate)
  DATAFLOW = across whichever boundaries it crosses, is it handed off without drift/stale/gate-miss?
             (cross the rate safely)

A value needs BOTH correct. Examples of the interaction:
- LOR mod: TIMING says quantise to the 1/16 step (feeds the pattern read); DATAFLOW says the latched
  step-value must have a single writer at the step edge and be pulled by both engine and playhead so
  they never disagree. Wrong rate -> jitter (TIMING failure). Right rate but two writers -> drift
  (DATAFLOW failure). The Sands playhead needed both.
- Scale mask: TIMING says it's control/screen-derived, not per-sample; DATAFLOW says compute-on-read
  (D2) so it can't go stale (bug 4). TIMING alone wouldn't have caught the stale-derived shape.
- wrapped (phrase boundary event): TIMING places it at the phrase rung; DATAFLOW bug 6 was it being
  written to the returned result, not engine.lastStepResult the consumer reads -- a hand-off failure
  at the same rung TIMING had already placed correctly.

So: **TIMING picks the rung; DATAFLOW keeps the value intact as it sits on / moves between rungs.**
The playhead-jitter fix (TIMING) and the six dataflow bugs (DATAFLOW) are the same illness -- multi-
rate shared mutable state -- seen from the "wrong sample rate" side and the "corrupted hand-off"
side respectively.

## Combined procedure (apply to any pattern-shaping or cross-rate value)
1. CLASSIFY the rate (TIMING): does it feed the pattern READ (quantise to 1/16 step), a queued EVENT
   (phrase boundary), latched CONFIG under lock (unlock), or the continuous OUTPUT (control rate)?
   Screen-only display values are pulled at draw() from the engine's rate, never cached across.
2. Once the rate is chosen, make the HAND-OFF safe at that rate (DATAFLOW): single writer at one
   defined point (D1); prefer compute-on-read / one resolver both sides pull from (D2/D4); publish
   unconditionally, gate only the effect (D3). Where a hot cache must stay pushed, single-writer from
   one resolver at one cycle point.
3. The gate that enforces the chosen rate belongs at the single store->engine read the de-param work
   created (TIMING implementation note) -- which is ALSO the single-writer point DATAFLOW wants. The
   two docs converge on the SAME code location: the store->engine boundary. That convergence is the
   practical payoff of unifying them -- one gate, at one point, satisfies both.

## Systematic pass (when actioned -- Rodney's "sort out which modulation to sample where")
Enumerate every pattern-shaping / modulation input and tabulate: input | feeds (read/event/config/
output) | correct rate (TIMING) | writer count now | hand-off shape risk (A/B/C) | fix. Candidates
already named: LOR (len/off/rot), A/B mix prob, spread prob, manual CA pins, dice/scatter/scene
events, per-output transpose (note: transpose is OUTPUT-continuous but TIE-LATCHED -- a special case:
sampled live except held across a true tie; see INTERTROPICAL_SPEC), lock-latched config. The
WriteLedger (DATAFLOW step 1) is the tool that makes this table VERIFIABLE rather than asserted --
build it first, then the table's "writer count now" column is machine-checked.

## Order of operations
DATAFLOW step 1 (the WriteLedger single-writer detector) is the prerequisite for doing the TIMING
systematic pass safely: it turns every rate-hand-off regression into a loud debug warning, so
re-timing a control can't silently reintroduce a shape-A drift. Build the ledger, then walk the
table. Neither doc's later steps should start before the ledger exists.
