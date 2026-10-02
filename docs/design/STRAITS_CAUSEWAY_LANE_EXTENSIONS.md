# Straits / Causeway — per-lane panel-extension model (Rodney)

STATUS: DESIGN DIRECTION. Post-consolidation (hard prerequisites below). The natural next
generalisation after lane uniformity: once every RHYTHM lane is the same kind of thing (poly +
correlated, no mono special cases), a panel should not hard-code WHICH lanes it exposes.

## The problem it solves
Straits grew lane-by-lane (rest, accent, then qmix), each lane = **16 per-voice knobs**. Adding
variation + legato = **5 lane-sets x 16 = 80 knobs**. Hard-coding that is bad either way: cram all 5
(huge, mostly unused) or pick a subset (= re-introducing "which lanes are standard" special-casing).
Causeway's modulation INPUTS have the same shape. Users want only the lanes they use.

## Model: TRUE PANEL EXTENSION (option B, Rodney's choice)
NOT a context-menu show/hide on a fixed panel. Each rhythm lane is a physically attachable EXTENSION
that docks on and GROWS the panel by that lane's width (16 knobs on Straits; 16 CV ins on Causeway).
Pay HP only for the lanes you use.
- **Scope = the RHYTHM FAMILY only:** REST, ACCENT, QMIX, VARIATION, LEGATO. **Octave is OUT** (it is
  pitch, not rhythm).
- **Base frame = frame + QMIX** (shown by default — qmix is the defining sequencer-quantiser axis after
  the mode collapse, so its primacy is structural). A bare Straits is "per-voice qmix".
- **Four attachable extensions:** REST, ACCENT, VARIATION, LEGATO. Add what you want.
- Applies to **both** Straits (per-voice knobs) and Causeway (per-voice CV mod inputs) — same model,
  two roles.

## HARD prerequisites (do NOT build this before these)
1. **Lane uniformity must be REAL in the engine.** VARIATION and LEGATO have no poly buffers / no
   spread yet (see SANDS_CONSOLIDATION.md "spread on VAR/LEG"). A variation/legato EXTENSION cannot
   carry correlated per-voice control until the Sands consolidation builds those poly+correlated lanes.
   So: **Sands consolidation first.**
2. **Docking/adjacency infrastructure.** Panel-extension expanders that attach and order themselves are
   exactly CONNECTION_MODEL_SPEC.md / the adjacency rules. Build the **connection rework first** —
   don't build lane-docking on top of the current connection bugs. (This REORDERS the roadmap: the
   connection rework moves up, since CA marks AND this both depend on it.)

## Dependency chain / order
mode collapse (6->3) -> Sands consolidation (lanes become poly+correlated, uniform) -> connection
rework (docking infra) -> THEN Straits/Causeway lane extensions.

## Possible further unification (note, not committed)
Straits (per-voice knobs), Causeway (per-voice CV), and the unified Sands (visual editor) may all be
the SAME configurable-lane frame in different roles — lane uniformity subsuming three modules' worth of
special-casing, as it already did for the three Sands. Worth revisiting once the extension model exists.
