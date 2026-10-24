# CC PROMPT — RESTORE the SweepSig fingerprint gate (the thing that gave Macro 0.4%; 70dd837 deleted it)

CONTEXT: Rodney BISECTED: earlier today Monsoon ~1% / Macro ~0.4% (his words: "This Landed well.
mosoon back to about 1%. and macro back to about .4%"). Now Macro-only 3%, East-attached 1%. The
regression is 70dd837 ("step-cache ... DELETE the /32 throttle + the SweepSig gate"). The 0.4% came
from the SWEEPSIG FINGERPRINT GATE — a memcmp shadow of every resolve input that SKIPPED the resolve
entirely when NOTHING changed. The deleted comment states the cost without it: "≈ 22.7 µs/tick ≈ 0.7
µs/sample ≈ 3.4% ... even when NOTHING changed". A /32 throttle ALONE is insufficient (Claude tried it,
e0981d5 reverted) — /32 reduces frequency but still resolves UNCONDITIONALLY every 32 ticks = the 3.4%.
The SKIP-ON-UNCHANGED gate is the thing. Restore it, now wrapping effectiveProbability.

## WHY EAST-SENSITIVE (the 3% vs 1%): with the gate gone, the resolve runs every tick regardless;
East-attached vs Macro-only differs in how often the step-cache's OWN per-sample scaffolding
(calcPlayhead×7 + the gate at :879) plus the unconditional resolve fire. The SweepSig gate made it
input-driven (idle ⇒ ~0 resolves) and thus config-robust. Restoring it removes the asymmetry.

## THE EXACT DELETED MECHANISM (from `git show 70dd837 -- src/StraitsSandsMacroVisual.*`)
HEADER — the SweepSig struct + probOutDiv (restore both, ADD the atom/tilt fields which the FEEL-TEST
added since — see note):
    rack::dsp::ClockDivider probOutDiv;   // /32 control-rate throttle (probOutDiv.setDivision(32) in ctor)
    struct SweepSig {
        bool valid=false; int nV=-1; int scaleIdx=-1; int sh=-1;
        uint8_t step[7]={};                    // per-lane masked playhead
        float pub[7][16]={};                   // pubSlewed cells read: [lane][voiceSlot 0..nV]
        const void* bank=nullptr;              // pe.warpBank_
        bool deleg[16][7]={};                  // pe.warpDelegated_ rows 0..nV
        float warpW[112]={}; dotModular::WarpF warpF[112]={};
        float warpTilt[112]={}, warpAtomLevel[112]={}, warpAtomMass[112]={};   // FEEL-TEST stores
        float globalWarpW[7]={}; dotModular::WarpF globalWarpF[7]={};
        float globalWarpTilt[7]={}, globalWarpAtomLevel[7]={}, globalWarpAtomMass[7]={};
    } sweepSig_;
GATE (CPP, before the resolve): recompute = probOutDiv.process(); then run = !valid || nV change ||
scale change || sh change || memcmp(step) || memcmp(macroOwnSpread) || (per-lane) memcmp(pub cells) ||
bank ptr change || memcmp(deleg) || memcmp(warpW/F/Tilt/AtomLevel/AtomMass own-slot prefix (nV+1)*7) ||
memcmp(global* full). ONLY if `run` → do the 7-lane resolve + write sweepSig_ with the new fingerprint.
Else → skip (ports hold). "Steady state: stopped+idle ≈ 0 resolves; playing ≈ step-rate."

## THE ADAPTATION (the one real task — don't blind-revert)
70dd837 ALSO correctly migrated the resolve to effectiveProbability (the ONE accessor) and deleted
macroOwnLaneProbability — KEEP THAT (later commits 7b1f191/807d7c6 depend on it). So: wrap the CURRENT
resolve (the step-cache's `resolver.laneProbabilityAtStep(ch+1, l, step)` loop, StraitsSandsMacroVisual
.cpp ~890) in the RESTORED SweepSig gate. The fingerprint must cover EXACTLY the inputs the CURRENT
accessor reads — verify against effectiveProbability/laneProbabilityAtStep's actual reads (pub cells,
macroOwnSpread, warpBank_ + warpDelegated_ + the bank warp/tilt/atom stores). If the accessor reads
something the old SweepSig didn't fingerprint (or vice versa), ADJUST the fingerprint to match the
current reads — an input not fingerprinted = stale output (wrong); a fingerprinted input the resolve
doesn't read = harmless but wasteful. Err toward covering all current reads.

## DO NOT
- /32 throttle ALONE (proven insufficient, e0981d5).
- Blind `git revert 70dd837` (breaks the accessor migration 7b1f191/807d7c6 depend on).
- Change effectiveProbability's correctness — only GATE whether it runs.

## CONTRACT / VERIFY
- MONSOON_PROFILE "sweep" bracket: idle ⇒ ≈0 resolves; Macro-only == East+Macro (the gate is input-
  driven, config-independent). Build -DMONSOON_PROFILE, read both configs.
- Rack: Macro-only CPU back to ~0.4-1% (== the known-good earlier today); NOT 3%.
- Prob-out values bit-identical to ungated (the gate only SKIPS unchanged recomputes — same values).
  Add/keep a test: perturb each fingerprinted input → resolve re-runs; perturb nothing → it skips.
- run_all.sh green.

## DONE =
- SweepSig gate restored (struct + /32 probOutDiv + the memcmp run-gate), wrapping effectiveProbability.
- Fingerprint covers EXACTLY the current accessor's reads (verified, incl. atom/tilt stores).
- MONSOON_PROFILE: idle≈0 resolves, Macro-only==East. Rack: ~0.4-1% both configs.
- Bit-identical outputs; skip-on-unchanged test; run_all.sh green.
- PR: measured sweep ns both configs before/after + Rack %.
