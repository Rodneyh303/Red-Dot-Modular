# CC PROMPT — UI-to-resolver STILL broken: TWO precise bugs (base-leak + undeleted old path). Claim≠code AGAIN.

CONTEXT: feat/ui-to-resolver, 2nd "done" claim. Rack: CPU DOUBLED AGAIN (Monsoon 0.7→2.0, now = Macro
2.0; East 0.4), East STILL modulates Macro warp. A correct migration makes CPU DROP (one resolve, not
two) — CPU going UP proves the old path still runs. Verified against the branch: TWO specific bugs, both
with comments FALSELY claiming they're fixed. Do NOT trust comments/commit messages — verify the code.

## BUG 1 — the MacroOwn lens's BASE is still East-contaminated (the persistent leak)
effectiveProbability (SequencerEngine.hpp:791) with ProbLens::MacroOwn:
  - line 804-805: swaps the SPREAD AMOUNT to macroOwnSpread[l] — CORRECT (Macro's own spread coeff).
  - line 807: base = SpreadInterp::polySlewedPub(pe, l, v, ...) — the BASE DRAW. *** THIS is the shared
    PUBLISHED slew path that EAST's spread WRITES. *** So the base is East-contaminated even though the
    spread coefficient is Macro's. => East leaks into Macro through the BASE, not the coefficient.
THE FIX: for the MacroOwn lens, the BASE must ALSO be Macro-own — NOT the shared polySlewedPub that East
writes. A true Macro-own value = Macro's own base draw + Macro's own spread, with NO read of the
East-written shared array. Trace what "Macro's own base" is (the pre-East-write authored draw / the
Macro-owned published value) and use it for the MacroOwn lens's base. The lens must be Macro-own ALL THE
WAY DOWN (base AND spread AND warp), not Macro-spread-over-East-base. (This is why both prior attempts
missed it — each fixed the layer it looked at; the leak is one layer beneath, in the BASE.)
VERIFY: with the fix, editing East spread on a MACRO-OWNED lane changes NEITHER masterLaneProbability's
return NOR the bars. Assert in the test (below).

## BUG 2 — the old path was NOT deleted (the CPU doubling)
Despite the comment ":823 macroOwnLaneProbability DELETED ... the third computation dies":
  - macroOwnLaneProbability STILL has 3 refs in SequencerEngine.hpp.
  - the /32 throttle + SweepSig gate STILL have 5 refs in StraitsSandsMacroVisual.cpp.
So the OLD computation STILL RUNS, alongside the NEW effectiveProbability that masterLaneProbability now
calls → TWO full resolves → CPU UP (0.7→2.0), not down.
THE FIX: ACTUALLY delete macroOwnLaneProbability (every ref) and the /32 throttle + SweepSig gate. Then:
  grep -c macroOwnLaneProbability src/dsp/engines/SequencerEngine.hpp   → MUST be 0
  grep -cE "/32|SweepSig" src/StraitsSandsMacroVisual.cpp               → MUST be 0
If a ref remains, that CONSUMER still uses the old path — migrate it to effectiveProbability, THEN
delete. Do NOT leave the old path "just in case" — its presence IS the CPU bug.

## WHY THIS KEEPS HAPPENING (process note — read it)
Both prior attempts wrote COMMENTS asserting the fix ("DELETED", "reads the ONE accessor") that the CODE
does not do. The verification that catches it is: (a) grep the ref COUNT (not the comment), (b) read the
BASE of the resolve (not just the coefficient), (c) the CPU MUST DROP. A green test + a "done" claim mean
NOTHING if CPU went up and the leak persists. This prompt's DONE section is the only acceptance.

## CONTRACT TEST (must drive the REAL path AND the base)
- Drive masterLaneProbability (the real prob-out entry). Edit East spread on a MACRO-OWNED lane →
  masterLaneProbability's return UNCHANGED (the base+spread are Macro-own). RED on current branch
  (base is East-contaminated) → GREEN when the base is Macro-own.
- ONE-PRODUCER: no path computes the prob-out except effectiveProbability (grep: 0 macroOwnLaneProbability).
- The test must read the BASE too — assert the MacroOwn value is independent of the East-written shared
  array (perturb the shared array, MacroOwn unchanged).

## DONE (the ONLY acceptance — all must hold):
1. grep -c macroOwnLaneProbability = 0; grep -cE "/32|SweepSig" (StraitsSandsMacroVisual) = 0.
2. effectiveProbability(MacroOwn) base does NOT read the East-written shared slew — Macro-own all the way
   down (base + spread + warp). Perturbing the East-written array leaves MacroOwn unchanged (test).
3. masterLaneProbability drives effectiveProbability; test edits East on a Macro-owned lane → output
   unchanged; RED→GREEN.
4. bash test/run_all.sh green.
5. Rack: CPU DROPS (Monsoon + Macro back toward ~0.7/0.4, NOT 2.0) — the one-resolve proof; AND East
   spread on a Macro-owned lane does NOT move Macro's warp output.
If CPU does not drop, Bug 2 is not fixed. If East still leaks, Bug 1 is not fixed. Both measurable in Rack.

## NOT THIS PROMPT
- Ownership polarity / voiceSlot (clean). The rate manager (later).
