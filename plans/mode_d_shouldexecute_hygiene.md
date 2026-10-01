# "Dead modes" — resolved (stale test patch) + two genuine follow-ups

## Resolution
The "A/B/C/D no gate output" symptom was a BAD TEST PATCH (module in a no-gate state), NOT a code
bug. A fresh patch works on master and at the pre-merge commit 22699aa — so a205093, the subgate
merge, and the engine are all fine for gate output. (A separate stale-INCREMENTAL-BUILD episode the
same evening was also environment, not source.)

## Process lesson (the actual takeaway)
On "everything is dead", rule out ENVIRONMENT FIRST, in this order, before suspecting code:
1. fresh patch + fresh module from the browser, clock patched, run active;
2. `make clean` (not incremental);
3. fully quit & reopen Rack so it reloads the .so.

## Worth adding: per-mode gate smoke test
A test that each of modes A..F emits a gate with a clock/gate running. Cheap, and it would have
distinguished "code broke" vs "patch/build broke" in seconds. Register in run_all.sh.

## OBSERVATION (not a prescription) — Mode D shouldExecute
While hunting the (non-)bug, noticed Mode D's `shouldExecute` branch (src/Monsoon.cpp ~811) reads:
```
} else if (modeSelect == 3) {
    const bool useSubGate = input.subGateConnected;
    shouldExecute = useSubGate ? input.subGateRise : true;   // comment says "ratchet on Gate 3"
}
```
It differs from Mode B's branch, and the comment references the pre-a205093 Gate-3 topology.
**UNKNOWN whether this is intentional or a leftover.** The `: true` (execute every sample when no
subgate is patched) may be deliberate for Mode D — do NOT assume it should match Mode B. If anyone
touches this area: first determine the INTENDED behaviour (and fix the stale comment either way);
only collapse the 1/3 branches into one twin path if verification shows D is meant to behave exactly
like B. No evidence this currently misbehaves. Not urgent.
