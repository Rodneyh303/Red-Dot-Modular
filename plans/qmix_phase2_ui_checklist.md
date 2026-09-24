# Q-Mix Phase 2 UI Implementation Checklist

## Status: Partially Started - Needs Systematic Completion

Phase 1 (engine) is complete. Phase 2 (UI) was started but needs systematic completion.

---

## Already Completed

- [x] Update `POLY_LANES` from 4 to 5 in SandsGrid.hpp
- [x] Update `MONO_LANES` from 6 to 7 in SandsGrid.hpp
- [x] Update `EAST_LANES` from 6 to 7 in SandsGrid.hpp
- [x] Update visual editor laneCount: POLY 4→5, MONO 6→7 (SandsVisualEditorV4.hpp line 366)
- [x] Add `PROB_OUT_QMIX` to StraitsEastSandsVisual.hpp OutputId enum
- [x] Update East prob-out jack creation loop to use POLY_LANES (StraitsEastSandsVisual.cpp line 300)

---

## StraitsEastSandsVisual.cpp - Remaining Updates

### Widget Creation (Constructor)

**Line 333-334: CV input jacks** - KEEP at 4 (4 columns: Len/Off/Rot/Spr)
```cpp
for (int c = 0; c < 4; ++c)  // CORRECT - 4 CV columns per lane
    bindInput<redDot::GoldPolyPort>("input_" + std::to_string(cvId(r,c)), ...
```

**Line 341-342: CV attenuverter knobs** - KEEP at 4 (4 columns)
```cpp
for (int c = 0; c < 4; ++c) {  // CORRECT - 4 attens per lane
    const int aLane = r, aCol = c;
```

**Line 385: Spread knobs** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
```
- Add "Q-MIX" to sprN array: `{"REST","MEL","OCT","ACC","QMIX"}`

**Line 414: Owner cells** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
```

### Module Initialization (StraitsEastSandsVisual constructor)

**Line 203-205: configOutput calls** - ADD 5th output
```cpp
// After the existing 4 configOutput calls, add:
static const char* ln[5] = {"REST", "MELODY", "OCTAVE", "ACCENT", "Q-MIX"};
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l)
    configOutput(StraitsEastVisualIds::PROB_OUT_REST + l,
        std::string("Probability ") + ln[l] + " (poly: ch1 master, ch2+ voices)");
```

### Step Function

**Line 710: Topology eastV1Owner** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int el = 0; el < 4; ++el) {
    int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
// NEW:
for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
    int eng = dotModular::EDITOR_TO_ENGINE_LANE_QMIX[el];
```

**Line 721: Topology eastPolyOwner** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int el = 0; el < 4; ++el) {
    int eng = dotModular::EDITOR_TO_ENGINE_LANE[el];
// NEW:
for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
    int eng = dotModular::EDITOR_TO_ENGINE_LANE_QMIX[el];
```

**Line 1082: Spread manager** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane)
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane)
```

**Line 1141: Display LOR (poly)** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
    int el = dotModular::ENGINE_LANE_TO_EDITOR[lane];
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
    int el = dotModular::ENGINE_LANE_TO_EDITOR_QMIX[lane];
```

**Line 1188: Display LOR (V1 mono)** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l=0; l<4; ++l) {
    int strand  = dotModular::MONO_LANE_TO_STRAND[l];
// NEW:
for (int l=0; l<dotModular::SandsGrid::POLY_LANES; ++l) {
    int strand  = dotModular::MONO_LANE_TO_STRAND[l];
```

**Line 1309: Display LOR (poly engine)** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l=0; l<4; ++l) {
// NEW:
for (int l=0; l<dotModular::SandsGrid::POLY_LANES; ++l) {
```

### Process Function

**Line 1358: Owner lights** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane)
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane)
```

**Line 1369: Prob-out early exit** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
```

