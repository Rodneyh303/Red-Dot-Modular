# Performance Analysis: 4x CPU Regression

**Status:** Critical - optimizations applied but still 4x slower than pre-refactoring  
**Cause:** Likely manager call chain + compiler inlining issues, not just hot-path micro-optimizations

## What We Know

✅ Pre-refactoring code: 1x CPU  
✅ Post-refactoring (all managers): 4x CPU  
✅ After lambda + param optimizations: Still 4x CPU  
❌ Root cause not yet identified  

## Critical Issues Found

### 1. Remaining Lambda in Hot Path (Line 852)
```cpp
auto onPhraseBoundary = [this]() { onPhraseBoundary_(); };
modeController->executeMode(..., onPhraseBoundary)
```
Created EVERY SAMPLE. Still a callback, defeating lambda optimization.

### 2. Manager Call Chain Depth
```
MeloDicer::process()
  → timingController->processRunGate()
  → timingController->processResetGate()
  → timingController->processGateEdges()
  → timingController->handleGate1Assignment()
  → timingController->handleGate2Assignment()
  → modeController->executeMode()
    → engine.execute*(...)
    → (more calls...)
```
20+ function calls/sample. At ~5-10 cycles/call = 100-200 cycles/sample lost.

### 3. Compiler Not Inlining
- Manager methods in separate `.cpp` files
- Without `-flto` (Link-Time Optimization), can't inline across files
- Each call has prologue/epilogue overhead
- Estimated cost: 100-200 cycles/sample

## Diagnosis Strategy

### Step 1: Check Build Flags
```bash
# Check Makefile or plugin.mk for:
grep -E "O[0-3]|flto|march" Makefile
# Should show: -O3 -flto -march=...
```

### Step 2: Profile to Find Hotspot
```bash
# Using perf (Linux):
perf record -F 997 -g ./build/plugin.so
perf report --show-total-period

# Should show which function uses most CPU time
# If TimingController/ModeController high → call chain issue
# If GateState high → may have changed behavior
# If PatternEngine high → engine may have gotten slower
```

### Step 3: Verify Compiler Optimization
Try rebuilding with explicit flags:
```bash
make clean
CFLAGS="-O3 -flto -march=native" make
# Test performance...
```

### Step 4: Identify The Culprit
Profile output will show:
- **If TimingController/ModeController top:** Manager call chain is bottleneck
- **If engine.execute*() top:** Engine complexity increased  
- **If GateState top:** Gate state processing changed
- **If broad:** Multiple small issues (worse case)

## Possible Solutions (In Order of Likelihood)

### Priority 1: Enable LTO (Easiest)
If not enabled, this alone could save 50% overhead:
```makefile
CFLAGS += -flto
LDFLAGS += -flto
```

### Priority 2: Remove Remaining Lambda
Change ModeController signature to NOT take callback:
```cpp
// BEFORE: callback pattern
modeController->executeMode(..., [this]() { ... })

// AFTER: enum pattern or direct call
modeController->executeMode(...);
if (modeController->checkPhraseBoundary()) {
    onPhraseBoundary_();
}
```

### Priority 3: Inline Critical Managers
Add explicit inline hints:
```cpp
// In .hpp files
inline bool TimingController::processRunGate(...) { ... }
inline void ModeController::executeMode(...) { ... }
```

### Priority 4: Reduce Call Chain Depth
Consolidate small managers:
```cpp
// Instead of:
timingController->processRunGate();
timingController->processResetGate();
timingController->processGateEdges();

// Do:
timingController->processAllGates();  // batched
```

### Priority 5: Direct Access Patterns
For ultra-hot paths, consider restoring some direct access:
```cpp
// Instead of: paramManager->getPolyRest(i)
// Cache:
float restProb = paramManager->getPolyRest(i);  // once per change

// Then use cached value
engine.voices[i].restProb = restProb;
```

## Testing Protocol

```bash
# Build with current code
git checkout master
make clean && make
# Measure CPU usage...

# Try LTO only
# (modify Makefile, rebuild)
# Measure again...

# If improved but still slow:
# Profile to find remaining hotspot

# Apply targeted fixes based on profile
```

## Expected Outcomes

- **LTO alone:** 30-50% improvement possible
- **Lambda removal:** 10-20% improvement
- **Both:** Could get to 2-3x (still not back to 1x)
- **Full optimization:** May need all 5 priorities to reach 1.5-2x

## If Still 3-4x After All This

Then the refactoring architecture itself may be fundamentally slower:
- Manager-based dispatch adds unavoidable overhead
- May need to revert to simpler architecture
- Or accept performance cost for better code organization

## Recommended Next Action

1. **Check build flags FIRST** - LTO might be all that's needed
2. **Profile SECOND** - Know where the CPU is actually going
3. **Fix based on profile** - Don't guess

This is a real issue, but it's solvable with data-driven approach.

