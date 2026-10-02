# Sequencer-quantiser mode collapse 6 → 3 — build plan

Spec (authority): `docs/design/MODE_COLLAPSE_6_TO_3.md`. This is a BUILD, phased, one commit per
phase, Rack listen + suite green between each. Mode behaviour is subtle and easy to break quietly
(see plans/mode_d_shouldexecute_hygiene.md), so each phase must be independently verifiable.

**Decisions (Rodney, fixed):**
- The collapse IS happening — it is the instrument's identity (one continuous seq↔quant axis).
- **NO old-patch migration.** Pre-release; break saved mode values freely. Do NOT write map-on-load
  code. (Revisit only at public release.)

**Current code reality (recon-confirmed):** the six executeMode* are already mostly ONE engine —
executeModeE/F call executeModeA with a phase clock view; C/D are the quantiser variants. So "six"
is a surface/dispatch fiction; the engine is already near three. The collapse is mainly DISPATCH +
PARAM + PANEL, not engine rewrite.

## Phase 0 — recon, write nothing, REPORT
Confirm: the modeSelect read/store/persist sites; the mode param range + labels; exactly how the 6
executeMode* map (E/F→A, C/D quantiser pair); where q-mix polarity is applied today; what the
`test_gate_mode_agnostic` + q-mix tests currently assert. Report what is genuinely 6-way vs already
collapsed, and the minimal set of sites the collapse touches.

## Phase 1 — q-mix POLARITY FLIP, alone, FIRST (highest subtlety, best tested)
Per spec: flip to **0 = generated, 1 = quantised** (currently high q-mix = generated). Do this in
ISOLATION before any structural change — it is a pure behaviour change the existing q-mix /
mode-agnostic tests can guard. Include the **patched-detection rule**: an UNPATCHED pitch input
forces q-mix to 0 (generate), so turning q-mix up with nothing patched is safe/silent, not nonsense.
Verify in Rack: q-mix=0 generates, q-mix=1 quantises the patched CV; unpatched → always generates.
Update/extend tests to assert the new polarity.

## Phase 2 — collapse DISPATCH 6 → 3 (clock / gate / phase)
Map the mode param to THREE: clock(gen rhythm, your tempo), gate(your events, perturbed),
phase(your time-base). Route the old C/D/F into their clock-source + q-mix (quantiser is now just
"that timing origin with q-mix up"), using the SHARED engine path. Minimise engine change — reuse
executeModeA/B and the phase/clock views; it is the dispatch + param that shrink. No migration.
Verify each of the 3 modes in Rack: generate AND quantise (q-mix sweep) within each.

## Phase 3 — panel
Mode column 6 → 3 positions; the SUBTITLE ("generative sequencer-quantiser" or the chosen wording)
in the freed ~27mm top-right; QMIX label/polarity text updated. Generator work, last (depends on the
routing being settled). Regenerate + panel_diff.

## Phase 4 — per-mode gate SMOKE test (land EARLY, not last)
Assert each surviving mode emits a gate with a clock/gate running. The collapse touches mode routing,
so this is the guard that catches a routing regression immediately — exactly the test whose absence
cost hours during the "dead modes" (stale-patch) scare. Register in run_all.sh.

## Order
Phase 0 → Phase 1 (polarity, verified) → Phase 4 (smoke test, so later phases are guarded) →
Phase 2 (collapse) → Phase 3 (panel). (Smoke test pulled early deliberately.)


## CV2 role clash (mono) — resolved: a simple "quantiser in" menu choice
The collapse exposes that CV2's ROLE was mode-dependent: seq mode used CV2 for MODULATION, quant mode
used it as the PITCH CV to quantise. With mode now a continuum there is no discrete mode to key off, so
CV2 needs an explicit role. **Scope is MONO ONLY** — poly quantisation has its OWN dedicated CV in on
Straits, so poly is unaffected.

**Resolution (small, no interaction logic):** a context-menu **"CV2 = quantiser in"** choice (mono).
- ON  -> CV2 is the quantiser pitch input.
- OFF -> CV2 is free for its other use. Its MODULATION role is available from JUNCTION anyway, so
  nothing is lost by reassigning it.
- **No q-mix composition rule needed.** The existing unpatched-pitch-detection rule already handles
  "no pitch source -> generate": if CV2 is not the quantiser in (or unpatched), there is simply no
  pitch source and q-mix generates — the menu choice does not need to interact with q-mix.
- Default: likely ON (quantising is the headline; CV2-as-quantiser-in is the expected behaviour;
  modulation users turn it off and patch Junction). Confirm at build.
- Jack headroom: 3 gate inputs but only 1 needed for CV, so reassigning does not starve anything.

(Earlier over-engineered framing — "assignment composes with patched-detection" — DROPPED; it is just a
binary quantiser-in choice, mono only.)
