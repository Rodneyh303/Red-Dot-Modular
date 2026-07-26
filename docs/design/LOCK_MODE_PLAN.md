# Lock mode — plan

Status: PLANNING (build after de-param completes: Macro done, Mono done, East pending).

## What lock mode is (from DISPLAY_STORE_ENGINE_SEPARATION.md)
Detach the UI from the engine so the user can make SEVERAL edits WITHOUT changing the sound,
then commit all at once on unlock. In the three-layer model:
- **lock ON:** engine frozen at the last committed store; user edits accumulate in DISPLAY
  only (not store, not engine).
- **unlock:** commit DISPLAY -> STORE -> ENGINE in one step.

This is the SAME mechanism as ceded-lane delegation, different driver:
- ceded lane: EXTERNAL (Macro) drives display, store frozen, engine plays Macro.
- lock mode:  the USER drives display, store+engine frozen, commit deferred to unlock.
The de-param already built the reusable shape: StoreKnob's displayValueFn/lockWhen is exactly
"show a value that isn't the committed one." Lock mode adopts it rather than a parallel path.

## NOT this: scale lock ("Conservation")
Shophouse's scale lock (guide vs enforce, SHOPHOUSE_SPEC.md) is a DIFFERENT, orthogonal
feature -- a per-note scale mask, not a UI/engine detach. Do not conflate. Lock mode here is
the edit-freeze/commit mechanism only.

## The de-param dividend
Every editable control is now STORE-BACKED (Macro + Mono complete; East pending). So lock mode
is no longer a scattered per-widget problem: it's ONE question -- "does the store commit to the
engine now, or defer to unlock?" -- asked at the store->engine boundary, plus a display layer
that holds uncommitted edits. Before de-param this would have needed touching every param
read; now it's a boundary concern.

## What OBEYS lock (freezes the engine; edits held in display until unlock)
The "main controls" -- everything that shapes the PATTERN the engine plays:
- **Macro:** globalLor, globalSpread, globalAtten, globalTap, globalDir, macroSend.
- **Mono:** lorBase[mono], spread[mono], monoAtten, monoLaneDir, monoOwner.
- **East (when migrated):** its LOR/spread/atten/dir/owner equivalents.
- **Straits/Monsoon core:** the grid probability edits, LOR grid, direction cells.
These are the pattern-defining surface. Locking them is the whole point: rearrange the pattern
silently, commit on unlock.

## What does NOT obey lock (keeps acting live during lock) -- candidates to decide
The principle: lock freezes USER-INTENT edits to the pattern. It must NOT freeze things that
are (a) live performance inputs, (b) automated/external signals, or (c) global transport, or
the module goes silent/unresponsive mid-performance. Candidates, each needs a ruling:

1. **Clock / transport** -- MUST stay live. Lock freezes the pattern, not time. The sequencer
   keeps advancing through the locked pattern. (Certain: does not obey lock.)
2. **CV inputs / gate-mods** (direction gate-mod, spread CV, atten CV jacks) -- these are
   EXTERNAL live signals, not user edits. Lean: stay live (they modulate the frozen base).
   BUT: if a CV is patched to "edit" a value the user is also locking, there's a conflict to
   resolve. Likely rule: CV modulates the committed base live; lock only freezes the base the
   user is dragging. (Decide: does CV write through lock, or also freeze?)
3. **Change Alley pin matrix** -- is a pin remap a "pattern edit" (obeys lock) or a live
   performance control (stays live)? It reshapes correlation, which IS pattern-defining, so
   LEAN obeys lock. But Alley is arguably a performance surface. (Decide.)
4. **Owner/delegation (monoOwner, topology)** -- changing who owns a lane during lock: does
   the ownership flip defer to unlock, or apply live? Ownership interacts with the display
   layer (a ceded lane already uses displayValueFn), so lock + ownership is the subtle corner
   the separation doc flagged. (Decide -- probably ownership applies live, since it's
   structural routing, not a pattern value.)
5. **Scale / Conservation (Shophouse)** -- ORTHOGONAL feature; does its own thing. Not part of
   lock mode. (Certain: separate.)
6. **Mod arcs / display overlays** -- pure display; unaffected by definition.

## Open design questions (from the separation doc's CAUTION list)
- **Per-lane vs global lock?** One lock for the whole module, or lock individual lanes? Global
  is simpler and matches the "make several edits, commit together" intent. Lean global first.
- **Unlock conflict resolution:** if a CV or Macro changed the engine while the user edited a
  frozen display, what wins on commit? (Display is user intent -> user edits win for the
  locked values; live signals resume modulating from the new committed base.)
- **Interaction with topology ownership:** see candidate 4. The one genuinely hard corner.
- **Visual feedback:** locked controls need to LOOK locked (the displayValueFn path already
  dims/differentiates; reuse it).

## Suggested build order (after East de-param)
0. Confirm the store->engine commit boundary is single-point per module (the de-param should
   have made it so -- one place reads the store into the engine each cycle). Lock intercepts
   THERE.
1. Add a per-module lock flag + a "display overlay" store (uncommitted edits live here while
   locked). StoreKnob already has displayValueFn -- point it at the overlay when locked.
2. Route commit-on-unlock: overlay -> store -> engine, one step.
3. Rule the candidates above (clock live, CV live, Alley obeys, ownership live, scale separate).
4. Visual lock state (reuse the dimming).
5. Test: lock, make edits (silent), unlock (pattern changes at once); clock never stops; CV
   keeps modulating.
