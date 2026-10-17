# CC PROMPT 04 — Phase 3 step 3: warp UI (panel columns + popup editor)

PREREQUISITE: CC_PROMPT_03 (mirror + w + warp composition) merged and green. This prompt adds
the controls that drive what already works underneath. The per-voice send-tap fix (prompt 01)
must also be merged — the w-send/tap columns reuse its dual-delta publish contract.

## READ FIRST
1. `CLAUDE.md`, `docs/design/CHAT_HANDOFF.md`.
2. `docs/design/SESSION_LOG_2026-10-18b.md` §7 — the FULL UI design (layout, column list,
   send/tap-all-on-Macro, popup gating, the audit-as-guard, subpanels-deferred). This prompt
   executes §7; read it completely first.
3. `panel_src/gen_east_clean.py`, `panel_src/gen_macro_mono.py` — the panel generators.
4. `test/audit_anchor_bind.py` — the geometry guard. THIS IS YOUR ONLY TESTABLE CHECK for UI
   work; you cannot verify Rack rendering. Keep it green and it proves anchors↔binds agree.

Branch: off `lane-expander-refactor`, `feat/warp-ui`. PR in.

## HARD CONSTRAINTS (from §7 — do not violate)
- DO NOT change the probability-bar size. Insert column space LEFT and RIGHT of the bars; the
  bars keep width, surrounding x shifts. (Grid = ED_X=88, ED_W=111, "matches hpp" in both gens.)
- Controls are FULL-SIZE (no shrinking jacks/knobs). Column pitch = 9mm.
- Panels go WIDE (interim ~60HP east / ~74HP macro — accepted). Subpanels are DEFERRED (blocked
  by the adjacency/user-insertion bug — §7); do NOT attempt a subpanel.
- You CANNOT feel-test Rack. For every control: wire it, keep the audit green, and say
  "needs Rack feel-test." Do not claim it renders/behaves correctly.

## THE AUDIT IS THE CONTRACT (the one gap to respect)
audit_anchor_bind.py asserts: every generator anchor has a widget bind, every bind resolves to
an anchor, AND counts match. The counts are HAND-MAINTAINED — a wrong/absent count is only
caught if you add it. So: for EVERY new control family, add its anchor in the generator, its
bind in the widget, AND its `counts` entry in audit_anchor_bind.py, IN THE SAME COMMIT. Then
`python3 test/audit_anchor_bind.py` — sands_east + sands_macro must stay green.

## STEPS

### STEP 1 — LEFT-of-bars columns, BOTH panels (per lane, 7 columns ≈ 63mm)
Insert left of the grid (shifts ED_X and everything downstream right): w-level knob,
w-mod-depth knob, w-mod jack, param1 jack, param1 attenuverter, param2 jack, param2
attenuverter. Descriptive anchor names (match the existing input_cv_/param_atten_ convention),
e.g. per editor-lane el: param_wlevel_<el>, param_wdepth_<el>, input_wmod_<el>,
input_param1_<el>, param_param1att_<el>, input_param2_<el>, param_param2att_<el>.
Wire binds in the widget by the same names. Add counts (each family = 7). Audit green.
NOTE: param1/param2 are per-distribution-KIND (a,b for Kumaraswamy; μ,σ for logit-normal;
inert for 1-param kinds). The knob is generic; the popup editor labels it per current kind.

### STEP 2 — RIGHT-of-bars columns, MACRO ONLY (2 columns ≈ 18mm)
After the existing 6-col send block (SEND_COL_X): w-mod-send knob, w-mod-tap knob. These are
authored per-voice via MACRO's tab (like the existing send/tap — §7), read by East, never on
East. Anchor + bind + counts. Audit green.
NB the tap is a CONTINUOUS [0,1] lerp (PRE↔POST), not a toggle — reuse the per-voice tap
pattern from prompt 01's macroTap[224].

### STEP 3 — the popup trigger button
A button near the deleg/dir cells, RIGHT of bars, per lane. Opens the distribution editor:
  - MACRO: ALWAYS available → edits Macro's own curve for the lane.
  - EAST: available ONLY when the lane is DELEGATED to East (topo.owner(v,lane)==EAST) →
    edits that voice's curve. Grey/disable otherwise.
Anchor + bind + counts for the button family. Audit green.

### STEP 4 — the popup editor itself
Reuse the EXISTING Phase 2 editor widget (src/ui/DistributionCurveWidget.hpp — the square
dual-axis CDF/PDF editor, already built and merged). The popup binds it to the lane's LaneDist
(Macro: lane; East: lane+voice via the tab). On edit → set the scoped dirty flag (prompt 03's
mask) → the mirror rebuilds → the lane's bars on the module redistribute live (the keystone).
The popup shows the CURVE; the bars behind/beside it show the effect. Do NOT occlude the bars
you're previewing — anchor the popup so the lane's bars stay visible.
The EFFECTIVE-DISTRIBUTION view is a SEPARATE visualiser module (not this popup, not the bars)
— out of scope here, note it as a later module.

### STEP 5 — regenerate panels + verify
Run the generators to emit the new SVGs. `python3 test/audit_anchor_bind.py` → east + macro
GREEN (anchors↔binds↔counts all consistent). `bash test/run_all.sh` green. The plugin build
(make, needs Rack SDK) is the HUMAN's check — do not block on it.

## DONE =
- Left columns (7, both panels) + Macro right columns (2) + popup button: anchors, binds,
  counts all added in-commit; audit_anchor_bind GREEN for east + macro.
- Popup wires DistributionCurveWidget to the lane/voice LaneDist; edit → dirty → mirror rebuild
  → bars redistribute (verified by prompt 03's mirror tests, NOT by you in Rack).
- Panels regenerated. run_all.sh green.
- PR body: list every new anchor family + its count; paste audit output; state clearly which
  parts need Rack feel-test (rendering, popup placement, knob feel) — you could not verify them.

## NOT THIS PROMPT
- The separate effective-distribution VISUALISER module.
- Parameter modulation wiring (theta mod of distribution params) — depends on the param
  jack/atten columns existing (this prompt) but the modulation latch/routing is later.
- Subpanels (deferred — adjacency bug).
- Boolean-lane response curves (deferred by proof).
