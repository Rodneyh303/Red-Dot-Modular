# CC PROMPT — show the variation playhead SCANNING (display-only; scanOfs_ reaches the Sands variation lane)

CONTEXT (Rodney, Rack): the freeze fix WORKS (no freeze, bipolar fwd/back). BUT the Sands variation
playhead does NOT move independently of the other lanes during a hold/stutter — so it is not showing the
true scanned step. The ENGINE scans (decideStepAdvance_ uses scanOfs_, SequencerEngine.cpp:975/1125) but
the DISPLAY computes every lane's playhead from totalStepsElapsed ONLY (getStepLightBrightness:513
tickForLight = totalStepsElapsed - 1 - relCurrent + relLight; the per-lane laneTickV_/laneTick_ caches
likewise). So during a held step the variation chevron sits still with the others, hiding the scan.

## THE FIX — DISPLAY ONLY: the variation lane's displayed playhead uses totalStepsElapsed + scanOfs_
In GATE MODE, the VARIATION lane's displayed playhead/chevron must be computed at the SCANNED position:
    variation display tick = (its normal tick) + scanOfs_
so while the main playhead HOLDS, the variation chevron visibly SCANS FORWARD (by scanOfs_) through the
variation window while the OTHER lanes sit at totalStepsElapsed. On advance, scanOfs_ resets → the variation
chevron re-aligns. This makes the scan VISIBLE (and is the visual confirmation the freeze-fix is working —
the independently-moving variation playhead IS the scan).
- VARIATION LANE ONLY. All other lanes unchanged (totalStepsElapsed). Gate mode only (scanOfs_ is 0
  elsewhere, so it's naturally a no-op in clock/phase).
- DIRECTION: the scan moves in the knob-sign direction (same as the advance decision) — forward chevron for
  positive Variation, backward for negative — consistent with how the chevron already shows lane direction.
- Apply scanOfs_ wherever the variation lane's displayed tick is computed: the getStepLightBrightness path
  (:513, if it's lane-aware for variation) AND the per-lane playhead the Sands visual reads
  (laneTickV_/laneTick_ for the variation strand, ~:764). Add scanOfs_ to the VARIATION strand's displayed
  tick only.
- NO ENGINE/AUDIO CHANGE: the decision already uses scanOfs_ correctly. This is purely making the DISPLAY
  match what the engine reads. Goldens (which pin audio/decisions) UNCHANGED — this touches only the light/
  chevron computation.

## DONE =
- In gate mode, the Sands VARIATION lane chevron scans forward/back (knob-sign) by scanOfs_ during holds,
  moving independently of the other lanes; re-aligns on advance. Other lanes unchanged.
- Display-only (goldens unchanged — no audio/decision change). Clock/phase unchanged (scanOfs_=0).
- Rack: during a stutter, you SEE the variation playhead scan ahead while the others wait, then everything
  advances together on escape — the visual confirmation of the freeze fix.
