# CC PROMPT — absolute-mode completions (mirror the two cycle fixes for abs1 geometry)

## Context — cycle is done and Rack-confirmed; abs1 was deliberately left RAW

Two cycle-mode fixes landed and Rodney confirmed them in Rack:
- **Commit WINDOW EDGE** (`voiceFrameEdgeLive_` cycle arm, `638879aa`): the lanes
  commit at the voice range STEP EDGE (left fwd / right rev), not at rotation.
- **Reverse playhead MIRROR** (the `frameT0` phase anchor, `d2dc35eb`): the
  prob-lane rotation-hit tracks the voice's DIRECTIONAL first step (left fwd /
  right rev).

BOTH explicitly left abs1 / absolute mode on the RAW value:
- `voiceFrameEdgeLive_`'s abs1 arm still returns
  `wrapMod(phaseClock + advanceOffset_, walkPeriod(d, lenG)) == 0` — rotation-keyed.
- the `frameT0` anchor is `abs1Read ? wnow : wnow - walkEdge(vd, L)` — abs1 keeps
  raw `wnow`.
So absolute mode almost certainly shows the SAME two bugs cycle did. This prompt
applies the analogous fixes to abs1's geometry.

## STEP 0 — Rodney confirms in Rack FIRST (these are EXPECTED, not yet confirmed)

Rodney feel-tests absolute mode (reverse, MID-RANGE rotation) and reports which of
the two show: (A) the commit firing at rotation rather than the range edge, and/or
(B) the prob-lane rotation-hit not mirroring to the voice's right extreme in
reverse. **Fix only what actually shows.**

## abs1 geometry — DERIVE, do NOT blind-copy cycle

abs1 (MASK_DYNAMIC) reads the FULL PHRASE: ring = lenG (the current global phrase),
the abs1 `rho = R - O` ruling. So the window edge and the entry-anchor for abs1
live in the lenG-ring geometry, NOT the lane-L geometry cycle uses. Work them out
reproduce-first; do NOT copy cycle's `wnow == walkEdge(d, L)` or
`wnow - walkEdge(d, L)` literally.

## Part A — abs1 commit edge (only if STEP 0 shows it)

`voiceFrameEdgeLive_` abs1 arm → fire at the abs1 range STEP EDGE (left fwd / right
rev) in the lenG-ring geometry, not at `vCoord==0` (rotation). Reproduce-first in
ABSOLUTE mode (mirror `test_cycle_commit_edge`): R-sweep, both dirs, assert the
commit read-cell is the window edge for every R. Red before, green after.

## Part B — abs1 reverse playhead mirror (only if STEP 0 shows it)

The `frameT0` anchor's abs1 branch → the analogous `wnow - <abs1 entry edge>`
reflection (derive abs1's directional entry in its full-phrase geometry), replacing
the raw `abs1Read ? wnow` path. Reproduce-first: extend
`test_problane_phase_coupling` to abs1/absolute mode — prob-rotation-hit step vs
voice-first-step step, both dirs, R-swept, assert reverse mirrors forward. Red
before, green after.

## Guardrails (every lesson from the week)

- Do NOT touch the CYCLE arms — correct and Rack-confirmed. These fixes are
  abs1-ONLY (the `abs1Read ?` branch / the abs1 arm).
- Do NOT touch `laneTickFor` / `walkFold` definitions (goldens + the bridge pin them).
- **GOLDENS BYTE-IDENTICAL — prove the audio is untouched** (as `d2dc35eb` did: the
  abs1 `frameT0` feeds ONLY the lane seeds; `voiceRead_` / the commit /
  `voiceFrameEdgeLive_` read `wnow`/`pos_`, untouched). If a golden moves, STOP —
  the change leaked into the audio.
- R==0 is DEGENERATE — sweep R, mid-range rotation (that is why cycle's bugs hid).
- Forward byte-identical (edge 0).
- **Rack is the ACCEPTANCE GATE** — stage unmerged; Rodney confirms the edge/mirror
  in absolute mode before anything merges. Headless green proves the law, not the look.

## Base

`fix/consolidation-restore-readcell-draw` (carries the cycle fixes `638879aa` +
`d2dc35eb`). Cut a fresh branch off it.
