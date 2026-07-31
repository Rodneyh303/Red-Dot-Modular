# Lock mode -- where we are / what's next (resume note)

Branch: feat/lock-mode (off master, which now has the merged undo suite). Everything below is
committed. This note is the "pick it up later" summary; detail lives in the referenced docs.

## Done this session
1. Merged undo suite to master (items 1,2,3,5 verified in Rack; item 4 deferred to scrub).
   Fast-forwarded master to master_deparam (de-param arc done). All aligned at a5659e1.
2. Lock mode AUDIT (LOCK_MODE_AUDIT.md): classified all ~15 lock-gate sites. KEY RESULT --
   every gated site already implements LATCH CORRECTLY. Lock manager is CONSOLIDATION of correct-
   but-scattered decisions, not a bug fix. "Inverts current behaviour" is narrow: only the OPEN
   rulings (transpose->LIVE, direction->LATCH) + making QUEUE first-class actually change behaviour.
3. LockManager FOUNDATION built (src/dsp/managers/MonsoonLockManager.hpp), wired as
   Monsoon::lockManager{engine.locked}. INERT so far -- nothing calls liveNow() yet. 30/30 green.
   - LockCategory {LATCH, LIVE, QUEUE}; category-keyed Control enum (NOT per-physical-control).
   - Control groups corrected to real hardware: BigFive (5 rhythm knobs), NoteSliders (12),
     OctaveRange (2) are SEPARATE entries. Spread/Lor/ABMix/Pins/Reseed LATCH; Clock/Mute/Display
     LIVE; Scatter QUEUE.
   - categoryOf() = §9 table as code; liveNow(Control) = the query replacing `if(!engine.locked)`.
