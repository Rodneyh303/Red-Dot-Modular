# Mode Collapse 6→3 — Phase 0 Recon (write nothing, report)

Spec (authority): `docs/design/MODE_COLLAPSE_6_TO_3.md`. Build plan: `plans/mode_collapse_6_to_3.md`.
This file is the **Phase 0 recon record** — no code, just the confirmed map of what the collapse
actually touches. Phase 1+ build against these facts.

---

## 1. How the six `executeMode*` actually map

The spec claim — "the engine is already mostly three" — is **confirmed and stronger than stated**.
Two layers:

**Controller layer** (`src/dsp/managers/MonsoonModeController.cpp:444`) — the real dispatch. The
switch at line 453 routes all six modes, but only **two engine entry points** are ever called:

| Mode | Controller fn                     | Engine call it makes                                       |
|------|-----------------------------------|------------------------------------------------------------|
| A (0)| `executeModeA()`          (:280) | `engine.executeModeA(clock, …)`                            |
| B (1)| `executeModeB()`          (:306) | `engine.executeModeB` / `executeModeBSubdivided`           |
| C (2)| `executeModeC()`          (:392) | **`engine.executeModeA(clock, …)`** + `beginQuantiserSource_` |
| D (3)| `executeModeD()`          (:411) | **`engine.executeModeB` / `Subdivided`** + `beginQuantiserSource_` |
| E (4)| `executeModeE()`          (:230) | **`engine.executeModeA(phaseView, …)`**                    |
| F (5)| `executeModeF()`          (:259) | **`engine.executeModeA(phaseView, …)`** + `beginQuantiserSource_` |

So: **A/C/E/F all run `engine.executeModeA`**; **B/D both run `engine.executeModeB`**. The only thing
C/D/F add over A/B/E is the `beginQuantiserSource_` (`:364`) flag toggle
(`engine.quantiserPitchSource = true`), cleared after `postExecute_`. E/F swap the `ClockEngine`
argument for a synthesized `phaseView` (edge-only, `sixteenthEdge = true`).

**Engine layer** (`src/dsp/engines/SequencerEngine.cpp:637`) — the genuinely three-way core is
`executeModeA` (clock/phase-driven grid) + `executeModeB` (gate-event-driven) + `executeStep`
(shared per-step cascade).

**Dead code confirmed:** the engine's own `executeModeC` (`SequencerEngine.cpp:1222`) and
`executeModeD` (`SequencerEngine.cpp:1236`) are **never called by the controller** (C→A, D→B). Pure
dead weight — the old fixed-quarter / sample-while-high Vermona logic. The collapse can delete them.

**Verdict:** the engine is *already* two functions (`executeModeA`, `executeModeB`) + a shared
`executeStep`. The "six" is a dispatch + flag-toggle fiction. The collapse is **dispatch + param +
panel**, not an engine rewrite.

## 2. Where `modeSelect` and q-mix polarity live

**`modeSelect`** is a bare `int` member, **declared twice**:
- `Monsoon` (the module) — the authoritative one, cycled/persisted/used in every `modeSelect ==`
  branch in `Monsoon.cpp`.
- `SequencerEngine::modeSelect` (`engines/SequencerEngine.hpp:549`) — a **stale mirror**, read at only
  two spots: the stepped-mode light logic (`SequencerEngine.cpp:370`) and Mode-B note-variation
  (`:773`). Must track the collapse in lockstep.

**`modeSelect` read/store/persist sites:**
- **Store/cycle:** `UIManager::processModeButton` (`managers/MonsoonUIManager.cpp:160`) —
  `(modeSelect + 1) % 6` (the `% 6` → `% 3` site).
- **Persist save/load:** `MonsoonPersistenceManager` (`managers/MonsoonPersistenceManager.cpp:44`)
  writes `json_integer(m->modeSelect)`; `:286` reads it back. **No migration** (pre-release) — old
  values 3/4/5 load as-is and misroute; acceptable per spec.
- **Dispatch branch table** (`modeSelect ==` in `Monsoon.cpp`): phase gating `:632`, CV2 gating
  `:610`, gate routing `:690`/`:733`/`:736`, gridPulse `:789`, **shouldExecute** `:808-819`, poly
  latch `:830`, jump replay `:868`/`:883`, cv1IsPhase `:907`, CV2 quant read `:1281`.
- **UI:** `MonsoonWidget.cpp:461` (6 lights), `:1190` (6-label menu). `MonsoonConfigurator.cpp:68`
  (button label).

**Q-mix polarity — current state (HIGH q-mix = generated):**
The decision lives at **one line, two call sites** in `SequencerEngine.cpp:462`:
```cpp
const bool qmixUseGenerated = quantiserPitchSource && (r_qmix < input.qmixLevel);
```
- `r_qmix` = uniform draw ∈ [0,1) from the q-mix strand.
- `qmixLevel` = the QMIX_LEVEL knob (0..1), resolved via `getEffectiveMonoQmix`
  (`Monsoon.cpp:302`) / `getEffectivePolyQmix` (`managers/MonsoonParameterManager.cpp:226`).
- **level=0 → `r < 0` never → always quantised**; **level=1 → `r < 1` always → always generated.**
  Opposite of target (0=generated, 1=quantised). Poly mirror at `:1011`, `qmixHit` latch at `:588`
  re-uses the same `r_qmix < input.qmixLevel` form.

**Polarity flip = a single comparison operator change at three sibling sites** (mono `:462`, poly
`:1011`, latch `:588`) — flip `<` to `>=`. Plus the **patched-detection rule** (spec §60): an
unpatched pitch input forces effective q-mix to 0. Today there is *no* patched-detection; `qmixLevel`
is just the knob (+ Causeway CV). The rule belongs in the effective-q-mix resolver
(`getEffectiveMonoQmix` / `getEffectivePolyQmix`), reading `pitchIn.isConnected()` /
`getChannels()`. **Most testable, isolated change** — why Phase 1 is first.

