# CC PROMPT 04 — warp UI (minimal, feel-testable): w-level column + curve-editor popup

PREREQUISITE: CC_PROMPT_03 (mirror + consumer routing) MERGED (it is — trunk 60c43d8).
This is the FIRST piece that lets a user SET warp, so it's where Rack feel-testing finally
has something to grab. SCOPE IS DELIBERATELY MINIMAL — see the scope note.

## READ FIRST
1. `CLAUDE.md`, `docs/design/CHAT_HANDOFF.md`.
2. `docs/design/SESSION_LOG_2026-10-18b.md` §7 — the FULL UI design (layout, popup gating,
   audit-as-guard). This prompt builds the SUBSET of §7 that connects to EXISTING code.
3. `panel_src/gen_east_clean.py`, `panel_src/gen_macro_mono.py` — the panel generators.
4. `test/audit_anchor_bind.py` — the geometry guard. YOUR ONLY TESTABLE CHECK for UI work;
   you cannot verify Rack rendering. Keep it green = anchors↔binds↔counts consistent.

Branch: off `lane-expander-refactor`, `feat/warp-ui-minimal`. PR in.

## SCOPE NOTE — why this is MINIMAL (read before planning)
§7 lists 7 left columns (w-level, w-mod-depth, w-mod jack, param1/2 jacks + attenuverters) +
Macro send/tap. But VERIFIED against trunk: ONLY static w-level exists (Monsoon.hpp:932-933
getWarpW/setWarpW over warpW[112]). There is NO w-modulation bank and NO param-modulation bank
yet — those are a LATER prompt (the modulation layer). So building their jacks/attenuverters NOW
would wire controls to NOTHING. This prompt builds ONLY what connects to existing code:
  - w-LEVEL knob (→ warpW[112], static per-voice depth — EXISTS)
  - the curve-editor POPUP (→ warpF[112], the per-voice distribution — EXISTS)
  - the live-preview keystone (edit → dirty → mirror rebuild → bars redistribute — 03 built the hook)
DEFERRED to a future "04b / modulation UI" prompt (stated in NOT THIS PROMPT): w-mod jack +
depth, param1/param2 jacks + attenuverters, Macro w-mod-send + w-mod-tap. They wait for the
modulation layer to exist. This keeps 04 feel-testable NOW without dangling controls.

## HARD CONSTRAINTS (from §7)
- DO NOT change the probability-bar size. Insert column space; bars keep width (ED_X=88,
  ED_W=111, "matches hpp" — the grid x is a SHARED constant, shifting it shifts downstream cols).
- Full-size controls (no shrinking). Column pitch = 9mm.
- You CANNOT feel-test Rack. For every control: wire it, keep the audit green, say "needs Rack
  feel-test" in the PR. Do not claim it renders/behaves correctly.

## THE AUDIT IS THE CONTRACT (the one gap to respect)
audit_anchor_bind.py asserts: every generator anchor has a widget bind, every bind resolves to
an anchor, AND counts match. Counts are HAND-MAINTAINED — a wrong/absent count is only caught if
you add it. So for EVERY new control family: anchor in the generator + bind in the widget + its
`counts` entry, IN THE SAME COMMIT. Then `python3 test/audit_anchor_bind.py` → east+macro green.

## DECISIONS ALREADY MADE (do NOT re-derive — these are the forks, settled)
- w-LEVEL is PER-VOICE on BOTH panels, authored via each panel's tab. Macro's tab authors the
  Macro-delegated default; East's tab authors that voice's own value (when the lane is
  East-delegated). Same delegation pattern as spread. Store = warpW[112] (getWarpW/setWarpW).
