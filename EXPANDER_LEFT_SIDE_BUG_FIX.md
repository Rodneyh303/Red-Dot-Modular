# Poly Expander Left-Side Detection Bug - Root Cause & Fix

## Problem Statement
After refactoring, the Poly Voice Expander is not detected when connected on the LEFT side of MeloDicer.
- ✅ Works on RIGHT side
- ❌ Doesn't work on LEFT side
- ✅ All other expanders work on both sides

## Root Cause Analysis

The walking code treats all three expanders identically:
```cpp
auto scan = [&](Module* start, bool left) {
    Module* curr = start;
    int depth = 0;
    while (curr && depth < 8) {
        if (curr->model == modelMeloDicerExpander) { ... }
        else if (curr->model == modelMeloDicerDNAExpander) { ... }
        else if (curr->model == modelMeloDicerPolyVoiceExpander) { ... }
        else { break; }
        
        curr = left ? curr->leftExpander.module : curr->rightExpander.module;
        depth++;
    }
};

scan(leftExpander.module, true);   // Scan LEFT direction
scan(rightExpander.module, false); // Scan RIGHT direction
```

### Potential Issues

1. **Model Not Yet Initialized**
   - modelMeloDicerPolyVoiceExpander is created in separate compilation unit
   - If PolyVoiceExpander.cpp hasn't been loaded yet, pointer is nullptr
   - LEFT scan happens in initialize() before all models registered?

2. **Asymmetric Cache Update on Left**
   ```cpp
   if (!cachedPolyVoiceExpander) cachedPolyVoiceExpander = ...;
   ```
   - Once cached on RIGHT side, LEFT side won't update it
   - But that shouldn't prevent FINDING it on LEFT

3. **Null Pointer Risk**
   ```cpp
   curr = left ? curr->leftExpander.module : curr->rightExpander.module;
   ```
   - If `left=true` but module has no left neighbor, curr becomes nullptr
   - Next iteration would exit loop
   - This is correct behavior though

4. **Model Type Mismatch**
   - What if the PolyVoiceExpander model was changed in refactoring?
   - Walking code checks `curr->model == modelMeloDicerPolyVoiceExpander`
   - If model was renamed/changed, comparison fails

## Recommended Fix

Add explicit validation and ensure model pointers are initialized:

```cpp
void MeloDicer::updateExpanderPointers() {
    // CRITICAL: Clear all pointers before rescanning
    cachedExpander = nullptr;
    cachedDnaExpander = nullptr;
    cachedPolyVoiceExpander = nullptr;

    scaleExpanderCount = 0;
    dnaExpanderCount = 0;
    polyExpanderCount = 0;

    // Validate that all model pointers are initialized
    // (They should be from plugin init(), but being defensive)
    assert(modelMeloDicerExpander != nullptr);
    assert(modelMeloDicerDNAExpander != nullptr);
    assert(modelMeloDicerPolyVoiceExpander != nullptr);

    // Walk expander chains in both directions
    auto scan = [&](Module* start, bool left) {
        if (!start) return;  // Defensive: handle null start
        
        Module* curr = start;
        int depth = 0;
        while (curr && depth < 8) {
            // Check each model type
            // All three should work on both left and right
            if (curr->model == modelMeloDicerExpander) {
                if (!cachedExpander) {
                    cachedExpander = dynamic_cast<MeloDicerExpander*>(curr);
                }
                scaleExpanderCount++;
            }
            else if (curr->model == modelMeloDicerDNAExpander) {
                if (!cachedDnaExpander) {
                    cachedDnaExpander = dynamic_cast<MeloDicerDNAExpander*>(curr);
                }
                dnaExpanderCount++;
            }
            else if (curr->model == modelMeloDicerPolyVoiceExpander) {
                if (!cachedPolyVoiceExpander) {
                    cachedPolyVoiceExpander = dynamic_cast<MeloDicerPolyVoiceExpander*>(curr);
                }
                polyExpanderCount++;
            }
            else {
                // Unknown module - stop walking (correct for Rack direct adjacency)
                break;
            }
            
            // Move to next module in the chain
            curr = left ? curr->leftExpander.module : curr->rightExpander.module;
            depth++;
        }
    };

    // Scan left and right directions
    scan(leftExpander.module, true);   // Scan LEFT
    scan(rightExpander.module, false); // Scan RIGHT
}
```

## Testing the Fix

```
Configuration 1 (RIGHT side):
MeloDicer -- Poly Expander
Expected: Found ✓
Current: Works ✓

Configuration 2 (LEFT side):
Poly Expander -- MeloDicer
Expected: Found ✓
Current: Broken ❌
After Fix: Should work ✓

Configuration 3 (Both sides):
Poly Exp -- MeloDicer -- DNA Exp
Expected: Both found ✓
After Fix: Both should work ✓

Configuration 4 (Chained):
Scale -- Poly -- MeloDicer
Expected: Both found ✓
After Fix: Both should work ✓
```

## Why This Might Not Be The Actual Issue

The walking code is logically sound. The issue might be:
1. Model objects not being registered in the right order
2. Timing issue in initialization
3. Something else entirely about how left-side modules are detected

**Need to verify:**
- Is `modelMeloDicerPolyVoiceExpander` actually pointing to a valid Model?
- Is the walking code actually being called for left-side scan?
- Are null checks protecting against uninitialized pointers?

## Next Investigation Steps

1. Add logging to see if scan is called for LEFT
2. Check if poly model is nullptr
3. Check if assertion would catch initialization issues
4. Profile exact scenario that fails

