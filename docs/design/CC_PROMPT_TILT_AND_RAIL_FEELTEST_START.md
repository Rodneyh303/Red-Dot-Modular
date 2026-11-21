# CC PROMPT — FEEL-TEST: add MO tilt + single rail-atom to the distribution editor (see the impact)

CPU fire is OUT (Monsoon ~1%, Macro ~0.4%). Change of pace: Rodney wants to SEE two distribution
features live in the popup CDF/PDF editor BEFORE the big UI-to-resolver / rate-manager refactors.
SCOPE = AUTHOR-AND-SEE on the DISPLAYED (voice, lane) only. NOT the full all-lanes/all-voices/CV-out/
modulation/coupling plumbing — this is a prototype to feel the shapes. Work on lane-expander-refactor.

## FEATURE 1 — MO TILT (continuous rail-ward shaping)
FULLY SPEC'D in docs/design/PALETTE_KUMARASWAMY_TILT_NOTE.md + CC_PROMPT_TILT_START.md. For the
FEEL-TEST, the MINIMAL version:
- Shared sample-side tilt stage R_p(t) = (1-p)*t / (1 - p*t), one mul + one div, applied after the
  member's invCDF: out = R_p(member.invCDF(u)). (= MobiusDistribution invCDF at a=1-p; Mobius exists,
  Distribution.hpp.)
- A "rail tilt" knob, p in (-P_MAX,+P_MAX), P_MAX=0.95, EVAL-SIDE clamp (clamp p before R_p). p=0 =
  bit-identity (neutral).
- Add the tilt param to the WarpF variant + a "tilt" row in WarpCurvePopup. The editor ALREADY renders
  any Distribution's CDF+PDF (DistributionCurveWidget) — so the tilt just reshapes the drawn curve live
  as p moves (curve bends toward a rail; PDF density piles near it). Nothing new to draw.
- p=0 loads existing patches bit-identical (purely additive).

## FEATURE 2 — INJECTED ATOM, FREE LOCATION (Rodney: atom LEVEL + AMOUNT knobs, anywhere in [0,1])
The injected atom (OUTPUT_LAYER_DISTRIBUTION_MAPPING.md "THE ONE USEFUL ATOM FEATURE") with ℓ FREE —
two knobs: LOCATION ℓ ∈ [0,1] (the output value the atom sits at — INTERIOR is the useful/default case;
the CORNERS 0 and 1 are allowed but NOT required) and AMOUNT/MASS m ∈ [0, M_MAX~0.9] (M_MAX keeps the
continuous part alive). m=0 = NEUTRAL (no atom, bit-identical).
- FORWARD (invCDF): a u-sub-interval of width m maps to exactly the output value ℓ; the rest is the
  (tilted) member invCDF rescaled over the remaining (1-m) of u. INSERT/DISPLACE model (atom adds mass
  at ℓ, pushes the rest) — the note's leaned default, simplest to feel first.
- Monotone-safe (a flat segment at ℓ in a monotone quantile stays monotone).
- EDITOR RENDERING (the new drawing bit — atom is a DISCONTINUITY at a MOVABLE ℓ):
    · CDF view: a VERTICAL RISER at output ℓ (the CDF jumps by m there). The riser moves as ℓ moves.
    · PDF view: a Dirac can't be drawn literally; draw a DISTINCT finite BAR at ℓ, height ∝ m, in a
      distinct colour/style so it reads as "a lump of probability here" vs the continuous density. The
      bar moves with ℓ.
- TWO knobs in WarpCurvePopup: "atom level" (ℓ) + "atom amount" (m). m=0 neutral. (ℓ can reach 0/1 =
  rail atom as a special case, but defaults interior.)

## COMPOSITION (both together — the note says they reinforce)
Tilt + atom compose and reinforce (tilt reshapes the continuous part; the atom sits as a riser/bar at
ℓ on top of it). Compose in the forward (tilt first, then inject the atom at ℓ) and render both live.
If ℓ is near the rail the tilt toward that rail visibly compounds with the atom; if ℓ is interior you
see the anchor sit inside the shaped curve.

## SCOPE FENCE (keep it a fast feel-test)
- DISPLAYED (voice,lane) only — author on the popup, see on the editor curve. Do NOT wire all 7 lanes /
  16 voices / CV-outs / modulation / cross-lane coupling. (Those are the deferred full features.)
- The engine CAN apply it on the displayed voice/lane so Rodney hears it too (reuse the warp mirror
  path — it already composes invCDF; tilt+atom just change what invCDF returns). But no new routing.
- Persistence: tilt p + rail + mass as additive params; neutral (p=0,m=0) = existing patches unchanged.

## TESTS (light — it's a prototype, but keep the neutrals honest)
- p=0 AND m=0 → bit-identical to the un-tilted un-atomed member (the neutral contract).
- tilt monotone for |p|<=P_MAX; atom monotone for m in [0,M_MAX]; both-together monotone.
- atom mass: P(output == ℓ) == m (the atom carries exactly m, at the set location ℓ).
- tilt direction: p>0 shifts mass toward one rail, p<0 the other.

## RESTORE THE LOST PROCESS-NS LOGGING (Rodney noted it went missing) — but SAFELY
The overall-process avgNs timer was REMOVED because it was a per-sample std::chrono pair (the SAME
pattern that caused the 124%). Do NOT restore it as always-on per-sample. If Rodney wants it back, add
it under #ifdef MONSOON_PROFILE (compile-flag, off by default) so it never costs CPU in normal builds —
the safe form from CC_PROMPT_cpu_section_profile. (Default: leave it off; read CPU from Rack's meter.)

## DONE =
- THREE knobs in WarpCurvePopup: "rail tilt" (p, ±0.95 eval-clamp) + "atom level" (ℓ ∈ [0,1]) + "atom
  amount" (m, [0,M_MAX~0.9]); all reshape the editor CDF/PDF live; atom draws as a movable CDF-riser +
  PDF-bar at ℓ.
- Forward composes tilt then atom; applied on the displayed voice/lane (engine hears it); neutral
  (p=0,m=0) bit-identical; existing patches unaffected.
- Light tests green; bash test/run_all.sh green.
- (optional) #ifdef MONSOON_PROFILE process-NS timer restored off-by-default.
- PR: "feel-test prototype — displayed voice/lane only; needs Rack: move the tilt knob (curve bends to
  rail), the rail-mass knob (riser/bar grows), both together (reinforce). Full routing deferred."

## NOT THIS PROMPT
- All-lanes / all-voices / CV-out / modulation / cross-lane coupling (the deferred full features).
- (ℓ is FREE here — the full location feature IS in this feel-test. What's still deferred below.)
- The UI-to-resolver / rate-manager refactors (after the feel-test).
