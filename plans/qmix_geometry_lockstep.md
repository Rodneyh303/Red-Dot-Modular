# Q-Mix Geometry C++ Lockstep Verification Plan

**Branch:** `feat/sands-qmix-geometry` (tip: f5260be)
**Working branch:** `master` (e500fa2)
**Related branch:** `feat/qmix-parity-checklist` (tip: 7a72821)
**Date:** 2026-09-17

## Executive Summary

The `feat/sands-qmix-geometry` branch has **already completed** the geometry work to add space for the q-mix row on the Sands expanders. This plan verifies that the C++ UI code is in lockstep with the Option B geometry defined in [`src/ui/SandsGrid.hpp`](../src/ui/SandsGrid.hpp:67) and the panel generators.

**Status:** Geometry work complete. This is a **verification and documentation** pass to confirm:
1. All three visual widgets use `QMIX_LANE_H` (13mm) correctly
2. Macro box shrink is applied consistently
3. Build and render verification passes

## Critical Project Rule: Check the Code, Not the Docs

**⚠️ IMPORTANT:** Several docs are stale relative to master after the Mode F and quantizer-unification commits. When verifying geometry or any implementation details:

1. **Source of truth:** [`src/ui/SandsGrid.hpp`](../src/ui/SandsGrid.hpp:67) for geometry, not any doc
2. **Always check the code first** — docs may lag behind implementation
3. **Trust the implementation** over documentation when they conflict
4. **Update docs** when you find staleness, but never assume docs are current

This is the project's own rule and applies to all verification work.

## Context

**Q-Mix Design Arc:**
- Q-mix is a full Sands lane with its own Philox RNG stream (STREAM_SOURCE_SELECT)
- Downstream: CA's eight poly in/out pairs → Intertropical routing → Keppel (specced on doc branches)
- `SequencerEngine::executeModeC/D` is dead code pending test-reference check

**Geometry Changes (Option B):**
- Lane height: 14mm → 13mm (`QMIX_LANE_H`)
- Slot 2 reserved for q-mix (appears as empty band until strand lands)
- Mono: 6→7 slots (bottom 98→105mm)
- East: 6→7 slots (bottom 98→105mm)
- Macro: 4→5 slots (bottom 70→79mm)

## Related Branch: feat/qmix-parity-checklist

The `feat/qmix-parity-checklist` branch (tip: 7a72821) contains a parity checklist for q-mix implementation. **However, per the project rule above, treat [`src/ui/SandsGrid.hpp`](../src/ui/SandsGrid.hpp:67) as the geometry source of truth**, not any checklist or doc.

**Action before merge:** Diff the two branches to identify any conflicts:
```bash
cd Red-Dot-Modular
git diff feat/sands-qmix-geometry feat/qmix-parity-checklist -- src/ui/SandsGrid.hpp
git diff feat/sands-qmix-geometry feat/qmix-parity-checklist -- panel_src/
git diff feat/sands-qmix-geometry feat/qmix-parity-checklist -- src/dsp/LaneMapping.hpp
```

If conflicts exist, resolve by:
1. Using `SandsGrid.hpp` from the branch with correct geometry (verify against code, not docs)
2. Cherry-picking non-conflicting improvements from the other branch
3. Testing build after resolution

## Verification Checklist

### Build Verification

```bash
cd Red-Dot-Modular
make clean
make -j$(nproc)
```

**Expected:** Clean build with no errors. Flag any environment-related issues (new toolchain).

## Geometry Implementation Status

### Single Source of Truth

[`src/ui/SandsGrid.hpp`](../src/ui/SandsGrid.hpp:67) defines Option B geometry:
- `QMIX_LANE_H = 13.f` (line 67) — the new lane height
- `MONO_SLOTS = 7`, `EAST_SLOTS = 7`, `POLY_SLOTS = 5` (lines 64-66)
- `slotCentre(slot)` (line 68) — uses QMIX_LANE_H
- `monoEditorHeight() = 91` (line 69) — 7 × 13
- `polyEditorHeight() = 65` (line 70) — 5 × 13

### Files to Verify

#### 1. [`src/MonsoonSandsVisualExpander.hpp`](../src/MonsoonSandsVisualExpander.hpp:43) (Mono Visual)

**Check:** Should use `QMIX_LANE_H` (13mm) geometry

