# CC PROMPT 08 — DIAGNOSE: East doesn't follow Macro DIRECTION on VAR/LEG (lanes 5,6)

CONTEXT: feat/warp-ui. Separate bug from the warp voiceSlot issue (though same era). Rodney (Rack):
East follows Macro's lane DIRECTION on editor lanes 0-4 (MEL/OCT/QMIX/REST/ACCENT) but NOT on
5=VARIATION, 6=LEGATO. Diagnostic-first — verify against code, do NOT guess-patch.

## VERIFIED FACTS (already checked — build on these, don't re-derive)
- The split is lanes 0-4 (follow) vs 5-6 VAR/LEG (don't). "accent" is lane 4 and DOES follow (it
  was a typo in the report; the non-followers are VARIATION + LEGATO).
- Loop bounds are NOT the cause: MONO_LANES == POLY_LANES == 7 (SandsGrid.hpp:30,33; the SANDS
  CONSOLIDATION unified them). readStrand loops MONO_LANES=7 (MonsoonSandsManager.cpp:461) so it
  DOES cover VAR/LEG. Do not chase loop bounds.
- VAR/LEG are a recently-PROMOTED class (SANDS CONSOLIDATION Step 3, MonsoonSandsManager.cpp:399-400:
  "lanes 5/6 VAR/LEG are NOW spread-active; the old 'VAR/LEG mono-only' note was STALE"). STRONG
  HINT: VAR/LEG were promoted to spread-active, but some per-lane subsystems may NOT have been
  updated for them — direction-follow is likely one left behind. This is a STALE-PROMOTION bug.
- VAR/LEG have DOCUMENTED special-cases that 0-4 don't:
    :309 "East CV exists only for the 4 poly lanes (MEL/OCT/REST/ACC); VAR/LEG [excluded]"
    :429-434 "VAR/LEG spread: applyMono, not the raw slewed copy"
    :314 "if (l < POLY_LANES && v1OwnsLane)" — an ownership gate; check if it or a sibling excludes 5,6
  The direction-follow divergence is almost certainly ONE of these VAR/LEG special-cases.

## METHOD
1. Find WHERE East follows Macro's lane DIRECTION (the direction/arrow per lane — the CRV/dir cells
   in the image's right column). Grep: direction / dir / arrow / caDir / laneDir / getDir/setDir /
   ownerDisp / the per-lane direction resolution in MonsoonSandsManager + the direction read in the
   engine. Identify the exact line where a lane's direction is set from Macro's.
2. Compare that path for lane l=0..4 vs l=5,6. Find the branch/gate/special-case that makes 5,6
   diverge: a `l < POLY_LANES` (=7, wouldn't exclude) vs a `l < 4` / `l < 5` / `l < SOME_SMALLER`
   bound, a VAR/LEG-specific skip, an East-CV-only-4-lanes exclusion that direction wrongly piggybacks
   on, or a readStrand/lorBase path that reads direction for 0-4 but defaults 5-6.
3. Confirm it's a STALE-PROMOTION (VAR/LEG promoted for spread but their direction-follow still on the
   old mono-only path) vs INTENTIONAL (VAR/LEG deliberately never follow Macro direction). If the
   design doc / a clear comment says VAR/LEG direction is intentionally East-local, it's NOT a bug —
   report that. Otherwise fix: route VAR/LEG direction through the SAME follow path as 0-4.
4. Watch for the OTHER VAR/LEG special-cases (East-CV-4-lanes :309, applyMono spread :429) that ARE
   intentional — don't "fix" those by lumping direction with them. Direction-follow is the ONE to
   extend to 5,6; the CV/spread special-cases stay.

## CONTRACT TEST
Assert: for a lane delegated/followed Macro→East, the resolved East direction == Macro's direction
for ALL editor lanes 0..6 — explicitly including 5=VAR and 6=LEG (the two that currently diverge).
RED on HEAD for 5,6; GREEN when they follow like 0-4. (If diagnosis #3 says it's intentional, the
test instead PINS the intended behaviour and this prompt closes as "not a bug".)

## DONE =
- The exact divergence line identified (the gate/special-case excluding VAR/LEG from direction-follow).
- Either: VAR/LEG direction routed through the same follow path as 0-4 (if stale-promotion) + test
  green for all 7; OR a clear finding that it's intentional + a pinning test + report to Rodney.
- bash test/run_all.sh green; needs Rack feel-test: East follows Macro direction on VAR/LEG too.

## NOT THIS PROMPT
- The warp voiceSlot/polyBank bug (CC_PROMPT_06/07) — separate, though same era.
- The intentional VAR/LEG East-CV / applyMono-spread special-cases — leave those.
