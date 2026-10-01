# REAL BUG (confirmed) — Mode D shouldExecute not unified with Mode B (a205093 incomplete)

Rodney was right: serious bug affecting Mode D. Earlier "stale build / false alarm" was WRONG — the
staleness is in the SOURCE, not the build.

## Root cause
`a205093` unified Mode D with Mode B for the ENGINE (`executeModeD` mirrors `executeModeB`, reads
`input.gate1` as main) and the INPUT ROUTING (Gate 2 = ratchet, Gate 3 = ghost) — but **left Mode D's
`shouldExecute` gate on the OLD pre-fix logic**. At `src/Monsoon.cpp` ~811:

```cpp
} else if (modeSelect == 3) {
    const bool useSubGate = input.subGateConnected;
    shouldExecute = useSubGate ? input.subGateRise : true;   // "Mode D continuous, or ratchet on Gate 3"
}
```

Two faults:
1. **`: true`** when no subgate is connected -> Mode D calls executeMode EVERY SAMPLE instead of on a
   gate edge -> step logic runs continuously, jams gate state, no coherent gate output.
2. The comment/logic still reference the OLD topology ("ratchet on Gate 3" / `subGateConnected` as the
   gate-3 ratchet) while the input routing above now populates subGate from GATE 2. Mismatch.

Mode B's `shouldExecute` (the `modeSelect == 1` branch just above) is correct; D's was never updated to
match. That is exactly the B-to-D-span symptom: B works, D broken.

## Fix
Mode D now SHARES Mode B's topology, so its `shouldExecute` must be IDENTICAL to Mode B's:

```cpp
} else if (modeSelect == 3) {
    const bool inGate = gate1High;
    shouldExecute = input.gate1Rise || (gate1High && engine.stepIndex == -1)   // main onset / held-at-start
                  || (inGate && input.subGateRise)                              // ratchet in-gate (Gate 2)
                  || (!inGate && input.ghostRise);                             // ghost in-gap (Gate 3)
}
```

i.e. collapse the `modeSelect == 1` and `== 3` branches into one (`modeSelect == 1 || modeSelect == 3`),
since "Mode D is Mode B's twin" (the §421 mode-agnostic invariant) — only the pitch source differs, and
that is handled downstream in executeModeD (quantise CV2), NOT in the execution gate. Sharing the branch
also PREVENTS this class of bug recurring (one branch, can't drift).

## Why the mode-agnostic test missed it
`test_gate_mode_agnostic` asserts B and D produce identical OUTPUT for the same input — but it likely
drives the engine path directly (executeModeB vs executeModeD), bypassing the module-layer
`shouldExecute` gate where the bug lives. So the engine twins are identical; the MODULE never calls D's
engine correctly. **Extend the test (or add a module-level one) to exercise the shouldExecute gate** —
assert Mode D steps on a Gate-1 edge exactly as Mode B does. Plus the per-mode gate SMOKE test (A..F
each emit a gate) still stands.

## Priority: hotfix before other work.
