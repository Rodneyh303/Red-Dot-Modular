# CC PROMPT — gate-mode step-advance probability (Variation) — STAGED build spec

Design source: STRAITS_POLY_SUBGATE_KNOBS.md (the gate-mode-Variation section — the full concept,
hierarchy, atom, bipolar, phase-hybrid). THIS doc is the CC-executable build spec: it STAGES the work,
fills the gaps that doc left (bipolar mapping, resolver sourcing, RDM relationship, golden pin), and marks
what is deferred. Branch off lane-expander-refactor. Build Stage 1 first, land green, then proceed.

## THE CONCEPT (one line)
In GATE MODE, Variation is repurposed to STEP-ADVANCE PROBABILITY: on a gate rise, p(advance the playhead)
vs p(repeat the current step). Freed because atom=tie on legato handles pitch-hold (Variation is otherwise
idle in gate mode — SequencerEngine bypasses it in Mode B).

## BASELINE (confirmed, do not re-derive)
- advancePlayhead(int dir) — SequencerEngine.cpp:190 — the single advance fn (takes direction).
- Called at :771, gated ONLY by `rm.ticked(Step)` and `muted` → UNCONDITIONAL today (advance every gate
  rise, before executeStep:821 decides rest). Rest controls whether you HEAR a step, not whether you VISIT
  it. Stage 1 makes the advance PROBABILISTIC.
- Variation = lane index 4 (LaneMapping.hpp); it has a resolved per-voice probability path (the resolver).

## STAGE 1 — main-gate UNIPOLAR probabilistic advance (build this first)
Scope: ONLY the main gate, ONLY forward-or-repeat (no bipolar, no subgate levels, no phase-mod).
- Gate the advance at :771 on a probability instead of unconditional: draw u (the voice's/pattern's Step-
  rate uniform — the SAME draw discipline as the other lanes, from the rhythm Philox stream, NOT a new
  stream); advance iff u < p_advance, else REPEAT (do not call advancePlayhead this rise).
- p_advance = the RESOLVED VARIATION probability in gate mode (per-voice, through the resolver/
  effectiveProbability path — NOT the raw knob; it must warp/correlate/CA like every other lane). In
  CLOCK/PHASE mode p_advance is not used (Variation keeps its existing clock/phase meaning).
- REPEAT semantics: the playhead does not move; the current step's material is re-decided (re-drawn
  rest/pitch/etc. for the repeated step) OR held — RULE: re-decide (a fresh draw on repeat), so stutter =
  evolving repetition not frozen (matches "machine-gun on the current step's material"). Confirm with
  Rodney if held-vs-redrawn is wanted; default = re-decide.
- GOLDEN PIN (mandatory): at p_advance = 1 (Variation set so gate-mode advance prob is 1), behaviour is
  BYTE-IDENTICAL to today (unconditional advance). The goldens MUST hold at p=1 — that is the identity-
  with-today regression gate. Only p<1 changes behaviour.
- Mode-gate: this logic is GATE MODE only (Mode B). Clock/phase advance unchanged.

## STAGE 2 — BIPOLAR advance (forward / repeat / backward) — after Stage 1 green
- Variation gate-mode knob/CV becomes BIPOLAR. Mapping RULE (concrete — fill the doc's gap):
  draw u ∈ [0,1); let p = resolved bipolar Variation value ∈ [-1, +1].
    p ≥ 0: advance FORWARD iff u < p, else repeat.   (p=+1 → always forward = Stage-1 identity)
    p < 0: advance BACKWARD iff u < |p|, else repeat.
    p = 0: always repeat (stutter).
- advancePlayhead(dir) already takes direction: forward = dir, backward = -dir (reverse the existing dir
  argument). WRAP: backward past step 0 wraps to N-1 (modulo pattern length), same as phase mode.
- CONSTRAINT (one playhead): main + subgates share ONE playhead; direction is set by the LAST gate event
  that advanced. No per-level independent playheads.
- GOLDEN PIN: at p=+1, byte-identical to Stage 1 / today.

## STAGE 3 — the THREE-LEVEL hierarchy (main / ratchet / ghost) — DEPENDS ON Straits subgate knobs
- MAIN gate: Variation (above).
- RATCHET subgate rise: own advance threshold.
- GHOST gate rise (separate input between main gates): own advance threshold.
DEPENDENCY: the ratchet/ghost thresholds are STRAITS CONSUMER-GROUP KNOBS, now specced as the NEXT STRAITS
STEP in CC_PROMPT_STRAITS_SUBGATE_KNOBS_SPEC.md (three-way source: own-subpanel > global-Straits | follow-
main). Stage 3 is unblocked ONCE those land (not an indefinite block — it's the planned next Straits build). Do NOT build Stage 3 before the Straits subgate consumer-group knobs land. (Each subgate
level reads its own threshold; same u-draw discipline; same one-playhead constraint.)

## STAGE 4 — phase-modulated / phase×gate hybrid (IDEAS — not yet a build)
Phase-signal-modulated advance prob (breathing momentum) and the ramp→bipolar phase×gate hybrid (continuous
blend between gate and phase mode) are DESIGN IDEAS in the source doc, NOT yet scoped for build. Leave for a
later spec once Stages 1-3 are proven. Do not build speculatively.

## RELATION TO THE RDM / DECISION INFRASTRUCTURE
Step-advance is a DECISION (does the playhead move / which way). It is NOT one of the RDM's five rhythm
predicates (rest/legato/accent/qmix/tie) — those decide what a VISITED step DOES; advance decides WHICH
step is visited. So it is a SEPARATE decision at the advance site (:771), not an RDM predicate. But it
should follow the RDM's discipline: pure decision from (resolved prob, draw), gate-mode-only, golden-pinned
at identity. Keep it a clean function (decideAdvance(p, u) → {FORWARD, REPEAT, BACKWARD}) so it is testable
in isolation like RhythmDecisionCore.

## OPEN (from the source doc — not for Stage 1)
- advance-on-legato-into-rest (existing T/F context menu): UNDECIDED whether to fold into the advance
  probability or keep as a separate toggle. Leave the existing toggle AS-IS for now; revisit after Stage 2.

## DONE (Stage 1) =
- Gate-mode main advance is probabilistic (u < resolved-Variation p → advance, else re-decide repeat),
  gate-mode only, clock/phase unchanged.
- decideAdvance kept as a pure testable function; new test asserts the table (p=1→always forward,
  p=0→always repeat, mid→ratio) and p=1 identity.
- GOLDENS HOLD at p=1 (byte-identical to today) — the regression gate. run_all.sh green, UNIFORMITY_STRICT
  green, -Wall -Wextra clean.