4. Expander classification (Rodney's functional mapping, confirmed vs §9):
   - Junction -> BigFive (LATCH), one category.
   - Interchange -> NoteSliders + OctaveRange (both LATCH). CONFIRM inputs separable or blanket.
   - Raffles -> ABMix (LATCH) + dice-gates (QUEUE) + slew (folds into QUEUE, sampled at boundary,
     NOT an independent axis -- no Control::Slew).
   - No expander needed a NEW enum entry -- category-keyed design held.

## Architecture (settled)
- Lock STATE = engine.locked (single bool). Engine keeps its OWN freeze checks (PatternEngine
  redrawRhythm/Melody/applyPendingSeeds :199/:328/:396) -- audio-thread, no manager round-trip.
- Lock MANAGER owns the CATEGORY MODEL: control->category map, liveNow() query, and (phase 2)
  boundary/unlock EVENTS + QUEUE (absorbing MonsoonExpanderManager's caV2PrevStep_/caV2PrevLocked_
  shadow state -- that's "step b" from the CA cleanup).
- Enum granularity = per control GROUP (12 note sliders = one entry), not per knob, not per
  category. Coverage = threading liveNow() through control SITES, not enum size.

## NEXT STEP (phase 1 migration, behaviour-preserving)
Thread liveNow() through the already-correct LATCH sites, replacing `if (!engine.locked)` with
`if (lockManager.liveNow(Control::X))`. liveNow(LATCH)==!locked so behaviour is IDENTICAL. The ~23
lock-behaviour test assertions must stay GREEN throughout -- that's the safety net.
Worklist (from LOCK_MODE_AUDIT.md), do incrementally, tests after each, commit per group:
- Spread writes: ExpMgr :385/406/434/463/548; SandsMgr :325/452/547 -> liveNow(Control::Spread).
- A/B mix: ModeController :63 -> liveNow(Control::ABMix).
- (Monsoon.cpp:319 reseed -> liveNow(Control::Reseed).)
Then thread the note/octave slider + big-5 knob sites (find their engine-write sites first).

## PHASE 2 (later, behaviour-CHANGING -- separate careful pass)
- OPEN rulings that invert: Transpose -> LIVE (add Control::Transpose); Lane Direction -> LATCH
  (add entry); Mono/Macro owner -> leans LIVE (add entry).
- QUEUE made first-class: Scatter + Raffles dice-gates arm-and-fire; the boundary/unlock EVENTS
  move from ExpMgr shadow state into the lock manager (CA cleanup "step b"). This overlaps the CA
  transform-apply path (applyPendingTransforms) already isolated on master.
- Per-expander finish: Causeway (likely one LATCH), Changi (LIVE?), Shophouse (mask LATCH /
  toggle orthogonal), Interchange note-vs-octave separability.
- Lock SCOPE context menu (§7): whole-module default; section/per-lane later.

## Watch-outs
- SequencerEngine has its OWN separate `locked` bool (SequencerEngine.hpp:397) -- check relation
  before touching either.
- Lock mode touches the LIVE signal path (unlike undo = history only). Bigger risk. One group at
  a time, build-verify in Rack, same discipline as undo.

## Phase 1 completion note

Phase 1 (behaviour-preserving migration of scattered LATCH guards to LockManager::liveNow) is
DONE. Migrated:
- ExpMgr spread x5 (:386/407/435/464/549) -> liveNow(Control::Spread).
- SandsMgr spread x3 (:325/452/547) -> liveNow(Control::Spread).
- ModeController A/B mix (:63) -> liveNow(Control::ABMix).
- Monsoon reseed (:319) -> liveNow(Control::Reseed).
All static-form (pass the same engine.locked) except reseed (instance form in Monsoon). 30/30 green
throughout -- behaviour identical (liveNow(LATCH)==!locked).

### Big-5 / NoteSliders / OctaveRange: NOTHING TO MIGRATE (important finding)
These controls are NOT gated by scattered if(!locked) checks. They flow to the engine
UNCONDITIONALLY (e.g. Monsoon.cpp:933 engine.accentProb = ...; big-5/note/octave populate
PatternInput every control tick). Lock is enforced DOWNSTREAM by the ENGINE FREEZE
(PatternEngine::redrawRhythm/Melody/applyPendingSeeds: if(in.locked) return) -- the values keep
being written but the engine ignores them when locked because it doesn't redraw. That IS correct
LATCH behaviour, via engine-freeze, which we agreed STAYS in the engine (audio-thread, no manager
round-trip).

So their Control enum entries (BigFive/NoteSliders/OctaveRange) exist for MODEL COMPLETENESS and
phase-2 use (if any becomes independently lockable), but there is no phase-1 call-site swap for
them. Phase 1 is complete.

### Engine freeze sites: intentionally NOT migrated
PatternEngine.cpp :199/:328/:396 (if(in.locked) return) stay engine-side by design -- the manager
reads the same lock; it does not absorb the freeze. Correct per the architecture.

### Phase 2 (next, behaviour-CHANGING) unchanged
Boundary/unlock events + QUEUE first-class (CA scatter, ExpMgr :110 shadow-state removal); OPEN
rulings (transpose->LIVE, direction->LATCH, owner); per-expander finish; lock-scope menu.

## Phase 2 progress (this session) + next precise step

### Done this session (phase 2)
- KEYSTONE: boundary/unlock detection moved from ExpMgr shadow state (caV2PrevStep_/
  caV2PrevLocked_, removed) into LockManager::tick()/queueFires(). Monsoon ticks it before
  expander sync; sync() takes caQueueFires. Behaviour-equivalent. QUEUE now first-class.
- Transpose -> LIVE: already correct in code (applied at genPitchLive output time,
  PatternEngine.cpp:131, downstream of the redraw freeze -- changes transpose the frozen pattern
  live under lock). No gate to invert; added Control::Transpose entry for model completeness.

### NEXT precise step: Direction -> LATCH (GENUINE behaviour change, needs a gate)
Traced the commit point. Direction uses a pending->committed mechanism:
  SequencerEngine.cpp:170  laneDir_[l] = laneDirPending_[l];  (+ laneDirV_ per voice :171)
gated by laneFlipQuant (StepEdge) -- commits at step edge, NOT gated by lock. So a direction change
made UNDER LOCK currently still commits at the next step edge. That is NOT latch behaviour.
To make direction LATCH: gate the pending->committed transition on !locked (don't promote pending
direction while locked; it commits at unlock). This is the FIRST phase-2 item that is a real
behaviour change with a code edit (transpose + note/octave sliders were already-correct no-ops).
Care: interacts with laneFlipQuant timing (StepEdge/other quant modes) -- gate the promotion, not
the quant logic. Owner (twin, LATCH) likely has an analogous pending/commit or direct-store path --
check whether owner writes are already frozen (store-backed, may already latch via the freeze) or
need the same gating.

### Remaining phase 2 after direction/owner
- Owner -> LATCH (check its commit path; twin with direction).
- Expander threading: Causeway/Junction/Interchange/Shophouse -> their LATCH categories; Changi
  LIVE. Most already flow through spread/big-5 paths already migrated or engine-freeze-enforced.
- CA scatter ARM path explicit through QUEUE; Raffles dice-gates QUEUE.
- Lock-scope menu (§7).
