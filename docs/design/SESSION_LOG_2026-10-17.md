# SESSION LOG — 2026-10-17 (long architecture+debug chat, distilled)
Not verbatim (can't export the raw chat). This is the SUBSTANCE: the reasoning arc, decisions + WHY,
corrections, and pointers to the docs each thread produced. Companion to CHAT_HANDOFF.md (handoff = where we
are; this = how we got here this session). Durable record; the chat itself is gone.

## ARC (chronological-ish)
1. SUBGATE / VARIATION design (gate mode): resolved ghost/ratchet rest+legato into a per-subgate "REST
   SOURCE" selector {None, Main, Variation, Both-OR}; dissolved a two-gate priority tension; variation =
   the assignable spare lane in gate mode; subgates INHERIT the full 2D copula (voice x time) for free by
   routing through first-class lanes. -> GATE_SUBDIVISION_STEP_GATE.md.
2. THE BRUTAL DEBUG (centerpiece): a melody-bar flicker on voice 2, then after a fix, on voice 16. 6+ failed
   CC rounds + 3 failed fixes by me (all "plausible but wrong"). ROOT CAUSE finally found: VoiceResolver::
   polyBankIndex was a LYING abstraction — code returned v-1 (=voiceSlot) but NAME + comments said "0-based
   poly bank, V2=0". Self-contradictory source -> nobody could reason correctly. ALSO found a hidden live
   audio bug: SpreadInterp/SpreadManager/MonsoonSandsManager spoke an OLD lane order while callers used
   unified PL_* -> 5/7 spread lanes CROSSED when spread!=0.
   THE FIX THAT WORKED: wrote CONTRACT TESTS (test_voice_conventions.cpp) that caught BOTH statically (after
   everything else failed in Rack). Then unified conventions on BOTH axes: voice ([16][16] storage,
   voiceSlot, polyBankIndex->polyVoiceSlot) and lane (unified PL_* order). Verified 3 ways: display + tests
   + output jacks. CI green. -> CHAT_HANDOFF.md, VOICE_CONVENTION_TESTS.md, V16_FLICKER_root_smell.md,
   convention_unification_INCOMPLETE.md.
   LESSON (load-bearing): verification is the bottleneck, not the fix. A reliable test turned an unfixable
   Rack-only ghost into a static failure solved in one pass. Verify against code/tests, never a plausible
   narrative.
3. OUTPUT-LAYER DISTRIBUTION WARPING (the new fun feature, bulk of the session):
   - Prompted by a VCV forum post (Morris, "subtractive and additive randomness"). Positioned dot.modular as
     CORRELATIONAL randomness (3rd category). The forum's wish (import external crafted values/scales) is
     already shipped (QMIX/quantiser/gate/pitch-blend + Scala/.dmtune authoring via Sikit/Colonnade/Duo).
     -> POSITIONING_correlational_randomness.md.
   - KEY: because the copula keeps the output UNIFORM(0,1) end-to-end (the invariant defended in the slew/mix
     correctness war), the output is a clean substrate -> apply any F^-1 at the output to reshape the MARGINAL
     per lane, correlation- + reversibility-preserving (Sklar). "Knew there was a reason to insist on uniform-
     preserving modulation." -> OUTPUT_LAYER_DISTRIBUTION_MAPPING.md.
   - Edit the CDF directly (not F^-1); PDF = slope; F^-1 = reflect+re-fit PCHIP; NO LUT (direct eval, control
     rate). 8-dist palette (all tunable + cheap inverse): Power, Mobius, Triangular, Kumaraswamy, Logit-
     normal, Logit-sigmoid, Truncated-normal, Drawn(PCHIP). Dropped Arcsine (no params), Bates/true-Beta/
     t-copula (iterative inverse). -> DISTRIBUTION_LAYER.md.
   - Precision: double (siblings of the double PhiInv/copula math layer); float at storage. No template.
   - .dmcurve = JSON (shares patch dataToJson schema); distribution = patch state (no .dmdist); shipped
     defaults + user curves all .dmcurve. -> DMCURVE_EDITOR.md.
4. BUILT IT (fast, on clean foundations): Distribution.hpp + MonotoneCurve.hpp (double, PCHIP) + contract
   tests = Phase 1, MERGED. Then the editor widget (Phase 2): square dual-axis CDF/PDF display, all dists
   render (parametric + drawn via draw-against-Distribution-interface), live param knobs, drag-to-edit,
   "convert parametric -> editable PCHIP". Iterated render-by-render (postage-stamp -> wide panel -> dual-
   axis fixing PDF clipping). Rodney confirmed drag feel + "to editable" good. -> DMCURVE_EDITOR.md.
5. WARP SCOPE + PURPOSE (reasoned carefully, several corrections):
   - PURPOSE: melody warp = per-voice handle on polyphonic CHAOS<->ORDER (global warp ~= fader adjust,
     redundant); octave warp = distributional layer of a 4-LAYER octave stack (Straits offset / Causeway mod
     / warp / copula correlation) answering the Melodicer "limited octave control" criticism. (Corrected an
     invented "warp=where/slew=how orthogonality" — slew is SHARED not per-voice.)
   - Mono value of a warp iff the lane LACKS per-outcome faders (octave/length: no faders -> mono value;
     melody: has faders -> polyphonic-only). Faders and warp are the same role, delivered differently.
   - Threshold/boolean lanes (rest, legato, accent, qmix; variation in GATE mode = binary subgate rest/
     legato): warp only reshapes a modulation-response curve -> marginal. BUT isolated-lane reasoning has a
     blind spot (rest+legato gap-distribution interaction; gate/subgate density). DECISION: add warp to ALL
     lanes EXPERIMENTALLY, FEEL it, peel back on cost + FUN; provisional cut = warp-worthy <=> visualiser-
     worthy. Reasoning sets the default; Rodney's ears set the verdict.
   - t-copula NOT needed: shared-extreme-warp + Gaussian copula ~= t-copula tail-clustering MUSICALLY,
     without the numerical pain (t-quantile unstable high df, Beta-inversion, reversibility risk). Beta^-1
     harder than PhiInv -> fine if amortizable to set-time (marginals), disqualifying in hot/reversible
     paths (copula). -> OUTPUT_LAYER_DISTRIBUTION_MAPPING.md, DISTRIBUTION_LAYER.md.
6. VISUALISER (effective-distribution): a Lantern-style display module, ROWS of VOICES (Sands transposed),
   showing effective melody + octave (+ note-length) distributions = fader WIDTHS x the Sands non-uniform
   draw, per voice, live. Serves Monsoon/Colonnades/Duo (attach to one, host-driven bar count). KEYSTONE:
   switchable UNIFORM<->TRANSFORMED view makes every layer's effect visible (depth-as-discovery). ->
   EFFECTIVE_DISTRIBUTION_VISUALISER.md.

## CORRECTIONS MADE (plausible-but-wrong caught — the discipline in action)
- polyBankIndex "the +1 is the bug" (mine, x2-3) -> wrong; it was the lying convention + lane crossing.
- "ratchet is un-gated" -> wrong (re-read doc comment not code); it's in-gate-only, structurally rest-linked.
- "warp=where / slew=how orthogonal per-voice" -> wrong; slew is shared; it's just the polyphonic point.
- float for distributions -> corrected to double (the math layer is double, not the float storage layer).
- Pattern: Rodney repeatedly caught plausible narratives; the fix was always CHECK THE CODE / FEEL IT.

## STATE AT SESSION END
HEAD ~a3d044c0 on lane-expander-refactor, CI green. War won + verified. Phase 1 merged. Phase 2 editor built.
NEXT: Phase 2.5 (Mobius + Logit-sigmoid in Distribution.hpp, remove Arcsine, param knobs, convert-to-PCHIP),
then Phase 3 (integration: warp into a lane, Macro/Sands scope, .dmcurve load/save, LIVE-PREVIEW keystone).

## WORKING MODEL (reminder): this chat = architecture + CC prompts; CC = execution (tested/mechanical only,
can't verify Rack feel); docs = the real memory; verify against code, not narrative.
