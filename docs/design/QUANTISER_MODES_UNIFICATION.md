# Quantiser modes = sequencer modes for external note CV (Rodney) -- the unification

Reframes the quantiser modes (was Vermona-style C/D) so they MIRROR the sequencer modes: each quantiser
mode is a sequencer mode, but reading EXTERNAL note CV instead of generating pitch internally. Post-V1
direction; captured from a design discussion (Rodney on commute, laptop staying home for the trip).

## Vermona original (what we're diverging from)
- C (Quantizer 1): quantises external CV (CV IN 2, 0-5V) on QUARTER NOTES (clock). Faders = scale;
  higher fader = wider quantisation range. FIXED quarter-note grid.
- D (Quantizer 2): same but sampled while external GATE IN 2 is high.

## The change: C driven by INTERNALLY GENERATED gates (not fixed quarter notes)
Make C like D, but driven by the engine's OWN generated gates instead of external Gate 2. The engine
already generates rhythms (probability-driven, any division/pattern), so C quantises the external CV at
WHATEVER RHYTHM THE ENGINE GENERATES -- 1/4, 1/8, 1/16, or any probabilistic pattern. Replaces Vermona's
fixed quarter-note grid with our rhythm engine as the sampling clock. Strictly more powerful, costs
nothing new (rhythm generation already exists).

## The symmetry (quantiser modes mirror sequencer modes)
| Sequencer | timing source            | Quantiser | same source, quantising external CV |
| A         | clock                    | (old C)   | clock / quarter notes               |
| B         | internal generated gate  | NEW C     | internally generated gates          |
| (ext)     | external gate            | D         | external Gate 2 (unchanged)         |
| E         | phase                    | NEW F     | phase (follows Mode E gates)        |
So: new C = the quantiser's Mode B (generated gates); F = the quantiser's Mode E (phase); D stays
external-gate. The quantiser side becomes the mirror of the sequencer side. Mode F completes the
symmetry (the prerelease doc already flagged the quantiser was missing the phase equivalent).

## The deep insight: timing engine + swappable pitch source
The module has always been a TIMING/GATING/PHRASING engine with a PITCH-GENERATION stage on top.
Quantiser modes just REPLACE the pitch-generation stage with pitch-from-external-CV, keeping everything
else. That's why "everything else applies as is" -- the timing/gate/legato/accent machinery is
pitch-source-agnostic (it decides WHEN/WHETHER notes happen, not WHAT pitch). Quantiser mode = keep the
timing engine, swap the pitch source.

## Consequences (Rodney)
1. **Poly quantise, mono algorithm.** quantize(vIn) is per-voltage already; iterate it across poly
   channels. The mono weighted radius-gated snap (SequencerEngine::quantize) applies unchanged per
   voice. Poly is free.
2. **Same legato rules, based on the quantiser note CV.** Legato/tie logic (hold across / re-articulate)
   keys off the EXTERNAL CV's note changes instead of generated notes. Same rules; the "note" is now the
   quantised external CV. Generated gates (new-C) provide the WHEN; external CV provides the WHAT.
   Legato in quantiser mode = incoming CV held across generated gate spans, re-articulated on generated
   gate onsets. Ties straight to the Keppel step/step-legato gate structure.
   - "Note changed?" = "quantised DEGREE changed" -> reuse degreeOf. Confirm that's the
     re-articulate-vs-hold trigger in quantiser mode.
3. **Sands melody + octave IGNORED in quantiser modes.** Melody draw + octave randomisation are
   PITCH-generation; pitch now comes from CV, so bypass them. Keep the rhythm/gate/legato/accent
   generation, drop pitch generation.
   - CAUTION (codebase risk-shape "compiles, plausible wrong value"): BYPASS melody/octave, don't just
     leave them computed-and-discarded -- an ignored-but-wired melody draw could perturb RNG state or
     lane position. Gate it out, don't compute-and-ignore.
4. **Everything else applies more or less as is** -- rest, accent, direction, lock, dice-scrub, the lane
   model all operate on WHEN/WHETHER not WHAT pitch, so they carry over. Only the pitch stage is swapped.

## Net
Quantiser modes end up like sequencer modes but for external note CV: new C = generated-gate-driven
(any rhythm), D = external-gate (unchanged), F = phase-driven (new). Poly, same legato on the CV's
degrees, Sands melody/octave bypassed, rest/accent/etc unchanged. The two halves of the module become
ONE timing engine with two pitch sources (internal draw vs external CV).

Cross-ref: MODES_C_D_QUANTIZER_PRERELEASE.md (the C=clock/D=gate starting point + the missing-phase-mode
observation), SequencerEngine::quantize (the mono algorithm), the legato/tie model + Keppel step/step-
legato gates (same phrasing structure), degreeOf (note-change detection).
