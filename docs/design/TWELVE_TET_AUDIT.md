# 12-TET-baked-in audit (quick pass) -- for Monsoon Micro (12-slider + 24-slider) planning

Decision context: Monsoon Micro = a 12-slider variant AND a 24-slider variant (two FIXED panels, static
art like current Monsoon -- NO widget-drawn reflow bank). Tuning owned internally (no premium-module
dependence; separating pitches from Monsoon Micro considered but LIKELY NOT). So the real question this
audit answers: which classes hardcode "pitch has 12 semitones" and would need to become "pitch has N
degrees" (N=12 or 24) for the Micro variants.

Method: grep %12, /12, [12], "semitone", scale-mask; then read context to drop false positives.

## GENUINELY 12-TET-BAKED (must generalise 12 -> N for Monsoon Micro)
The pitch-generation + scale pipeline. These are the real work:
- GateState (.hpp/.cpp): semiPlayRemain[12] (per-semitone LED flash), lastSemitone, triggerNote(...,
  int semitone,...). The per-note state -- N-scoped.
- PatternEngine (.hpp/.cpp): semiWeights[12], pickSemitone(const float weights[12], r) "pick 0..11
  weighted by faders", pitch-gen returns semitone. THE core pitch draw. N-scoped.
- SequencerEngine (.hpp/.cpp): activeSemiList[12], activeSemiWeight[12], faderCache[12]. The active
  scale-degree working set. N-scoped.
- MonsoonScaleManager (.hpp/.cpp): scale MASK is a 12-bit field (bits 0-11 = C..B), getSemitoneWeight
  respects the mask. Scale system is 12-wide. N-scoped (mask -> N-bit).
- MonsoonParameterManager (.hpp/.cpp): semitone sliders normalised 0..1 (12 of them), per-lane pitch
  mod indices 0..11 = semitones / 12 = octLo / 13 = octHi. Transpose -12..+12. N-scoped (+ the 12/13
  octave-index sentinels shift to N/N+1).
- MonsoonCVRouter (.hpp/.cpp): "quantize CV to nearest semitone" -- quantiser is 12-grid. N-scoped.
- Monsoon.cpp/.hpp + MonsoonWidget.cpp: the 12 SEMI params/sliders (SEMI0..SEMI11) + panel binding.
  The Micro variants = new param banks (12 and 24) + new panels. The panel/param layer.
- MonsoonShophouseExpander.cpp: centres[12]/rects[12] = the 12 shutter click-zones (root select);
  root-on-clicked-semitone. 12-wide root selector -> N-wide.
- ScaleList.hpp / NoteValues.hpp: the scale interval tables + note names, 12-based. For Micro these
  become tuning-degree tables (or the tuning defines them).

## PITCH-12 but DISPLAY-ONLY (follows once the core is N; renderer change)
- Lantern.cpp: N[12] note-name array + volts*12 -> semitone -> row (the piano-roll keyboard mapping).
  This is the pitch-axis RENDERER -- already flagged: pitchV is a voltage, so swapping keyboard ->
  cents-ruler/degree-lanes is draw-code. Follows the core; not engine work.

## NOT 12-TET (false positives -- NO change). Confirmed pitch-agnostic:
- StraitsSandsMacroVisual [12] = 12 CV-DEPTH KNOBS (one per fader) -- coincidental count, not pitch math.
- ClockEngine "6/12/24" = PPQN timing subdivision, NOT pitch.
- SandsGrid "12" = a comment number.
- Intertropical: "semitone" only in the TRANSPOSE knob (-24..+24 semis). For Micro this becomes the
  transpose UNIT question (octave-restrict decided) -- bounded, already noted. Not core pitch.
- All SANDS / Raffles / Junction / Causeway / Straits / Changi / Change Alley: no pitch-12 (operate on
  gates/probability/routing/voice-indices). Confirmed earlier + by this grep (absent from the genuine
  list).

## Summary
The 12-TET assumption is CONCENTRATED in one pipeline: [faders -> ParameterManager -> semiWeights ->
PatternEngine.pickSemitone -> GateState note -> CV out] + [ScaleManager mask + ScaleList tables +
Shophouse root select] + [Lantern render]. ~10 classes, all in the pitch/scale path, none in the
expander ecosystem. Generalising "12" -> "N" through that pipeline (N=12 and N=24 param banks + panels)
is the Monsoon Micro build. Contained, but touches the CORE draw (PatternEngine/GateState) = the
high-regression-risk part -> POST-LIBRARY. Everything outside this pipeline is already N-agnostic.
