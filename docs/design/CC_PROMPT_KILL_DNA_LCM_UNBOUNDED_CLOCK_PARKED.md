# PARKED — kill DNA_LCM: totalStepsElapsed → unbounded int64 net-step counter, local-mod everywhere

STATUS: DESIGNED, PARKED. Do NOT start until unified-addressing lands (this touches its core clock /
driverTickAt). Land it WITH or right AFTER unified-addressing, as its own focused, golden-pinned change.

## THE SMELL (Rodney)
totalStepsElapsed wraps at DNA_LCM = 1,441,440 (= LCM of all lane periods 1..16 incl pendulum/pingpong
factors). Problems: (1) a MAGIC COMPOSITE MODULUS that BALLOONS with config — at 32 steps it becomes
LCM(1..32)+factors ≈ 144 BILLION; (2) its magnitude is MEANINGLESS (not a musical position, just a phase to
mod down); (3) with a SHORTER master length it still counts a phantom 1.4M cycle unrelated to the actual
pattern; (4) it mixes badly with gate-mode gate counts (what does a gate count mod DNA_LCM mean?).

## THE FIX — unbounded net-step counter, continuity handled LOCALLY
totalStepsElapsed becomes an UNBOUNDED int64 that just counts NET STEPS the playhead has moved (±1 per
advance), NEVER wrapped by a magic modulus. int64 at 1k steps/sec wraps in ~300M years — "unbounded" is
safe. Every lane POSITION is already computed via driverTickAt/laneTickFor, which mods by the LANE'S OWN
length LOCALLY — so continuity for any length (11, 13, 32, anything) is preserved BY CONSTRUCTION with NO
global modulus. DNA_LCM is deleted.
WHY it works: the only reason DNA_LCM existed was to keep the number small enough for direct reads/casts.
Make driverTickAt the SOLE path to a position (it mods locally) and the global wrap is unnecessary.

## THE BLOCKERS TO FIX (verified — the only non-driverTickAt uses)
Everything goes through driverTickAt EXCEPT these three — fix them:
1. SequencerEngine.cpp:377-378 — the `% DNA_LCM` on increment. DELETE the mod; just ++/-- the int64.
2. SequencerEngine.cpp:1741, 1797 — pass `(int)totalStepsElapsed` into a legato/variation delegation call.
   An unbounded int64 cast to int OVERFLOWS at 2^31. FIX: confirm the callee mods locally; if it needs a
   bounded value, mod-before-cast by the RELEVANT lane/pattern length there (NOT DNA_LCM), or change the
   signature to take the int64 and mod locally. Must not pass a raw unbounded value into an int.
3. SequencerEngine.cpp:671 — the Mode E phase-jump snap sets totalStepsElapsed = (absStep+1) % DNA_LCM.
   Map to the PATTERN LENGTH (or just set the int64 to the absolute net-step the phase implies), NOT
   DNA_LCM. The snap only needs a value that driverTickAt will mod correctly — any representative net-step
   works.
(Lines 410/412/417/621/623/625/742 already go through driverTickAt — unchanged.)

## GOLDEN PIN (the proof — this must change NOTHING)
Removing the wrap must leave EVERY lane's read position BIT-IDENTICAL. The golden battery is the proof:
run the full battery before/after, assert identical. This is a pure representation change (how the clock is
stored), not a behaviour change — if any digest moves, the refactor is wrong (a direct-read site was
missed). GOLDEN_A/B/BS + UA_* all unchanged.

## PAYOFF
- DNA_LCM GONE. 32 steps "just works" (no 144B constant). Shorter master "just works" (counter = honest
  net-step count). Gate-mode variation phase-offset (CC_PROMPT_STEP_ADVANCE_PHASE_FIX.md) lands in a world
  with no magic modulus — the phase offset is just more net-steps, trivially coherent.
- The counter MEANS something simple: "net steps the playhead has moved, ever." Legible, scalable,
  config-independent.

## DONE =
- DNA_LCM deleted; totalStepsElapsed an unbounded int64 net-step counter (no modulus on ±1).
- The 3 blocker sites fixed (increment mod removed; the 2 int-casts mod-locally-or-take-int64; phase-snap
  to pattern length / net-step). driverTickAt remains the sole position path (grep: no raw totalStepsElapsed
  comparison/cast that assumes bounds).
- Golden battery BIT-IDENTICAL before/after (the proof it's representation-only). run_all + strict green.

## SEQUENCE: with/after unified-addressing (shares driverTickAt / the core clock). NOT concurrent.
