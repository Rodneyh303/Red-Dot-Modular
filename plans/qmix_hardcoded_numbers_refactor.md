# Q-Mix Hardcoded Numbers Refactoring Recommendations

## Problem
The q-mix strand addition exposed many hardcoded magic numbers throughout the codebase (4, 6, 16, 18, 21, 24, 28, etc.). This made the implementation error-prone and difficult to maintain.

## Current Constants (in SandsGrid.hpp)
```cpp
namespace dotModular::SandsGrid {
    constexpr int POLY_LANES = 5;  // REST, MEL, QMIX, OCT, ACC
    constexpr int MONO_LANES = 7;  // MEL, OCT, REST, ACC, QMIX, VAR, LEG
    constexpr int EAST_LANES = 7;  // Same as MONO_LANES
}
```

## Recommended Additional Constants

### 1. Storage Array Sizes
Add to `SandsGrid.hpp`:
```cpp
namespace dotModular::SandsGrid {
    // Existing
    constexpr int POLY_LANES = 5;
    constexpr int MONO_LANES = 7;
    constexpr int EAST_LANES = 7;
    
    // NEW: Derived constants for storage arrays
    constexpr int VOICE_SLOTS = 16;  // V1/mono + V2..V16
    constexpr int LOR_ITEMS = 3;     // Length, Offset, Rotation
    constexpr int ATTEN_COLS = 4;    // LEN, OFF, ROT, SPR
    
    // LOR base storage: 16 voice slots × 7 mono lanes × 3 LOR items
    constexpr int LOR_BASE_SIZE = VOICE_SLOTS * MONO_LANES * LOR_ITEMS;  // = 336
    constexpr int LOR_BASE_STRIDE = MONO_LANES * LOR_ITEMS;              // = 21
    
    // Spread storage: 16 voice slots × 5 poly lanes
    constexpr int SPREAD_SIZE = VOICE_SLOTS * POLY_LANES;  // = 80
    constexpr int SPREAD_STRIDE = POLY_LANES;              // = 5
    
    // Mono attenuverter storage: 7 mono lanes × 4 columns
    constexpr int MONO_ATTEN_SIZE = MONO_LANES * ATTEN_COLS;  // = 28
    
    // Global arrays (Macro scope)
    constexpr int GLOBAL_LOR_SIZE = POLY_LANES * LOR_ITEMS;    // = 15 (was 12, needs update)
    constexpr int GLOBAL_ATTEN_SIZE = POLY_LANES * ATTEN_COLS; // = 20 (was 16, needs update)
    constexpr int GLOBAL_TAP_SIZE = POLY_LANES * 2;            // = 10 (was 8, needs update)
}
```

### 2. Module Input/Output Counts
Add to each module's namespace:

**StraitsMacroVisualIds:**
```cpp
namespace StraitsMacroVisualIds {
    // Derived from POLY_LANES
    constexpr int NUM_PROB_OUTPUTS = dotModular::SandsGrid::POLY_LANES;  // = 5
    constexpr int NUM_CV_INPUTS_PER_LANE = 4;  // LEN, OFF, ROT, SPR
    constexpr int NUM_CV_INPUTS = dotModular::SandsGrid::POLY_LANES * NUM_CV_INPUTS_PER_LANE;  // = 20
    constexpr int NUM_DIR_MOD_INPUTS = dotModular::SandsGrid::POLY_LANES;  // = 5
}
```

**SandsMonoVisualIds:**
```cpp
namespace SandsMonoVisualIds {
    constexpr int NUM_LOR_CV_INPUTS = dotModular::SandsGrid::MONO_LANES * 3;  // = 21
    constexpr int NUM_SPREAD_CV_INPUTS = dotModular::SandsGrid::POLY_LANES;   // = 5
    constexpr int NUM_DIR_MOD_INPUTS = dotModular::SandsGrid::MONO_LANES;     // = 7
    constexpr int NUM_DELEG_MOD_INPUTS = dotModular::SandsGrid::POLY_LANES;   // = 5
    constexpr int NUM_PROB_OUTPUTS = dotModular::SandsGrid::MONO_LANES;       // = 7
}
```

**StraitsEastVisualIds:**
```cpp
namespace StraitsEastVisualIds {
    constexpr int NUM_POLY_CV_INPUTS = dotModular::SandsGrid::POLY_LANES * 4;  // = 20
    constexpr int NUM_VARLEG_CV_INPUTS = 2 * 3;  // VAR/LEG × (LEN/OFF/ROT)
    constexpr int NUM_DIR_MOD_INPUTS = dotModular::SandsGrid::EAST_LANES;      // = 7
    constexpr int NUM_DELEG_MOD_INPUTS = dotModular::SandsGrid::EAST_LANES;    // = 7
    constexpr int NUM_PROB_OUTPUTS = dotModular::SandsGrid::POLY_LANES;        // = 5
}
```

## Refactoring Strategy

### Phase 1: Add Constants (Low Risk)
1. Add all derived constants to `SandsGrid.hpp`
2. Add module-specific constants to each module's namespace
3. Build and verify no changes in behavior

### Phase 2: Replace Enum Calculations (Medium Risk)
Replace hardcoded enum calculations with constants:
```cpp
// BEFORE
enum InputId {
    CV_START = 0,
    SPR_CV_START = CV_START + 21,       // hardcoded
    DIR_MOD_START = SPR_CV_START + 5,   // hardcoded
    NUM_INPUTS = DELEG_MOD_START + 5    // hardcoded
};

// AFTER
enum InputId {
    CV_START = 0,
    SPR_CV_START = CV_START + NUM_LOR_CV_INPUTS,
    DIR_MOD_START = SPR_CV_START + NUM_SPREAD_CV_INPUTS,
    DELEG_MOD_START = DIR_MOD_START + NUM_DIR_MOD_INPUTS,
    NUM_INPUTS = DELEG_MOD_START + NUM_DELEG_MOD_INPUTS
};
```

### Phase 3: Replace Storage Array Sizes (Medium Risk)
```cpp
// BEFORE
float lorBase[336] = {0};
float spread[80] = {0};
float monoAtten[28] = {0};

// AFTER
float lorBase[dotModular::SandsGrid::LOR_BASE_SIZE] = {0};
float spread[dotModular::SandsGrid::SPREAD_SIZE] = {0};
float monoAtten[dotModular::SandsGrid::MONO_ATTEN_SIZE] = {0};
```

### Phase 4: Replace Accessor Strides (High Risk - Test Thoroughly)
```cpp
// BEFORE
float getLorBase(int slot, int bank, int c) const { 
    return editor.lorBase[slot*21 + bank*3 + c]; 
}

// AFTER
float getLorBase(int slot, int bank, int c) const { 
    return editor.lorBase[slot*LOR_BASE_STRIDE + bank*LOR_ITEMS + c]; 
}
```

## Benefits
1. **Single source of truth**: Change POLY_LANES in one place, everything updates
2. **Self-documenting**: `LOR_BASE_SIZE` is clearer than `336`
3. **Compile-time validation**: Mismatched sizes cause compile errors, not runtime crashes
4. **Easier maintenance**: Future lane additions only require updating base constants

## Risks
- Accessor functions with strides are performance-critical (audio thread)
- Ensure constants are `constexpr` so they compile to literals
- Test thoroughly after each phase

## Recommendation
Implement Phase 1 now (add constants), defer Phases 2-4 to a future refactoring branch after q-mix is stable.
