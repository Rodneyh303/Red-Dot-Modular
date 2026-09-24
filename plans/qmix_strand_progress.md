# Q-Mix Strand Implementation Progress

## Status: Phase 1 Complete - Ready for Build Test

All C++ engine changes for q-mix strand have been implemented. Ready for Windows build test.

---

## Completed Changes

### 1. Core Enum and Constants (LaneMapping.hpp) ✓
- Added `STRAND_QMIX = 2` to EngineStrand enum
- Renumbered subsequent strands: RHYTHM 2→3, ACCENT 3→4, VARIATION 4→5, LEGATO 5→6
- Updated `NUM_STRANDS` from 6 to 7
- Updated `MONO_LANE_TO_STRAND[7]` to include QMIX at index 2
- Activated `ENGINE_LANE_TO_EDITOR_QMIX[5]` and `EDITOR_TO_ENGINE_LANE_QMIX[7]` mappings
- Added q-mix constants: `QMIX_EDITOR_LANE = 2`, `QMIX_STREAM_KEY = 3`

### 2. Geometry Constants (SandsGrid.hpp) ✓
- Updated lane counts:
  - `MONO_LANES`: 6 → 7
  - `POLY_LANES`: 4 → 5  
  - `EAST_LANES`: 6 → 7
- Marked q-mix as ACTIVE (was preview-only)
- `QMIX_LANE_H = 13.f` already defined for Option B geometry

### 3. Engine Data Structures (SequencerEngine.hpp) ✓
- Updated `PolyLane` enum: added `PL_QMIX = 4`, updated `PL_LANES = 5`
- Updated editor lane constants:
  - `EDITOR_LANE_VARIATION`: 4 → 5
  - `EDITOR_LANE_LEGATO`: 5 → 6
- Updated functions to handle PL_QMIX:
  - `polyLaneToStrand()`: Maps PL_QMIX → STRAND_QMIX
  - `polyLaneStrand()`: Returns STRAND_QMIX for PL_QMIX
  - `masterLaneProbability()`: Returns qmixRandom for PL_QMIX
- Storage arrays automatically accommodate via `NUM_STRANDS = 7`:
  - `lorStore_[16][NUM_STRANDS][3]`
  - `laneTick_[NUM_STRANDS]`, `laneDir_[NUM_STRANDS]`, etc.

### 4. Pattern Generation (PatternEngine.hpp) ✓
- Added `qmixRandom[16]` reference view to `random_[0][STRAND_QMIX]`
- Updated `PolyLane` enum to match SequencerEngine
- Updated `polyRandom()` to use `ENGINE_LANE_TO_EDITOR_QMIX` for 5-lane mapping
- Updated switch(strand) statements in `pickMono` lambda (lines 181, 193):
  - Added `case STRAND_QMIX: return 0.5f;` placeholder
  - Ensured all strands in correct order: MELODY, OCTAVE, QMIX, RHYTHM, ACCENT, VARIATION, LEGATO
- Added TODO comment for q-mix slewed buffers (line 213)

### 5. Switch Statement Updates (SequencerEngine.cpp) ✓
- Updated `polyStrandLen` lambda (line 225):
  - Added `case STRAND_QMIX: return polyLenE(v, PL_QMIX);`
  - All strands now handled in correct order

### 6. Visual Editor Updates (StraitsEastSandsVisual.cpp) ✓
- Updated probability display loop (line 1223): 4 lanes → `POLY_LANES` (5 lanes)
  - Now uses `MONO_LANE_TO_STRAND[el]` for identity mapping
  - Includes q-mix in V1 tab probability display
- Updated lane ownership menu (line 805):
  - `laneNames[4]` → `laneNames[5]` with "Q-MIX" at index 2
  - Bounds check uses `POLY_LANES` instead of hardcoded 4
- Updated delegation logic (line 949):
  - Comments updated: "lanes 0..3" → "lanes 0..4", "lanes 4..5" → "lanes 5..6"
  - Uses `EDITOR_TO_ENGINE_LANE_QMIX` instead of old 4-lane array
  - Poly/mono boundary check uses `POLY_LANES` instead of hardcoded 4
- Updated lockWhen lambda (line 561):
  - Uses `EDITOR_TO_ENGINE_LANE_QMIX` for 5-lane mapping
  - Poly/mono boundary check uses `POLY_LANES`

