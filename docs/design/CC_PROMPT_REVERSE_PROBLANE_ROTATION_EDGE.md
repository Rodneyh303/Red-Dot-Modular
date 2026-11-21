# CC PROMPT — reverse: the PROBABILITY-LANE playhead hits rotation at the RIGHT edge

Supersedes `CC_PROMPT_REVERSE_VOICE_ROTATION_EDGE.md`, which drove `e2a450ab` —
BACKED OUT (see the post-mortem below). Read that post-mortem before writing code.

## THE GOAL (Rodney, 2026-11-19)

In REVERSE mode the **probability-lane playhead** must hit its rotation when the
**voice lane reaches the RIGHT side of its range** — currently it hits rotation at
the LEFT side. Forward is correct (rotation at the LEFT side). The property, which
makes this verifiable: *a probability lane is allowed to move when its own rotation
aligns with the voice range edge — LEFT in forward, RIGHT in reverse.*

## POST-MORTEM — why the last attempt (`e2a450ab`) failed. Do NOT repeat it.

- It worked the **wrong surface**: it changed the **voice-range STRIP** playhead
  draw (`StraitsEastSandsVisual` / `StraitsSandsMacroVisual`, `voiceReadPosition` →
  `playheadSlotBar`). Rodney's target is the **probability-LANE** playhead (the
  per-lane rows), not the voice-range strip.
- Changing the strip draw **regressed a known-good visual**: the probability lanes
  appeared to move at the voice ROTATION step instead of the boundary edge. It was
  reverted in `25fd83b2` and the strip draw is back to trunk's read-cell draw.
  **Do not touch the voice-range strip playhead draw again** — it is correct now.
- It passed EVERY headless test and was still wrong in Rack, because the tests
  pinned the slot **LAW** (the math), not the visual **relationship** Rodney
  watches. **Headless green does NOT mean visually correct.** See the acceptance
  gate below.
- MEASURED fact to keep in mind: the engine **commit** (when the lanes move) fires
  at the voice range edge already — LEFT fwd / RIGHT rev — byte-identical across
  rotations. **Do not change commit timing** (`voiceFrameEdgeLive_`). This task is
  the probability-lane PLAYHEAD position/rotation alignment, nothing else.

## BASE

Branch off `fix/consolidation-restore-readcell-draw` (it carries the `walkFold` /
`WalkPolicy` consolidation AND the restored trunk-good strip draw). `walkFold` and
`playheadSlotBar` (the slot law, in `PositionResolver`) now exist — USE them;
do not re-derive a rotation/anchor fold by hand.

## STEP 1 — DIAGNOSE FIRST (reproduce + describe for Rack), no fix yet

- The probability-lane playhead is driven by `lanePlayStep[l]` / `lanePlayDir[l]`
  (`SandsVisualEditorV4`, ~`:452`, set via `setLanePlayStep` each frame). Each lane
  carries its OWN dir + rotation (a lane can be Forward under a Reverse voice).
- Headlessly establish, across an R-sweep (R = 0..L-1), for a REVERSE lane: at what
  step does the probability-lane playhead sit at its rotation, and which voice range
  edge (LEFT/RIGHT) is the voice at then? Show the current (LEFT) vs the target
  (RIGHT), forward as the correct control. Write this as a concrete table in the
  commit/PR so Rodney can confirm it matches what he sees in Rack BEFORE the fix.
- **R==0 is DEGENERATE** — rotation == the left edge there, so the bug is invisible.
  Sweep non-zero R; this is why it hid for days.

## STEP 2 — THE FIX (through the consolidated law, per-lane)

- Route the probability-lane playhead through `playheadSlotBar` / `walkFold` using
  THAT lane's own dir + rotation, so in reverse it reaches rotation at the RIGHT
  edge (mirroring forward's left), for EVERY R.
- Keep it engine-free and display-only IF the diagnosis shows the engine read is
  already correct (it was, for the strip). If the diagnosis shows the defect is
  actually in a shared read the lanes consume, STOP and report — do not change a
  read the audio depends on without flagging it.
- Do NOT touch: the voice-range strip draw (`25fd83b2`, correct), `voiceFrameEdgeLive_`
  / commit timing (correct), `laneTickFor` or `walkFold`'s definition (goldens + the
  bridge pin them), the rotation-tick chevron (it marks content position — its job).

## STEP 3 — TESTS pin the LAW; RACK is the acceptance gate