**Line 1421: Prob-out main loop** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
```

---

## StraitsSandsMacroVisual.hpp - Updates Needed

### OutputId Enum

**Line 170-173: Add PROB_OUT_QMIX**
```cpp
// OLD:
enum OutputId {
    PROB_OUT_REST = 0, PROB_OUT_MEL, PROB_OUT_OCT, PROB_OUT_ACCENT,
    NUM_OUTPUTS
};
// NEW:
enum OutputId {
    PROB_OUT_REST = 0, PROB_OUT_MEL, PROB_OUT_OCT, PROB_OUT_ACCENT, PROB_OUT_QMIX,
    NUM_OUTPUTS
};
```

---

## StraitsSandsMacroVisual.cpp - Updates Needed

### Widget Creation

**Line 192: Prob-out jacks** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l)
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l)
```

**Line 200: CV input jacks** - UPDATE outer loop to POLY_LANES, keep inner at 4
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
    for (int c = 0; c < 4; ++c)  // KEEP at 4
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
    for (int c = 0; c < 4; ++c)  // KEEP at 4
```

**Line 206: CV attenuverters** - UPDATE outer loop to POLY_LANES
```cpp
// OLD:
for (int c = 0; c < 4; ++c) {
    static const char* LN[4] = {"REST","MEL","OCT","ACC"};
// NEW:
for (int c = 0; c < 4; ++c) {
    static const char* LN[5] = {"REST","MEL","OCT","ACC","QMIX"};
```

**Line 225: Spread knobs** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
    static const char* LN[4] = {"REST","MEL","OCT","ACC"};
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
    static const char* LN[5] = {"REST","MEL","OCT","ACC","QMIX"};
```

**Line 243: Send knobs** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane)
    for (int item = 0; item < 4; ++item) {  // KEEP at 4
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane)
    for (int item = 0; item < 4; ++item) {  // KEEP at 4
```

**Line 262: Tap switches** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
    static const char* LN[4] = {"REST","MEL","OCT","ACC"};
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
    static const char* LN[5] = {"REST","MEL","OCT","ACC","QMIX"};
```

**Line 284: Direction cells** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane)
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane)
```

**Line 325: Direction mod jacks** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane)
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane)
```

### Module Initialization

**Add configOutput for 5th output** - similar to East

### Step Function

**Line 368: Topology monoV1Owner** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l)
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l)
```

**Line 384: Load from engine (mono)** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
    const auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR[l]];
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
    const auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR_QMIX[l]];
```

**Line 394: Save to engine (mono)** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
    auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR[l]];
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
    auto& lane = visualEditor->currentState.lanes[dotModular::ENGINE_LANE_TO_EDITOR_QMIX[l]];
```

**Line 547: Display LOR (poly)** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
    int el = dotModular::ENGINE_LANE_TO_EDITOR[lane];
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
    int el = dotModular::ENGINE_LANE_TO_EDITOR_QMIX[lane];
```

