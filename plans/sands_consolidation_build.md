# Sands consolidation — the RIGHT-SIZED build (not overblown)

Design authority: docs/design/SANDS_CONSOLIDATION.md (final decision = kill Mono, keep+extend Macro,
move sends right; East generalised to mono later; full one-module dissolve DEFERRED). This plan is the
concrete, low-risk build: mostly ADDITIVE + DELETION, no re-architecture.

## Prereq
Mode collapse (6->3) finished, tested in Rack (gate + phase), and MERGED to master first. Do not start
on top of the unmerged branch.

## The build (each its own commit, suite green + Rack check between)

### 1. Add the last two lanes (VARIATION, LEGATO) to the uniform lane set
- The grid is driven by `dotModular::SandsGrid::POLY_LANES` (currently 5: MEL/OCT/QMIX/REST/ACC). Add
  VARIATION + LEGATO so the full rhythm-family lane set is uniform (7 lanes). Update LaneMapping /
  EDITOR_TO_ENGINE_QMIX and the static_asserts accordingly.
- This is the lane-uniformity step everything else depends on.

### 2. Spread on VARIATION and LEGATO
- They currently have mono slewed buffers but NO poly buffers / no spread (SANDS_CONSOLIDATION.md
  "spread on VAR/LEG"). Add the poly buffers + per-voice slew/pre-remap path + spread for these two
  lanes, same as the other poly lanes. SpreadInterp N_SPREAD_LANES 5 -> 7, mapping tables, per-voice
  application. THIS is the only real engine work in the consolidation.

### 3. Macro panel — move SEND knobs to the RHS, 6 columns aligned per-lane
- Currently send columns are fixed X (COL_J1=8 J2=18 A1=30 A2=39 SPREAD_X=49 ED_X=58). Relocate the
  send-related knobs to the RIGHT of the panel, each lane's send group aligned HORIZONTALLY next to its
  related lane row -> **6 columns of send-related knobs**, one per lane group, row-aligned to the lane.
- This frees centre space for the 2 new lanes and the artwork (below). Panel is generated
  (gen_macro_mono.py) — edit the generator, re-run, panel_diff. Preserve pre/post tap semantics
  (OWNERSHIP_SPEC §9).

### 4. Helix artwork room — split helix left / sands right
- Make room for the helix art: **helix on the LEFT, the sands editor/lanes on the RIGHT** (or the split
  that reads best once sends have moved RHS — confirm visually). The send-knob relocation + lane
  layout should leave a clean region for the helix.

### 5. Deprecate MONO — goodbye
- Macro already works in mono, so it SUBSUMES Mono (SANDS_CONSOLIDATION.md). Remove the Mono module:
  unregister from plugin (Monsoon.cpp addModel), move src for Mono visual to deprecated/, drop its
  slug from plugin.json, remove its generator path. Pre-release: no patch migration. Verify nothing
  else references the Mono module.

### 6. Deprecate "Per-voice articulation (East VARIATION/LEGATO)" context-menu item
- MonsoonWidget.cpp:1069 `add("Per-voice articulation (East VARIATION/LEGATO)", &m->engine.
  perVoiceArticulation)`. This was the binary "soup" flag; the correlation matrix + spread-on-VAR/LEG
  (step 2) supersede it (graded per-voice variation correlated to mono replaces the on/off). REMOVE the
  menu item. Decide on `perVoiceArticulation` the ENGINE flag: if VAR/LEG now go through spread/
  correlation always, remove the flag and its branches (Lantern.cpp:303/437 reference it — update those
  too); if a transitional default-on is simpler, leave the flag true and dead for now and note it. Pick
  the clean removal if step 2 makes it fully redundant.

## Order & risk
2 (spread on VAR/LEG) is the only real engine work — do it carefully with tests. 1,3,4 are
additive/panel. 5,6 are deletion. Audit question still open: does killing Mono lose lane-owner /
same-playhead (SANDS_CONSOLIDATION.md)? Confirm in step 5's recon before deleting.
