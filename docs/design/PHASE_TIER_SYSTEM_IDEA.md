# PHASE TIER SYSTEM — calibration + two phase inputs + distribution distortion (Rodney)

Three ideas that UNIFY into one model: phase becomes a TIERED, CALIBRATABLE, DISTRIBUTION-SHAPED addressing
system — the same tiered/addressable/distribution-driven philosophy as everything else, now on the PHASE
input. It also RESOLVES the hard 32-step phase question (D9) by refusing the 1-bar-vs-2-bar binary.

## THE PROBLEM IT SOLVES (D9 restated — Rodney's framing)
At 16 steps, phase mode is simple because phase IS the transport (0-10V = one bar = the whole pattern); the
"no jumps between BARS, only within" rule is well-defined (one bar). At 32 steps = 2 bars this forks:
- (A) phase = whole transport (2 bars, span 192): LOSES "no jumps between bars" (phase can cross bars).
- (B) phase = one bar (span 96), transport runs TWO phase cycles: KEEPS "no jumps between bars" but phase ≠
  transport — you must CLOCK THE CYCLES (count which bar), making it more like clock mode.
Neither is free. The doc's current D9 answer (hardcode span → 192 = option A) silently drops the no-jumps-
between-bars rule.

## IDEA 1 — PHASE CALIBRATION (span is a USER PARAMETER, not hardcoded)
A phase calibration input/knob: "how many BARS (or QUARTER NOTES) does 0-10V phase span?"
- span = 1 bar  → phase is bar-local (option B); no-jumps-between-bars preserved (the natural/default).
- span = 2 bars → phase spans the whole 32 (option A); jumps anywhere.
- span = 1 quarter → beat-local phase (finer).
Implementation: PPB = p16 * (calibration_in_sixteenths) — a MULTIPLIER, not a hardcoded 16-or-32. Dissolves
A-vs-B into a parameter. DEFAULT = 1 bar (preserves the 16-step musical meaning; 16-step behaviour
bit-identical). Supersedes D9's hardcoded 192.

## IDEA 2 — TWO PHASE INPUTS, HIERARCHICAL (like subgates) — the real resolution
Don't CHOOSE 1-bar-or-2-bar: have BOTH, as two inputs forming a tier hierarchy EXACTLY like the subgate
tier maps onto the main gate:
- MASTER PHASE (transport phase): 0-10V → position across the WHOLE transport (all 32 / 2 bars). "Where in
  the whole pattern." (= option A span.)
- BAR PHASE: 0-10V → position WITHIN a bar (16 / 1 bar). "Where within the current bar." (= option B span,
  bar-local by construction → no-jumps-between-bars preserved HERE.)
They COMPOSE outer-selects/inner-positions, the SAME nesting as ratchet/ghost within the main gate: master
selects the bar-region, bar-phase positions within it. So BOTH musical behaviours coexist, selected by WHICH
input you patch — cleaner than a calibration knob (it's the subgate pattern — separate controls per tier —
already used everywhere). Calibration (idea 1) still applies PER INPUT for finer span control.

## IDEA 3 — PHASE DISTORTION through the DISTRIBUTION LAYER (pure reuse)
Phase (0-10V → 0-1) passes through a COPY of the distribution layer (warp / tilt / ATOMS), calibrated
0-10V = 0..1:
- REUSE not invention: the distribution layer already maps [0,1]→[0,1] with warp/tilt/atoms; phase is
  already a [0,1] position. "Distort phase" = run phase through the existing distribution transform. Zero
  new DSP.
- ATOMS ON PHASE = probabilistic phase JUMPS / STUTTER: an atom is a point mass → phase snaps to a definite
  position with some probability; mostly smooth, occasionally snaps. Warp = non-linear phase scan; tilt =
  biased early/late in the bar.
- COMPOSES downstream: distorted phase drives the playhead → warped/atomic SCANNING of the pattern → feeds
  all the per-voice / correlation / mask machinery. The dependence/distribution theory applied to the PHASE
  SCAN itself.

## THE UNIFYING FRAME — a PHASE TIER system paralleling the GATE TIER system
Gates: main / ratchet / ghost, per-level decisions (the subgate tier). Phase: MASTER / BAR, per-tier span
(calibration) + per-tier DISTRIBUTION distortion (warp/tilt/atoms). Same architectural shape, reused:
nested tiers, per-level controls, distribution-driven. The subgate CONCEPT generalises from gates to phase.
- Per-tier distortion: master phase through its own warp/tilt/atoms (transport-level scan) + bar phase
  through its own (bar-level scan) → two independently-distortable, atom-punctuated, nested phase scans.

## STATUS
IDEAS — not scheduled. Idea 1 (calibration) is the immediate better answer to D9 (default 1 bar preserves
16-step meaning; the hardcoded-192 D9 answer drops no-jumps-between-bars). Ideas 2+3 are the richer future
(a full phase tier system). Capture now; sequence after 32 steps lands. Distribution reuse (idea 3) makes it
cheaper than it looks — the machinery exists.
