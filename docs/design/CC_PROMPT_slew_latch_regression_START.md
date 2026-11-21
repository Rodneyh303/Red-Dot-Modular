# CC PROMPT — FIX: slew no longer affects probability at phrase boundary (ABMix-lock coupling regression)

CONTEXT: Rodney — moving the slew knob (macro OR east, rhythm/melody/qmix) no longer changes the drawn
PROBABILITIES at the next phrase boundary. On BOTH lane-expander-refactor AND the strong-types branch →
it is a PRE-EXISTING TRUNK bug, NOT the strong-types work (those were behaviour-identical). Slew
reshapes the EXISTING buffered draws via the MA copula (no new draw needed) — so this is about the
effective-value RE-DERIVE at the roll, not drawing.

## ROOT CAUSE (pinned — and the code's OWN comment is the spec being violated)
rhythmSlewLatched/melodySlewLatched/qmixSlewLatched are latched ONLY inside PatternEngine::latchMix
(PatternEngine.cpp:857/861/865). latchMix is called ONLY when (abR||abM||abQ)
(MonsoonModeController.cpp:178-188), where abR/abM/abQ = the ABMix LOCK liveness
(LockManager::liveNow(Control::ABMix, ...)). So SLEW LATCHING IS WRONGLY GATED ON THE ABMix LOCK:
when ABMix is locked (not live), latchMix is never called → SlewLatched never updates → at the
phrase-boundary roll the re-derived slewed buffers use STALE slew → probability doesn't change.
THE SPEC IT VIOLATES (MonsoonSandsManager.cpp:987-995, verbatim): "SLEW IS INTENTIONALLY EXCLUDED
from this check... latchMix latches it EVERY CONTROL-RATE CYCLE, but the slewed buffers are only
re-derived at the ROLL (phrase boundary)... The counter advance at the roll is the correct trigger."
=> Slew is SUPPOSED to latch UNCONDITIONALLY every control cycle; the ABMix-liveness gate (added by the
LOCK_SCOPE_MENU per-stream A/B work) broke that by making the whole latchMix call conditional.
Slew has NO own Lock Control (Control enum = Spread/ABMix/... no Slew) → it is meant to be
unconditionally latched.

## THE FIX — decouple slew latching from the ABMix gate
Slew must latch EVERY control cycle regardless of abR/abM/abQ. The MIX latching (rhythmMixLatched etc.)
correctly stays ABMix-gated — only SLEW must come out of the gate. Cleanest: SPLIT the latch —
  - latchMix(mix values, applyR/M/Q)  → latches ONLY the *Mix* fields, still gated on abR/abM/abQ.
  - latchSlew(slew values)            → latches rhythm/melody/qmixSlewLatched UNCONDITIONALLY, called
    EVERY control cycle (not inside the if(abR||abM||abQ)).
At ModeController ~178: call latchSlew(...) ALWAYS; call latchMix(...) only if (abR||abM||abQ).
(Per-stream note: if a stream's A/B is frozen, its MIX holds (correct) but its SLEW still latches — slew
is NOT an A/B-mix property, it's an independent distributional param. If you want slew ALSO freezable
per-stream later, that needs its OWN lock control — out of scope; today it's unlocked = always latches.)

## VERIFY THE RE-DERIVE STILL FIRES AT THE ROLL
After slew latches, the slewed buffers re-derive at the phrase-boundary ROLL (redrawRhythm/Melody/Qmix
advance the counter → recomputeEffective* reads the new SlewLatched via slewKnobToR → new r → ZCache
miss → recompute). Confirm that path is intact (it is, on trunk) — the ONLY break is the latch not
updating. Do NOT add a mid-phrase slew recompute (the spec comment warns that fires at control rate =
CPU spike; the roll is the correct trigger).

## CONTRACT TEST (red-first)
- With ABMix LOCKED (not live): move slew → at the next roll, the effective probability for that
  stream CHANGES (SlewLatched updated, buffers re-derived). RED on current (SlewLatched stale under
  ABMix lock) → GREEN after decoupling.
- With ABMix live: slew still works (unchanged).
- Mix latching still respects the ABMix lock (frozen A/B mix stays frozen) — no regression there.
- macro AND east both (the symptom was both).

## DONE =
- latchSlew split out, called unconditionally every control cycle; latchMix (mix only) stays ABMix-gated.
- Slew change → probability change at the roll, WHETHER OR NOT ABMix is locked, macro + east.
- Contract test red-first→green; bash test/run_all.sh green; CPU unchanged (no mid-phrase recompute added).
- PR: "needs Rack: move slew with ABMix locked → probabilities change at next phrase boundary."

## NOT THIS PROMPT
- Mid-phrase slew re-derive (intentionally excluded — CPU; roll is the trigger).
- Giving slew its own lock control (future, if per-stream slew freeze is wanted).
- The strong-types audit findings (OutputGenerator off-by-one, Lantern stale comment) — separate.
