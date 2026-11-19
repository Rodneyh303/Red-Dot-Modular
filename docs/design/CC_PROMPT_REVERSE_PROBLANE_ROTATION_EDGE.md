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
