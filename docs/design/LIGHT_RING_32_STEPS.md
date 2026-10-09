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
- Possibly ENLARGE the ring now that it does less (bigger radius -> more circumference per segment), if
  the panel allows.

## OPEN: adapt-to-length vs fixed 32 segments
To be decided — does the ring render:
- **Adapt to length:** the CURRENT pattern length's segments (12-step pattern -> 12 segments; 32 only at
  max). Reflects actual length; segments stay as large as possible for short patterns.
- **Fixed 32:** always 32 segments, with the active length highlighted within them.
Lean/decision TBD. (PATTERN_LENGTH_PARAM is variable 1..32, so adapt-to-length is natural, but fixed-32
gives a constant visual frame.)

## Summary
Segmented ring, position-only, red quarter-note / green otherwise (the original's cue = beat anchors +
identity reminder), bright playhead + optional trail, no scale, possibly enlarged, adapt-vs-fixed-length
TBD. Preserves the iconic ring at 32 without the rectangular fallback.
