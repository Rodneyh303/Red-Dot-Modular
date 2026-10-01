# [DOWNGRADED — the "dead modes" symptom was a STALE TEST PATCH, not a code bug]

**Resolution:** the "A/B/C/D no gate output" symptom was a BAD TEST PATCH (module in a no-gate
state), NOT a code regression. A FRESH PATCH works on master and at the pre-merge commit 22699aa.
So a205093, the subgate merge, and the engine are all FINE for gate output. (This evening also had a
separate genuine stale-INCREMENTAL-BUILD episode — different thing; both are environment, not source.)

**Lesson (process):** when "everything is dead" appears, rule out environment FIRST, in this order:
(1) fresh patch + fresh module from the browser, clock patched, run active; (2) `make clean` not
incremental; (3) fully quit & reopen Rack so it reloads the .so. Only after all three, suspect code.

---

## Still worth doing — but as HYGIENE, not a hotfix
The Mode D `shouldExecute` branch (src/Monsoon.cpp ~811) IS genuinely inconsistent with the rest of
a205093 (which moved D to Mode B's topology):

```
} else if (modeSelect == 3) {
    const bool useSubGate = input.subGateConnected;
    shouldExecute = useSubGate ? input.subGateRise : true;   // stale: comment says "ratchet on Gate 3"
}
```
- `: true` makes D call executeMode every sample when no subgate is patched (vs B gating on gate1Rise).
- The comment/logic reference the OLD Gate-3 topology while routing now feeds subGate from Gate 2.

This did NOT cause the dead-modes symptom (that was the patch), and may be benign in practice, but it
IS a divergence from "Mode D is Mode B's twin". **Clean it up by COLLAPSING the `modeSelect == 1` and
`== 3` branches into one** (`if (modeSelect == 1 || modeSelect == 3)` using Mode B's shouldExecute) so
they cannot drift — one code path for the twin relationship. VERIFY first whether D currently
misbehaves (e.g. executes-every-sample causing any audible/stepping difference from B); if it does,
this is a real fix; if not, it is hygiene that prevents a future divergence bug. Not urgent.

## Still worth adding regardless
A per-mode gate SMOKE test (A..F each emit a gate with a clock/gate running) — cheap, and it would
have let us distinguish "code broke" from "patch/build broke" in seconds tonight instead of hours.
