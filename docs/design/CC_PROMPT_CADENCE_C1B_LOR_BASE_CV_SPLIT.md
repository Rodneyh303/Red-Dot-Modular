# CC PROMPT — CADENCE C1b: LOR base/CV split + lock integration + edit-suspend (one cadence, shared infra)

Follow-on to C0+C1 (merged: `effectiveVoiceLor`/`effectiveLaneLor` chokepoints + `latchedVoiceLor_`
phrase-latch). C1 latches the COMBINED base+CV value at the phrase, so a knob/drag edit of the LOR
lags to the next phrase — wrong. And C1's latch is BLIND to lock mode (a parallel "hold the LOR"
mechanism beside `LockManager`). C1b fixes both, reusing the instrument's ONE boundary/queue system.

DECIDED (Rodney, 2026-11-16):
- **Base** (param/drag) applies **LIVE** (immediately); on drag release, the edited value stands at
  once. Base is NOT latched.
- **CV modulation** is sampled/committed at the **phrase boundary** (the cadence — tames per-sample
  chaos). `effective = base_live + cv_committed_at_boundary`.
- **Edit-suspend**: while an LOR gesture is active, CV reception is SUSPENDED for the edited scope —
  MACRO edit = that lane across ALL voices; EAST edit = that lane on the ONE voice — so the user
  edits a stable target. CV resumes at the next boundary after release.
- **Lock**: editing while LOCKED PENDS and commits at UNLOCK (answer 1A), using the SAME pending
  mechanism as direction (answer 2 = yes) — the shared "cache until boundary/unlock" system that CA,
  cantonment, and dice already use.

## ⚠️ DISCIPLINE — behaviour change (like C1, not the refactors)
Not byte-identical. REPRODUCE-FIRST + NEW PINS + deliberately-regenerated goldens explained in the PR.
Stage A (fold the latch into lock/boundary) may be near-identical; Stages B/C change behaviour.

## CARDINAL RULE
VERIFY AGAINST CODE AND TESTS. No Rack here; the Sands feel-test (base-live drag, mod-suspend,
lock-pend) is Rodney's leg. Prove each stage with a test that FAILS before / PASSES after.

## BRANCH
Off `lane-expander-refactor` (after C0+C1 merges there). `feat/cadence-c1b-lor-base-cv-split`.

## THE SHARED INFRASTRUCTURE TO REUSE (do NOT build a parallel mechanism)
- `LockManager::boundaryNow()` — step wrapped ⇒ phrase boundary (`playedColumn < prevStep_`).
- `LockManager::queueFires()` — `(boundaryNow && !locked) || unlockNow` — THE "commit pending now"
  signal (phrase boundary while unlocked, OR flush at the unlock edge). CA/cantonment/dice ride this
  (`expanderManager.sync(engine, lockManager.queueFires())`, Monsoon.cpp:1834).
- `LockManager::liveNow(Control::Lor, locked, scopeLiveMask, melodyAxis)` — the lock gate (LATCH:
  live iff unlocked OR the axis's `ScopeBit` is set in `scopeLiveMask`). Already used for LOR in
  MonsoonSandsManager/ExpanderManager.
- `ScopeBit` (`SB_SANDS_R/M`, MonsoonLockManager.hpp) — the per-Sands-axis scope mask; SINGLE SOURCE
  OF TRUTH shared with the engine (`SequencerEngine.hpp:388-400` `dirLive_`). REUSE for the
  edit-suspend scope — do NOT invent a parallel scope.
- TEMPLATE to mirror: direction-under-lock (`SequencerEngine.cpp:426-445`) — `laneDirPending_[l]`
  holds the new value; `laneDir_[l] = laneDirPending_[l]` commits only when `dirLive_(l)`. LOR does
  the analogous pend-and-commit.

## STAGE A — fold C1's latch into the lock/boundary system (reconcile the parallel mechanism)
`refreshVoiceLorLatches`/`latchedVoiceLor_`/`latchedLaneLor_` currently sample at the phrase edge with
NO lock awareness. Re-express the latch so the commit rides `boundaryNow()`/`queueFires()` and is
gated by `liveNow(Control::Lor, …)` — i.e., the latch IS the shared boundary-commit, lock-aware, not
a separate edge sampler. Scope-gate per `dirLive_`-style (`ScopeBit`). Where C1's latch already fired
exactly at the phrase boundary unlocked, this stage is behaviour-preserving; the NEW behaviour is
only that it now also pends/commits correctly under lock.

## STAGE B — split base from CV (base live, CV at boundary)
The base and CV are COMBINED at the manager before the engine sees them (e.g. Monsoon.cpp:1879-1880,
`LENGTH_PARAM` + `LENGTH_INPUT` voltage; same for `OFFSET_INPUT`, the `DNA_*_INPUT`, `VLOR_CV`).
- Pass **base** and **CV** to the engine SEPARATELY (stop pre-combining).
- Engine: `effective = base + cv_committed`, where **base is applied LIVE** (gated only by
  `liveNow` for lock — unlocked ⇒ immediate; locked ⇒ base pends, commits at unlock like direction)
  and **cv_committed** is the CV contribution sampled at `boundaryNow()` (Stage A's commit).
- Net: a knob/drag changes `base` and is seen IMMEDIATELY (unlocked); CV wiggles only move the window
  at the phrase boundary.

## STAGE C — edit-suspend during an active LOR gesture
While an LOR gesture is active (the Sands `SandsRangeGestureCore` drag — `Z_START/END/ROTATION/WINDOW`),
SUSPEND the CV commit for the edited scope so the window tracks only the live base:
- MACRO gesture ⇒ suspend that lane's CV across ALL macro-following voices.
- EAST gesture ⇒ suspend that lane's CV on the ONE edited voice.
Implement the scope via the existing `ScopeBit`/`scopeLiveMask` axis mask (the suspend is a transient
"this axis is live-base-only" window), NOT a new scope concept. On release, CV resumes at the NEXT
boundary (hold the pre-gesture CV sample through the gesture). Modulation does NOT apply during the
gesture — this is the "miles better than a constantly moving target" behaviour.

## NON-SCOPE
- C4 (display): once base is live in the engine, the Sands range shows base live during/after a drag
  and the effective window (base + committed-CV) steps at the phrase. SEPARATE prompt (Rodney's leg).
- The maintained bidirectional draw buffer, 32-step UI, C3 direction — unrelated.

## ACCEPTANCE (reproduce-first, staged)
- Stage A: a test pinning the LOR latch commits at `boundaryNow()` AND pends under lock, committing at
  the unlock edge (`queueFires`'s `unlockNow`) — scope-gated. Marches unchanged where unlocked.
- Stage B: pin that a mid-phrase BASE change takes effect IMMEDIATELY (unlocked), while a mid-phrase
  CV change takes effect only at the next boundary. FAILS on the current combined-latch engine.
- Stage C: pin that, during an active gesture on (macro lane | east lane+voice), a CV change on that
  scope does NOT move the effective window until release+next boundary; other scopes/voices unaffected.
- `bash test/run_all.sh` green; nested-LOR/lock goldens regenerated deliberately + explained in the PR;
  goldens without LOR base/CV/lock interaction BYTE-IDENTICAL. `test_16_triage` green. Lock suite
  (`test_cantonment_pending` and the lock tests) green — the shared mechanism must not regress.

## DELIVERABLE
On `feat/cadence-c1b-lor-base-cv-split`, PR into `lane-expander-refactor`. Stages A→B→C, each a
reviewable commit with its reproduce-first pin. Rodney feel-tests (base-live drag, mod-suspend scope,
lock-pend) + then the C4 display prompt follows.