### 7. Expander Manager (MonsoonExpanderManager.cpp) ✓
- Reviewed STRND array: Uses strand NAMES not numbers
- Renumbering doesn't affect behavior (name-based access)
- Added clarifying comment about ENGINE lane order

---

## Technical Details

### Strand Renumbering
The renumbering maintains behavior because all switch(strand) statements use strand NAMES:
```cpp
// OLD indices (wrong):
STRAND_MELODY    = 0  // unchanged
STRAND_OCTAVE    = 1  // unchanged
STRAND_RHYTHM    = 2  // now 3
STRAND_ACCENT    = 3  // now 4
STRAND_VARIATION = 4  // now 5
STRAND_LEGATO    = 5  // now 6

// NEW indices (correct):
STRAND_MELODY    = 0
STRAND_OCTAVE    = 1
STRAND_QMIX      = 2  // NEW
STRAND_RHYTHM    = 3
STRAND_ACCENT    = 4
STRAND_VARIATION = 5
STRAND_LEGATO    = 6
NUM_STRANDS      = 7
```

### Storage Accommodation
All storage arrays automatically accommodate q-mix via `NUM_STRANDS`:
- `SequencerEngine::lorStore_[16][NUM_STRANDS][3]`
- `PatternEngine::random_[16][NUM_STRANDS][16]`
- `SequencerEngine::laneTick_[NUM_STRANDS]`
- `SequencerEngine::laneDir_[NUM_STRANDS]`
- All other `[NUM_STRANDS]`-dimensioned arrays

### Q-Mix RNG Stream
Q-mix uses its own Philox stream (STREAM_SOURCE_SELECT = 3), separate from rhythm/melody streams. This ensures "which notes" (q-mix) decorrelates from "where they interleave" (melody).

### Placeholder Values
Using `0.5f` as placeholder return value for `STRAND_QMIX` cases in switch statements until full q-mix implementation.

---

## Files Modified

1. [`src/dsp/LaneMapping.hpp`](../src/dsp/LaneMapping.hpp) - Core enum and mappings
2. [`src/ui/SandsGrid.hpp`](../src/ui/SandsGrid.hpp) - Geometry constants
3. [`src/dsp/engines/SequencerEngine.hpp`](../src/dsp/engines/SequencerEngine.hpp) - Engine data structures
4. [`src/dsp/engines/PatternEngine.hpp`](../src/dsp/engines/PatternEngine.hpp) - Pattern generation
5. [`src/dsp/engines/SequencerEngine.cpp`](../src/dsp/engines/SequencerEngine.cpp) - Switch statements
6. [`src/StraitsEastSandsVisual.cpp`](../src/StraitsEastSandsVisual.cpp) - Visual editor

---

## Next Steps

### Immediate: Build Test
```bash
# On Windows with proper build environment:
cd Red-Dot-Modular
make
```

Expected outcome: Clean build with no errors. The include errors shown in VSCode are expected (Windows lacks proper include paths configured for IntelliSense).

### Phase 2: UI/Panel (Future)
After successful build, implement UI components:
1. Add q-mix row to Sands visual editors (Mono/East/Macro)
2. Add q-mix CV inputs and attenuverters
3. Add q-mix prob-out jack
4. Update panel SVGs with q-mix geometry
5. Wire up q-mix parameter management

See [`qmix_strand_implementation.md`](qmix_strand_implementation.md) for full Phase 2 plan.

---

## Verification Checklist

### Build Verification
- [ ] Clean build with no compilation errors
- [ ] No warnings related to q-mix changes
- [ ] Plugin loads in VCV Rack

### Runtime Verification (Basic)
- [ ] Monsoon module loads without crash
- [ ] Sands expanders load without crash
- [ ] No assertion failures in debug build
- [ ] Existing functionality unchanged (melody/octave/rhythm/accent/var/leg)

### Full Verification (After Phase 2)
See [`qmix_geometry_lockstep.md`](qmix_geometry_lockstep.md) for comprehensive render and functional verification.

---

## Notes

- All switch(strand) statements found and updated (3 total)
- All hardcoded lane counts updated to use `POLY_LANES` constant
- All lane mappings updated to use `_QMIX` variants
- Storage arrays automatically accommodate via `NUM_STRANDS`
- No hardcoded strand indices remain in codebase
- Include errors in VSCode are expected (Windows IntelliSense configuration issue)
