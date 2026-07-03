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

---

## Directional borrowing rule (display authority)

Confirmed principle governing what each editor's display shows:

**Borrowing is one-way, upward only. East (downstream) can borrow Macro (upstream); Macro
never borrows East.**

- **East, on a lane it OWNS:** shows the shared base draw with EAST's own LOR + spread.
- **East, on a lane it CEDED to Macro:** shows MACRO's LOR + spread (East reaches UP and taps
  Macro — correct; the base-spread displayValueFn fix).
- **Macro, always:** shows the shared base draw with MACRO's own LOR + spread. Never East's.

So both spread display fixes are the SAME rule in the two directions:
- East following Macro on a ceded lane = downstream borrowing upstream = ALLOWED.
- Macro showing East's spread = upstream borrowing downstream = FORBIDDEN (the leak; option a).

Consistency check: East-owned lane shows own-LOR+own-spread; Macro shows own-LOR+own-spread;
LOR already worked this way (macroBase/macroCVDelta). Spread must match. This is why Macro's
prob display computes base-draw + Macro's-own-spread rather than reading the East-modulated
shared final.

---

## KNOWN ISSUE (to investigate) — Mono spread cede/reclaim, in East+Mono+Macro

User observation (config: East + Mono + Macro): Mono has the spread cede/reclaim issue that was
ALREADY FIXED FOR EAST. On a Mono lane delegated to Macro then reclaimed: LOR restores but spread
does NOT — OR spread doesn't track Macro while delegated. User to RECHECK which symptom (they are
different bugs with different fixes — do not fix until confirmed):

  Symptom A — "spread not restored on reclaim": the store/reclaim bug. Mono's spread path lacks
    the display/store separation East got (the SPREAD_* displayValueFn split): the ceded display
    value is being written into Mono's spread STORE, so reclaim restores Macro's value not Mono's.
    Fix template = the East fix: knob DISPLAYS Macro's base via a display value while the param
    (store) stays Mono's; engine arbitrates via combineSpread. Mirror it in the Mono visual /
    MonsoonSandsManager spread path.

  Symptom B — "spread not tracking Macro while delegated": the display-follow bug. A ceded Mono
    lane should SHOW Macro's base spread; if it shows Mono's own instead, the follow is missing
    (Mono's equivalent of the East poly/V1 spread-follow). Fix = wire the ceded-lane display to
    Macro's macroBase[lane][3], same as East's spreadDisplayValue.

Both are the SAME class as the East spread work (LOR got the separation, spread didn't — one
layer over), consistent with the recurring finding that spread lags LOR in the display/store
separation. Mono's spread path: MonsoonSandsManager (readStrand/spread) + the Mono visual
expander's spread knobs. Confirm symptom, then apply the matching East-derived fix.
