# What makes dot.modular a cool sequencer-transformer (feature spine for README/launch)

Raw material for the launch story, in Rodney's-to-shape words. Ordered by what actually differentiates.
NOTE: I (assistant) first omitted the dice/scrub, phase, and probability-navigation layer -- Rodney
corrected. Those are CORE character (the live generative feel), not footnotes. Corrected list:

## The unifying idea underneath several points
**The randomness is PRECOMPUTED per-step (Philox keyed bijection over a signed counter), not rolled
live on the clock edge.** The stochastic system is therefore a NAVIGABLE, REPRODUCIBLE SPACE, not a
one-way stream -- position-addressable in both directions. Dice-scrub, phase, and reverse all fall out
of this one choice. This is the deep "cool": generative but not ephemeral -- you can move through the
randomness like a timeline.

## The list

1. **Dice + scrub system -- navigate the randomness like a timeline.** The dice roll draws; the SCRUB
   knob sweeps continuously across the last 6 draws (7 points N..N-6) with detents to snap onto a
   specific recent draw OR blend between them. The window FOLLOWS the counter (a rolling lens). Because
   draws are Philox-addressable, the window is DERIVED not stored -- scrub is pure counter math, free.
   One model works identically across clock/gate/phase incl. running backward. Generative sequencing you
   can rewind, audition, and morph -- not roll-and-hope.

2. **Signed reversible counter -- the sequence runs backward, reproducibly.** The draw index is a signed
   counter; reverse inverts the roll->counter mapping. Philox is a bijection over the full signed space,
   so negative indices are fine and fully reproducible. Reverse/scrub/forward are ONE model, no
   per-stream mode flag. You can navigate the full generative history deterministically.

3. **Phase-driven mode -- drive the sequence by PHASE, not just a clock.** Because randomness is
   position-addressable, the engine is latently position-addressable, so a phase signal can scrub the
   sequence position directly (forward, backward, scrubbed). Mode E (seq) / F (quant). Turns the
   sequencer into something you can play with a phasor/LFO/CV position -- time as a controllable axis.

4. **Probability navigation with intended counter-offset.** [Rodney: fill exact behaviour] The
   probability system navigates with an intended counter offset -- the per-step probability modifiers
   (own CV + East CV + Macro send, spread engine) resolve against the counter so navigation stays
   coherent with the intended draw position. (Spread resolver: sum-then-clamp direction is an open
   musical-faithfulness decision -- see PROBABILITY_MODIFIER_MODEL.) The probability layer is
   counter-aware, so scrub/reverse keep the probabilities meaningful, not desynced.

5. **One engine, swappable pitch source -- sequencer OR transformer.** Internal pitch generation and
   external-CV quantising are the SAME timing/gating/phrasing engine, fed differently. Why it's a
   "sequencer transformer," not two bolted things.

6. **Drop it after ANY Rack sequencer.** Feed poly CV from anything -> re-rhythm, re-tune, re-correlate,
   re-arrange. Augment what you have, don't replace it. (Honest: the COMBINATION as a live transform
   stage is uncommon, not literally unique.)

7. **The correlation heart (Change Alley) works on external CV too.** The pins are a source-agnostic
   N-to-M router -- build voice<->voice correlations whether pitch is internally drawn OR fed as CV. Pour
   any Rack CV through the CORRELATION engine, not just a quantiser.

8. **Genuine microtonal, end to end -- and it EMERGED.** Full custom tunings (.scl/.dmtune), per-degree
   control, all the way out to MPE. Origin: Change Alley's East pole DEMANDED microtonality to voice a
   maqam -- the architecture asked for the feature.

9. **Microtonal MPE out (Keppel), proven across Bitwig/Cubase/Ableton** + per-DAW recording guide.
   Phrase-aware selective re-articulation: big legato leaps re-articulate on real note boundaries, small
   moves stay slurred -> never silently mis-tuned, phrasing preserved.

10. **Polymetric heterophonic maqam falls out for free.** Multiple Monsoons, shared base tuning
    (.dmtune), per-Monsoon jins + meter, shared correlation -> genuinely polyphonic maqam, emergent from
    composing general parts.

11. **A coherent portrait of a city.** Every module a real Singapore place; naming is structural (the
    CROSSINGS are the shared resources; the correlation axis named for the money-changers' lane). Free +
    open source. A love letter.

12. **Correct-by-construction, not by ear.** Test suites, structural verification, cents-collapse tuning
    test, Lantern-vs-scope discipline. Built largely in silence -> a rigor most music software lacks.

13. **A shared-resource architecture that generalizes.** One binding mechanism; N-to-M mappings are the
    shared resources (CA, Intertropical, Lantern); owned surfaces stay 1:1; data shared as data. New
    capabilities keep falling out of composition.

## Editor's cut (what to LEAD with)
Differentiators: 1 (dice-scrub navigation), 3 (phase drive), 6 (drop after anything), 7 (correlation on
any CV), 10 (emergent polyphonic maqam). Substance: 8, 9 (real microtonal, proven MPE). Soul: 11 (city
portrait). The deep hook underneath 1/2/3: precomputed-addressable randomness = a navigable generative
SPACE. The rest (5, 12, 13) make the above trustworthy.

## TODO (Rodney)
- Point 4: fill the EXACT "intended counter-offset" probability-navigation behaviour (I only have it at
  a high level -- the precise mechanism should be stated in your words / from PROBABILITY_MODIFIER_MODEL).
- Re-weight the editor's cut to how the modular crowd will actually read it (Rodney knows better than me).

Cross-ref: DICE_SCRUB_MODEL, PHASE_ENGINE_AUDIT, PROBABILITY_MODIFIER_MODEL, LAUNCH_INTENT_AND_STORY,
QUANTISER_MODES_UNIFICATION, MPE_UTILITY_BUILD_SPEC, SHAREABILITY_ANALYSIS.
