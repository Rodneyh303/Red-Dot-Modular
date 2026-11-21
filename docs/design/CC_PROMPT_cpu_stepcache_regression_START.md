# CC PROMPT — CPU regression BISECTED to 70dd837 (/32 throttle deleted → step-cache gates worse)

CONTEXT: Rodney — performance was RESTORED earlier today, BROKEN now. Monsoon ~1% with East attached,
~3% Macro-only (East-absent = worse). BISECT: among today's CODE commits, the CPU-relevant one is
70dd837 "step-cache the prob-out resolve; DELETE the /32 throttle + SweepSig gate" (UI-to-resolver step
3). The /32 throttle is EXACTLY what had tamed this sweep (its own deleted comment: "per-sample
recompute re-derived a constant ~32× per control block; /32 = control rate; a step lasts orders of
magnitude longer than /32"). 70dd837 removed it for a step-cache that gates WORSE. (The slew commits
6ea010c/b543a0a are last + trivial — 3 float assigns at control rate — NOT the cause; do NOT revert
them. The pairChecked rack-scan PRE-dates today — also not today's regression.)

## THE REGRESSION (pinned)
StraitsSandsMacroVisual.cpp:879 — the step-cache recompute gate:
  if (!row.valid || step != row.step || nV != row.nVoices) { ...full 7-lane effectiveProbability
  resolve + prob-CV writes... }
Two problems vs the old /32 throttle:
1. HEAVIER PER FIRE: effectiveProbability (the new ONE accessor) is a FULLER resolve than the old
   throttled lightweight path. Even at step-rate, 7 lanes × (1+nV) channels × the full resolver each
   step can exceed what the /32-throttled old path cost. The step-cache fires LESS OFTEN but does MORE
   per fire — net worse.
2. EAST-SENSITIVE GATE HOLE: the gate also fires on `nV != row.nVoices` (voice-count change) and
   `!row.valid`. If East-absence makes nV or valid FLICKER per cycle (or row.valid is reset each cycle
   when East's params are absent), the "step-cache" recomputes EVERY control cycle in Macro-only =
   the 3%. CONFIRM: log how often the recompute branch at :879 fires in Macro-only vs East+Macro (the
   MONSOON_PROFILE "sweep" bracket at :845 already measures this block — read it in both configs).

## THE FIX — two options, measure then choose
MEASURE FIRST: build -DMONSOON_PROFILE, read the "sweep" bracket ns/sample in BOTH configs. If sweep
dominates in Macro-only and fires far more than once-per-step → the gate hole (problem 2). If it fires
once-per-step but is just expensive → problem 1.

OPTION A (pragmatic, if the step-cache can't be made to gate tightly): RESTORE the /32 throttle ON TOP
of the step-cache — recompute only when (step changed OR nV changed) AND the /32 divider fires, OR keep
the step-cache but ALSO require probOutDiv.process() as the old code did. The /32 demonstrably worked
(0.4% Macro). This is the safe restore-known-good.
OPTION B (correct, if the hole is identifiable): fix the gate so it does NOT fire every cycle in
Macro-only — find why nV/valid flicker when East is absent and make them stable (East-absence should
NOT invalidate the Macro step-cache; Macro's resolve doesn't depend on East for a Macro-owned lane).
The gate should fire ONLY on a real step advance, East presence irrelevant.
EITHER WAY: the fix must make Macro-only CPU == East+Macro CPU (~1%), and NOT per-sample/per-cycle
recompute. Preserve the correctness the step-cache bought (the resolve is still bit-correct) — only its
FIRING FREQUENCY is the bug.

## DO NOT
- Revert the slew commits (6ea010c/b543a0a) — correct + trivial, unrelated to CPU.
- Revert 70dd837 wholesale — it also did the correct /32-SweepSig→accessor migration; keep the
  migration, fix the GATING (restore the throttle or close the hole).
- Add a mid-phrase/ per-sample recompute.

## CONTRACT / VERIFY
- MONSOON_PROFILE "sweep" ns/sample EQUAL (within noise) Macro-only vs East+Macro after fix.
- Rack: Monsoon ~1% in BOTH configs; Macro-only no longer 3%; East-absence no penalty.
- The prob-out values still bit-correct (step-cache correctness preserved); run_all.sh green.
- PR: measured sweep ns before/after in both configs + Rack %.

## NOT THIS PROMPT
- Slew (done, needs Rack re-test). pairChecked rack-scan (separate pre-existing, its own prompt 348c21c).
- strong-types audit findings.
