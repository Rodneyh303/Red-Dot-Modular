# Q-Mix Strand Implementation Plan

**Branch:** `feat/sands-qmix-geometry` (tip: f5260be)  
**Date:** 2026-09-17  
**Prerequisite:** Geometry work complete (verified in `qmix_geometry_lockstep.md`)

## Executive Summary

This plan implements the q-mix strand as a full Sands lane with its own Philox RNG stream (STREAM_SOURCE_SELECT). The geometry is already in place; this adds the **data model** (strand enum, arrays, params) atomically.

**Critical Rule:** Check the code, not the docs. Several docs are stale after Mode F and quantizer-unification commits.

## Q-Mix Context (from RANDOM_VS_INPUT_MODULE_CONCEPT.md)

**What is q-mix?**
- Per-voice probability of taking a GENERATED note vs external INPUT note in quantizer mode
- Completes the external-input symmetry: gates ← rhythm/legato/rest/accent; melody CV ← q-mix
- Uses its own Philox RNG stream (STREAM_SOURCE_SELECT = 3)
- Position 3 on Sands (after MEL/OCT, before REST/ACCENT) — data-flow adjacency

**Distribution:**
- Big5 → Big6 on Monsoon (q-mix as headline modulation target)
- Poly knobs → Straits expanders
- CV inputs → Causeway
- Mix-in tap → Sands Macro

## Implementation Scope

### Phase 1: Engine Strand (This Plan)

**Files to modify:**
1. [`src/dsp/LaneMapping.hpp`](../src/dsp/LaneMapping.hpp:107-112) — Activate STRAND_QMIX enum
2. [`src/dsp/engines/SequencerEngine.hpp`](../src/dsp/engines/SequencerEngine.hpp) — Add q-mix arrays
3. [`src/Monsoon.hpp`](../src/Monsoon.hpp) — Add q-mix params/storage
4. Engine managers — Wire q-mix into processing pipeline

**What this adds:**
- `STRAND_QMIX = 2` in EngineStrand enum (NUM_STRANDS 6→7)
- Per-voice q-mix probability arrays (16 voices × 16 steps)
- LEN/OFF/ROT params for q-mix strand
- Philox stream key = STREAM_SOURCE_SELECT (3)

### Phase 2: UI/Panel (Future)

**Not in this plan:**
- Sands visual editor updates (slot 2 becomes editable)
- Monsoon Big6 knob
- Straits poly knobs
- Causeway CV inputs
- Panel regeneration

## Detailed Implementation

### 1. Activate STRAND_QMIX Enum

**File:** [`src/dsp/LaneMapping.hpp`](../src/dsp/LaneMapping.hpp:32-44)

**Current (lines 32-44):**
```cpp
enum EngineStrand {
    STRAND_MELODY    = 0,
    STRAND_OCTAVE    = 1,
    STRAND_RHYTHM    = 2,   // REST
    STRAND_ACCENT    = 3,
    STRAND_VARIATION = 4,
    STRAND_LEGATO    = 5,
    NUM_STRANDS      = 6,
};
```

**Change to:**
```cpp
enum EngineStrand {
    STRAND_MELODY    = 0,
    STRAND_OCTAVE    = 1,
    STRAND_QMIX      = 2,   // Q-mix (quantizer mode: blend generated vs input)
    STRAND_RHYTHM    = 3,   // REST (was 2)
    STRAND_ACCENT    = 4,   // (was 3)
    STRAND_VARIATION = 5,   // (was 4)
    STRAND_LEGATO    = 6,   // (was 5)
    NUM_STRANDS      = 7,   // (was 6)
};
```

**Impact:** This renumbers strands 2-5 → 3-6. Must update all references.

