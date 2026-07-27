# Intertropical — scene sequencer (design)

Status: DESIGN (build AFTER the control-layer trilogy: East de-param -> undo -> lock). Concept
frozen here while vivid; not started, to avoid forking the current arc.

## Concept
A graphical sequential switch scoped to VOICES — the arrangement layer over dot.modular's
generation layer. Monsoon generates rich per-voice material (Sands, spread, Change Alley
correlation); Intertropical arranges which voices SOUND in which passage, over time. This is
the form/section layer the instrument lacked.

Name: the ITCZ (Intertropical Convergence Zone) drives the monsoon cycle, so Intertropical
driving scene changes off Monsoon's phrase/boundary crossings is the same causal shape, not a
cosmetic label — scene advance IS a monsoon-cycle crossing.

## Main display: 8 x 17 grid
- **8 columns = 8 scenes.**
- **Top row (row 0): repeat count 1..8** per scene — how many phrase-boundary CROSSINGS the
  scene holds before advancing.
- **Rows 1..16: voice/channel opt-in** — each cell clickable, coloured = voice IN the scene,
  hollow = out. A scene is a column: its repeat count (top) + its 16-voice membership mask.
- Similar grid discipline to Sands (SandsGrid.hpp row-grid alignment) — reuse the pitch/top/
  height conventions.

## Outputs: poly, routed
Poly outputs for GATE, CV, ACCENT, LEGATO, SLEG. As Monsoon plays, Intertropical routes ONLY
the voices in the active scene to these outputs. A graphical sequential switch: the scene mask
gates which of the 16 voices reach the poly outs.

## Advance: boundary crossings, phase-aware
- Repeat count = number of phrase-boundary CROSSINGS (NOT forward steps), in EITHER direction,
  because phase drive can run the sequencer backward. Counting crossings is the phase-coherent
  definition — consistent with the stateless lane-position model (position = pure function of
  totalStepsElapsed).
- **Scene membership is READ only at the phrase boundary.** This dissolves the mid-phrase
  phase-wobble/jitter question entirely: membership is sampled once per boundary, acted on,
  done. (Same move as slew being sampled at the boundary — resolved by WHEN it's read, not by a
  category ruling.)

## Lock-mode classification (settled by Rodney's four constraints)
Intertropical is the OUTPUT/arrangement layer, downstream of all generation. By the
read-vs-map principle (LOCK_SEMANTICS.md 9): output-mapping stays LIVE.
- **De-parammed from the start.** No host params ever, no DAW automation. Nothing to de-param;
  built store-backed on the new substrate natively — sidesteps the trilogy by construction.
- **No modulation.** No CV/automation INTO Intertropical, so no modulation-latching question.
- **LIVE during lock mode.** You lock the PATTERN (what voices generate); the ARRANGEMENT keeps
  playing — scenes keep advancing, outputs keep switching. Freeze the material, keep moving
  through which voices you hear. Same live side as transpose.
- **Two things, both true at once:** grid EDITS (clicking cells) apply live (output routing,
  not latched). The membership the engine ACTS on is read at the phrase boundary. So a live
  edit to the current scene takes effect at the NEXT boundary, not mid-phrase — rearrange
  freely, hear it snap in on the beat.
- Scene advance shares the SAME boundary clock as queued regeneration (dice/scatter release).
  They co-occur on the same crossings but are independent: a scene advance switches routing; a
  queued redraw changes material. No interaction to resolve — different layers.

## Visual feedback
- Playing-scene indicator (which column is active).
- Remaining steps/crossings in the current scene (countdown toward advance).
- Direction indicator if phase drive is running backward (optional).

## Companion: Lantern shows Intertropical's output (PREFERRED over a second module)
Lantern already has a viewMode enum (Notes/Velocity/Prob, persisted — Lantern.cpp:102) and both
grid and piano-roll rendering (LANTERN_SPEC.md, LANTERNS_PIANO_ROLL_SPEC.md). So showing "what
Intertropical is routing" is a NEW viewMode / source on the EXISTING Lantern, not a new module.
- Rationale: one visualiser with a clean mode switch beats two overlapping visualisers. And a
  Lantern mode that reads Intertropical's routed output keeps the display-source dependency
  INSIDE one module that knows how to render several sources, rather than two modules agreeing
  on a protocol. The preferred option is also the more maintainable one.
- Second Lantern is the fallback only if mode-crowding on one Lantern becomes a real UI problem.

## Why build AFTER the trilogy (not now)
- Focus: lock + undo designs are freshly settled and want BUILDING now; forking into a large
  new module (8x17 grid + 5 poly output types + playhead + Lantern mode ≈ Sands-scale surface)
  risks a half-built scene sequencer next to a half-built lock mode.
- Substrate: Intertropical is store-backed/output-only by design, so it WANTS the de-parammed,
  lock-aware substrate to exist first — build it once, correctly, on the finished foundation.

## Open (decide at build)
- Exact repeat-count semantics at the boundary: does a scene with N repeats advance ON the Nth
  crossing or AFTER it? (Off-by-one to pin against the playhead display.)
- Backward phase across a scene boundary: does the scene index decrement (true reversal) or
  does "crossings" only ever count UP toward advance regardless of direction? Membership-read-
  at-boundary makes either safe, but the playhead semantics differ.
- Default scene when none are defined / all cells hollow (probably: pass nothing, or pass all —
  decide which is the sensible empty state).
