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

## FEATURE 2 — SINGLE RAIL-ATOM (injected atom at a rail — the "single rail")
The theory's injected interior atom (OUTPUT_LAYER_DISTRIBUTION_MAPPING.md "THE ONE USEFUL ATOM FEATURE")
with ℓ PINNED to a rail for this feel-test — i.e. one atom at output 0 OR 1:
- Two scalars: RAIL (0 or 1 — which rail) and MASS m in [0, M_MAX] (M_MAX~0.9 so the continuous part
  survives). m=0 = NEUTRAL (no atom, bit-identical).
- In the invCDF forward: a u-sub-interval of width m maps to exactly the rail value; the rest is the
  (tilted) member invCDF rescaled over the remaining (1-m) of u. INSERT/DISPLACE model (atom adds mass
  at the rail, pushes the rest) — the note's leaned default; it's the simpler one to feel first.
- Monotone-safe (flat segment in a monotone quantile stays monotone).
- EDITOR RENDERING (the one new drawing bit): an atom is a DISCONTINUITY —
    · CDF view: a VERTICAL RISER at the rail (the CDF jumps by m at output 0 or 1). Natural to draw.
    · PDF view: a Dirac can't be drawn literally (infinite spike). Draw a DISTINCT finite BAR at the
      rail, height ∝ m, in a distinct colour/style so it reads as "a lump of probability here" vs the
      continuous density. (A visual convention — label it clearly; the feel-test will say if it reads.)
- A "rail mass" knob (+ a rail 0/1 selector) in WarpCurvePopup, m=0 neutral.

## COMPOSITION (both together — the note says they reinforce)
Tilt-toward-the-rail + atom-at-that-rail compound (tilt makes the near-rail region dense, atom piles at
it). The editor should show BOTH live: tilt bends the continuous part toward the rail, the atom sits as
the riser/bar at it. Just compose them in the forward (tilt first, then inject the atom) and render both.

## SCOPE FENCE (keep it a fast feel-test)
- DISPLAYED (voice,lane) only — author on the popup, see on the editor curve. Do NOT wire all 7 lanes /
  16 voices / CV-outs / modulation / cross-lane coupling. (Those are the deferred full features.)
- The engine CAN apply it on the displayed voice/lane so Rodney hears it too (reuse the warp mirror
  path — it already composes invCDF; tilt+atom just change what invCDF returns). But no new routing.
- Persistence: tilt p + rail + mass as additive params; neutral (p=0,m=0) = existing patches unchanged.

## TESTS (light — it's a prototype, but keep the neutrals honest)
- p=0 AND m=0 → bit-identical to the un-tilted un-atomed member (the neutral contract).
- tilt monotone for |p|<=P_MAX; atom monotone for m in [0,M_MAX]; both-together monotone.
- atom mass: P(output == rail) == m (the atom carries exactly m).
- tilt direction: p>0 shifts mass toward one rail, p<0 the other.

## RESTORE THE LOST PROCESS-NS LOGGING (Rodney noted it went missing) — but SAFELY
The overall-process avgNs timer was REMOVED because it was a per-sample std::chrono pair (the SAME
pattern that caused the 124%). Do NOT restore it as always-on per-sample. If Rodney wants it back, add
it under #ifdef MONSOON_PROFILE (compile-flag, off by default) so it never costs CPU in normal builds —
the safe form from CC_PROMPT_cpu_section_profile. (Default: leave it off; read CPU from Rack's meter.)

## DONE =
- "rail tilt" knob (p, P_MAX=0.95 eval-clamp) + "rail mass" knob (m, M_MAX~0.9) + rail 0/1 selector in
  WarpCurvePopup; both reshape the editor CDF/PDF live; atom draws as CDF-riser + PDF-bar.
- Forward composes tilt then atom; applied on the displayed voice/lane (engine hears it); neutral
  (p=0,m=0) bit-identical; existing patches unaffected.
- Light tests green; bash test/run_all.sh green.
- (optional) #ifdef MONSOON_PROFILE process-NS timer restored off-by-default.
- PR: "feel-test prototype — displayed voice/lane only; needs Rack: move the tilt knob (curve bends to
  rail), the rail-mass knob (riser/bar grows), both together (reinforce). Full routing deferred."

## NOT THIS PROMPT
- All-lanes / all-voices / CV-out / modulation / cross-lane coupling (the deferred full features).
- Interior (non-rail) atom location (ℓ pinned to rail for this feel-test; free ℓ is the full feature).
- The UI-to-resolver / rate-manager refactors (after the feel-test).
