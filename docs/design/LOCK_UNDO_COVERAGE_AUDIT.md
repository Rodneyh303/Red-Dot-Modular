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

### G3 — Sands-axis live-under-lock depth — PARTIAL
- `dirLive_(strand)` uses `scopeLiveMask & (melodyAxis ? kSandsM : kSandsR)`; covered
  indirectly via `test_lor_phrase_latch` / `test_slew_latch_regression`. No direct test that
  each Sands scope bit (SB_SANDS_R/M, SB_DICE) independently gates live-under-lock.
- **Priority: LOW** (indirect coverage exists; direct coverage would harden, not fix).

---

## Recommended order

1. **G1 dice undo** — high value, low effort, rack-free. Start here.
2. **G2 Straits Sands visual lock+undo** — needs a store-edit shim or a small rack-free core
   (same Cantonment/CA precedent); schedule as its own PR.
3. **G3** — opportunistic hardening, bundle with G2 if the Sands scaffolding is already open.

## Not in scope (deliberately)
- Feel-tests already queued for Rodney (reset-edge commit decision, rateManager Phrase reverse
  blast radius) — those are behavioural questions, not coverage gaps.
- #371/#372 gate-2 flip (pin correct reverse) — tracked separately; depends on #376 landing.
