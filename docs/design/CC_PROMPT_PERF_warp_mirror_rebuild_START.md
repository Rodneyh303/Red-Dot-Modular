# CC PROMPT PERF — warp mirror rebuild is FULL-scope in the recompute path (1%→8% regression)

CONTEXT: Rodney's Rack CPU meter: Monsoon went 1% → 7-8% (a ~7x regression) over the warp/resolver
work. Diagnosed to the warp MIRROR rebuild. Work on lane-expander-refactor.

## THE BUG (diagnosed to specific lines — verify, don't guess)
The per-lane recompute functions each call rebuildWarpMirrorALL() — a FULL 7-lane × 16-voice × 16-step
(1792-cell) rebuild — when they only rewrote ONE lane-group's random_:
  - PatternEngine.cpp:474  recomputeEffectiveMelody  → rebuildWarpMirrorAll()  (only MEL/OCT changed)
  - PatternEngine.cpp:558  recomputeEffectiveRhythm  → rebuildWarpMirrorAll()  (only rhythm lanes changed)
  - PatternEngine.cpp:634  recomputeEffectiveQmix    → rebuildWarpMirrorAll()  (only QMIX changed)
TWO problems compound:
  1. SCOPE: each rebuilds ALL 7 lanes when only its own 1-2 lanes' cells changed → ~7x wasted work.
  2. FREQUENCY: these recomputes are CACHE-GATED on (N, frac, r=slew) (lines 651/705/749). They DON'T
     fire when idle — BUT any slew modulation / frac change / playhead motion MISSES the cache and
     re-runs, each time doing the full 1792 rebuild. The comment calls it "draw/scrub-rate,
     prompt-blessed cost" — but under slew modulation it fires FAR more than draw rate. Full 1792
     rebuild ≈ 64µs (measured); ×3 lanes ×frequent = the 1%→8%.

## THE FIX: scope each recompute's rebuild to the lanes it actually changed
Replace rebuildWarpMirrorAll() in each recompute with a rebuild of ONLY that recompute's lanes, ALL
16 voices (recompute touches all voices).
CAUTION — the existing scoped call is WRONG for this: rebuildWarpMirrorLaneDelegating(lane) has a
`if (!warpDelegated_[v][l]) continue;` guard that SKIPS own-slot voices. The recompute rewrote random_
for ALL voices (delegated or not), so skipping own-slot voices → STALE mirror. So DON'T use
LaneDelegating here.
ADD a new scope: rebuildWarpMirrorLane(int lane) — this lane, ALL 16 voices, NO delegation guard:
    void PatternEngine::rebuildWarpMirrorLane(int lane) {
        if (!warpBank_) return;
        const int l = (lane >= 0 && lane < dotModular::NUM_STRANDS) ? lane : 0;
        for (int v = 0; v < 16; ++v)
            for (int s = 0; s < 16; ++s)
                warped_[v][l][s] = warpMixCell(v, l, s);
    }
Then, in each recompute, rebuild its OWN lanes only (CONFIRM the exact lane set each recompute's
random_ write touches — grep the PL_* lanes written in each; do NOT assume). Likely:
  - recomputeEffectiveMelody (:474): rebuild PL_MELODY + PL_OCTAVE  (+ whatever else it writes)
  - recomputeEffectiveRhythm (:558): rebuild PL_REST + PL_ACCENT + PL_VARIATION + PL_LEGATO
  - recomputeEffectiveQmix   (:634): rebuild PL_QMIX
The rebuild lane-set MUST exactly cover the lanes that recompute rewrote, else stale mirror. VERIFY by
reading which polyRandom(v, PL_*) each one writes.

## CORRECTNESS GUARD (must not regress behaviour)
This is a SCOPE optimisation — the warped_ values must be IDENTICAL to before, just computed over fewer
cells. Contract: after a recompute, warped_ for the recompute's lanes == what rebuildWarpMirrorAll would
have produced for those lanes; warped_ for OTHER lanes == unchanged (they weren't touched). Extend
test_warp_mirror: assert recomputeEffectiveMelody updates MEL/OCT warped_ cells and leaves
REST/ACCENT/QMIX/etc. cells BYTE-IDENTICAL (not rebuilt). RED if a recompute still rebuilds all / leaves
its own lanes stale.

## MEASUREMENT (Rodney, in Rack — can't be done in CI/container)
The regression is the Rack per-module CPU meter (1%→8%). After the fix, Rodney re-checks the meter:
target back toward ~1-2%. The contract test proves correctness; the CPU meter proves the perf win.
State in the PR: "needs Rack CPU-meter check — target ~1-2%, was 7-8%."

## DONE =
- rebuildWarpMirrorLane(lane) added (all voices, no delegation guard).
- The 3 recomputes (:474/:558/:634) rebuild ONLY their own lanes (lane-set verified against their
  random_ writes), NOT All.
- test_warp_mirror: per-recompute scope correctness (own lanes updated, other lanes byte-identical).
- bash test/run_all.sh green. PR: needs Rack CPU-meter confirmation.

## NOT THIS PROMPT
- The voiceSlot/ownership-polarity warp bug (separate).
- Any resolver / seam / clamp work.
- The UI-triggered rebuilds (WarpCurvePopup requestWarpRebuild*) — those are rare (user edits), fine.
- The other rebuildWarpMirrorAll sites (:120 init, :159 first-wire, :474/558/634 are THIS fix,
  persistence :390 load, SandsManager :177 topo-change) — leave init/first-wire/load/topo-change as
  All (they're genuinely rare one-shots); ONLY the 3 recompute-path ones are the hot regression.
