# Per-voice-per-lane direction UI (Step 3d) — design spec

## States (4, per voice-per-lane)
1. **Forward** (→) — sign +1, no auto-flip. Reads forward.
2. **Reverse** (←) — sign −1, no auto-flip. Reads backward.
3. **Pendulum** (↔) — bounce at endpoints, NO repeat. Auto-flip sign at each phrase boundary
   when the lane tick is at the window edge. Same as today's `lanePendulum_`.
4. **Ping-pong** («») — bounce at endpoints, WITH endpoint repeat. The endpoint step is played
   twice (once arriving, once departing in the new direction). New logic: when the lane tick
   reaches the window edge and is about to flip, hold the tick at that position for one extra
   step (don't advance), THEN flip and advance on the next step.

## Granularity
Per-voice-per-lane: 15 poly voices × 6 strands = 90 controls (East), plus 6 mono (on Mono/Macro
for mono lanes). Macro has 4 poly lanes (no VAR/LEG per-voice) = 15×4 = 60 + 4 mono.

## Engine changes
- Replace the ±1 sign + bool pendulum with a 4-state enum per voice-per-lane:
  `enum class LaneDir { Forward, Reverse, Pendulum, PingPong }`.
  Storage: `LaneDir laneDir_[6]` (mono) + `LaneDir laneDirV_[15][6]` (poly), + pending versions.
- `advancePlayhead`: for Pendulum, flip sign at the window wrap (existing behaviour).
  For PingPong, detect "at endpoint, about to turn" → skip the advance for one step (repeat
  the endpoint), set a `pingPongHold_` flag, then on the NEXT step clear the flag and flip.
  This needs a per-voice-per-lane hold flag (or a simpler approach: a "repeat count" that
  delays the flip by one step).
- The effective sign computation (mono × voice) stays; the enum just drives sign + auto-flip +
  repeat logic.

## UI (physical control, panel-surface)
- A new multi-state widget (like `OwnerCell` but 4-state, cycling Fwd→Rev→Pend→PingPong→Fwd).
  Drawn as a small cell with a direction glyph (→ ← ↔ «») in the lane's colour.
- Placed next to the existing owner/delegation cell at the right end of each editor lane.
- On East: 6 lanes × (visible voice tabs) — but per-voice-per-lane means it changes with the
  selected voice tab (like the owner cell does). So it's 6 cells per tab (one per lane),
  mirroring the owner cell row.
- On Mono: 6 cells (mono lanes only, no per-voice).
- On Macro: 4 cells (poly spread lanes).

## Panel changes (all 3 Sands)
- +1HP width on each panel (East 43→44HP, Macro 43→44HP, Mono 43→44HP).
- New control column for the direction cell, right of the owner cell column.
- `OWNER_X` shifts right; `PROB_OUT_X` shifts right; panel width constants update.
- SVG kit markers: `param_dir_<lane>` (mono) / per-voice via the existing voice-tab proxy pattern.

## Persistence
- `MonsoonPersistenceManager`: save/load the 4-state enum per voice-per-lane (replaces the
  current `laneSignPending_` + `lanePendulum_` save).

## Migration from context menu
- The existing "Lane direction" submenu (Reverse + Pendulum per strand) stays for now as a
  quick-test path; it writes the same enum. Eventually it's removed once the panel UI is
  validated. Both write `laneDirPending_`; the engine promotes at the boundary.

## Files
1. `SequencerEngine.hpp` — LaneDir enum, laneDir_/laneDirV_ storage, helpers.
2. `SequencerEngine.cpp` — advancePlayhead ping-pong repeat logic, pendulum flip.
3. `StraitsEastSandsVisual.hpp` — direction cell IDs + config.
4. `StraitsEastSandsVisual.cpp` — bind DirCell widget, save/load per-voice.
5. `MonsoonSandsVisualExpander.hpp/.cpp` — mono direction cells.
6. `StraitsSandsMacroVisual.hpp/.cpp` — macro direction cells.
7. `gen_east_clean.py`, `gen_macro_mono.py` — +1HP, new control column, kit markers.
8. `MonsoonPersistenceManager.cpp` — save/load LaneDir enum.
9. New widget: `DirCell` (or extend `OwnerCell` to be multi-purpose).
10. `MonsoonWidget.cpp` — update the Lane Direction context menu to write the enum.

## Implementation order (recommended)
1. Engine: LaneDir enum + storage + advancePlayhead (pendulum + ping-pong logic).
2. Context menu: update to write the enum (quick test path, no panel change yet).
3. Panel generators: +1HP, new column, kit markers.
4. Widget: DirCell, bind on all 3 Sands.
5. Persistence.
6. Build + verify.