**Expected implementation:**
```cpp
static inline float rowY(int lane) {
    return dotModular::SandsGrid::slotCentre(dotModular::LaneMapping::laneSlot(lane));
}
```

Where:
- `laneSlot(lane)` maps existing lane k → editor slot (skips slot 2 = q-mix)
- `slotCentre(slot)` uses `QMIX_LANE_H` (13mm) as single source of truth

**Verify:** All Mono visual widget placements (jacks, attens, owner cells, direction cells, prob outs) align with 13mm lanes

#### 2. [`src/StraitsEastSandsVisual.hpp`](../src/StraitsEastSandsVisual.hpp:54) (East Visual)

**Check:** Should use `QMIX_LANE_H` (13mm) geometry

**Expected implementation:**
```cpp
static constexpr float ED_LANE_H = dotModular::SandsGrid::QMIX_LANE_H;  // 13

static inline float rowY(int r) {
    return dotModular::SandsGrid::slotCentre(dotModular::LaneMapping::laneSlot(r));
}
```

**Verify:** All East visual widget placements (4 poly lanes + 2 VAR/LEG display lanes) align with 13mm lanes

#### 3. [`src/StraitsSandsMacroVisual.hpp`](../src/StraitsSandsMacroVisual.hpp:52) (Macro Visual)

**Check:** Should use `QMIX_LANE_H` (13mm) geometry

**Expected implementation:**
```cpp
static constexpr float ED_LANE_H = dotModular::SandsGrid::QMIX_LANE_H;  // 13

static inline float rowY(int r) {
    return dotModular::SandsGrid::slotCentre(dotModular::LaneMapping::laneSlot(r));
}
```

**Verify:** All Macro visual widget placements (4 poly lanes) align with 13mm lanes

### Index-2 Component/ID Renumbering

**Context:** The panel generators use `ESLOT=[0,1,3,4,5,6]` to skip slot 2 (q-mix).

**Files to verify:**
- [`src/MonsoonSandsVisualExpander.cpp`](../src/MonsoonSandsVisualExpander.cpp) — widget construction
- [`src/StraitsEastSandsVisual.cpp`](../src/StraitsEastSandsVisual.cpp) — widget construction
- [`src/StraitsSandsMacroVisual.cpp`](../src/StraitsSandsMacroVisual.cpp) — widget construction

**Check:** Component binding should use `laneSlot(k)` where generators use `ESLOT[k]`

### Engine Lane Enum

**Status:** [`src/dsp/LaneMapping.hpp`](../src/dsp/LaneMapping.hpp:32-44) currently has 6 strands. The q-mix strand (STRAND_QMIX = 2) is **staged but not active** (lines 107-112). The enum renumbering happens atomically with the q-mix strand data (arrays, params). For geometry preview, NUM_STRANDS stays 6.

## Macro Box Shrink Verification

### Context

The Macro blend box (mix-in send controls) needs to shrink and move down into the space reclaimed by the shorter lanes.

### Files Requiring Updates

#### 1. [`src/StraitsSandsMacroVisual.cpp`](../src/StraitsSandsMacroVisual.cpp:650) — `draw()` method

**Check:** BLEND_TOP should be 85mm (moved down from 76mm)

**Expected (line 650):**
```cpp
const float BLEND_TOP=85.f, SEND_Y0=10.f, SEND_DY=9.5f, SEND_DX=6.f, BGAP=2.5f;
```

**Verify:** "MIX IN" label baseline at ~81.5mm (BLEND_TOP - 3.5f)

#### 2. [`panel_src/gen_macro_mono.py`](../panel_src/gen_macro_mono.py:73) — Generator constants

**Check:** BLEND_TOP=85.0, BLEND_H=35.0

**Expected (line 73):**
```python
BLEND_TOP=85.0; BLEND_H=35.0; BGAP=2.5; GROUP_W=ED_W/5.0
```

**Impact:** If changed, panel SVG regeneration required

## Build and Render Verification

### Render Verification Checklist

**Environment note:** New toolchain — flag anything that looks environment-related vs code-related.

#### Mono Visual (MonsoonSandsVisualExpander)
- [ ] All 6 lane rows vertically aligned with editor lanes
- [ ] CV jacks (LEN/OFF/ROT) centered on lane rows
- [ ] Attenuverters centered on lane rows
- [ ] Spread controls (REST/MEL/OCT/ACC) aligned with correct lanes
- [ ] Owner cells (4 poly lanes) aligned with lanes
- [ ] Direction cells (6 lanes) aligned with lanes
- [ ] Prob out jacks (6 lanes) aligned with lanes
- [ ] Editor recess height = 91mm (7 × 13)
- [ ] Slot 2 (q-mix) appears as empty band in editor

