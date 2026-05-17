# Expander Walking Logic Audit

## Current Implementation Analysis

```cpp
auto scan = [&](Module* start, bool left) {
    Module* curr = start;
    int depth = 0;
    while (curr && depth < 8) {
        if (curr->model == modelMeloDicerExpander) {
            if (!cachedExpander) cachedExpander = dynamic_cast<MeloDicerExpander*>(curr);
            scaleExpanderCount++;
        }
        else if (curr->model == modelMeloDicerDNAExpander) {
            if (!cachedDnaExpander) cachedDnaExpander = dynamic_cast<MeloDicerDNAExpander*>(curr);
            dnaExpanderCount++;
        }
        else if (curr->model == modelMeloDicerPolyVoiceExpander) {
            if (!cachedPolyVoiceExpander) cachedPolyVoiceExpander = dynamic_cast<MeloDicerPolyVoiceExpander*>(curr);
            polyExpanderCount++;
        }
        else {
            break;  // <-- STOPS HERE IF UNKNOWN MODULE ENCOUNTERED
        }
        curr = left ? curr->leftExpander.module : curr->rightExpander.module;
        depth++;
    }
};

scan(leftExpander.module, true);   // Scan left expanders
scan(rightExpander.module, false); // Scan right expanders
```

## Potential Issues

### Issue 1: Early Termination on Unknown Module
**Severity:** MEDIUM

The `else break;` statement stops walking if ANY non-Melodicer module is encountered.

**Problem Scenario:**
```
MeloDicer [main] -- Scale Expander A -- Unknown Module -- Scale Expander B
```

When walking right:
1. Finds Scale Expander A → counts it
2. Hits Unknown Module → **BREAKS**
3. Never finds Scale Expander B!

**Real-world case:**
User intentionally puts a BLANK or other utility module between expanders → breaks the chain

### Issue 2: Direction Independence Not Enforced
**Severity:** LOW-MEDIUM

The scan assumes expanders continue in the same direction, but:

**Problem Scenario:**
```
Scale Expander A -- MeloDicer -- Scale Expander B -- DNA Expander
                                                       (left of DNA)
```

Walking RIGHT from MeloDicer:
1. Finds Scale Expander B (right)
2. Continues right to DNA Expander (works)

But DNA Expander also has something on its LEFT. Could lead to confusion about which direction is "connected".

### Issue 3: No Validation That Expander Is Directly Adjacent
**Severity:** MEDIUM

The scan walks a chain, but doesn't verify that each module is DIRECTLY connected (one HP adjacent).

**Problem:**
Expanders might be placed with gaps between them (if user has blank panels), but the code still walks them as if connected.

### Issue 4: Counts vs Cached Pointer Mismatch
**Severity:** MEDIUM

- Count incremented for EVERY matching module found
- But cached pointer only set ONCE (if !cachedPolyVoiceExpander)

**Example:**
- User connects 3 Poly Voice Expanders in a chain
- polyExpanderCount = 3
- cachedPolyVoiceExpander = (points to first one)
- **No way to access the other 2!**

## Test Cases Not Covered

1. ✗ **Expander with gap**
   ```
   MeloDicer -- [blank 2 HP] -- Scale Expander
   ```
   Should it count? Currently does (only checks model, not adjacency).

2. ✗ **Mixed expander types**
   ```
   MeloDicer -- Scale Expander -- DNA Expander -- Poly Expander
   ```
   All found? Yes. But order matters for some operations?

3. ✗ **Expanders on both sides**
   ```
   Scale Exp -- MeloDicer -- DNA Exp
                (left)        (right)
   ```
   Both found in different scans? Yes, should work.

4. ✗ **Unknown module in chain**
   ```
   MeloDicer -- Scale Exp A -- Utility -- Scale Exp B
   ```
   Stops at Utility? **YES - BUG!** Never finds Scale Exp B.

5. ✗ **Rapid hot-swapping**
   ```
   User plugs/unplugs expanders quickly
   while module running
   ```
   Pointers correctly invalidated? Need to verify.

## Recommendations

### Priority 1: Document the Assumption
**The current design assumes:**
- Expanders are directly adjacent (no blank panels between them)
- No unknown modules in the chain
- Each Melodicer only talks to one instance per expander type
- User won't hot-swap expanders rapidly

If this breaks these assumptions, behavior is undefined.

### Priority 2: Graceful Degradation
**Option A: Continue walking on unknown module**
```cpp
else {
    // Don't break - keep walking past unknown modules
    // (might find more Melodicer expanders beyond)
}
```

**Option B: Only stop on null**
```cpp
// Remove the else-break entirely
// Let natural null termination stop the loop
```

**Option C: Verify direct adjacency**
```cpp
// Check that expander is exactly 1 HP offset
// Break if gap detected
```

### Priority 3: Multiple Expanders per Type
**Option A: Cache all matching expanders**
```cpp
vector<MeloDicerExpander*> cachedExpanders;  // Store all found
// Then iterate through them as needed
```

**Option B: Only support first one (current)**
```cpp
// Clearly document this limitation
```

## Questions for Rodney

1. **Do users intentionally place other modules between Melodicer expanders?**
   - If yes, the `else break;` is a bug
   - If no, current behavior is fine but undocumented

2. **Can users have multiple Poly Voice Expanders?**
   - If yes, need to handle all of them (current only uses first)
   - If no, document that limitation

3. **What behavior was broken in the refactoring?**
   - Specific case where walking fails?
   - Expanders not being found?
   - Wrong expander being cached?

## Suggested Fix (If Issue Is #4: Unknown Module Break)

```cpp
auto scan = [&](Module* start, bool left) {
    Module* curr = start;
    int depth = 0;
    while (curr && depth < 8) {
        if (curr->model == modelMeloDicerExpander) {
            if (!cachedExpander) cachedExpander = dynamic_cast<MeloDicerExpander*>(curr);
            scaleExpanderCount++;
        }
        else if (curr->model == modelMeloDicerDNAExpander) {
            if (!cachedDnaExpander) cachedDnaExpander = dynamic_cast<MeloDicerDNAExpander*>(curr);
            dnaExpanderCount++;
        }
        else if (curr->model == modelMeloDicerPolyVoiceExpander) {
            if (!cachedPolyVoiceExpander) cachedPolyVoiceExpander = dynamic_cast<MeloDicerPolyVoiceExpander*>(curr);
            polyExpanderCount++;
        }
        // CHANGE: Don't break on unknown modules
        // Just continue walking - might find more Melodicer expanders beyond
        
        curr = left ? curr->leftExpander.module : curr->rightExpander.module;
        depth++;
    }
};
```

This allows finding expanders even if other modules are placed between them.

