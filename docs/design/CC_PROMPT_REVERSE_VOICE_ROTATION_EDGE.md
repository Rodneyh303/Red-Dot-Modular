# CC PROMPT — reverse voice: playhead reaches rotation at the RIGHT edge

## The one-line bug (Rodney, Rack feel-test, 2026-11-19)

In **reverse** mode the voice lane's playhead reaches its **rotation** when the
range hits its **LEFT** step edge. It must reach rotation at the **RIGHT** edge.
Forward is correct (rotation at the LEFT edge). This is the voice range's own
playhead *position within the range* — NOT when the lanes commit.

## DO NOT TOUCH — the commit timing is correct

`voiceFrameEdgeLive_` and **when** the probability lanes move is correct and
landed (CC_FIX_CYCLE_COMMIT_WINDOW_EDGE, 638879aa): the lanes move at the voice
range edge (left forward / right reverse). **Do not change the commit edge, the
commit cadence, or voiceFrameEdgeLive_.** This task is ONLY the playhead's
position/rotation alignment within the range in reverse.

## The property (the verifiable definition)

A lane is allowed to move when **its own rotation aligns with the voice range
edge** — LEFT in forward, RIGHT in reverse. Two distinct things sit on the range:

- **rotation** — a CONTENT position (the rotation tick): where the voice reads
  content cell `R`.
- **the step edge** — a fixed SLOT position: the left slot (box-offset 0) or the
  right slot (box-offset L-1).

The alignment the feel-test demands: the step at which the voice reads **rotation
content R** must be the step at which the playhead sits at the **direction-entry
edge** — the LEFT slot in forward, the RIGHT slot in reverse. Forward satisfies
this; reverse currently reads R at the LEFT slot and must read it at the RIGHT.

## Concrete current behaviour (headless, voice L=8, O=0, global 16, clean fwd clock via advancePlayhead)

The CONTENT read (`voiceRead_ - O`, i.e. the content cell under the playhead),
per transport step, R=3:

```
FWD: 3 4 5 6 7 0 1 2     # content R=3 read at the entry (step 0); sweeps up; commit at the LEFT slot
REV: 3 2 1 0 7 6 5 4     # content R=3 read at step 0 too; sweeps down; commit at the RIGHT slot
```

The commit edges are right (fwd left slot / rev right slot — do not touch). The
defect is the *slot the playhead occupies when it reads rotation content R*:
in reverse it is the LEFT slot, it must be the RIGHT slot — i.e. reverse should
ENTER its range from the RIGHT edge reading R and sweep left, mirroring forward
which enters from the LEFT reading R and sweeps right.

## Likely family / candidate sites (investigate, do not assume)

- This is almost certainly the same reverse-anchor off-by-one family as the
  earlier two (`laneTickFor(Reverse) = -(t0+1)`; the `anchorBias(+1)` read fix
  7aae5ee9). The reverse read/position anchor lands content R one edge off.
- Candidate sites: the voice read's reverse rotation anchor (where `R` maps to a
  slot); the UI playhead slot `lanePlayStep[]` / the `calcPlayhead` fold and the
  rotation-tick `rotStep` in `src/ui/SandsVisualEditorV4.hpp` (~1296–1370); and
  `PositionResolver` if the slot position should come from there. Determine first
  whether the content-R-to-slot mapping is wrong in the ENGINE read (testable) or
  only in the UI draw (needs Rodney's visual feel-test).
- Do NOT touch `laneTickFor` (goldens depend on it) or the commit edge.

## Reproduce-first (mandatory) + the degeneracy trap

- **R==0 is degenerate**: at R=0 the rotation content sits at the left slot, where
  rotation already equals the edge, so the bug is invisible. Every existing test
  runs R==0 — that is exactly why this and the commit-edge bug hid for days.
  **Sweep R across 0..L-1.**
- Write a test that, for a reverse voice, asserts the playhead sits at the RIGHT
  edge slot at the step it reads rotation content R, for EVERY R; and the forward
  control asserts the LEFT edge slot. It must FAIL on the current engine (reverse
  reads R at the left slot) and PASS after. Mirror `test_cycle_commit_edge`'s
  R-sweep structure.
- If the fix is UI-only (slot draw), the headless test may not reach it — say so
  explicitly and mark it "needs Rack feel-test"; do not fake a passing test.

## Verify against the property

For every rotation R, both directions: the playhead reads rotation content R at
the entry edge (fwd LEFT slot / rev RIGHT slot); the commit still fires at the
range edge (unchanged); full suite stays green (it is all R==0, so the position
change must not perturb it). Then Rodney feel-tests reverse with rotation set
mid-range.