#### East Visual (StraitsEastSandsVisual)
- [ ] All 6 editor lanes (4 poly + VAR/LEG) vertically aligned
- [ ] CV jacks (4 lanes × 4 cols) centered on lane rows
- [ ] Attenuverters (4 lanes × 4 cols) centered on lane rows
- [ ] VAR/LEG CV jacks (2 lanes × 3 cols) aligned with lanes 4/5
- [ ] Owner cells (4 poly lanes) aligned with lanes
- [ ] Direction cells (6 lanes) aligned with lanes
- [ ] Delegation cells (6 lanes) aligned with lanes
- [ ] Prob out jacks (4 lanes) aligned with lanes
- [ ] Editor recess height = 91mm (7 × 13, same as Mono)
- [ ] Slot 2 (q-mix) appears as empty band in editor

#### Macro Visual (StraitsSandsMacroVisual)
- [ ] All 4 lane rows vertically aligned with editor lanes
- [ ] CV jacks (4 lanes × 4 cols) centered on lane rows
- [ ] Attenuverters (4 lanes × 4 cols) centered on lane rows
- [ ] Spread base trimpots (4 lanes) aligned with lanes
- [ ] Owner cells (4 lanes) aligned with lanes
- [ ] Direction cells (4 lanes) aligned with lanes
- [ ] Prob out jacks (4 lanes) aligned with lanes
- [ ] Editor recess height = 65mm (5 × 13)
- [ ] Slot 2 (q-mix) appears as empty band in editor
- [ ] **Blend box top at ~85mm** (moved down from 76mm)
- [ ] **Blend box height ~35mm** (shrunk from 38mm)
- [ ] **"MIX IN" label baseline at ~81.5-83.5mm**
- [ ] Send knobs (4 lanes × 4 items) fit within new box
- [ ] Tap knobs (2 per lane) fit within new box

### Side-by-Side Alignment Test

**Critical:** With Mono, East, and Macro side-by-side:
- [ ] Lane 0 (MELODY) tops align at 14mm across all three
- [ ] Lane 1 (OCTAVE) tops align at 27mm (14 + 13)
- [ ] Slot 2 (q-mix empty band) aligns at 40mm
- [ ] Lane 2 (REST) tops align at 53mm (14 + 3×13)
- [ ] Lane 3 (ACCENT) tops align at 66mm (14 + 4×13)
- [ ] Mono lanes 4/5 (VAR/LEG) continue at 79mm, 92mm
- [ ] East lanes 4/5 (VAR/LEG display) continue at 79mm, 92mm
- [ ] Macro editor bottom at 79mm (14 + 5×13)
- [ ] Mono/East editor bottom at 105mm (14 + 7×13)

### Panel Regeneration

If generators were modified:
```bash
cd Red-Dot-Modular/panel_src
python gen_macro_mono.py  # Regenerates Macro panel SVG
python gen_east_clean.py  # Regenerates East panel SVG (if modified)
# Mono uses gen_macro_mono.py for its panel too
```

## Next Steps

If verification passes, the branch is ready to merge. If issues are found, document them and create a follow-up plan.

**Downstream work:**
- Q-mix strand implementation (STRAND_QMIX, arrays, params)
- CA eight poly in/out pairs
- Intertropical routing
- Keppel integration
- Remove dead code: `SequencerEngine::executeModeC/D` (pending test-reference check)

## Summary

The `feat/sands-qmix-geometry` branch has completed the geometry work. This verification plan confirms:

1. **Geometry implementation:** All three visuals use `QMIX_LANE_H` (13mm) correctly
2. **Macro box shrink:** BLEND_TOP moved to 85mm, BLEND_H shrunk to 35mm
3. **Slot 2 reservation:** Q-mix band appears empty until strand lands
4. **Build/render:** Clean build, all widgets align with 13mm lanes

**Key principle:** `SandsGrid.hpp` is the single source of truth. Generators and C++ follow from it.

**Status:** Geometry preview-only. The q-mix strand (data, arrays, params) lands atomically in a future commit.