**Line 565: Display LOR (mono)** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int el = 0; el < 4; ++el) {
    const int engLane = dotModular::EDITOR_TO_ENGINE_LANE[el];
// NEW:
for (int el = 0; el < dotModular::SandsGrid::POLY_LANES; ++el) {
    const int engLane = dotModular::EDITOR_TO_ENGINE_LANE_QMIX[el];
```

**Line 584: Direction cues** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
    int el = dotModular::ENGINE_LANE_TO_EDITOR[l];
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
    int el = dotModular::ENGINE_LANE_TO_EDITOR_QMIX[l];
```

**Line 617: Display own LOR** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
```

### Draw Function

**Line 676: Draw send arcs** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
```

**Line 686: Draw send items** - KEEP at 4 (4 items per lane)
```cpp
for (int it = 0; it < 4; ++it) {  // CORRECT - 4 items (Len/Off/Rot/Spr)
```

### Process Function

**Line 712: Prob-out early exit** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
```

**Line 720: Direction mod** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
// NEW:
for (int lane = 0; line < dotModular::SandsGrid::POLY_LANES; ++lane) {
```

**Line 740: Prob-out main loop** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
```

---

## MonsoonSandsVisualExpander.cpp - Updates Needed

### Widget Creation

**Line 254: Owner cells** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l) {
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l) {
```

**Line 329: Delegation mod jacks** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane)
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane)
```

**Line 335-337: Prob-out jacks** - UPDATE to MONO_LANES
```cpp
// OLD:
for (int l = 0; l < 6; ++l) {
// NEW:
for (int l = 0; l < dotModular::SandsGrid::MONO_LANES; ++l) {
```

### Step Function

**Line 363: Topology monoV1Owner** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int l = 0; l < 4; ++l)
// NEW:
for (int l = 0; l < dotModular::SandsGrid::POLY_LANES; ++l)
```

**Line 541: Delegation mod** - UPDATE to POLY_LANES
```cpp
// OLD:
for (int lane = 0; lane < 4; ++lane) {
// NEW:
for (int lane = 0; lane < dotModular::SandsGrid::POLY_LANES; ++lane) {
```

### Process Function

**Line 521: Prob-out early exit** - UPDATE to MONO_LANES
```cpp
// OLD:
for (int l = 0; l < 6; ++l)
// NEW:
for (int l = 0; l < dotModular::SandsGrid::MONO_LANES; ++l)
```

**Line 571: Prob-out main loop** - Already uses loop variable, verify bounds

---

## Additional Files to Check

### Monsoon.cpp
- Line 873: `for (int i = 0; i < 4; ++i)` - cv3Lane modulation - UPDATE to POLY_LANES
- Line 1103: `for (int i = 0; i < 4; ++i)` - Raffles slew CV - UPDATE to POLY_LANES
- Line 1113: `for (int i = 0; i < 4; ++i)` - cv3 offsets - UPDATE to POLY_LANES

### MonsoonExpanderManager.cpp
- Line 607: `for (int lane = 0; lane < 4; ++lane)` - Lock scope - UPDATE to POLY_LANES

### MonsoonSandsManager.cpp
- Line 130: `for (int l = 0; l < 4; ++l)` - Topology - UPDATE to POLY_LANES
- Line 301: `for (int l = 0; l < 4; ++l)` - Spread - UPDATE to POLY_LANES
- Line 611: `for (int lane = 0; lane < 4; ++lane)` - Spread values - UPDATE to POLY_LANES

---

## Testing Checklist

After all updates:

- [ ] Build succeeds without warnings
- [ ] Plugin loads in VCV Rack
- [ ] East module shows 5 poly lanes (MEL/OCT/QMIX/REST/ACC)
- [ ] Macro module shows 5 poly lanes
- [ ] Mono module shows 7 lanes (MEL/OCT/QMIX/REST/ACC/VAR/LEG)
- [ ] All 5 prob-out jacks work on East/Macro
- [ ] All 7 prob-out jacks work on Mono
- [ ] CV inputs work for all lanes
- [ ] Spread knobs work for all lanes
- [ ] Owner cells work for all poly lanes
- [ ] Direction cells work for all lanes
- [ ] No crashes when switching tabs
- [ ] No crashes when connecting/disconnecting modules
- [ ] Existing patches still work (backward compatibility)

---

## Notes

- **CV columns**: Always 4 per lane (Len/Off/Rot/Spr) - never change these loops
- **Lane loops**: Update to POLY_LANES (5) or MONO_LANES (7) or EAST_LANES (7)
- **Mapping arrays**: Use `_QMIX` variants (EDITOR_TO_ENGINE_LANE_QMIX, etc.)
- **String arrays**: Add "Q-MIX" or "QMIX" as 5th element
- **Comments**: Update to reflect 5 poly lanes, 7 mono/east lanes

---

## Estimated Time

- Loop updates: 2-3 hours
- Testing: 1-2 hours
- Bug fixes: 1-2 hours
- **Total**: 4-7 hours

---

## Priority Order

1. **Critical** (module won't load): OutputId enums, configOutput calls
2. **High** (crashes): process() function loops, topology loops
3. **Medium** (UI broken): widget creation loops, step() function loops
4. **Low** (cosmetic): comments, string arrays

Start with Critical, then High, then test before proceeding to Medium/Low.
