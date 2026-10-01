# Mode B legato rework — the "consecutive gates should tie, pauses should not" problem

Branch: `feat/mode-b-legato` (off the current subGate-ghost work on master).

## DECISION (Rodney, RESOLVED): Option A — restore the unconditional slurForward bridge.
Cross-gap ties governed by the legato knob are the instrument's intended character. No timer,
no next-step-rest peek. REST + legato-probability own the breaks. Implement A; delete the
`prevGate1High` gap machinery added for the failed state-based attempt.

## The problem, precisely

In Mode B the note DURATION = Gate 1's high width. So when Gate 1 falls, the note's
natural end is *now*. Legato (a tie/slur into the next note) requires the gate to stay
HIGH across the fall→next-rise gap — that is the whole point of MODEL 1
([`mode_b_spec_impl.md`](mode_b_spec_impl.md:80)):

```
gap (Gate 1 low):  gs.gateHeld = gs.slurForward   // bridge iff this note committed to slur
```

`slurForward` is committed at the note's ONSET by the leading-edge legato roll
(`r_legato_tie < legatoProb`, [`SequencerEngine.cpp:614`](../src/dsp/engines/SequencerEngine.cpp:614)).
It does NOT know how long the coming gap will be — so the original bridge held the gate
across ANY gap until the next rise. That is:

- **The behaviour Rodney first reported as a BUG**: "legato holds across a gap … even a
  long pause between main gates." (Held too much.)
- Yet it IS the original MODEL 1 intent (§4): bridge the fall→rise, chains fall out, break
  only at a note that doesn't commit or at a REST.

Every tightening attempt since then killed ALL ties instead of just the long ones:
- gate the bridge on `subgatesActive` → no effect in Rack.
- remove the bridge entirely → "we can never tie across gates now."
- 2 ms grace window → real sequencer gaps are 30–60 ms, so nothing ties.
- tempo-adaptive 1/16 grace → Rodney rejected ("worse than the original").
- state-based back-to-back (overlap or ≤1-sample gap) → "still too tight."

## Why the tightenings all fail

There are only two gate topologies a user actually patches into Gate 1:

| Source                    | Gate width vs period | Gap between consecutive gates |
|---------------------------|----------------------|-------------------------------|
| Sequencer 16th trigger    | ~50 % duty           | ~30–60 ms (HUGE vs any grace) |
| Overlapping / gate-tie out| ~100 % / overlap     | ≈0 (rare hardware feature)    |

So a *time/sample* window can only ever catch the overlap case, which almost no source
produces. A single Gate-1 cable cannot overlap itself. Hence: **any grace-window model is
structurally incapable of tying two normal consecutive sequencer gates** — the gap is
always far larger than any "back-to-back" tolerance. This is the dead end we keep hitting.

## The reframe — "consecutive" is a STEP relationship, not a TIME relationship

Musically, Rodney's rule is: **two notes tie when they are ADJACENT in the sequence with
nothing silent between them** — i.e. the next gate is the very next step, not a step after
a skipped/rested beat. That is a *sequence-position* fact, which we already have, NOT a
gap-duration fact, which we don't.

The engine already advances the playhead one step per gate rise. "The previous played
step actually sounded" is exactly [`prevPlayedSounded`](../src/dsp/engines/SequencerEngine.cpp:488)
inside `executeStep`, and a REST between two gates already breaks the chain
([`SequencerEngine.cpp:512`](../src/dsp/engines/SequencerEngine.cpp:512) `slurReachesHere`).
So the LEGATO DECISION is already correct at the next rise — the only thing missing is that
in Mode B the GATE has already physically dropped, so `wasHeldMono` reads FALSE at that rise
and the connect branch is unreachable.

### Insight

The bridge exists ONLY to keep `wasHeldMono`/the gate alive from one rise to the next so
`executeStep`'s existing legato machinery can connect. It should bridge **until the next
gate rise, unconditionally, whenever `slurForward` is set** — because whether the connection
actually HAPPENS is then re-decided at that next rise by the existing rules (which already
break on REST and on a non-committing predecessor). The gap DURATION is irrelevant; the
sequence relationship is what matters, and that is judged at the rise, not during the gap.

That is literally the ORIGINAL MODEL 1 bridge. So the fix is NOT another gap heuristic —
it is to **restore the unconditional slurForward bridge** and instead fix WHY it felt like
"legato across a pause." The pause case that annoyed Rodney is a REST or a gap that SHOULD
break; the break belongs in the DECISION (rest / non-adjacency), not in a gap timer.

