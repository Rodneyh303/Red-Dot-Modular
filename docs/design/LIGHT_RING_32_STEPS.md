# Light ring at 32 steps — graphics plan

## Decision: keep the SEGMENTED RING, position-only
When (not if) steps expand to 32, keep the iconic segmented light ring. Two changes:
- **Remove the pattern-detail role** (pattern lives on Sands' lane grid anyway) -> the ring shows
  PLAYHEAD POSITION ONLY.
- Position-only means each segment carries ONE BIT (playhead / not) -> legible even when small (the
  cramping was partly because segments encoded pattern state; one-bit segments read fine at 32).

Keep SEGMENTED (not a continuous sweep) — the segmentation reads the beat/step clearly, which a smooth
arc loses. Do NOT fall back to the rectangular grid (Plan B) — the segmented ring scales to 32 once it's
position-only with beat anchors, and the ring is iconic to the instrument's identity.

## Legibility at 32 (position-only)
- **The eye tracks the MOVING playhead**, not the resting segments: render the playhead as a bright
  segment (+ optional short fading TRAIL behind it for motion/direction + catching fast playheads).
  Resting segments being small doesn't matter; the moving lit one must be clear.
- **Beat-anchor emphasis (keep the original's colour cue):** the original uses RED for the quarter-note
  position, GREEN otherwise. KEEP THIS as the visual reminder of the original + the beat anchor: emphasise
  the quarter-note segments (red) against the rest (green). Like a clock face (hour marks vs minute
  ticks) — you locate the playhead relative to the red quarter anchors, so 32 small segments stay
  readable. This red-quarter/green-rest scheme is the "visual reminder of the original" to preserve.
- **No numeric scale needed** (decided — don't add step-number labels; the beat-anchor colouring is
  enough).
- **ENLARGE the ring — the PRIMARY lever (Rodney).** Lamp/segment count fits around the CIRCUMFERENCE,
  which scales LINEARLY with radius (C = 2*pi*r). So even a MODEST enlargement meaningfully helps: ~+30%
  radius -> ~+30% circumference -> ~30% more arc per lamp — can be the difference between "too cramped"
  and "fine" at 32. Removing the pattern-detail role (position-only now) may free visual budget to grow,
  and position-only one-bit lamps don't "waste" the extra room. So enlargement is the most DIRECT fix
  (more physical room per lamp); the beat-anchor colouring (red quarters) + optional trail are then
  BACKUP/polish rather than necessities.
  - Constraint: panel REAL ESTATE + visual PROPORTION. Check the space around the current ring and what a
    bigger ring would displace; a too-large ring can unbalance the panel. Measure-the-panel question —
    even +20-30% radius may be enough and achievable if there's any breathing room.

## DECIDED (D8 in STEPS_32_PLAN): 32 lamps, FIXED — the master-step scale
Rodney: "Should we replace the 16 step circular ring with a 32 step one or accept that 32 steps is
2 bars so twice around 16 steps? Probably 32 better as Master steps are directly indicated in it."
REPLACE with 32. Twice-around-16 rejected on five counts (full reasoning in STEPS_32_PLAN.md D8):
- **Injectivity:** on 16 lamps lamp = step % 16 — steps s and s+16 share a lamp, and any walk with
  off+len > 16 (the D3 class) folds the window onto itself. On 32, lamp s IS master step s — the
  direct indication Rodney wants — and the live window reads as an ARC on the constant scale
  (start lamp `off`, span `len`; the playhead's jump back IS the boundary — the boundary-vs-ring
  formulation made visible on the panel).
- **No new % 16:** the playhead compare is already `lightIdx == stepIndex` (SequencerEngine.cpp:874)
  — frame-generic as written; twice-around REQUIRES `stepIndex % 16` in the display path, a NEW
  frame-16 landing exactly where the P4 sweep removes them.
- **Pass-blind:** D7 made the ACTIVE PASS load-bearing (macroFrameRead_ :643, identity-row voiceRead_
  :681); a twice-around ring cannot distinguish pass 1 from pass 2, and the bar line is undrawable
  (lamp 0 fires both passes). At 32, pass 2 is the far half and D6(a)'s mid-ring bar tick at 16 exists.
- **The 2-bar reading is MARKED, not lost:** the ring is resolution-agnostic (D6) — 2 bars of 16ths
  and 1 bar of 32nds are the SAME 32-frame; the reading decides MARKING only. D6(a) (recommended):
  8 red quarter anchors — the `i % 4` widget rule survives VERBATIM at 32 — plus the bar tick.
  Twice-around collapses both bars' quarters onto the same 4 lamps: the 2-bar reading becomes
  invisible exactly when the pattern becomes 2 bars.
- **Cost is a wash:** P5 regenerates the SVGs either way (anchors are generator-emitted; the
  legibility lever is RADIUS — circumference scales with radius, not lamp count). Twice-around
  "saves" 16 automated anchors and leaves Monsoon the ONLY surface at 16 beside Sands stepW/32 and
  Lantern N_STEPS=32 — a special case against the EXTENSION LAW.

This also closes the adapt-vs-fixed question below: **FIXED 32.** "Master steps directly indicated"
needs a CONSTANT angular address per master step — adapt-to-length re-spaces the segments (step 8's
lamp would move on every length change), defeating the indication and losing the clock-face property
the original's fixed red quarters had. The active length is READ on the fixed scale (the membership
arc — `isStepInWindow` extends mechanically), not re-rendered into it. At the default (16, 0) the far
half stays dark — honest: the playhead never enters it (the REPRODUCTION LAW visible on the panel:
the default window IS the 16 engine on the 32 frame).

## Summary
Segmented ring — now 32 lamps, FIXED master-step scale (D8), enlarged (the primary legibility lever),
position-only, red quarter-note / green otherwise (the original's cue = beat anchors + identity
reminder; 8 red anchors at 32 under D6(a), bar tick at 16), bright playhead + optional trail, numeric
scale per the note above (no labels unless asked). Preserves the iconic ring at 32 without the
rectangular fallback — and the ring now SHOWS the frame, the window and the pass, which twice-around
could not.