- POPUP GATING: Macro button ALWAYS available (edits Macro's curve for the lane). East button
  available ONLY when the lane is delegated to East for the CURRENTLY-SELECTED TAB's voice
  (topo.owner(selectedVoice, lane)==EAST); grey/disable otherwise. (The gate keys on the SELECTED
  tab's voice, not a fixed v.)
- The curve the popup edits = warpF[slot][lane] where slot = the selected tab's voice (Macro:
  the Macro-delegated slot; East: the selected voice). NOT "LaneDist" — the merged bank is
  warpF/WarpF (Monsoon.hpp getWarpF/setWarpF).
- PER-LANE, the w-level knob and the popup button are the SAME on every lane (no lane is special
  here — booleans get a w-level too; it's just inert until the engine warps that lane, which for
  now is melody/octave only per 03's gate). Do NOT gate the w-level column by lane.

## STEPS

### STEP 1 — the w-level column, BOTH panels (1 column ≈ 9mm, LEFT of bars)
Insert ONE column left of the grid: a w-level knob per lane (7 knobs). Anchor name per
editor-lane el: `param_wlevel_<el>`. Bind in the widget to getWarpW/setWarpW for (selected
tab's voice, el). Add `counts`: param_wlevel_ = 7, BOTH panels. Store-backed (StoreKnob), not
host-exposed. Shifting ED_X right by 9mm moves the grid + everything downstream — the audit
catches any bind that didn't move with it.

### STEP 2 — the popup trigger button, per lane (RIGHT of bars, near deleg/dir cells)
One button per lane. Anchor `param_warpedit_<el>` (or an input/button family as the panel
convention uses). Gating per DECISIONS above (Macro always; East iff selected voice's lane is
East-delegated). Add counts = 7. Audit green.

### STEP 3 — the popup editor
Reuse the EXISTING Phase-2 editor widget (src/ui/DistributionCurveWidget.hpp — square dual-axis
CDF/PDF editor, merged). The popup binds it to warpF[slot][lane] (slot = selected tab's voice).
Picking/editing a curve calls setWarpF, which must TRIGGER THE MIRROR REBUILD (03's hook —
PatternEngine.hpp ~194 rebuildWarpMirror*; find the right scoped entry: a single (voice,lane)
edit → the per-(voice,lane) rebuild, or mark that cell dirty). Then the lane's bars on the module
redistribute live = the KEYSTONE. Anchor the popup so the lane's bars stay VISIBLE (don't occlude
what you're previewing).
The (mode,conc) authoring basis for Kumaraswamy EXISTS (abFromModeConc/modeConcFromAb,
Distribution.hpp) — if the editor exposes Kumaraswamy params, label them mode/concentration via
those helpers, NOT raw (a,b). Other kinds: their own param names.
The EFFECTIVE-DISTRIBUTION view is a SEPARATE visualiser module — out of scope, note as later.

### STEP 4 — regenerate panels + verify
Run the generators → new SVGs. `python3 test/audit_anchor_bind.py` → east+macro GREEN.
`bash test/run_all.sh` green. Plugin build (make, Rack SDK) is the HUMAN's check — don't block.

## DONE =
- w-level column (7 knobs, both panels) → warpW; popup button (7, gated) → opens editor;
  editor binds warpF + triggers the mirror rebuild → bars redistribute live.
- All new families: anchor + bind + counts in-commit; audit_anchor_bind GREEN east+macro.
- Panels regenerated; run_all.sh green.
- PR body: list every new anchor family + count; paste audit output; state clearly which parts
  need Rack feel-test (rendering, popup placement, the sight-leads-sound-by-one-step feel, the
  curve-drag-redistributes-bars keystone) — you could not verify those.

## NOT THIS PROMPT (the deferred modulation UI — "04b", waits for the modulation layer)
- w-MOD jack + w-mod-depth knob (no w-modulation bank exists yet).
- param1/param2 jacks + attenuverters (no param-modulation bank exists yet).
- Macro w-mod-SEND + w-mod-TAP columns (modulation routing — later, reuses prompt 01's tap).
- The separate effective-distribution VISUALISER module.
- Subpanels (BLOCKED — connection-model migration, see CONNECTION_MODEL_MIGRATION_NOTE.md).
- Boolean-lane warp wiring (deferred by proof -18b §3; the w-level column is present but inert
  on booleans until their engine consumer warps — that's fine, not a bug).
