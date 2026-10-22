# CC PROMPT — MO rail-tilt (KwMO) palette feature

The DESIGN IS ALREADY FULLY SPEC'D in docs/design/PALETTE_KUMARASWAMY_TILT_NOTE.md — math,
engineering fit, constraints, palette-wide applicability, and a 7-point test battery. READ THAT NOTE;
it is the authority for the HOW. This prompt only (a) corrects the stale sequencing premise and
(b) makes the one open implementation-shape decision. Work on lane-expander-refactor.

## CORRECTION TO THE NOTE'S SEQUENCING (read before planning)
The note says "try the tilt WHEN WE DO THE CLAMP" and frames it as clamp's mass-feeder. THAT PREMISE
IS STALE. Since that note was filed, the clamp feature was RETIRED (OUTPUT_LAYER_DISTRIBUTION_MAPPING.md,
"CLAMP BUILD-CASE VERDICT" + "FINAL ATOM/CLAMP RESOLUTION" + "THE ONE USEFUL ATOM FEATURE"): rail atoms
are perceptually inert (no exact-rail decision exists), and the injected-interior-atom feature subsumes
clamp anyway. So:
- The tilt is now evaluated STANDALONE — it is a clean CONTINUOUS feature (the MO member / rail-tilt
  knob), useful on its own (rail-ward density shaping on value lanes + CV outs).
- Its eventual composition PARTNER is the INJECTED INTERIOR ATOM (if ever built), NOT the clamp. The
  note's "RAILS / mass-feeder / two-rail clamp-by-target-mass" sections describe composition with a
  feature that is NOT being built — treat them as RETAINED THEORY, not an implementation target. Build
  the TILT ONLY. Do not implement any clamp / rail-atom / lambda machinery from the RAILS section.
- The P_MAX feel-test and the clamp-calibration decision were framed as "one listening session" with
  the clamp. Decouple: the P_MAX feel-test stands alone (the derivation recipe in the note's P_MAX
  PROVENANCE section is still valid — tilt a uniform at 0.90/0.95/0.98/0.99, find rail-hug→rail-pinned
  by ear). Ship with P_MAX=0.95 PROVISIONAL (the note's value) as the eval-side authoring clamp; the
  feel-test refines it later. No clamp-calibration decision is needed (no clamp).

## THE IMPLEMENTATION-SHAPE DECISION (the note left this open: "member-as-filed vs SHARED TILT STAGE")
DECISION: SHARED TILT STAGE (not a Kumaraswamy-only member). Rationale from the note's PALETTE-WIDE
APPLICABILITY section: R_p is a universal monotone self-map of [0,1], composes with EVERY member
(monotone∘monotone), p=0 bit-identity holds palette-wide in one stroke, and R_p is EXACTLY
MobiusDistribution's invCDF at a=1-p (VERIFIED: MobiusDistribution exists, Distribution.hpp:269). So
implement ONE shared tilt stage applied after any member's invCDF, NOT nine subclasses. Convention:
SAMPLE-SIDE tilt — out = R_p(member.invCDF(u)) — per the note's pinned CONVENTION (member-independent
knob meaning "bend toward bottom/top rail"; do NOT ship the u-side alternative).
On the MOBIUS member specifically the tilt is ABSORBED (a <- a·(1-p)) — handle per the closure identity
m_a∘m_b = m_{ab} (either special-case it or let it compose; the note's test 7 asserts the identity).

## WHAT TO BUILD (all per the note's Engineering fit section)
- The shared tilt: R_p(t) = (1-p)·t / (1 - p·t), one multiply + one divide, double, stateless, pure.
- A `p` (signed tilt, "rail tilt" knob) authored in (-P_MAX, +P_MAX), P_MAX=0.95, EVAL-SIDE clamp
  (clamp p before computing R_p — the clampR/CONC_MAX precedent, Distribution.hpp:206). Modulation
  overshoot caught at the eval boundary; |p|>=1 can never reach the hot path.
- Wire it into the WarpF variant + fromDistribution/toDistribution + a third params row ("tilt") in
  WarpCurvePopup (the note's WARP SLOTS + AUTHORING items). Reuses the landed modeConcFromAb/
  abFromModeConc helpers; the tilt composes AFTER the (mode,conc)->(a,b) map so the knobs stay
  orthogonal.
- Persistence: new kind tag via the strcmp pattern in WarpF.hpp; p=0 loads existing patches
  bit-identical (purely additive, no compat shims).
- Label the knob "rail tilt" (the note's HONEST CAVEAT: the hazard-taxonomy is a Kw result, not a
  per-member promise).

## TESTS — the note's 7-point battery (test/, mirror the reversed-power battery)
Implement tests 1-7 exactly as the note's "Tests" section lists them. The load-bearing ones:
- (1) p=0 BIT-IDENTITY to the untilted member across the (mode,conc) grid — the neutral-param contract.
- (3) eps-monotonicity floor: effective curve strictly increasing for all |p|<=P_MAX (tilt must not
  flatten the curve).
- (5) tilt-direction: median of Q shifts toward 0 for p>0, toward 1 for p<0, |shift| monotone in |p|.
- (7) PALETTE-WIDE (since shared-stage is chosen): for EVERY member, u->R_p(member.invCDF(u)) strictly
  increasing across the grid for all |p|<=P_MAX; Mobius absorption identity tilt(Mobius(a),p) ==
  Mobius(a·(1-p)) to fp tolerance; uniform+tilt == pure Mobius reference curve.

## DONE =
- Shared sample-side tilt stage R_p over any member; eval-side P_MAX=0.95 clamp; "rail tilt" knob row
  in WarpCurvePopup; WarpF variant + persistence tag; p=0 bit-identical (existing patches unaffected).
- NO clamp/rail-atom/lambda machinery (that feature is retired; RAILS section is retained theory only).
- Tests 1-7 green; bash test/run_all.sh green.
- PR: note P_MAX=0.95 is PROVISIONAL pending the standalone feel-test (recipe in the note); "needs Rack
  feel-test: rail-tilt on value lanes + CV outs, and the P_MAX top-stop derivation."

## NOT THIS PROMPT
- The clamp / rail atoms / two-rail target-mass model / lambda (RETIRED — do not build).
- The injected interior atom (separate future feature; the tilt's eventual composition partner, not now).
- The warp voiceSlot/ownership bug, the perf rebuild fix, the resolver/seam work (all separate).
