# Lane-count audit — remaining hardcoded 5/6 that should be 7

## Crash root cause (confirmed by Claude)

The Rack crash (signal 22 / abort) is memory corruption from lane-count OOB writes.
Step 1 changed `POLY_LANES` 5→7 and `NUM_STRANDS` 6→7, but several arrays/bounds
still use literal `5` or `6`. The garbage `markSemi`/`__static_initialization` stack
is the classic signature: OOB write corrupts adjacent memory, crash manifests later
in an unrelated frame.

## Confirmed already-fixed (7)

- `spreadTargetMode[7]` in PatternEngine.hpp and Monsoon.hpp ✓
- `macroLOR_[7]` in SequencerEngine.hpp ✓
- `editorLane()` bound `< 7` and `ENGINE_LANE_TO_EDITOR_QMIX[7]` ✓
- `spread[kVoiceSlots][NUM_STRANDS]` in SequencerEngine.hpp ✓
- Input ID layouts (MonsoonSandsVisualExpander, StraitsSandsMacroVisual) ✓
- Initializer arrays (EDN, editorDirCol, laneName) in Macro/East ✓

## STILL WRONG — must fix

### 1. MonsoonSandsVisualExpander.cpp:484 — N_SPREAD_LANES = 5
```
static const int N_SPREAD_LANES = 5;
static const int SPREAD_TO_BUFFER[N_SPREAD_LANES] = { 0, 1, 2, 4, QMIX_BUFFER_LANE };
for (int l = 0; l < N_SPREAD_LANES; ++l)
    paramMgr->setLaneSpread(SPREAD_TO_BUFFER[l], monsoon->engine.spreadE(0, l));
```
**Issue**: `spreadE(0, l)` for l=5,6 would return the VAR/LEG spread values.
But N_SPREAD_LANES=5 means they're never pushed to the param manager.
This is a **functional gap** (VAR/LEG spread missing) but NOT a crash cause
(the loop only runs 5 times, so no OOB write).

However, `spreadE(0, l)` calls `editorLane(l)` → `ENGINE_LANE_TO_EDITOR_QMIX[l]`
→ returns editor lane 5 (VAR) or 6 (LEG). `spread[0][5]` and `spread[0][6]`
are in-bounds (NUM_STRANDS=7). So no crash from this path.

**Fix**: Change to `N_SPREAD_LANES = 7` and add VAR/LEG buffer entries IF
VAR/LEG become spreadable. For now, leave at 5 (functional gap, not crash).

### 2. MonsoonSandsVisualExpander.cpp:210 — SN[5]
```
const char* SN[5] = {"REST","MEL","OCT","ACC","QMIX"};
```
**Issue**: Only 5 entries. If indexed by `POLY_LANES` loop (7), SN[5] and SN[6]
are OOB reads → garbage pointer → crash when used as string.
**Fix**: Widen to 7: `{"REST","MEL","OCT","ACC","QMIX","VAR","LEG"}`

### 3. MonsoonWidget.cpp:1475 — lane < 5 in spread target submenu
```
for (int lane = 0; lane < 5; ++lane) {
    sub->addChild(createSubmenuItem(laneNames[lane], ...));
```
**Issue**: Only shows 5 spread target lanes. VAR/LEG can't get spread target.
**Fix**: Change to `SandsGrid::POLY_LANES` (7). Also check `laneNames[]` size.

### 4. StraitsSandsMacroVisual.cpp:176 — lane < 6
```
if (ed && lane >= 0 && lane < 6) {
    ed->currentState.lanes[lane].length = ...
```
**Issue**: Bound is 6, should be 7 (MAX_LANES). Lane 6 (LEG) would be skipped.
**Fix**: Change to `SandsVisualEditorV4::MAX_LANES` (7) or `POLY_LANES` (7).

### 5. SpreadManager.hpp — std::array<float, 5> and lane < 5 (4 sites)
```
std::array<std::array<float, 5>, 8> spread = {};
if (lane >= 0 && lane < 5) { ... }
```
**Issue**: SpreadManager only handles 5 lanes. If called with lane 5 or 6
(VAR/LEG), the `lane < 5` guard catches it and returns 0 (no crash, just silent skip).
**Fix**: Widen to 7 when VAR/LEG become spreadable. For now, guards prevent crash.

### 6. SpreadManager.hpp:70 — avgCache_[5]
```
mutable std::array<std::array<float, 16>, 5> avgCache_ = {};
```
**Issue**: Only caches 5 lanes. If `calculateAveragePolyValue` is called for lane 5/6,
it would OOB. Need to check the caller.
**Fix**: Widen to 7 or add guard.

### 7. SpreadManager.hpp:409 — MacroSpreadManager setSpread lane < 5
```
void setSpread(int lane, float value) {
    if (lane >= 0 && lane < 5) { ... }
```
**Issue**: Same as #5 — guarded, no crash, but VAR/LEG can't be set.
**Fix**: Widen to 7 when VAR/LEG become spreadable.

### 8. SpreadResolver.hpp:31-32 — "exactly 4 spread lanes" comment
```
// There are exactly 4 spread lanes; VAR/LEG (editor 4,5) have no spread
```
**Issue**: Comment is stale (was 4, then 5 with QMIX, now should be 7 or at least 5).
Not a crash cause (comment only).
**Fix**: Update comment.

## NOT a problem (verified)

- `editorLane()` — bound `< 7`, table 7 entries ✓
- `spread[kVoiceSlots][NUM_STRANDS]` — NUM_STRANDS=7 ✓
- `spreadERef/spreadE` — go through `editorLane()` which is bounded ✓
- `lorStore_[kVoiceSlots][NUM_STRANDS][LOR_ITEMS]` — NUM_STRANDS=7 ✓
- `laneTick_[NUM_STRANDS]` — 7 ✓
- `macroLOR_[7]` — 7 ✓
- `spreadTargetMode[7]` — 7 ✓

## The static_assert prevention

The real fix is adding `static_assert` tying all lane-indexed arrays to `NUM_STRANDS`
or `POLY_LANE_COUNT` so a future lane-count change can't silently leave one behind.
This is the same single-source lesson as the panel geometry bug.

## Crash prime suspects (in priority order)

1. **SN[5]** at MonsoonSandsVisualExpander.cpp:210 — OOB read if indexed by POLY_LANES
2. **lane < 6** at StraitsSandsMacroVisual.cpp:176 — skips lane 6 (LEG)
3. **avgCache_[5]** at SpreadManager.hpp:70 — OOB if accessed for lane 5/6
4. **lane < 5** at MonsoonWidget.cpp:1475 — skips VAR/LEG in spread target menu
