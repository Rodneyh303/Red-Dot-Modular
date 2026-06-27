# Sands lane-index audit — why mapping errors persisted, and the fixes

## The four lane orderings
1. **EDITOR**        MEL=0 OCT=1 REST=2 ACC=3 VAR=4 LEG=5   (what the user sees/clicks)
2. **ENGINE/SPREAD** REST=0 MEL=1 OCT=2 ACC=3               (PolyLane, spreadEffective[], sprId/sprCvId)
3. **PE-BUFFER**     REST=0 MEL=1 OCT=2 LEG=3 ACC=4 VAR=5   (PatternEngine slewed*/monoDraw, MonoSandsParameterManager.laneSpread)
4. **MONO-PARAM**    collapsed to EDITOR in lane-collapse step 2 (now identity)

## Bridges (named tables in dsp/LaneMapping.hpp + local)
- `ENGINE_LANE_TO_EDITOR = {2,0,1,3}`  engine→editor (12 display uses)
- `EDITOR_TO_ENGINE_LANE = {1,2,0,3}`  editor→engine (Mono ownership macroBase)
- `SPREAD_LANE_TO_EDITOR` = alias of ENGINE_LANE_TO_EDITOR
- `SPREAD_TO_BUFFER = {0,1,2,4}`       spread/engine→PE-buffer (Mono setLaneSpread)

## Root cause (the "one definition rule" kept failing because…)
The rule was applied to the *accessor name* (cvId, spreadEffective, lenId) but NOT
to the *meaning of its index*. The same array/function was written in one ordering
and read in another **at conversion sites**. Every off-by-one lived exactly where
code crossed between two of the four orderings. Naming the constant (PL_MELODY etc.)
removed raw-literal hazards but did NOT remove convention-mismatch hazards, because
both sides used a correct-looking named index from *different* conventions.

## Bugs found & fixed (this pass)
- **BUG 1 — East octave CV modulated melody.** The gen binds 4 CV jacks per lane at
  `cvId(engineLane*4 + col)` (col 0=LEN,1=OFF,2=ROT,3=SPR). The poly consumer read
  `cvId(r,c)` with a *2-rows-per-lane* scheme (r=0,1 REST; 2,3 MEL; 4,5 OCT…), so
  only REST LEN/OFF aligned; OCT/ACC read out of bounds (cvId ≥16). Fixed: the
  consumer now passes `(engineLane, item)` so `cvId(lane,item)=lane*4+item` matches
  the gen exactly. (MonsoonExpanderManager.cpp combineLOR + spread-cv reads.)
- **BUG 2 — Mono spread arc off-by-one (rings on the wrong lane).** The arc stored
  the *editor* lane but read the *engine*-indexed `spreadEffective[]`, rotating the
  rings (MEL row showed REST's value, etc). Fixed: store the spread index; read
  `spreadEffective[sprIdx]` and gate on `sprCvId(sprIdx)`. (MonsoonSandsVisualExpander.cpp)
- **BUG 2b — Mono setLaneSpread double mismatch.** `setLaneSpread(el, spreadEffective[el])`
  used `el`=editor lane both to index the engine-ordered `spreadEffective[]` AND as
  the PE-buffer-ordered arg of setLaneSpread. Fixed: `SPREAD_TO_BUFFER[sprIdx]` for
  the arg, `spreadEffective[sprIdx]` for the value.

## Macro is the model: it stays ENGINE-indexed end-to-end and has no such bug.
Its spread loop uses `{SPREAD_REST,MEL,OCT,ACC}` (engine), pushes the engine index
to the arc, reads `spreadEffective[engine]`, and publishes `spreadEffective[0..3]`
in engine order. No conversion → no off-by-one.

## Rule going forward (sharper than "one definition")
1. Name every lane index by its CONVENTION at the declaration (`int eng`, `int ed`,
   `int sprIdx`, `int buf`) — not just `int lane`.
2. Convert ONLY through the named bridge tables, never by hand.
3. Minimise conversions: keep a path in one convention as long as possible (Macro).
4. An array's index convention is part of its contract — document it at the decl
   (done for spreadEffective[] = engine order) and never index it in another order.
