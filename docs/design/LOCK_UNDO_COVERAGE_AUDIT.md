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

### G4 — Warp distributions & phase distortion — NOT lock-gated; curve-draw undo gap
**CORRECTION** (an earlier draft of this section wrongly called these "param-backed, native
undo"). They are NOT Rack params. Rodney's de-paramming exercise moved the whole distribution
set off Rack's param array (which has a hard slot cap). Re-traced through `WarpCurvePopup`,
`SandsDistributionExpander`, `SandsPhaseDistSubpanel`, `PatternEngine`, `Monsoon`:

- **De-paramming confirmed intact.** Spread, warp tilt / atom-level / atom-mass / gap-loc /
  gap-size (= param1/param2/atom/gap), and the kind are ALL de-parammed custom store state.
  **Zero** of them appear in Monsoon's `ParamIds` enum and there are **zero** `configParam`
  calls for them. `warpParamValue(s.f, p)` reads `s.f` — a float inside a `PhaseDistSet` — not
  a Rack `ParamQuantity`. So warp is correctly NOT a standard Rack param, consistent with
  spread. Nothing regressed into a param.
- **`PhaseDistStore`'s "lock"/"snapshot" is NOT the user Lock/Undo feature** — it is a
  per-audio-block reader-copy + `std::mutex` for audio/UI thread consistency
  (PHASE_DISTRIBUTION_SPEC §14). Unrelated mechanism; easy to mistake by name.
- **Undo — mostly covered, one real gap.** The de-parammed knobs are bound via
  `bindStoreKnob` (StoreBound), whose `onDragEnd`/`resetToDefault` push a coalesced
  `StoreEditAction` — so **spread and the warp tune knobs (tilt/atom/gap) ARE undoable**. The
  exception: the **freehand curve-draw** (`WarpCurveEditor` interior-point drag) — its
  `onEdit` only calls `requestWarpRebuild*`, pushing **no** `StoreEditAction`. So editing the
  *drawn* distribution kind is **not undoable**. (Undo gap, drawn-kind only.)
- **Lock gating — absent, confirmed.** No warp/phase setter checks `locked` or queues to the
  phrase boundary. A warp/phase edit under lock changes the sounding output immediately,
  unlike CA/Cantonment/Sands (which defer to the boundary). Rodney has decided this is a
  **defect to fix**: phase and warp SHOULD respect lock.
- **Priority: HIGH (lock-gating — approved work).** Plus a small follow-up: a `StoreEditAction`
  for the freehand curve-draw.

#### G4 implementation plan — make warp/phase respect lock
The apply points are clean and the boundary signal already exists (`LockManager.queueFires()`
/ `boundaryNow()`, rack-free + tested):

- **Warp.** The UI writes the store immediately (knob + undo stay live) and bumps
  `warpUiScope_`; the audio thread drains that scope once per block at `Monsoon.cpp:~826`
  (`switch (warpUiScope_) { rebuildWarpMirror* }`). Gate that drain: when `locked`, hold
  `warpUiScope_` (it already accumulates/widens to ALL) and consume it only when
  `lockManager.queueFires()` fires (boundary, or the unlock edge). Result: edit + display +
  undo stay live under lock; the SOUNDING distribution defers to the phrase boundary — exactly
  the CA/Sands contract.
- **Phase distortion.** Reader picks up new sets via `PhaseDistStore::snapshotSet` per block.
  Gate the reader's pickup the same way (defer the set-swap to the boundary under lock). More
  involved than warp (per-block snapshot), so schedule as its own step after warp.
- **Testability.** The decision is `lockManager.queueFires()` — already rack-free-tested. The
  warp gate itself is a small `Monsoon.cpp` edit, pinned by the plugin-TU syntax smoke + a
  source-shape grep (the `test_played_column_consumers` pattern). Boundary-commit behaviour
  (sounding deferred, applied at edge) is the feel-test.

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
4. **G4 warp / phase-distortion lock-gating** — APPROVED. Make warp + phase edits defer their
   sounding change to the phrase boundary under lock (gate the apply point on
   `lockManager.queueFires()`). Warp first (single drain point in `Monsoon.cpp`), phase-dist
   second (per-block snapshot). Plus a small follow-up: `StoreEditAction` for the freehand
   curve-draw (the one de-parammed edit still missing undo).

## Not in scope (deliberately)
- Feel-tests already queued for Rodney (reset-edge commit decision, rateManager Phrase reverse
  blast radius) — those are behavioural questions, not coverage gaps.
- #371/#372 gate-2 flip (pin correct reverse) — tracked separately; depends on #376 landing.
