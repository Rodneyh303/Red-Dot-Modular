# CC PROMPT — FIX: Monsoon sync() re-populates the whole Macro per-voice grid every cycle when East absent (the 3%)

CONTEXT: MEASURED with MONSOON_PROFILE (Rodney, both configs). The Macro-only 3% vs East+Macro 1% is
ENTIRELY the "sync" section: controlDiv sync = 515-578 ns/sample Macro-only vs 33-34 ns/sample East+Macro
(~15x). Everything else is identical between configs. sync = lockManager.tick + expanderManager.sync
(Monsoon.cpp:1448); lockManager.tick is trivial → it's expanderManager.sync. (pairChecked rafExp = 1-2ns
→ INNOCENT, do not touch it.)

## ROOT CAUSE (pinned to the block)
MonsoonExpanderManager::sync, the MACRO-drives-poly population block (~MonsoonExpanderManager.cpp:782):
  auto* mmV = findMonsoonEitherSide(macroVis);
  for (v=1..effPolyVoices)              // up to 15 voices
    for (lane=0..PL_LANES)              // 7 lanes
       polyLenERef/polyOffERef/polyRotERef = round(macroBase...)   // LOR population
    ... per-lane spread taps (getMacroTap × 7 lanes) ...           // spread population
This runs EVERY CONTROL CYCLE, UNCONDITIONALLY, whenever Macro is present. When East is ABSENT, Macro
owns the whole grid → Macro populates all 15×7 cells (+ spread taps) every cycle = the ~500ns. When East
is present, East handles/owns part of the per-voice population so Macro's unconditional block is smaller
or skipped → 33ns. So East-absence isn't a scan bug — it's Macro re-doing the FULL per-voice LOR+spread
population every cycle with NO change-detection. It recomputes identical values ~1500×/sec.

## THE FIX — gate the Macro population on change (skip-on-unchanged), like the SweepSig gate
The populated values (polyLen/Off/Rot + spread taps) depend only on: macroBase[lane][0..2] (the LOR
knobs/CV), effPolyVoices, the spread inputs (macroVis spread params/CV the taps read), and ownership
(macro/east presence). These change RARELY (knob/CV moves, patch changes) — not every cycle. So:
- FINGERPRINT those inputs (a small shadow: macroBase[7][3], effPolyVoices, the spread params the taps
  read, the presence/ownership flags). memcmp each control cycle; if UNCHANGED, SKIP the whole
  population block (the engine arrays already hold the correct values from last time).
- Only when the fingerprint changes → re-run the 15×7 population + taps, then update the shadow.
This is the SAME pattern as the restored SweepSig gate (Macro prob-out). Steady state (knobs still) →
~0 population work → Macro-only drops to ~East+Macro.
VERIFY the populated values are IDENTICAL to ungated (the gate only skips UNCHANGED recomputes — same
engine arrays). A CV that moves every sample (e.g. an LFO into a LOR CV) will legitimately re-populate
each cycle — that's correct (the input really changed); the win is the COMMON idle/knob-static case.

## ALTERNATIVELY / ALSO: this is squarely RATE-MANAGER territory
This block is control-rate work that should be EVENT-DRIVEN (re-run on input change), not every-cycle.
It's exactly what the rate manager's declareProducer(at-rate, on-change) would own. If the rate manager
is imminent, this could be its FIRST migration (the populate = a Step/Control producer, gated on its
input fingerprint). Either land the standalone fingerprint gate now (fast, unblocks the 3%), OR fold it
into the rate-manager work as the first concrete producer. Rodney's call on sequencing. Standalone gate
is the quick win; rate-manager fold-in is the structural home.

## CONTRACT / VERIFY
- MONSOON_PROFILE "sync" ns/sample: Macro-only == East+Macro (within noise), idle ≈ few ns.
- Rack: Monsoon ~1% in BOTH configs; Macro-only no longer 3%.
- Populated engine arrays (polyLen/Off/Rot, spread-applied) BIT-IDENTICAL to ungated — a moving LOR CV
  still re-populates (test: perturb macroBase → re-runs; static → skips).
- promote/ownership behaviour unchanged. run_all.sh green. MONSOON_PROFILE OFF for release.

## DONE =
- The Macro per-voice population block in expanderManager.sync gated on an input fingerprint (skip when
  unchanged). sync Macro-only == East+Macro in the profile; Rack both ~1%.
- Values bit-identical; skip-on-unchanged test; run_all.sh green.
- PR: sync ns before/after both configs + Rack %.

## NOT THIS PROMPT
- pairChecked rafExp (1-2ns, innocent). The SweepSig prob-out gate (done, a94a8bf — different section).
- Slew (separate). Strong-types audit findings.
