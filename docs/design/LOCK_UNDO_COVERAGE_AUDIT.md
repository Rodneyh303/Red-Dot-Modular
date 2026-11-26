# Lock & Undo coverage audit

**Status:** audit only — no behaviour changed. Prioritised gap list for Rodney to direct.
**Date:** 2026-11-26
**Context:** CA/Cantonment reverse-boundary fix landed (#376). This is the "broader lock
and undo sweep" that follows it — a systematic map of where lock and undo are exercised by
tests, and where they are not. Lock and undo were implemented months ago and have not been
swept since.

---

## How to read this

Two independent mechanisms, audited together because they share commit timing:

- **Lock** — freeze pattern/RNG; queue position-authority events; flush on the unlock edge.
  Contract: `queueFires() = (boundaryNow_ && !locked_) || unlockEdge`, `unlockEdge =
  (prevLocked_ && !locked_)`.
- **Undo** — snapshot/restore rings. Several independent rings, one per subsystem; they do
  **not** share a code path, so coverage of one says nothing about the others.

A module is "covered" only if a test exercises the *interaction* (locked boundary defers;
unlock edge flushes; undo restores the pre-change state) — not merely that the type compiles
or that an unrelated signal routes through it.

---

## Covered — with the test that pins it

| Subsystem | Mechanism | Test |
|---|---|---|
| Core sequencer | lock: pattern freeze, RNG not advanced, seed queued, unlock fires | `test_lock_behaviour` |
| Cantonment | lock + undo (pending transforms, boundary commit) | `test_cantonment_pending`, `test_cantonment_module`, `test_cantonment_transforms` |
| Change Alley | commit under lock; undo ring | `test_lock_mode_commit` (#375), `test_change_alley_module` S8 (#373) |
| CA / Cantonment | reverse-boundary commit timing (engine wrap, not column heuristic) | `test_boundary_engine_edge` (#376) |
| Sands axes (R/M) | live-under-lock: LOR latch, slew hold, dir | `test_lor_phrase_latch`, `test_slew_latch_regression`, `test_lock_behaviour` |
| Store-backed edits | undo action replay (generic) | `test_StoreEditAction` |
| Fold | undo restore | `test_fold_undo_restore` |
| Intertropical | scene advance + post-reset swallow (reference for correct boundary) | `test_intertropical_scene` (#374) |

---

## Gaps — nothing exercises these

### G1 — Monsoon dice undo (`diceUndoRing`) — UNTESTED
- Ring buffer `diceUndoRing[DICE_UNDO_RING]` (16 deep) in `Monsoon.hpp` is an audio-thread
  snapshot buffer; the UI-thread undo reads it (`MonsoonWidget.cpp:808 act->s =
  mon->diceUndoRing[...]`). Capture sits at `Monsoon.hpp:1823`.
- The snapshot struct and the pattern state it restores (`PatternEngine`) are rack-free.
  **Tractable** as a rack-free test: set pattern → capture → roll (mutate) → apply snapshot →
  assert pattern == pre-roll, and the ring wraps/depth behave.
- Risk if wrong: a dice reroll cannot be cleanly undone, or undo restores a stale/mismatched
  slot. No guard today.
- **Priority: HIGH** (real undo path, zero coverage, cleanly testable rack-free).

### G2 — Straits Sands visual lock + undo (`StraitsSandsMacroVisual`, `StraitsEastSandsVisual`) — UNTESTED
- ~99 lock/undo/store call-sites across the two visuals. Existing tests that touch these
  files (`test_macro_own_output`, `test_resolver_lor_bridge`, `test_sweepsig_gate`,
  `test_ui_to_resolver`, `test_variation_scan_display`, `test_warp_*`) are about signal
  routing and output isolation — **none** exercise lock deferral or undo restore here.
- `StoreEditAction` itself is tested generically (`test_StoreEditAction`); what is untested is
  the *interaction* — does a pin edit under lock defer to the boundary, and does undo restore
  the pre-edit pin state.
- More module-bound than G1 (VCV Widget + store), so a rack-free test needs the store edit
  path factored or shimmed. **Medium feasibility, higher effort.**
- **Priority: MEDIUM** (generic StoreEditAction gives partial assurance; the lock interaction
  is the real hole).

### G4 — Warp distributions & phase distortion — PARAM-BACKED (native undo), NO lock gating
Raised in the sweep: do the newer warp-distribution and phase-distortion features have lock
and undo? Traced through `PhaseDistortion`, `SandsDistributionExpander`,
`SandsPhaseDistSubpanel`, `PhaseDistStore`:

- **Undo — covered natively, no custom action needed.** Both are driven by Rack **params**
  (warp reads via `warpParamValue(s.f, p)`; the phase-dist set/kind indices are
  `PHASE_SET_PARAM` / `SUBPHASE_SET_PARAM`). Rack pushes a `history::ParamChange` for every
  param edit automatically, so these undo for free. There is **no** custom `history::Action`
  anywhere in the warp/phase-dist tree — and, because the editable state *is* the params,
  none is required. The phase-dist subpanel's `onButton` is set **selection** only, not a
  value edit.
- **`PhaseDistStore`'s "lock"/"snapshot" is NOT the user Lock/Undo feature** — it is a
  per-audio-block reader-copy + `std::mutex` for audio/UI thread consistency
  (PHASE_DISTRIBUTION_SPEC §14). Unrelated mechanism; easy to mistake by name.
- **Lock gating — absent.** Warp has **no Monsoon-Lock control by design** (it is a
  distributional param, not an A/B-mix property — documented at `PatternEngine.hpp:1363`).
  Phase distortion is likewise **not gated by Monsoon Lock**: a param edit under lock takes
  effect immediately rather than queueing to the phrase boundary like CA/Cantonment/Sands.
  Whether that is intended (distributional params are "always live") or an oversight is a
  **design call for Rodney**, not a mechanical gap.
- **Verdict:** not a test gap today. Native param-undo holds; there is no custom edit state to
  snapshot. The only open item is the **design question** of whether phase-distortion param
  edits should respect Monsoon Lock (queue-to-boundary) the way the other voice-affecting
  edits do. If Rodney decides they should, that becomes a feature + a boundary-commit test;
  until then there is nothing to pin.
- **Priority: DESIGN DECISION** (no code/test until the lock-gating question is answered).

### G3 — Sands-axis live-under-lock depth — PARTIAL
- `dirLive_(strand)` uses `scopeLiveMask & (melodyAxis ? kSandsM : kSandsR)`; covered
  indirectly via `test_lor_phrase_latch` / `test_slew_latch_regression`. No direct test that
  each Sands scope bit (SB_SANDS_R/M, SB_DICE) independently gates live-under-lock.
- **Priority: LOW** (indirect coverage exists; direct coverage would harden, not fix).

---

## Recommended order

1. **G1 dice undo** — DONE (`test_dice_undo`, 5 cases: undo/redo round-trip, ORDER-MATTERS,
   stream independence). High value, low effort, rack-free.
2. **G2 Straits Sands visual lock+undo** — needs a store-edit shim or a small rack-free core
   (same Cantonment/CA precedent); schedule as its own PR.
3. **G3** — opportunistic hardening, bundle with G2 if the Sands scaffolding is already open.
4. **G4 warp / phase-distortion** — no test to write; awaiting Rodney's call on whether
   phase-dist param edits should be Monsoon-Lock-gated. Native param-undo already holds.

## Not in scope (deliberately)
- Feel-tests already queued for Rodney (reset-edge commit decision, rateManager Phrase reverse
  blast radius) — those are behavioural questions, not coverage gaps.
- #371/#372 gate-2 flip (pin correct reverse) — tracked separately; depends on #376 landing.
