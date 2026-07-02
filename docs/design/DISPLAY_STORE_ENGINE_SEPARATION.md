# Display / Store / Engine separation — a reusable control pattern

Status: design principle + active use in the spread base-value fix; noted as the intended
foundation for Monsoon **lock mode** (not yet built).

## The pattern

A control value has THREE distinct roles that are often collapsed into one field (a bug):

- **STORE** — the value the user set; the source of truth; what persists (JSON) and what
  save/restore reads. NEVER written by display logic.
- **DISPLAY** — what the knob/arc/grid shows right now. A *derived* view. May differ from the
  store (because of ownership/follow, or deferred edits). Written freely; never written back
  into the store as a side effect.
- **ENGINE / PLAYBACK** — what actually makes sound. Fed from the store (or an arbitration of
  stores), applied per-frame. Separate from what's displayed.

Rule: data flows STORE → DISPLAY and STORE → ENGINE. DISPLAY never flows back into STORE
except at an explicit, intentional commit point.

This is the standard "single source of truth + unidirectional data flow" / model-view
separation (see MVC/MVVM, reactive derived-state, and VST/CLAP base-vs-effective parameter
values). Collapsing display and store into one field is the classic anti-pattern that caused
the spread cede/reclaim bug.

## Where LOR already does this (the template)

`SandsVisualEditorV4.hpp` lane:
- STORE:   `length/offset/rotation` (saved/restored via `lorId` params)
- DISPLAY: `dispLength/dispOffset/dispRotation`, written by `setDisplayLOR()`
- ENGINE:  `combineLOR` owner-switch in MonsoonExpanderManager (playback arbitration)
The Macro-follow writes ONLY the display (`setDisplayLOR`), so a ceded lane SHOWS Macro's LOR
while the store stays East's → reclaim reverts cleanly. Three clean layers.

## Where spread FAILED (and the base-level fix)

Spread's `SPREAD_*` trimpot was display + input + store-source all in one. "Follow Macro" =
force the trimpot = write the store = clobber → no revert. Fix: give the spread knob a
display value distinct from the stored param, so a ceded lane's knob DISPLAYS Macro's base
spread while the param (store) stays at East's value (frozen, locked — not editable by East on
a ceded lane, same as LOR). Reclaim reveals the untouched East store.
(Modulation/arc display is a SEPARATE later concern — base values first.)

## The lock-mode connection (why this matters beyond spread)

Monsoon **lock mode** is the SAME three-layer mechanism, used differently:
- Intent: detach the UI from the engine so the user can make several UI changes WITHOUT
  changing the sound; then unlock to commit the new UI positions to the engine at once.
- In these terms: lock = freeze ENGINE at the last committed store; let the USER drive DISPLAY
  freely (edits accumulate in display, not store/engine); unlock = commit DISPLAY → STORE →
  ENGINE in one step.
- Ceded-lane spread and lock mode are the same separation with different drivers:
  - ceded lane: an EXTERNAL source (Macro) drives display; store frozen; engine plays Macro.
  - lock mode:  the USER drives display; store+engine frozen; commit deferred to unlock.

So building the display/store split cleanly for spread should use a REUSABLE "displayed value
≠ committed value, with an explicit commit point" shape on the control — lock mode can then
adopt it rather than needing a parallel mechanism.

CAUTION: do NOT design lock mode now (open questions: which params lock, per-lane vs global,
unlock conflict resolution, interaction with topology ownership). Just don't paint the spread
split into a corner that blocks reuse. Build the specific thing well, general shape in mind.