- A headless test (mirror `test_playhead_slot_edge`): per-lane R-sweep, MIXED lane
  dirs under one voice, asserting each reverse lane's playhead hits rotation at the
  RIGHT-edge step and each forward lane at the LEFT — for every R; plus the old-law
  mirror pinned as still-violating. This proves the LAW only.
- **ACCEPTANCE GATE — do NOT consider this done on headless green.** This is a
  visual change; `e2a450ab` proves headless green can still be wrong in Rack. Stage
  it on the branch, leave it UNMERGED, and hand Rodney the feel-test: reverse lane
  with rotation set mid-range — the probability-lane playhead must hit rotation as
  the voice reaches the RIGHT side of its range, with the boundary-edge "lanes move"
  visual UNCHANGED from trunk. Nothing merges until Rodney confirms the look.

## Full suite green + the STEPS_32 goldens byte-identical at every step, or stop and explain.

---

## UPDATE 2 (2026-11-19, pm) — CC's attempt (`lanePlayheadBar` dirBias) was the WRONG LEVER

CC's branch `fix/problane-rotation-edge` added a `dirBias` to `lanePlayheadBar`
(`tick - dirBias`). Rodney tested it (and the `tick` / `tick + dirBias`
alternatives) in Rack: **no difference to the issue.** Two findings explain why,
and redirect the work:

### It is a REFLECTION (mirror), not a ±1 shift
Rodney's screenshots (reverse vs forward, V1) show the prob-lane playheads already
correctly reversed and already sitting ON their rotation cells. The defect is only
*which voice-edge that coincides with*:
- FORWARD (correct): prob lane on rotation ⟺ voice playhead at the LEFT extreme
  (its first step).
- REVERSE (now, WRONG): prob lane on rotation ⟺ voice still at the LEFT extreme.
- REVERSE (want): prob lane on rotation ⟺ voice at the RIGHT extreme — the mirror,
  because in reverse the voice's FIRST STEP is the right end.
A ±1 bias on the draw cannot turn a left-extreme coincidence into a right-extreme
one (a full window reflection). **Revert the `lanePlayheadBar` dirBias edit — it is
a confirmed no-op for this bug.**

### The real lever: the lane PHASE coupling (`laneTick_` / `laneRecomp`), not the draw
The playhead is fed by `ph = eng.laneTick_[el]`. `laneTick_` is computed in
`laneRecomp` (`SequencerEngine.cpp:1435`) as `walkFold(dir, t0+1, len)`, anchored
to the voice frame's `t0`. The principle "prob lane hits rotation ⟺ voice at its
FIRST step" (the UAS core, day 1) lives in that phase tie. It holds forward and
fails to MIRROR in reverse. The reflection must happen in that coupling —
how the lane's rotation-hit phase tracks the voice's DIRECTIONAL first step — NOT
in the downstream draw. Do NOT touch `walkFold`/`laneTickFor` definitions (goldens
+ bridge pin them); the fix is in how the lane phase/anchor relates to the voice's
first step under reverse.

### Reproduce-first — MEASURE THE COUPLING (this is the acceptance test)
1. **Reproduce the config faithfully.** Reversing the VOICE must reverse the lanes
   (as in Rodney's Rack — the lane's effective dir follows the voice). An earlier
   headless probe set the voice reverse but the lane came through `dir=FWD` — that
   is NOT Rodney's config. FIND how the voice's reverse propagates to the lane dir
   (`effectiveMonoLaneLor`'s dir) and set it up so the lanes actually reverse,
   else the test proves nothing.
2. **Measure the coupling, both directions, R-swept (R==0 DEGENERATE — use mid-range
   rotation):** the transport step at which the prob-lane playhead sits on its
   rotation cell, and the step at which the VOICE is at its first step (its
   directional entry extreme — LEFT fwd / RIGHT rev). Assert they COINCIDE in both
   directions (reverse mirrors forward). It FAILS now (reverse coincides at the
   voice's LEFT extreme); it PASSES when reverse coincides at the RIGHT.
3. Use a WINDOWED voice range (e.g. offset 3, length 6 → cells 3..8, Rodney's
   `8 7 6 5 4 3` example) so left and right extremes are distinct, not adjacent.

### Rack is still the acceptance gate
Stage unmerged; Rodney confirms the mirror in Rack (reverse: prob lanes on rotation
when the voice is at the RIGHT extreme, forward unchanged) before anything merges.
