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
