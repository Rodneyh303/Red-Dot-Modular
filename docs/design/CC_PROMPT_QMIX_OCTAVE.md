# CC PROMPT  QMIX OCTAVE: a second Qmix read on Straits (degree vs octave as two axes)

Design (Rodney, decided). Build on lane-expander-refactor.

## WHAT QMIX IS
Qmix is the per-note crossfade between the GENERATED pitch source and the CV input (the generative-
sequencer-quantiser). Today ONE Qmix value chooses both the within-octave DEGREE and the OCTAVE together.

## WHAT EXISTS (grounding)
- genPitchLive (PatternEngine.cpp:282) already takes SEPARATE randoms: r_semi (degree, via pickSemitone)
  and r_oct (octave). The two axes are already split at GENERATION.
- What is NOT split: the Qmix crossfade (generated vs external) applies one Qmix value upstream of both.
- Qmix has its own Philox stream (seedQmixPhilox). Per-voice Qmix level mirrors rest/accent.
- IMPLEMENTATION NOTE for CC: locate the exact site where the Qmix value crossfades generated vs external
  pitch BEFORE coding. The two thresholds must act there. Trace it; do not assume.

## THE MODEL (like subgates: multiple reads of one stream, different thresholds)
Split the single Qmix crossfade into TWO independent reads of the SAME Qmix stream:
- QMIX DEGREE (the existing Qmix): threshold for the within-octave degree  generated vs external.
- QMIX OCTAVE (new, on Straits): threshold for the octave  generated vs external.
Both read the same Qmix random, each against its own threshold  the subgate pattern.

## DEFAULT (bit-identical gate)
- Monsoon alone, no Straits: octave follows the SINGLE Qmix exactly as today. One knob, both axes.
- Straits attached but the Qmix-octave knob UNTOUCHED: octave behaves exactly as the single Qmix. Attaching
  Straits changes NOTHING until the new knob moves.
- Golden battery bit-identical in both cases  the gate. No re-pins.

## CONTROLS (the Straits subgate pattern)
- GLOBAL: a Qmix-octave knob on the Straits base (the global octave threshold).
- SUBPANEL: per-voice Qmix-octave, with the three-way source resolution (own-subpanel > {global | follow-
  degree-Qmix}) and the SAME Macro/East delegation as the other Straits subpanels. Follows the existing
  subgate-knob subpanel build  reuse it, do not invent a new mechanism.
- The default follow is "follow the degree Qmix", so an unset octave threshold reproduces today's behaviour.

## THE MUSICAL CASE (the design target  for the test loadout)
One external theme comes in as the DEGREE pattern; voice groups take that shared degree but generate their
OWN octaves (and, with subgates, their own note lengths): a bass pinned low, a lead high, a pad mid, all
tracking the one melody. Pin a loadout that demonstrates: shared external degree + per-voice-divergent
generated octave via per-voice Qmix-octave thresholds.

## STAGES
- STAGE 1: split the crossfade into two reads (degree + octave), octave threshold = degree threshold by
  default. Bit-identical. The split with no new control is the proof.
- STAGE 2: the global Qmix-octave knob on Straits base. Default follow = degree-Qmix  bit-identical until moved.
- STAGE 3: the per-voice Qmix-octave subpanel (reuse the subgate-knob subpanel), three-way resolution,
  Macro/East delegation.

## GATE
- <=16 goldens bit-identical at every stage with the octave threshold following degree. GOLDEN_A untouched.
- New pins: octave threshold at the generated extreme -> octave always generated; at the external extreme ->
  octave always from CV; mid -> crossfade. The musical-case loadout (shared degree, divergent octave).
- run_all green (both SDK legs).
