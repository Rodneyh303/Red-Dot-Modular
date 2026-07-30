# Lock mode audit -- every lock-gate site classified (done with manager design in mind)

Classified each `if (!locked)` / `if (in.locked) return` site against LOCK_SEMANTICS.md §9 using
the READ-VS-MAP principle (shapes/reads probability arrays = LATCH; maps finished output = LIVE).

## Result summary
Every current site implements LATCH, and every one is CORRECT. There are NO wrong-category sites
and NO mis-implementations among the gated sites. The scattered `if (!locked)` checks are all
"skip the write to the engine arrays when locked" = the value latches = correct LATCH behaviour.

This is important for the manager design: the lock manager is NOT fixing broken behaviour. It is
CONSOLIDATING correct-but-scattered LATCH decisions into one place, and providing the category
model so FUTURE controls (and the OPEN-leaning ones) get classified once, centrally.

## Site-by-site classification

### Engine freeze (LATCH via total freeze) -- STAYS IN ENGINE
- PatternEngine.cpp:199  redrawRhythm       -> if(in.locked) return  [LATCH-freeze]
- PatternEngine.cpp:328  redrawMelody       -> if(in.locked) return  [LATCH-freeze]
- PatternEngine.cpp:396  applyPendingSeedsAndRedraw -> freeze seeds/RNG/patterns [LATCH-freeze]
  VERDICT: correct. These are the generation core. They MUST stay engine-side (audio thread,
  no manager round-trip). The lock manager does NOT absorb these -- it READS the same lock bool.
  The engine owns lock STATE + its own freeze; the manager owns the CATEGORY MODEL for controls.

### Monsoon.cpp
- Monsoon.cpp:319  reseed-on-restart gated by !locked  [LATCH]
  VERDICT: correct. Reseeding is generation; frozen under lock. Stays (engine-adjacent).

### Expander manager -- spread/prob writes into engine arrays (LATCH)
- ExpMgr:385  poly REST spread  -> engine write skipped when locked  [LATCH]
- ExpMgr:406  poly MELODY spread -> [LATCH]
- ExpMgr:434  poly OCTAVE spread -> [LATCH]
- ExpMgr:463  poly ACCENT spread -> [LATCH]
- ExpMgr:548  Macro V1 LOR/spread write -> [LATCH]
  VERDICT: all correct LATCH (spread = generation shaping, read-vs-map = READ = LATCH).
  These are the SITES THAT SHOULD BECOME MANAGER QUERIES: replace `if (!engine.locked)` with
  `if (lockMgr.liveNow(Control::Spread))` so the category (Spread=LATCH) lives in the manager.

### Expander manager -- CA transform apply (QUEUE + unlock-flush)
- ExpMgr:109  (vBoundary && !locked) || vUnlock -> applyPendingTransforms  [QUEUE]
  VERDICT: this is the QUEUE category (arm-and-fire-at-boundary) PLUS the unlock-flush. This is
  step b -- it becomes the lock manager's boundary/unlock EVENTS + queue ownership. The shadow
  state (caV2PrevStep_/caV2PrevLocked_) moves INTO the lock manager as legitimate business.

### Sands manager -- spread writes (LATCH)
- SandsMgr:325  mono/voice-1 spread gated  [LATCH]
- SandsMgr:452  SPREAD lock-gated (LOR above runs UNDER lock -- the LATCH/LIVE asymmetry!) [LATCH]
- SandsMgr:547  Macro global spread gated  [LATCH]
  VERDICT: correct LATCH. NOTE the documented asymmetry at :452 -- LOR runs under lock, spread
  doesn't. Per §9 both LOR and spread are LATCH; the "LOR runs under lock" is because LOR is
  seeded once (event-driven), not re-applied per frame, so there's nothing to gate. Spread IS
  re-applied per frame so it needs the gate. Same category, different application cadence.
  -> MANAGER QUERY candidates (lockMgr.liveNow(Control::Spread)).

### Mode controller -- A/B mix latch (LATCH)
- ModeController:63  latchMix gated by !locked  [LATCH]
  VERDICT: correct. §9 resolved A/B MIX = LATCH (upstream generation, pre-spread pre-pins; the
  "live crossfade" instinct was checked and rejected). -> MANAGER QUERY (Control::ABMix).

## What the lock manager owns (confirmed by the audit)
1. Lock STATE lives in the engine (engine.locked) -- unchanged. Engine keeps its own freeze
   checks (:199/:328/:396) -- LIVE-critical, audio-thread, no round-trip.
2. Lock MANAGER (new, Monsoon-side coordinator) owns:
   a. The control -> category MAP (the §9 table as DATA): Spread/LOR/ABMix/pins/direction... =
      LATCH; transpose = LIVE; clock/mute/display = LIVE; scatter = QUEUE. Single source of truth.
   b. liveNow(Control) query: call sites ask "should I apply this now?" instead of `if(!locked)`.
      Category logic lives here; call sites stop knowing WHY.
   c. Boundary + unlock EVENTS (absorbs ExpMgr's caV2PrevStep_/caV2PrevLocked_ shadow state) and
      the QUEUE for arm-and-fire-at-boundary controls (scatter). This is step b's proper home.
3. Engine-side freeze and manager-side category model READ THE SAME lock bool. No duplication of
   STATE; consolidation of DECISION.

## Migration shape (low-risk, because behaviour is already correct)
Because every gated site is already correct LATCH, migration is REFACTOR-ONLY for the LATCH set:
replace scattered `if (!engine.locked)` with `if (lockMgr.liveNow(Control::X))` where liveNow
returns !locked for LATCH controls. Behaviour identical; decision centralized. VERIFIABLE: tests
that currently pin lock behaviour (the ~23 assertions noted in §9) must stay green through the
refactor. THEN the OPEN-leaning controls (transpose=LIVE, direction=LATCH) and QUEUE (scatter)
get their real category, which is where behaviour may CHANGE (and where the inversion the doc
warns about actually lives -- but that's the transpose/direction rulings, not the already-correct
spread/LOR/mix LATCH set).

## Key insight
The doc's "lock mode inverts current behaviour" is NARROWER than it sounds. The spread/LOR/ABMix
LATCH behaviour is ALREADY correct and just needs centralizing (safe refactor). The inversion
applies to the specific OPEN rulings (transpose -> LIVE, direction -> LATCH) and making QUEUE
first-class. So lock mode splits cleanly into: (1) safe consolidation refactor of the correct
LATCH set into the manager, then (2) the genuinely behaviour-changing rulings on the smaller
OPEN/QUEUE set. Phase 1 is low-risk and delivers the manager; phase 2 is the careful part.