## 3. What the `mode` param and tests cover

**MODE_PARAM** is a `configButton` (momentary), **not** a ranged switch — no `min/max/enum` config to
change. `modeSelect` is a free `int`, cycled `%6`. Collapse changes `%6`→`%3` + menu labels; the param
itself needs no reconfig.

**Tests covering modes / q-mix:**
- `test_gate_smoke.cpp` — the "tonight" guard. Drives each of 6 modes (A/B/C/D/E/F) through
  `engine.executeModeA`/`executeModeB` and asserts a gate emits with a clock/gate running. Already
  exists, registered in `run_all.sh:62`. Phase 4 *adds* per-mode routing assertions (currently tests
  engine entries, not controller dispatch); pulling it forward is right.
- `test_gate_mode_agnostic.cpp` — the §421 invariant. Asserts (A) generator vs (B)
  quantiser-flag+force-generated are **bit-identical**, (B) vs (C) quantised-CV differ only in pitch,
  and the **gate envelope is invariant across the q-mix range** (0/0.25/0.5/0.75/1.0). **Direct guard
  for the polarity flip** — but currently encodes the *old* polarity (`qmixLevel=1.0 →
  forceGenerated`). Phase 1 must **invert its expectations**: after the flip, `qmixLevel=0 →
  generated`, `qmixLevel=1 → quantised`. Structure (A-vs-B bit-identical, envelope-invariant) stays;
  the *qmixLevel values* flip.
- `test_qmix_rng.cpp`, `test_qmix_poly.cpp`, `test_ca_qmix_source_select.cpp` — q-mix
  stream/distribution/CA-routing, **polarity-agnostic** (test the draw, not the comparison). Pass
  through the flip unchanged.
- `test_edge_cases.cpp:749` / `test_MeloDicer.cpp:608` assert `validModeSelect` is 0..3 — **stale**
  (modes go to 5). After collapse they become 0..2; update.

## 4. Minimal set of sites the collapse touches

### Phase 1 — polarity flip (isolated, highest subtlety)
1. `SequencerEngine.cpp:462` — mono `qmixUseGenerated`: `<` → `>=`
2. `SequencerEngine.cpp:1011` — poly `qmixUseGenerated`: `<` → `>=`
3. `SequencerEngine.cpp:588` — `qmixHit` latch: `<` → `>=`
4. **Patched-detection rule** in `getEffectiveMonoQmix` (`Monsoon.cpp:302`) +
   `getEffectivePolyQmix` (`managers/MonsoonParameterManager.cpp:226`): unpatched → 0; per-channel per
   spec §60-66.
5. Test inversion: `test_gate_mode_agnostic.cpp:107` (swap which qmixLevel means generated).

### Phase 4 — smoke test (pulled EARLY, before Phase 2)
6. Extend `test_gate_smoke.cpp` to assert the **3 surviving modes** each emit a gate through the
   (collapsing) dispatch; keep the `shouldExecute` mirror (`:64`) in lockstep with `Monsoon.cpp`.
   Register already present.

### Phase 2 — dispatch collapse 6→3
7. `MonsoonModeController.cpp:444-462` — `executeMode` switch 6→3 (clock/gate/phase), folding C/D/F's
   `beginQuantiserSource_` into the q-mix path.
8. `MonsoonModeController.cpp:280` `executeModeA/B/C/D/E/F` — collapse to three; the quantiser twins
   become "that timing origin with q-mix up".
9. **Delete dead code** `SequencerEngine.cpp:1222` `executeModeC` + `:1236` `executeModeD` + their
   `hpp:737` declarations.
10. **`Monsoon.cpp` branch table** — every `modeSelect ==` listed in §2 collapses to 3 values
    (clock=0/gate=1/phase=2). ~10 sites.
11. `SequencerEngine::modeSelect` (`engines/SequencerEngine.hpp:549`) mirror (`:370`, `:773`) — update
    range checks.

### Phase 3 — panel (last)
12. `MonsoonUIManager.cpp:166` `%6`→`%3`.
13. `MonsoonWidget.cpp:461` (6→3 lights), `:1190` (6→3 labels).
14. `MonsoonConfigurator.cpp:68` label.
15. Panel generator (`panel_src/gen_straits.py`) — 3-position mode column, subtitle in freed ~27mm,
    QMIX label/polarity text. Regenerate + panel_diff.

## 5. Risk confirmations (from recon)

- ✅ **Engine already three** — A/C/E/F→`executeModeA`, B/D→`executeModeB`. No engine rewrite; the
  dead `executeModeC/D` delete is pure cleanup.
- ✅ **No migration** — `modeSelect` persists as a free int; pre-release, old 3/4/5 values just
  misroute. Removes the fiddliest part entirely.
- ✅ **Polarity flip is 3 lines + a resolver rule** — guarded by the existing
  `test_gate_mode_agnostic` invariant (after inverting its expectations). Most testable, de-risks
  everything downstream.
- ✅ **Smoke test exists and is registered** (`run_all.sh:62`) — pulling Phase 4 forward turns a
  silent routing regression into one red test.

Both de-risking facts hold against the code: the engine is near-three, and there is no migration code
to write.

## Order
Phase 0 → Phase 1 (polarity, verified) → Phase 4 (smoke test, so later phases are guarded) →
Phase 2 (collapse) → Phase 3 (panel). (Smoke test pulled early deliberately.)
All commits on a fresh branch off master.
