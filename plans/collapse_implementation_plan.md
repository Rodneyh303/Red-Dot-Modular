# Implementation plan — [15]+mono → [16][16] collapse

## Order (incremental commits, bit-compare after each)

### Commit 1: Collapse slewed* arrays + fix publish functions (THE ROOT FIX)
- PatternEngine.hpp: `slewedRhythm[16]` + `slewedPolyRhythm[15][16]` → `slewedRhythm[16][16]`
- Same for: Melody, Octave, Qmix, Accent, Variation, Legato (7 pairs)
- Fix publishSlewedRhythm/Melody/Qmix: `pubSlewedPoly*[v] = slewed*[v]` (direct, no alias/shift)
- Fix all write sites in recomputeEffective* (blend lambda writes `slewedRhythm[0][i]` for V1, `slewedRhythm[v+1][i]` for poly)
- Fix for-voice methods (same pattern)
- Fix SpreadInterp monoSlewed: `slewedMelody[step]` → `slewedMelody[0][step]`
- Fix SpreadInterp polySlewed: `slewedPolyMelody[voice]` → `slewedMelody[voice+1]`
- Fix Sands manager snapshot/remap loops
- Fix Draw structs: `RhythmDraw.rhythm[16]` + `polyRhythm[15][16]` → `rhythm[16][16]`
- Fix CachedDraw structs: `zRhythm[16]` + `zPolyRhythm[15][16]` → `zRhythm[16][16]`
- Fix rawDraw*PatternAt: loops `< 15` → `< 16`
- Fix patternRhythmAt/MelodyAt/QmixAt: `applyZ(zw, r)` for mono, `zPolyRhythm[v]` for poly → unified

### Commit 2: Collapse preRemapSlewed* arrays
- Same pattern as commit 1

### Commit 3: Collapse Source arrays
- Same pattern

### Commit 4: Collapse cachedPolySpread[15][7] → [16][7]

### Commit 5: Collapse SequencerEngine per-voice state
- laneTickV_/laneSignV_/laneDirV_/lanePingPongHoldV_ [15] → [16]
- wasHeldPolyPrev[15] → [16]
- varlegLocalEast_[15][2] → [16][2]
- Fix all < 15 loops → < 16

### Commit 6: Collapse ZWindowCache MAX_FIELDS
- stride changes from `4 + 4*15` to `4*16`

### Commit 7: Fix remaining < 15 loops in managers/visuals
- MonsoonSandsManager, MonsoonExpanderManager, StraitsEastSandsVisual, etc.

### Commit 8: Delete EL2ENG (dead identity)

## Key convention
- `slewedRhythm[voiceSlot][step]` where voiceSlot = 0 (V1), 1 (V2), ..., 15 (V16)
- `pubSlewedPolyRhythm[voiceSlot][step]` — same, NO alias at [0]
- `pubSlewedRhythm[step]` — DELETE (replaced by pubSlewedPolyRhythm[0][step])
- All loops: `for (v = 0; v < 16; ++v)` (was `for (v = 0; v < 15; ++v)` for poly + separate mono line)

## What stays [15] (Category 2 — param/panel)
- POLY_*_PARAM_1..15 enums
- Straits binding (param_*_1..15)
- gen_straits.py
- nvIdxForVoice bank >= 15 check (poly BANK index, 15 poly banks)
