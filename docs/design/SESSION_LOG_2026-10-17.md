# SESSION LOG — 2026-10-17 (long architecture chat; digest, not verbatim)
The connective narrative of one very long chat — how the threads related, the decisions + their
justifications, and where plausible-but-wrong reasoning got CAUGHT. Per-topic detail is in the docs each
thread produced (pointers inline). Companion to CHAT_HANDOFF.md (that = "where we are"; this = "how we got
here this session").

## ARC (roughly chronological)
1. SUBGATE / VARIATION design (gate mode). Resolved ghost/ratchet rest+legato into a per-subgate REST SOURCE
   selector {None, Main, Variation, Both-OR} — ONE rest per cell, source-selectable -> dissolved a two-gate
   tension + a missing "rest beats legato" priority. Traced facts: ratchet in-gate-only (main rest closes
   gate -> no ratchet, structural); ghost in-gap. Variation = the assignable gate-mode spare lane (note
   length = gate length there, so variation is freed). Then: subgate rest+legato inherit the FULL system
   (per-voice + cross-voice copula + temporal slew = 2D correlation) for free, because they route through
   first-class lanes. [GATE_SUBDIVISION_STEP_GATE.md]
2. THE BRUTAL WAR — voice-2 then V16 melody-bar flicker + a hidden spread-lane crossing. Root (found after
   ~6 failed CC rounds + 3 wrong fixes from me): LYING CONVENTIONS. polyBankIndex returned v-1 but was
   NAMED/commented as 0=V2 ("voiceSlot = polyBankIndex+1" was false). Same disease on the LANE axis
   (SpreadInterp/SpreadManager/MonsoonSandsManager spoke an OLD lane order while callers passed unified
   PL_*; 5-of-7 lanes crossed when spread!=0 — a live audio bug nobody could see). FIX: eradicate on both
   axes (voice: [16][16]+voiceSlot+polyBankIndex->polyVoiceSlot; lane: unify to PL_*). GUARDED by CONTRACT
   TESTS (test_voice_conventions.cpp) which turned the unfixable Rack-only ghost into a STATIC failure — the
   thing that finally cracked it. Verified 3 ways (display + tests + output jacks). CI green. [COLLAPSE_15_
   TO_16.md, V16_FLICKER_root_smell.md, VOICE_CONVENTION_TESTS.md, convention_unification_INCOMPLETE.md]
3. TOOLING: parallelized + precompiled test harness (run_all.sh), fixed an MSYS2/Git-Bash hang (wait -n ->
   portable batch wait). Faster verification = the enabler of everything after.
4. OUTPUT-LAYER DISTRIBUTION WARPING (the big new feature, the bulk of the session). Prompted by a VCV forum
   post on "subtractive vs additive randomness". Insight: the UNIFORMITY invariant we defended in the slew/
   mix war makes the output a clean substrate -> apply F^-1 at the output to reshape the MARGINAL per lane,
   correlation + reversibility preserved (Sklar). Built Phase 1 (Distribution + MonotoneCurve, double,
   tested, merged) and Phase 2 (dmcurve editor widget: square dual-axis CDF/PDF, all dists render, live
   param morph, drag-edit, convert-parametric->PCHIP). [OUTPUT_LAYER_DISTRIBUTION_MAPPING.md, DISTRIBUTION_
   LAYER.md, DMCURVE_EDITOR.md, EFFECTIVE_DISTRIBUTION_VISUALISER.md, POSITIONING_correlational_randomness.md]

## KEY DECISIONS + WHY (the durable content)
- Edit the CDF directly (not F^-1); PDF = slope; F^-1 = reflect+re-fit PCHIP. No LUT — direct eval at
  control rate (cheap; LUT was solving a per-sample problem we don't have).
- 8-dist palette, all TUNABLE + cheap-inverse: Power, Mobius, Triangular, Kumaraswamy, Logit-normal,
  Logit-sigmoid, Truncated-normal, Drawn(PCHIP). Selection principle (3 clauses): distinct shape + cheap
  inverse (closed-form/PhiInv, no LUT) + TUNABLE (fits the morph-with-knobs UI). Dropped Arcsine (no params
  -> dead-end in morph UI). Mobius pdf = a/((1-a)x+a)^2 (bounded gentle tilt, identity at a=1). Logit-sigmoid
  kept alongside logit-normal (logistic vs normal squash; cheaper, heavier tails).
- The Distribution interface IS a monotone [0,1]->[0,1] bijection -> stat dists, warps, drawn curves are ONE
  object (cdf=fwd, invCDF=inv, pdf=slope).
- Precision: distributions are DOUBLE (siblings of the copula/PhiInv math layer — reversibility-critical,
  U_EPS 1e-12); storage float; cast at storage boundary; NO template (fights the polymorphic interface).
  (Corrected an initial float lean once we saw GaussianCopula is double.)
- "Tractable inverse or it's out" governs both levels: distributions (closed-form/PhiInv, or set-time LUT
  for the rare hard case); copula (Gaussian's PhiInv, NOT t's Beta-inversion). Beta^-1 harder than PhiInv
  (iterative incomplete-beta) — fine if amortizable to set-time (a marginal), disqualifying in a hot/
  reversible path (a copula). So NO t-copula: numerically unstable at high df, threatens reversibility — AND
  shared-extreme-warp + Gaussian copula ~= t-copula's tail-clustering MUSICALLY anyway.
- .dmcurve = JSON (consistent w/ .dmtune, SHARES the patch dataToJson schema -> one serializer). Distribution
  choice = PATCH STATE (no .dmdist file). Shipped defaults + user curves all .dmcurve. Editor = popup (live-
  preview co-visibility), pinned endpoints, click-add/dbl-remove.
- Visualiser (EFFECTIVE_DISTRIBUTION_VISUALISER.md): own module (Lantern pattern), rows-of-voices, melody +
  octave + note-length bar groups; effective = fader-WIDTHS x the non-uniform draw landing on them; the
  uniform<->transformed TOGGLE is the pedagogical keystone (makes every layer visible, answers "too deep").
- WARP PURPOSE (honest, after correcting an invented "warp=where/slew=how" orthogonality — slew is SHARED
  not per-voice): melody warp = per-voice handle on polyphonic CHAOS<->ORDER (global warp ~= fader adjust,
  redundant); octave warp = distributional layer of a 4-LAYER octave stack (Straits offset / Causeway mod /
  warp / copula correlation) that answers the Melodicer "limited octave control" criticism. Warp has MONO
  value iff the lane LACKS per-outcome faders (octave/length have none -> mono value; melody has faders ->
  polyphonic-only).
- WARP per-lane SCOPE (PROVISIONAL/experimental): value/range lanes (melody/octave/note-length-in-clock) =
  clear value. Threshold/boolean lanes (rest/legato/accent/qmix; variation IS binary in gate mode = subgate
  rest/legato) = warp only a marginal modulation-response curve. BUT isolated-lane reasoning has a blind
  spot (interactions: rest+legato gap-distribution; gate/subgate density). DECISION: add warp to ALL lanes
  experimentally (cheap add/remove), FEEL it, peel back on cost + FUN. Provisional cut = warp-worthy <=>
  visualiser-worthy. Reasoning sets the default; ears set the verdict.

## CORRECTIONS (plausible-but-wrong, caught — the meta-lesson)
- My 3 wrong V16 "fixes" (index swaps that were no-ops: polyVoice()==currentSlot() for poly). Caught by
  Rodney pushing to trace, then by the contract test. -> the bug was a LYING abstraction, unfixable by
  reasoning from the lie.
- My invented "warp=where / slew=how" per-voice orthogonality -> slew is SHARED. Rodney caught it. Real
  story: warp = chaos-level control / octave-stack completion.
- My initial float lean for distributions -> double (match the copula math layer). Rodney flagged the double
  in GaussianCopula.
- My over-caution on "convert-to-PCHIP is too much" and "logit-sigmoid redundant" -> both good, cheap, kept.
META-LESSON (applies to code AND design): VERIFY against code/tests/ears, never a plausible narrative. The
map is not the territory; check the territory. For code the territory is the test/engine; for music it's the
ears (perceptual questions -> build cheap + FEEL, don't settle analytically).

## WHERE IT ENDED / NEXT
Phase 2 editor built + delightful. NEXT: Phase 2.5 (add Mobius + Logit-sigmoid, remove Arcsine, param knobs,
convert-to-PCHIP) -> Phase 3 (integration: wire warp into a lane, Macro/Sands scope, .dmcurve load/save, the
LIVE-PREVIEW KEYSTONE: drag curve -> Sands bars redistribute). Branch lane-expander-refactor, CI green.
Working model continues: THIS chat = architecture + CC prompts; CC = execution; Rodney feel-tests in Rack.
