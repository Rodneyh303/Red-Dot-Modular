# CC FIX  Macro lanes fold over the global length: the lane SEED (t0) law, and consolidate East/Macro seeding

Build on fix/macro-voicelane-shared-frame. Authority: this doc; LOR_EDITING_BUG_LIST.md.

## THE BUG (located)
Macro lanes cycle over the first min(16, global length) steps in BOTH cycle and absolute mode, instead of over their own
LOR window like East. Cause is NOT the length argument (mlen is already the Macro lane's own LOR: macroLOR_[l],
SequencerEngine.cpp:1019). It is the SEED t0 passed to laneRecomp:
- EAST mono (SequencerEngine.cpp:1004-1005): monoT0 branches on voiceLorIdentity  identity uses the raw/ bounded phase,
  MAPPED uses frameT0[0] (the bounded frame value that already respects the window).
- EAST poly (:1013-1016): voiceT0 same law, per voice, with frameT0[v].
- MACRO (:1020): macroT0 = passG ? macroLanePhase_[l] : (wG + offG)  NO identity/mapped branch; the non-pass arm seeds
  from the GLOBAL walk wG+offG, so laneRecomp reduces it modulo the lane length and the lane folds over the global pattern.

The three call sites all call the SAME core (laneRecomp) but each computes len + t0 INLINE, and Macro's inline t0 has
drifted from East's. That drift is this bug (and the class of bugs 17-21).

## FIX PART 1  one shared SEED law
Extract the len + t0 computation into ONE helper used by all three loops:
  struct LaneSeed { int len; int64_t t0; };
  LaneSeed laneSeed(window, l, v, passG, wG, offG, lenG, frameT0, frameStep, lanePhase*, advanceOffset_) const;
- window selects the source: MONO (strandLen + lanePhase_), POLY v (polyStrandLenOf(v) + lanePhaseV_[v]),
  MACRO (macroLOR_[l] or strandLen for VAR/LEG + macroLanePhase_).
- The IDENTITY vs MAPPED branch (voiceLorIdentity) is INSIDE the helper and identical for all three: mapped uses the
  bounded frame value (frameT0), identity uses the pass-through/bounded phase. Macro gets the SAME branch it lacked.
- Each loop then: auto s = laneSeed(...); laneRecomp(dir, s.len, s.t0, frameStep, tick, sign);
- laneRecomp itself is UNCHANGED (already the shared core).

## FIX PART 2  the Macro mapped seed
With the helper, Macro's mapped case uses the Macro frame value (the bounded walk under the Macro window), not wG+offG.
Macro has ONE global window, so its "frame value" is the macro frame (macroFrameRead_/macroLanePhase_ bounded by
macroLOR_), computed once, shared by all lanes. Confirm macroLanePhase_ is the bounded Macro walk; if it is already the
bounded value, the fix is to USE it in both arms (identity and mapped) via the helper, not the raw wG+offG.

## CONSOLIDATION (the structural point, Rodney)
Do NOT merge the East and Macro VISUAL classes  East is per-voice/opt-in, Macro is global; the UIs differ. Consolidate
the ENGINE SEED computation only: the one laneSeed helper is the single place the window->seed law lives, so East mono,
East poly and Macro cannot drift again. This is the DRY fix for the whole 17-21 class.

## GATES
- MACRO LANE WINDOW: a Macro lane with len=7, off=0 under a 16-step global pattern cycles over 7 steps (its window), NOT
  16. Cycle AND absolute mode. V1..V16 identical (Macro is global).
- EAST UNCHANGED: East mono and poly lane ticks bit-identical to before (the helper reproduces the existing inline law).
- SAME WINDOW -> SAME TICK: an East voice and a Macro lane with the same window and direction produce the same laneTick
  for every tick (a direct pin  the drift-proof test).
- Non-divisive polymeter PRESERVED: the UA_NONDIV goldens unchanged (the identity pass-through arm is in the helper,
  unchanged).
- run_all green on both SDK legs; array-guard and anchor-audit clean.

## DO NOT
- Merge the visual classes.
- Change laneRecomp.
- Change the non-divisive pass-through (polymeter) law.
- Special-case any voice or lane index.