**Activate planned mappings (lines 117-123):**
```cpp
// Already defined, just activate by using NUM_STRANDS_QMIX
constexpr int MONO_LANE_TO_STRAND_QMIX[7] = { 0, 1, 2, 3, 4, 5, 6 };
constexpr int ENGINE_LANE_TO_EDITOR_QMIX[5] = { 3, 0, 1, 4, 2 };
constexpr int EDITOR_TO_ENGINE_LANE_QMIX[7] = { 1, 2, 4, 0, 3, POLY_NONE, POLY_NONE };
```

### 2. Add Q-Mix Arrays to SequencerEngine

**File:** [`src/dsp/engines/SequencerEngine.hpp`](../src/dsp/engines/SequencerEngine.hpp)

**Add per-voice q-mix probability arrays:**
```cpp
// Q-mix strand (quantizer mode: blend generated vs input melody)
// Per-voice probability arrays (16 voices × 16 steps)
float qmixProb[16][16] = {};  // Per-voice, per-step q-mix probability
int   qmixLen[16]      = {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16};
int   qmixOff[16]      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int   qmixRot[16]      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
```

**Add to strand accessors:**
Update `melodyLen()`, `melodyOff()`, `melodyRot()` pattern to include `qmixLen()`, `qmixOff()`, `qmixRot()`.

### 3. Add Q-Mix to Monsoon Module

**File:** [`src/Monsoon.hpp`](../src/Monsoon.hpp)

**Add to editor storage:**
```cpp
struct EditorState {
    // ... existing fields ...
    
    // Q-mix strand (per-voice, 16 steps each)
    float qmixProb[16][16] = {};  // [voice][step]
    int   qmixLen[16]      = {16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16};
    int   qmixOff[16]      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    int   qmixRot[16]      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
};
```

**Add accessors:**
```cpp
// Q-mix strand accessors (per-voice)
float getQmixProb(int voice, int step) const;
void  setQmixProb(int voice, int step, float prob);
int   getQmixLen(int voice) const;
void  setQmixLen(int voice, int len);
int   getQmixOff(int voice) const;
void  setQmixOff(int voice, int off);
int   getQmixRot(int voice) const;
void  setQmixRot(int voice, int rot);
```

### 4. Update Strand References

**Files to check for STRAND_RHYTHM/ACCENT/VARIATION/LEGATO references:**
- [`src/dsp/managers/MonsoonSandsManager.cpp`](../src/dsp/managers/MonsoonSandsManager.cpp)
- [`src/dsp/managers/MonsoonExpanderManager.cpp`](../src/dsp/managers/MonsoonExpanderManager.cpp)
- All files with `switch(strand)` statements

**Pattern:** Anywhere that uses `STRAND_RHYTHM` (was 2, now 3), update to use the new value or use the name (not the number).

### 5. Wire Q-Mix into Processing Pipeline

**Key integration points:**

1. **Philox RNG:** Q-mix uses STREAM_SOURCE_SELECT (key = 3)
   - File: [`src/dsp/PhiloxRng.hpp`](../src/dsp/PhiloxRng.hpp)
   - Verify STREAM_SOURCE_SELECT is defined

2. **Strand reading:** Add q-mix to `readStrand()` in MonsoonSandsManager
   - Read q-mix LEN/OFF/ROT from editor
   - Push to engine arrays

3. **Quantizer mode:** Q-mix only active in quantizer mode
   - Check mode before using q-mix values
   - Dim in sequencer mode (UI concern, Phase 2)

### 6. Update SandsGrid Counts

**File:** [`src/ui/SandsGrid.hpp`](../src/ui/SandsGrid.hpp:64-66)

**Current:**
```cpp
static constexpr int   MONO_SLOTS  = MONO_LANES + 1;  // 7  (== post-q-mix MONO_LANES)
static constexpr int   EAST_SLOTS  = EAST_LANES + 1;  // 7
static constexpr int   POLY_SLOTS  = POLY_LANES + 1;  // 5  (q-mix is per-voice → a poly lane)
```

