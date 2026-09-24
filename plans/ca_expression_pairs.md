# Change Alley → 60HP: 8 correlation pair jacks + 8-slot connect row — build plan

Branch `feat/sands-qmix-geometry`. Panels generated (`gen_change_alley_v2.py`); geometry mirrored in
`MonsoonChangeAlleyV2.hpp` under the "MUST MATCH" discipline. Read done — findings + plan + the ONE
decision I need before coding Phase 1.

## Key findings from the code (some differ from the brief's assumptions)

1. **CA has NO kit / NO components layer / NO anchors.** Every port is placed by the widget with
   `createInputCentered<PJ301MPort>(mm2px(Vec(lx(J_DOM, flip), y)), …)` using constants that MUST MATCH
   the generator. The generated SVG draws *art only* (wells/rings), no `id=` shapes. The ConnectMark is
   a `redDot::ConnectMark` child added by the widget (bottom, right of the legend), not an anchor.
   → **The brief says "kit anchors in the components layer so they bind by name." CA doesn't work that
   way and adding a kit to it would be a much larger change.** See DECISION below.

2. **The q-mix permutation table EXISTS.** `qmixSrc[16]` is a full parity sibling of
   `rhythmSrc`/`melodySrc` — row-radio, scattered, persisted, undoable — staged into the engine as
   `caQmixSrc[16]`. So **all 8 pairs (3 rhythm / 3 melody / 2 q-mix) can wire to REAL permutations**;
   no straight-passthrough TODO is needed. (I'll still confirm the exact array to read in the Phase 3
   report before building DSP, as the brief requires.)

3. **Width is computed, not chosen.** `PW_RAW = 2*(CTRL_W + GUTTER0) + GRID_W`, rounded up to HP;
   slack absorbed into the gutter. Current = **56HP**. `CTRL_W` is the per-side control-strip width;
   the right side is the left mirrored via `lx(x, flip) = PW_MM - x`. So adding an outer column on the
   LEFT and letting the mirror produce the RIGHT one is automatic and row-aligned for free.

4. **Ports declared today:** `ChangeAlleyV2Ids::NUM_INPUTS = 63`, and **zero outputs** (`config(…, NUM_INPUTS, 0, …)` — literal 0, no `OutputIds` enum). This feature adds CA's output side for the first time.

## Phase 1 — panel geometry (commit alone, render, show you)

- Add ONE outermost column each side at the existing `JACK_P` (8.5mm): 8 poly-IN wells down the far
  left, 8 poly-OUT wells down the far right (the mirror gives the right side).
