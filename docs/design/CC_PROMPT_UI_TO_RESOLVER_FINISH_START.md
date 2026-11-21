# CC PROMPT — FINISH UI-to-resolver: the prob-OUT was NOT actually rewired (claim ≠ code)

CONTEXT: feat/ui-to-resolver claims "done" but Rack shows the bug PERSISTS (Macro output modulated by
East) AND CPU went UP (0.3%→2.0%, Macro now > Monsoon). Both contradict "done". Verified against the
branch: the migration is PARTIAL — the accessor was added and the BARS rewired, but the actual prob-OUT
was left on the OLD path, and the old path was NOT deleted. Finish it. Work on feat/ui-to-resolver (or
re-branch from it).

## WHAT CC ACTUALLY DID vs CLAIMED (verified)
DONE: effectiveProbability accessor added (SequencerEngine.hpp, "THE ONE effective-probability accessor");
BARS rewired to it (StraitsSandsMacroVisual.cpp — 5 calls, 0 old-path); direction via resolveDirValue;
test_ui_to_resolver added.
NOT DONE (despite commit messages + comments claiming it):
1. THE PROB-OUT STILL USES THE OLD PATH. masterLaneProbability (SequencerEngine.hpp:734) — which feeds
   the Macro prob CV OUT the scope reads — STILL calls `pe.finalRandomByStrand(strand, ...)`, and
   finalRandomByStrand (PatternEngine.hpp:513) STILL returns `warpRow(0, s)` = HARDCODED VOICE 0, which
   does NOT go through the resolver's spread DELEGATION. So East's spread (which writes the shared row)
   STILL leaks into the Macro output. This IS Rodney's "Macro modulated by East" bug — untouched.
   The comment at SequencerEngine.hpp ("the prob-outs ALL read THIS function") is ASPIRATIONAL — the
   wiring was never changed. masterLaneProbability must call effectiveProbability, NOT finalRandomByStrand.
2. macroOwnLaneProbability NOT deleted (still 3 refs in SequencerEngine.hpp) despite the ":810
   macroOwnLaneProbability DELETED" comment. The /32 throttle + SweepSig gate NOT removed (still 5 refs
   in StraitsSandsMacroVisual.cpp) despite commit 70dd837's title. So the OLD computation STILL RUNS
   alongside the new effectiveProbability → TWO resolves → the CPU went UP (0.3→2.0), not down.
3. THE TEST IS GREEN BUT RACK IS BROKEN — same failure mode as the original CC_PROMPT_05 test:
   test_ui_to_resolver checks effectiveProbability DIRECTLY, but the real OUTPUT wiring
   (masterLaneProbability → finalRandomByStrand → warpRow(0)) does NOT call effectiveProbability, so
   the test never exercises the broken path. A green test at the accessor ≠ the output using it.

## THE FIX (finish the migration — make code match the claims)
1. REWIRE THE PROB-OUT: masterLaneProbability (SequencerEngine.hpp:734) → call effectiveProbability for
   the correct (voice, lane, step), NOT finalRandomByStrand. Decide the voice per Rodney's A/B
   (CC_PROMPT_warp_display_scope): the Macro out is the MONO (V1) out → effectiveProbability(V1,...);
   do NOT leave the hardcoded warpRow(0). finalRandomByStrand's warpRow(0) is the voice-0 residue —
   route the out through the resolver's explicit-voice value.
2. ACTUALLY DELETE the old paths now nothing uses them: macroOwnLaneProbability (SequencerEngine.hpp),
   the SpreadInterp-in-UI include if now unused, the /32 throttle + SweepSig gate
   (StraitsSandsMacroVisual.cpp). Confirm with grep: 0 refs to macroOwnLaneProbability, 0 to the /32
   gate, after. (If something still references them, THAT consumer wasn't migrated — migrate it too.)
   CPU MUST drop (one resolve, not two) — verify on the Rack meter.
3. THE CONTRACT TEST MUST EXERCISE THE REAL WIRING, not the accessor in isolation (the 05 lesson):
   test_ui_to_resolver must assert that masterLaneProbability (the actual prob-out entry point) returns
   effectiveProbability's value — i.e. drive the OUTPUT path and assert it equals the one-producer value,
   AND that editing East spread on a Macro-OWNED lane does NOT change the prob-out (the one-way / no-leak
   invariant, checked THROUGH masterLaneProbability, not through effectiveProbability directly). RED on
   the current branch (prob-out still warpRow(0)), GREEN when rewired. If the existing test is green on
   the current broken branch, it is testing the wrong entry point — fix the test to drive the output.

## DONE =
- masterLaneProbability → effectiveProbability (explicit voice, no warpRow(0) hardcode for the out).
- macroOwnLaneProbability + /32 gate + SweepSig + unused SpreadInterp-in-UI DELETED; grep-confirmed 0 refs.
- test_ui_to_resolver drives the REAL output path (masterLaneProbability), asserts one-producer + no
  East→Macro-output leak THROUGH that entry point; RED on current branch → GREEN after.
- bash test/run_all.sh green; Rack CPU-meter: Macro back toward ~0.3% (ONE resolve); Rack feel-test:
  East spread on a Macro-owned lane does NOT move the Macro output.

## NOT THIS PROMPT
- Ownership polarity / voiceSlot derivation (verified clean).
- The rate manager (separate, later).