**Change to (activate the counts):**
```cpp
static constexpr int   MONO_LANES = 7;      // MEL, OCT, QMIX, REST, ACCENT, VARIATION, LEGATO
static constexpr int   POLY_LANES = 5;      // MEL, OCT, QMIX, REST, ACCENT
static constexpr int   EAST_LANES = 7;      // (adds VARIATION, LEGATO display)
```

**Remove the `_SLOTS` aliases** — they were interim. With q-mix active, the counts ARE the real lane counts.

## Testing Strategy

### Unit Tests

**Create:** `test/test_qmix_strand.cpp`
```cpp
// Test q-mix strand basics
- Q-mix LEN/OFF/ROT wrapping
- Per-voice independence
- Philox stream key = 3
- Quantizer mode gating
```

### Integration Tests

1. **Strand isolation:** Q-mix doesn't affect other strands
2. **Mode switching:** Q-mix dims in sequencer mode
3. **Per-voice:** Each voice has independent q-mix state
4. **Philox stream:** Q-mix draws from STREAM_SOURCE_SELECT

### Manual Verification

1. Build clean: `make clean && make -j$(nproc)`
2. Load in VCV Rack
3. Switch to quantizer mode
4. Verify q-mix strand exists in engine (debug print)
5. Verify slot 2 in visual editor (still empty until Phase 2)

## Commit Strategy

**Single atomic commit:**
```
feat(qmix): Add q-mix strand (STRAND_QMIX, arrays, params)

Implements q-mix as a full Sands lane with its own Philox RNG stream
(STREAM_SOURCE_SELECT = 3). Q-mix is per-voice probability of taking
generated vs input melody in quantizer mode.

Changes:
- LaneMapping.hpp: STRAND_QMIX = 2, renumber RHYTHM/ACCENT/VAR/LEG 3-6
- SequencerEngine.hpp: Add qmixProb/Len/Off/Rot arrays (16 voices)
- Monsoon.hpp: Add q-mix editor storage + accessors
- SandsGrid.hpp: Activate real lane counts (MONO 7, POLY 5, EAST 7)
- Managers: Wire q-mix into readStrand/processing pipeline

Q-mix uses STREAM_SOURCE_SELECT Philox stream (key=3), distinct from
rhythm/melody streams. Active in quantizer mode only.

Geometry already in place (feat/sands-qmix-geometry). UI/panel updates
in Phase 2.

Tested: Clean build, strand isolation, per-voice independence.
```

## Risk Assessment

**Medium risk:**
- Strand renumbering (RHYTHM 2→3, etc.) touches many files
- Must update all switch(strand) statements
- Philox stream key must be correct (3 = STREAM_SOURCE_SELECT)

**Mitigation:**
- Grep for all STRAND_ references before committing
- Test build after each file change
- Verify strand isolation (q-mix doesn't affect others)
- Use strand names, not numbers, in new code

## Next Steps (Phase 2)

After this commit:
1. Sands visual editor: Make slot 2 editable
2. Monsoon: Add Big6 q-mix knob
3. Straits: Add per-voice q-mix knobs
4. Causeway: Add q-mix CV inputs
5. Macro: Add q-mix mix-in tap
6. Panel regeneration: All three Sands visuals

## References

**Design docs (check code first!):**
- `docs/design/RANDOM_VS_INPUT_MODULE_CONCEPT.md` — Full q-mix spec
- `docs/design/CA_PANEL_THREE_STREAM_LAYOUT.md` — CA's q-mix stream
- `docs/design/CONTEXT_RECOVERY.md` — Q-mix design arc summary

**Code (source of truth):**
- [`src/ui/SandsGrid.hpp`](../src/ui/SandsGrid.hpp:67) — Geometry
- [`src/dsp/LaneMapping.hpp`](../src/dsp/LaneMapping.hpp:107-112) — Planned mappings
- [`src/dsp/PhiloxRng.hpp`](../src/dsp/PhiloxRng.hpp) — Stream keys
