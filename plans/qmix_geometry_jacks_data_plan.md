# Q-Mix Geometry / Jacks / Data — Four-Commit Plan

**Branch:** `feat/sands-qmix-geometry` (state committed by Rodney; q-mix lane in, Rack runs).
**Rule:** Check the CODE, not the docs. The two plan docs in `plans/` (qmix_geometry_lockstep.md,
qmix_phase2_ui_checklist.md) are STALE — they describe a "q-mix as empty preview gap / QMIX_LANE_H
duplicate" design that the code has already moved past. This plan supersedes them.

**Confirmed design decision (Rodney):** Q-mix is a PLAIN lane like every other — 13 mm for ALL lanes,
no q-mix-specific constants, no gap. Slot 2 gets full jacks + knobs identical to the other lanes.

**Commit discipline:** FOUR separate commits, one per task. Do NOT squash. Build + render-verify
after each. Flag anything that looks toolchain/environment-related (new machine) vs code-related.

---

## Pre-flight verification (do FIRST, before any edits) — implementer runs git

Architect mode cannot run git; these are for the implementer.

1. **Confirm branch tip:**
   ```bash
   cd Red-Dot-Modular && git branch --show-current && git log --oneline -3
   ```
   Expect `feat/sands-qmix-geometry`.

2. **RECONCILE RESULT (done — no conflict):** Diffed `feat/sands-qmix-geometry` (c614b24) against
   `origin/feat/qmix-parity-checklist` (merge-base e500fa2). The parity branch is **BEHIND on q-mix,
   not a competing variant**: it has `NUM_STRANDS=6`, NO `STRAND_QMIX`, NO `_QMIX` mapping tables, and
   `SandsGrid` MONO/EAST=6 / POLY=4 / no QMIX_LANE_H. i.e. it is essentially the PRE-q-mix state.
   → **This branch is authoritative for geometry + data. Do NOT merge/cherry-pick the parity branch's
     code** (it would regress q-mix). Its only value is its checklist/spec DOCS. No third-variant risk.
   Original reconcile command (kept for reference):
   ```bash
   git show 7a72821:src/ui/SandsGrid.hpp
   git diff feat/sands-qmix-geometry feat/qmix-parity-checklist -- \
       src/ui/SandsGrid.hpp panel_src/ src/dsp/LaneMapping.hpp
   ```
   - If `7a72821`'s SandsGrid already matches the Option-1 target below (LANE_H=13, no QMIX_LANE_H
     duplicate, no ESLOT gap), cherry-pick / take it as the base instead of hand-editing.
   - If it's a DIFFERENT variant, this plan's Task 1 is authoritative (Rodney's decision); note the
     divergence in the commit message and do NOT merge the parity geometry commit as-is.

3. **Pull the two cited unmerged docs** for reference (per user — they live on the parity branch):
   ```bash
   git show feat/qmix-parity-checklist:<lane-parity-checklist-path>
   git show feat/qmix-parity-checklist:<per-voice-blend-mux-spec-path>
   ```
   Use them to sanity-check Task 4's data wiring (per-voice blend mux), but code is source of truth.

4. **Confirm SandsGrid is still the single source of truth** (already verified in audit):
   - `src/ui/SandsGrid.hpp` owns lane geometry; the three widget `.hpp`s derive `ED_LANE_H`/`ED_H`
     from it; the generators mirror it. Task 1 restores that lockstep (currently broken — see below).

---

## ARCHITECTURAL RULE (Rodney) — LaneMapping.hpp is the ONE reconciliation point

`src/dsp/LaneMapping.hpp` is the single source of truth for ALL editor↔engine↔param index
conversions (the MVC model). **Do NOT add new index-bridging code** anywhere (generators, widgets,
managers). Where an editor lane must become an engine lane (or vice-versa), route through the
LaneMapping tables (`ENGINE_LANE_TO_EDITOR_QMIX`, `EDITOR_TO_ENGINE_LANE_QMIX`, `MONO_LANE_TO_STRAND`,
`SPREAD_LANE_TO_EDITOR`). Any existing index difference is a development-history artifact to be
COLLAPSED onto LaneMapping, not a thing to bridge with fresh ad-hoc math.

**Direct consequence for Task 3 (the ID mismatch):** the C++ East binds `input_{cvId(r,c)}` with
`r = EDITOR lane 0..4` (loop `r < POLY_LANES`), i.e. component IDs are EDITOR-lane-indexed. The
generator currently emits `input_{lane*4+p}` with `lane = DISPLAY_ORDER[row]` = ENGINE lane. That
divergence (editor-id vs engine-id) is the "shape not found" root cause. FIX = make the generator
emit IDs on the SAME convention the C++ uses (editor-lane index), deriving any display-row→engine
ordering it needs for VISUAL placement from the LaneMapping tables — NOT by hand-rolling a second
mapping. Same rule for Macro/Mono. After Task 3, generators and C++ must agree purely through
LaneMapping; grep for any stray `DISPLAY_ORDER`/hand-coded permutation and replace with the table.

## Current-state findings (from code audit — the "why")

**Geometry is half-migrated and self-inconsistent:**
- `SandsGrid.hpp`: `LANE_H = 14`; `monoHeight()/polyHeight()/monoBottom()/polyBottom()` all compute
  from 14. A SEPARATE, UNUSED `QMIX_LANE_H = 13` + `slotCentre()/monoEditorHeight()/polyEditorHeight()`
  exist — **no widget references them** (grep-confirmed).
- Widgets bind `ED_LANE_H = SandsGrid::LANE_H` (=14) and `rowY(r)=ED_Y+(r+0.5)*ED_LANE_H` → render
  **14 mm lanes, full 5/7 lanes, NO gap** (q-mix already a real data lane; laneCount=5/7 in
  `SandsVisualEditorV4`).
- Generators hardcode `LANE_H=13`, `ED_H=91/65`, and `ESLOT=[0,1,3,4]`/`[0,1,3,4,5,6]` — **13 mm art
  WITH an empty slot-2 gap**.
- Net: C++ = 14 mm / no gap; SVG art = 13 mm / gap. Mismatched on BOTH axes → the grid overruns the
  MBS art bottom-right AND q-mix renders empty. `laneSlot()` in LaneMapping.hpp is DEPRECATED/unused.

**Engine/data ordering (verified, for Task 4):**
- `LaneMapping.hpp`: strand enum `MELODY0 OCTAVE1 QMIX2 RHYTHM3 ACCENT4 VARIATION5 LEGATO6`,
  `NUM_STRANDS=7`. `MONO_LANE_TO_STRAND` is identity (editor lane == strand). Poly tables
  `ENGINE_LANE_TO_EDITOR_QMIX[5]={3,0,1,4,2}`, `EDITOR_TO_ENGINE_LANE_QMIX[7]={1,2,4,0,3,-1,-1}`,
  `PL_QMIX=4`.
- `PatternEngine`: `random_[16][NUM_STRANDS][16]` has a `qmixRandom` view; `polyRandom()` maps via
  `ENGINE_LANE_TO_EDITOR_QMIX`. Storage exists.
- **Gap:** `remapSlewedByPins`/`pickMono` return `0.5f` placeholders for `STRAND_QMIX` (documented
  TODO), and there are no `slewedPolyQmix`/`slewedQmix` buffers, so the q-mix lane has no live draw →
  renders empty. This is Task 4's core.

---

## Task 1 — Geometry: LANE_H 14→13, reclaim below, one constant, no gap  (COMMIT 1)

**Goal:** 13 mm for every lane everywhere; q-mix is a normal lane at index 2; grid clears the MBS art;
C++ and generators back in lockstep from the single SandsGrid source.

### 1a. `src/ui/SandsGrid.hpp` — collapse to ONE height constant
- Set `LANE_H = 13.f` (was 14).
- Keep `monoHeight()/polyHeight()/monoBottom()/polyBottom()/laneCentre()` — they now yield 13-based
  values automatically (mono 7×13=91 → bottom 105; poly 5×13=65 → bottom 79).
- DELETE the duplicate q-mix block: `QMIX_LANE_H`, `slotCentre()`, `monoEditorHeight()`,
  `polyEditorHeight()` (unused; their values now equal the LANE_H-based helpers).
- Update the stale header comments (the 14 mm arithmetic in lines 12–16, 46–49) to 13 mm.

### 1b. `src/dsp/LaneMapping.hpp` — remove the deprecated gap helper
- DELETE `laneSlot()` (deprecated, unused) and the `QMIX_EDITOR_LANE`-gap prose that frames slot 2 as
  a preview gap. Keep `QMIX_EDITOR_LANE=2` if still referenced by round-trip asserts (grep first);
  otherwise remove. Keep the `_QMIX` mapping tables (they are the live source of truth).

### 1c. Three widget `.hpp`s — confirm they follow SandsGrid (mostly no-op after 1a)
- `MonsoonSandsVisualExpander.hpp`: `ROW_BOT = monoBottom()` (now 105), `N_LANES=MONO_LANES=7`,
  `rowY(lane)=ROW_TOP+(lane+0.5)*(ROW_BOT-ROW_TOP)/N_LANES` → 13 mm. Verify no literal 14/6 remain.
- `StraitsEastSandsVisual.hpp`: `ED_H = monoHeight()` (now 91), `ED_LANE_H = SandsGrid::LANE_H` (now
  13), `N_EDITOR_LANES=EAST_LANES=7`. `rowY(r)=ED_Y+(r+0.5)*ED_LANE_H`. Fix stale comments (says 84/6).
- `StraitsSandsMacroVisual.hpp`: `ED_H = polyHeight()` (now 65), `ED_LANE_H = LANE_H` (13). Same rowY.
- These are the "rowY in the three visual widget .hpps" the user named — they auto-track once
  SandsGrid is 13 and counts are 7/7/5 (counts already correct).

### 1d. Generators — drop the gap, mirror SandsGrid, reclaim below
- `panel_src/gen_east_clean.py`:
  - `LANE_H` stays 13, `ED_LANES=7`, `ED_H=91`. **Remove `ESLOT`** and change `ctrlY(k)=rowY(k)`
    (no skip). The lane divider loop already runs `range(1,ED_LANES)` — fine.
  - The control-graphics + component loops must place a full complement on ALL 5 spread lanes
    (editor rows 0..4 = MEL/OCT/QMIX/REST/ACC) — this is where Task 3 adds the QMIX jacks; Task 1
    only moves them to 13 mm rows. (Keep VAR/LEG rows 5/6 at LEN/OFF/ROT only.)
  - MBS watermark + waves + footer: verify they sit BELOW `ED_Y+ED_H` (=105) with clearance; nudge
    `mbs()`/`waves()` y if the taller 7×13 editor now overlaps (this is the "reclaim below / clears
    the MBS" requirement).
- `panel_src/gen_macro_mono.py` (`gen_macro` + `gen_mono`):
  - `gen_macro`: `LANE_H=13`, `ED_LANES=5`, `ED_H=65`. Remove `ESLOT=[0,1,3,4]`; `ctrlY(k)=rowY(k)`.
  - `gen_mono`: `LANE_H=13`, `N=7`, `ROW_BOT=105`. Remove `ESLOT=[0,1,3,4,5,6]`; `ctrlY(k)=laneY(k)`.
  - Verify Helix / logo / MBS art placement clears the taller editor.
- `panel_src/dotmod_design.py`: check its `mbs()` / any shared lane constants match 13 mm (user named
  it explicitly). Grep for `14`/`LANE_H` there.

### 1e. Render-verify (Task 1 exit)
- `make -j` (implementer). Regenerate SVGs: `python panel_src/gen_east_clean.py`,
  `python panel_src/gen_macro_mono.py` (writes East, Macro, Mono panels).
- In Rack: all three lane grids at 13 mm, lane-0 tops align at 14 across the three, grid bottom clears
  the MBS/Helix art bottom-right. Editor recess = 91 (Mono/East), 65 (Macro). Q-mix band is a normal
  lane (may still render empty of DATA until Task 4 — that's expected).
- **Commit 1:** "Sands geometry: LANE_H 14→13, single source, drop q-mix gap".

---

## Task 2 — Macro box shrink  (COMMIT 2)

**Goal:** BLEND_TOP 82→85, BLEND_H 38→35; matching change in the Macro `draw()`; "MIX IN" label
baseline down ~3 mm to ~83.5.

**Note (flag):** the Macro `draw()` in `StraitsSandsMacroVisual.cpp` is ALREADY out of lockstep with
the generator — cpp currently has `BLEND_TOP=76, SEND_DY=9.5, GROUP_W=ED_W/4`; generator has
`BLEND_TOP=82, SEND_DY=9.0, GROUP_W=ED_W/5`. Part of this task is re-syncing them, not just the delta.

### 2a. `panel_src/gen_macro_mono.py` (`gen_macro`, ~line 73)
- `BLEND_TOP = 85.0` (was 82), `BLEND_H = 35.0` (was 38). Keep `GROUP_W=ED_W/5.0` (5 lanes),
  `SEND_Y0/SEND_DY/TAP_ROW_DY` — verify the 3 rows (2 send + 1 tap) still fit inside 35 mm:
  `SEND_Y0(10) + 2*SEND_DY(9) = 28` + trim radius < 35 ✓.

### 2b. `src/StraitsSandsMacroVisual.cpp::draw()` (the `BLEND_TOP=76…` line, ~651)
- Set the SAME constants the generator uses so labels sit on the drawn boxes:
  `BLEND_TOP=85.f, BLEND_H`-derived spacing, `GROUP_W=ED_W/5.f` (was /4 — the current /4 is a bug
  vs the 5-group art), `SEND_Y0=10.f, SEND_DY=9.f, SEND_DX=6.f, TAP_ROW_DY=9.f`.
- "MIX IN" label baseline → ~83.5 (currently `BLEND_TOP-3.5`; with BLEND_TOP=85 that's 81.5 — move to
  `BLEND_TOP-1.5` = 83.5, matching the user's "~83.5" and "down ~3mm" from the old 81.5-ish).
- The per-group `laneName[]` already includes QMIX (5 entries) and the loop runs `POLY_LANES` — good.

### 2c. Render-verify + **Commit 2:** "Macro mix-in box shrink (BLEND_TOP 85, BLEND_H 35)".

---

## Task 3 — Jacks: add + align QMIX, complete East ACCENT, index-2 renumber  (COMMIT 3)

**Goal:** QMIX row gets its 4 CV input jacks (+ attens + spread) in all three panels; East ACCENT
column gets its missing knobs; every lane's components exist and align to identical column geometry.

### Root causes (from audit)
- **East generator:** graphics DO draw a QMIX row (`gen_east_clean.py` ~212 `yq=rowY(2)`), but the
  **component/ID layer** (`range(4)` + `DISPLAY_ORDER` 4-lane) never emits QMIX marker shapes
  (`input_16..19`, QMIX `param_*`, `output_prob_4`, `param_dir_6`, `input_dir_mod_6`,
  `input_deleg_mod_6`) → the C++ `bindInput("input_22".."25")` etc. warn "shape not found" and the
  knobs are absent. ACCENT-with-labels-no-knobs is the same class (component markers not emitted for
  the full 5-lane set in the right order).
- **Macro generator:** component loop is `range(4)` (`gen_macro_mono.py` ~107) and the dir/prob/
  dir_mod loops are all `range(4)` (~131–141) → no QMIX component IDs; DISPLAY_ORDER_5 has a `-1`
  placeholder for q-mix ("markers TODO").
- **Index-2 renumber:** with q-mix a real lane at editor slot 2, every ENGINE-lane component index
  the generators emit via `DISPLAY_ORDER`/`lane*4+p` must use the 5-lane `_QMIX` mapping so the SVG
  ids match the C++ `cvId/attenId/PROB_OUT/dirModId` for all 5 poly lanes. Confirm nothing downstream
  still assumes the old 4-lane indices (grep `DISPLAY_ORDER`, `*4+`, `PROB_OUT`).

### 3a. East generator (`gen_east_clean.py`)
- Component layer: iterate the **5 spread lanes** in editor order (0..4) mapping each to its engine
  lane via `EDITOR_TO_ENGINE_LANE_QMIX`, emitting `input_{cvId}`, `param_{attenId}`, `param_{spread}`
  at `rowY(editorLane)`. Emit QMIX's `output_prob_2`-row, `param_dir_2`, `input_dir_mod_2`,
  `input_deleg_mod_2` — i.e. drop the `range(4)` caps to `range(EAST_LANES)`/`range(POLY_LANES)` as
  appropriate. VAR/LEG (rows 5/6) already handled — keep, but shift to their new 13 mm rows.
- Verify the emitted ids exactly match the C++ binds in `StraitsEastSandsVisual.cpp`
  (`cvId(r,c)=r*4+c`, spread `SPREAD_R/M/O/A/Q`, `dirModId/delegModId=DIR/DELEG_MOD_START+lane`,
  `output_prob_<editorLane>` with the QMIX table). Fix the STALE comment block (~242) that still says
  `cvId=r*2+c inputs 0..11`.

### 3b. Macro generator (`gen_macro_mono.py`, `gen_macro`)
- Component loop → `range(POLY_LANES)`; resolve engine lane via `EDITOR_TO_ENGINE_LANE_QMIX` (replace
  the `DISPLAY_ORDER_5` `-1` placeholder). Emit QMIX `input_/param_/param_spread`, plus
  `param_dir_2`, `input_dir_mod_2`, `output_prob_2`-row (loops ~131–141 → `range(POLY_LANES)`).
- The send-group markers already loop 5 groups for graphics; ensure the `param_send_/taplor_/tapspr_`
  markers exist for the q-mix group too (currently keyed off `DISPLAY_ORDER` 4-lane).

### 3c. Mono generator (`gen_macro_mono.py`, `gen_mono`)
- Already 7-lane `N=7` but with `ESLOT` gap (removed in Task 1). Ensure QMIX row (editor slot 2) emits
  its CV jacks + attens + spread (QMIX is a spread lane) and `prob_out_2`, `dir/deleg` markers.
  `SPR_TO_EDITOR`/`N_SPREAD` must include QMIX (5 spread lanes now, was 4).

### 3d. C++ side — confirm binds cover all 5/7 lanes (mostly already done in crash-fix pass)
- Grep the three `.cpp` constructors for any residual `< 4`/`< 6` component-bind loops; ensure they're
  `POLY_LANES`/`EAST_LANES`/`MONO_LANES`. (The earlier crash-fix widened data arrays and NUM_INPUTS;
  this task ensures the SVG has matching shapes so no "shape not found" warnings remain.)

### 3e. Render-verify + **Commit 3:** "Sands jacks: add/align QMIX components, complete East ACCENT".
- Exit criteria: NO `[SvgKit] shape not found` warnings in the Rack log for any of the three modules;
  every lane (incl QMIX) shows its full jack+knob complement, columns aligned across lanes.

---

## Task 4 — QMIX data: LOR + spread wired to the engine  (COMMIT 4)

**Goal:** the q-mix lane carries live data (draws, LOR window, spread) like the others in all three
panels; confirm engine enum matches LaneMapping ordering.

### 4a. Confirm ordering (no-op verify)
- `SequencerEngine::PolyLane { PL_REST0, PL_MELODY1, PL_OCTAVE2, PL_ACCENT3, PL_QMIX4 }` and
  `dotModular::STRAND_QMIX=2` with `MONO_LANE_TO_STRAND`/`ENGINE_LANE_TO_EDITOR_QMIX` round-trip
  asserts already present — verify they compile/pass. Nothing to change if asserts hold.

### 4b. Engine draw buffers for q-mix (the empty-lane fix)
- `PatternEngine`: add the q-mix slewed buffers paralleling the others — `slewedQmix[16]` (mono) and
  `slewedPolyQmix[15][16]` (poly). Q-mix draws from its OWN Philox stream
  (`QMIX_STREAM_KEY=3`/`STREAM_SOURCE_SELECT`), per LaneMapping's design note — add a `qmixPhilox`
  (or reuse the source-select stream) and populate these buffers in the same place rhythm/melody
  slewed buffers are filled (`redrawRhythm/Melody` / `patternRhythmAt` analogues).
- `remapSlewedByPins`/`pickMono`: replace the two `STRAND_QMIX → 0.5f` placeholders with real reads
  from the new q-mix slewed buffers (mono row + poly rows), and add the `doM`/`doR` family gating
  (decide q-mix's axis — it's a source-select lane; treat as its own or melody-family; document the
  choice). Promote into `random_` like the other strands at the tail of the remap.

### 4c. Manager wiring (LOR + spread), all three modules
- `MonsoonSandsManager::processDNA` mono block + `MonsoonExpanderManager` poly block: the loops are
  already `POLY_LANES` and use `EDITOR_TO_ENGINE_LANE_QMIX`, so QMIX lane (editor 2 → engine 4) is
  iterated. Verify:
  - `setStrand(..., STRAND_QMIX, ...)` / `polyLenERef(v, PL_QMIX)` get written from the q-mix
    LOR store (they will, since bank/stride were widened to 5 in the crash-fix pass).
  - Spread: `spreadERef(slot, PL_QMIX)` / `getSpread(slot, 2)` feed `SpreadInterp::apply(pe, lane=2,…)`
    for q-mix. **NOTE:** `SpreadManager` (poly) is capped at `lane<4` (`spread[8][4]`) — widen it to 5
    lanes so East's per-voice q-mix spread isn't silently dropped (currently a no-op, non-crashing).
  - `SpreadResolver`/`SpreadInterp` lane arg: confirm they accept lane 2 (q-mix) with a real target
    (the q-mix draw), not the 0.5 placeholder.
- East/Macro/Mono `step()` display loops already read `polyLenE(pv, lane)` / `finalRandomByStrand`
  over `POLY_LANES`, so once 4b populates the q-mix buffers the panels will show q-mix data with no
  further widget change. Verify the q-mix editor row (index 2) shows bars + a moving playhead.

### 4d. Persistence (verify — already widened in crash-fix pass)
- `MonsoonPersistenceManager` array sizes were already grown (lorBase 336, spread 80, macroOwn 80,
  macroSend/Atten 320, global* 15/20/10/5, monoAtten 28, monoOwner 5). Confirm q-mix values now
  round-trip (save a patch with a q-mix LOR/spread edit, reload, confirm it persists).

### 4e. Render-verify + **Commit 4:** "QMIX data: LOR + spread wired to engine (q-mix draw buffers)".
- Exit criteria: q-mix lane renders live bars + playhead in Mono, East (poly + V1), Macro; q-mix
  prob-out jack emits non-flat CV; q-mix spread + LOR knobs audibly/visibly affect the lane; patch
  save/reload preserves q-mix edits.

---

## Cross-cutting notes / risks

- **`SpreadManager` (poly) `spread[8][4]` cap at lane<4** — widen to 5 for East per-voice q-mix spread
  (Task 4c). Non-crashing today (silently drops q-mix spread), so it's a data-completeness fix, not a
  crash fix.
- **Two stale plan docs** (`qmix_geometry_lockstep.md`, `qmix_phase2_ui_checklist.md`): mark them
  superseded by this file (or delete) in whichever commit touches them, so the next reader isn't
  misled by the "preview gap / QMIX_LANE_H" narrative.
- **`macroSpreadModulatesLane` / East spread-arc `getModNorm` guards `>= 4`** — these still reject the
  QMIX lane for the display mod-arc (cosmetic; not a crash). Fold into Task 4 if q-mix spread arcs are
  wanted, else note as follow-up.
- **Environment-vs-code:** the IntelliSense "cannot open source file <cmath>" errors in the editor are
  toolchain/includePath noise on the new machine, NOT code errors — ignore for build correctness
  (the Makefile build is authoritative). Flag any REAL compiler errors from `make` separately.
- **Do not squash.** One commit per task; build + render-verify between each.

## Execution order summary
1. Pre-flight git reconcile (7a72821 vs 2131d7d) → adopt or supersede.
2. Commit 1 — geometry (SandsGrid 13, drop gap, generators + dotmod_design).
3. Commit 2 — Macro box shrink (gen + draw() re-sync).
4. Commit 3 — jacks (QMIX/ACCENT component markers + index-2 renumber, no shape-not-found).
5. Commit 4 — q-mix data (engine draw buffers + spread/LOR wiring + SpreadManager widen).