- Shift the existing control group inboard by one `JACK_P` step: the expression column takes
  `MARGIN + J_HALF`; the current `J_DOM…LIGHT_X` group starts one `JACK_P` further in. `CTRL_W` grows by
  `JACK_P`, so `PW_RAW → HP` recomputes. Expected **56HP → 60HP** (I'll report the exact number and how
  I distributed the rounding slack — the brief wants it split into margins, not piled on one side; the
  generator already splits slack symmetrically into `GUTTER`, and I'll keep that symmetry).
- 8 rows over the matrix's vertical extent, grouped **3 rhythm / 3 melody / 2 q-mix** using the same
  `GROUP_GAP` the verb blocks use, so streams read as blocks. IN row k and OUT row k share a y.
- Ring colour by stream: rhythm **white**, melody **red**, q-mix **green** (matches the legend / true-
  reverse rings). Drawn by the generator's ring colour + the widget's port theme.
- **8 connect-mark slots**, top-right, one contiguous row, right-aligned to the margin. Slot k = pairId
  k (fixed, never packed); filled `pairColour(k)` when connected, dim when empty; primary on a second
  axis (ring/tick, not brightness) — consumes the Phase-2 `HostBadge` vocabulary. I'll leave ≥ one
  label-height gap below the COLLAPSE INTER label and **report the exact clearance mm**.
- **Remove** the single bottom-centre ConnectMark — the 8-slot row replaces it.
- Logo + title stay as they are (centred, top), NOT grouped with the marks.

## Phase 2 — ports
- Extend `ChangeAlleyV2Ids::InputIds` with `EXPR_IN_START` (8) and add a new `OutputIds` enum with
  `EXPR_OUT_START` (8) + `NUM_OUTPUTS`. `config(…, NUM_INPUTS, NUM_OUTPUTS, …)`.
- `configInput/configOutput` labelled by stream+index ("Rhythm expr in 1", …, "Q-mix expr out 2").
- Placement: follow CA's established pattern — widget `addInput/addOutput` at the new column x (mm),
  matching the generator (unless you pick the anchors route below).

## Phase 3 — DSP (report before building)
- Each pair: `out channel k = in channel perm[k]` (or its inverse — I'll state which in the report),
  where `perm` is that stream's live table: rhythm→`rhythmSrc`, melody→`melodySrc`, q-mix→`qmixSrc`.
  Set output channel count to 16 (poly), phrase-granular update (the tables only change on scatter/verb
  edits). All 8 pairs wire to real tables (q-mix exists). I'll confirm the exact indexing direction and
  the "same permutation object Keppel consumes" correctness point before writing code.

## DECISION TAKEN: Option B-full (anchors + migrate ALL existing CA ports to bind-by-name)

CA gets a `components` anchor layer like Monsoon, and its widget converts to the SvgPanelKit
`Compose<W, ShapeQuery, Bind, Reload>` pattern, binding every port/param/light by name. Staged:

- **B0 (generator):** `gen_change_alley_v2.py` emits an invisible `components` layer of
  `<circle id="…" fill="none" stroke="none">` anchors at the SAME mm the art draws, computed from the
  SAME loops. Anchor id scheme (mirrors the widget's index loops so both stay in lockstep):
  - per (verb, side, sub) with r = rowId(verb,side,sub):
    `input_domain_{r}`, `input_codomain_{r}`, `param_grain_{r}`, `param_btnD_{r}`, `param_btnC_{r}`,
    `light_pending_{r}`
  - COLLAPSE only: `param_leader_{li}` (li = side*TYPES+sub) at KNOB2
  - ROTATE only: `param_step_{sIdx}` at KNOB2
  - SCATTER only: `input_scback_dom_{si}`, `input_scback_cod_{si}`, `param_screv_d_{si}`,
    `param_screv_c_{si}`
  - true-reverse (per ty): `input_truerev_{ty}`, `param_truerev_{ty}`, `light_truerev_{ty}`
  - NEW this feature: `input_expr_{k}` (far-left), `output_expr_{k}` (far-right) for k=0..7;
    `light_hostslot_{k}` for k=0..7 (top-right 8-slot connect row). Bottom connect mark removed.
- **B1 (widget):** replace the hardcoded `mm2px(Vec(lx(...)))` calls with `bindInput/bindParam/
  bindLight` by those names, driven by the identical loops. `loadPanel()` + `Compose`. Behaviour-
  neutral: same widgets, same ids, positions now read from anchors (== the art mm), so panel_diff
  is unchanged.
- The generator stays the single geometry source; the "MUST MATCH" hpp constants become the anchor
  positions instead of duplicated placement maths.

## (superseded) earlier decision prompt
**Ports/marks placement: follow CA's existing hardcoded-mm "MUST MATCH" pattern, or introduce a kit /
components-layer with anchors as the brief literally says?**
- **(A) Hardcoded mm (recommended):** matches every other port on CA, no new kit machinery, lowest risk,
  keeps the generator "art-only." The generator and `.hpp` stay in lockstep as they already must. The
  brief's "bind by name" isn't how CA is built.
- **(B) Anchors:** add a `components` layer + kit binding to CA. Larger, novel change for this module,
  and inconsistent with CA's 60+ existing hardcoded ports (they'd stay hardcoded while only the 8 new
  ones bind by name — a split pattern).

I recommend **(A)**. If you want (B) I'll scope it as its own step.

## Verify
`panel_diff.py` old vs new: differences confined to the two new columns, the inboard-shifted content,
the top-right 8 marks, and the removed bottom mark. Build both themes; report any anchor the kit can't
find (only relevant under option B).
