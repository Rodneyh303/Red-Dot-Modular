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

## FIXED-ORDER, RIGHT-SIDE, CONTIGUOUS docking (Rodney — the simplification)
Not flexible connection — the expanders dock ONLY to the RIGHT, in a FIXED CANONICAL ORDER, read
CONTIGUOUSLY from the base. This sidesteps nearly all expander complexity:
- **Position encodes identity.** A fixed order (e.g. REST -> ACCENT -> VARIATION -> LEGATO) means the
  base knows what the Nth right-neighbour IS by position — no discovery/identification of "which lane
  is this expander".
- **Plain Rack expander chaining** (leftExpander/rightExpander), NOT the CA flexible-connection model.
  So this likely does NOT need the connection rework as a prerequisite (that still stands for CA marks).
- **Gap rule:** lanes are read contiguously rightward until the first non-matching/absent neighbour;
  anything past a gap is ignored. User learns "keep them contiguous, in order". Simple and predictable.
- **Order rule:** pick one — base assigns lanes by POSITION regardless of intent, OR base only
  recognises the expected expander at each position. Decide at build; position-assigns is simplest.
- Deterministic rightward growth; no scattered-expander confusion.

## Prerequisites (loosened by fixed-order docking)
1. **Lane uniformity must be REAL in the engine (STILL REQUIRED).** VARIATION and LEGATO have no poly
   buffers / no spread yet (SANDS_CONSOLIDATION.md "spread on VAR/LEG"). A variation/legato extension
   cannot carry correlated per-voice control until the Sands consolidation builds those lanes. So:
   **Sands consolidation first.**
2. **Connection rework — NO LONGER a prerequisite for THIS** (fixed-order right docking uses plain Rack
   expander chaining, not the CA connection model). It remains needed for CA marks, but it no longer
   BLOCKS lane extensions, so it can run in PARALLEL rather than before.

## Dependency chain / order (shortened)
mode collapse (6->3) -> Sands consolidation (lanes poly+correlated, uniform) -> THEN Straits/Causeway
lane extensions. (Connection rework parallel, not blocking this.)

## Possible further unification (note, not committed)
Straits (per-voice knobs), Causeway (per-voice CV), and the unified Sands (visual editor) may all be
the SAME configurable-lane frame in different roles — lane uniformity subsuming three modules' worth of
special-casing, as it already did for the three Sands. Worth revisiting once the extension model exists.