## Candidate solutions (pick one)

### A. Restore unconditional bridge; let REST + legato-prob own the breaks (recommended)
- Bridge: `gateOpen = !isRest && (gate1High || (slurForward))` — no timer, no back-to-back test.
- A pause between notes that should NOT tie must correspond to a step whose decision is REST
  (rest wins → gate low → chain breaks) OR a note that didn't commit `slurForward` (legato
  roll didn't fire → no bridge). Turning the LEGATO knob down makes ties rare; turning REST
  up punches the holes that break chains.
- What Rodney saw as "legato across a pause" was slurForward firing on a note before a gap
  that he heard as silence. Under A that is governed by the legato probability: at moderate
  legato it will sometimes tie across a gap — which is the instrument's stochastic character,
  not a bug. **DECISION NEEDED: is that acceptable?** If yes, A is the whole fix (revert to
  original) and the rest of this doc is moot.

### B. Bridge only to the IMMEDIATE next rise, and only if that rise is the adjacent step
- Keep the unconditional bridge for gate CONTINUITY, but ALSO require the connect at the next
  rise to see an adjacent (non-rested) predecessor — which `executeStep` ALREADY enforces via
  `prevPlayedSounded` + `slurReachesHere`. So B == A at the engine level; the only addition is
  making sure a bridged-but-then-rested gap cleanly drops. This is essentially A plus an audit
  that REST always wins over a live bridge (it does: [`SequencerEngine.cpp:520`](../src/dsp/engines/SequencerEngine.cpp:520)).

### C. A dedicated "max bridge = 1 step" clamp measured in GATE EVENTS, not time
- Count gate RISES since the slurForward commitment. Allow the bridge to survive exactly ONE
  upcoming rise (the adjacent note); if a second rise arrives while still low (i.e. the note
  the user considers "next" was itself skipped), drop. In practice the bridge is consumed at
  the first rise anyway (the connect either happens or a fresh note starts), so C also collapses
  to A for the normal case. C only differs if there are gate rises with the gate never going
  high — which doesn't happen for real gates.

### Conclusion of the analysis
A, B, C all converge on the SAME engine behaviour: **restore the unconditional slurForward
bridge; rely on REST-wins + the leading-edge legato roll for the breaks.** The gap-duration
framing was the wrong axis the whole time. The only open product question is whether
occasional stochastic ties across an audible gap (when legato is high and the next step is
not a rest) are musically acceptable — Rodney to confirm.

## If occasional cross-gap ties are NOT acceptable (the only reason to add machinery)

Then the break must be a SEQUENCE fact we can test at the fall, not a timer. The only clean
sequence signal available at the fall is: *does the pattern's RHYTHM lane mark the NEXT step
as a note or a rest?* If the next step is a programmed REST, drop the bridge at the fall
(don't even wait for the rise). That is deterministic, tempo-independent, and matches "tie
only into an adjacent sounding step." Cost: peek one step ahead on the rhythm strand at the
fall. **This is the only proposal that adds real value over "revert to original," and only
if answer to A's DECISION is "no."**

## Recommended path
1. Commit the current (state-based) work to `feat/mode-b-legato` as a checkpoint.
2. Implement **A** (revert to unconditional bridge) — one-line change in the IMPL 2b gate
   driver; delete the `prevGate1High` gap machinery.
3. Rack test with LEGATO knob sweeps + REST sweeps. Confirm: consecutive gates tie at high
   legato; rests break chains; low legato = few ties.
4. ONLY if Rodney still hears unwanted cross-gap ties: add the "next-step-is-rest peek" break
   (the §"If … NOT acceptable" proposal), which is deterministic and needs no timer.

## Files in play
- [`Monsoon.cpp`](../src/Monsoon.cpp:902) IMPL 2b — the Mode B gate driver (the bridge lives here).
- [`Monsoon.hpp`](../src/Monsoon.hpp:574) — `prevGate1High` (added for the state-based attempt;
  remove if A is chosen).
- [`SequencerEngine.cpp`](../src/dsp/engines/SequencerEngine.cpp:401) `executeStep` — the legato
  decision (UNCHANGED in all options; it is already correct).
- [`test_subgate.cpp`](../test/test_subgate.cpp:1) — the ghost↔main slur tests model
  `ghostSounding` directly, so they are independent of the bridge and stay green.
```
```
