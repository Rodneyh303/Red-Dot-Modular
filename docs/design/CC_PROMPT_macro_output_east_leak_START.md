# CC PROMPT — fix BACKWARDS delegation: East leaks into Macro's OUTPUT (bars already fixed, output not)

CONTEXT: Rodney's repro — lane DELEGATED TO EAST (same on V1 and V2):
  - Move SPREAD on the East-delegated lane → East bars move (correct, East owns it) AND Macro's
    probability OUTPUT (scope) changes (WRONG) — but Macro's probability BARS do NOT change.
So East's modulation is LEAKING BACKWARDS into Macro's OUTPUT. Delegation is Macro→East ONLY —
"Macro never borrows East" (the design intent, stated in code). Work on lane-expander-refactor.

## ROOT CAUSE (pinned in code — confirmed, with the code's OWN comment documenting it)
The bug is ALREADY DIAGNOSED and HALF-FIXED in the tree:
- StraitsSandsMacroVisual.cpp:721-723 comment: "show MACRO's OWN probability — the shared base draw
  with Macro's OWN spread — NOT the shared finalRandomByStrand (which East's spread writes on an
  East-owned lane, leaking East's modulation into Macro's display). One-way borrowing: Macro never
  borrows East."
- So the BARS were FIXED: they read macroOwnProbability(...) (StraitsSandsMacroVisual.cpp:534) — Macro's
  OWN spread, NOT the shared array. That's why Rodney's BARS correctly do NOT move on an East edit.
- BUT the OUTPUT was NOT fixed: the Macro probability CV OUT path reads the SHARED
  finalRandomByStrand (SequencerEngine.hpp:731: pe.finalRandomByStrand(strand, masterLaneStep(polyLane))),
  which returns warpRow(0,...)=random_[0] — and East's spread WRITES random_[0] on an East-owned lane.
  So East's modulation reaches Macro's OUTPUT.
=> Rodney's TWO symptoms are ONE root cause: the one-way-borrowing fix was applied to the DISPLAY path
   (macroOwnProbability) but NOT the OUTPUT path (still the leaky finalRandomByStrand). Bars = Macro-own
   (fixed), Output = shared-East-contaminated (not fixed) → they disagree, and the output leaks East.

## THE FIX — route Macro's OUTPUT through the SAME Macro-own value the bars use
Macro's probability OUTPUT must read Macro's OWN value (the macroOwnProbability equivalent: Macro's
base draw + Macro's OWN spread, ignoring East's write to the shared array), NOT the shared
finalRandomByStrand that East writes. Same "one-way borrowing" the bars already enforce — now on the
output.
- macroOwnProbability currently lives in the VISUAL class (StraitsSandsMacroVisual.cpp:534) — so this is
  NOT a literal reuse. Factor the Macro-own resolution into the ENGINE (where the output is produced) so
  BOTH the bars and the output read it, OR give the output path an engine-side equivalent. The principle:
  ONE Macro-own accessor, both the bars and the output read it — so they can never diverge AND neither
  borrows East. (This is the same "one effective value, both consumers read it" discipline as every other
  bars-vs-output fix this session.)
- VERIFY which output this is: SequencerEngine.hpp:731 feeds the prob-out via finalRandomByStrand; confirm
  that is the Macro-panel probability CV out Rodney scopes (vs the mono/standalone out). The leaky read is
  finalRandomByStrand → warpRow(0) → random_[0], which East writes on an East-owned lane. The fix target
  is: Macro's out reads Macro-own, not random_[0]-as-written-by-East.

## WHY THIS IS NOT THE EARLIER "voice mismatch" PROMPT
The earlier bars/scope prompt (CC_PROMPT_warp_display_scope) was about a VOICE mismatch
(bars=displayed poly voice vs out=hardcoded V1) under WARP edits. THIS is about OWNERSHIP leak under
SPREAD edits on an East-DELEGATED lane — East's spread writing the shared array that Macro's output
reads. Related (both are "Macro output reads the wrong source") but distinct cause: here the wrong
source is East-contaminated-shared, not a wrong voice. Fixing the output to read Macro-own addresses
BOTH the voice and the ownership leak if macroOwnProbability resolves the correct (voice, Macro-own,
delegation) — confirm it does.

## PRESERVE: bars stay CONTINUOUS (sight-leads-sound)
Do NOT flatten the bars or make bars==output by value — the bars are continuous/live, the output is
latched per-step (intended). The fix is about the SOURCE (Macro-own vs East-shared), not the timing.
Bars already read Macro-own continuously; make the OUTPUT read Macro-own (latched). Same source, timing
differs — correct.

## CONTRACT TEST
- Lane delegated to EAST: editing East's spread/warp on that lane changes EAST's bars AND East's output,
  but changes NEITHER Macro's bars NOR Macro's output. (Macro never borrows East.)
- Lane delegated to MACRO: editing Macro changes Macro's bars AND output AND the delegating voices'
  output; East's own-slot edit on that lane changes nothing (Macro overrides).
- Assert at the SOURCE level (who-responds), not bars==output value (the latch makes them differ).
RED on current HEAD (East spread changes Macro output), GREEN when Macro's output reads Macro-own.

## DONE =
- Macro's probability OUTPUT reads Macro-OWN (base + Macro spread, one-way), NOT the East-written shared
  finalRandomByStrand. One Macro-own accessor shared by bars + output (they agree, neither borrows East).
- Bars stay continuous; output stays latched; source aligned.
- Contract test: East-delegated → East edits don't touch Macro bars OR output; Macro-delegated → Macro
  drives, East inert. RED→GREEN.
- bash test/run_all.sh green. PR: "needs Rack re-test — East-delegated lane: move East spread → Macro
  bars AND scope both unchanged; Macro knob on a Macro-owned lane → Macro bars + scope both move."

## NOT THIS PROMPT
- Ownership polarity / voiceSlot derivation (verified clean).
- The perf rebuild fix, resolver/seam, MO tilt (separate).
