# Positioning — subtractive / additive / CORRELATIONAL randomness; and the tuning ecosystem
Prompted by Mahlen Morris (StochasticTelegraph) VCV forum post "Subtractive and additive randomness"
(2026-10-15): most generative modules are SUBTRACTIVE — generate complex randomness, carve it with knobs
(quantizers/prob gates = filters). He proposes ADDITIVE randomness (sum shaped distributions) + letting
modules IMPORT user-crafted values / scales from outside the knobs. Independent community member reasoning
about where randomness-modules SHOULD go — and landing on things dot.modular already ships, more developed.

## The three categories (the framing)
- SUBTRACTIVE: filter internal randomness down (quantizer, prob gate). The default everywhere.
- ADDITIVE: sum shaped random distributions up (his Chances example).
- CORRELATIONAL (dot.modular / Monsoon): shape the DEPENDENCE STRUCTURE between random values while
  PRESERVING their marginals — the copula. Cross-voice (spread/CA) + temporal (slew, ρ=r, OU bridge) +
  the 2D voice×time structure. Neither carving nor summing: sculpting the JOINT distribution. A third,
  more sophisticated axis than the post reaches toward.

## The forum WISH → dot.modular already SHIPS it (and more)
His "import user-crafted values instead of internal source":
- QMIX — CONTINUOUS blend internal-generated ↔ external-quantised (he proposed binary connected/not; we
  have the continuous knob).
- Quantiser mode — quantise EXTERNAL pitch CV through the instrument's rhythm/timing/correlation structure
  (= his "slips but with external scales/values").
- Gate mode — external gate drives the step (external source = WHEN, engine = WHAT/how-correlated).
- External pitch blend + gate modifier — blend own-crafted external values on pitch/gate axes.
His SPECIFIC example "scales/keys not on the parameter knobs":
- Scala (.scl) + .dmtune import — arbitrary tunings, 12-TET or microtonal.
- BEYOND the wish — a full TUNING TOOLCHAIN: Sikit / Colonnade / Duo TAKE, CREATE, and MODIFY Scala
  tunings and SAVE user tunings (.dmtune, 12-TET or microtonal). Author → edit → save → feed into Monsoon's
  copula-correlated generative/quantising engine. The post wishes for the input port; we have the input
  port + the tuning authoring tools + the save format + the correlation engine downstream.

## Net positioning
dot.modular isn't "another generative quantizer" (subtractive) or "another probability summer" (additive) —
it's a CORRELATIONAL generative instrument ecosystem: author microtonal tunings, feed them through a copula
engine that blends your own crafted sources at every axis (pitch/gate/timing/scale) with internally-
generated, correlated material. The forum post is reaching toward one corner of what this already is.
Independent validation that the external/internal-blend + external-scale direction is recognised-as-valuable,
not idiosyncratic.

## FUTURE / HARD — accepting USER-SUPPLIED PROBABILITY (Rodney: "would be hard")
The post also implies importing user-crafted PROBABILITY (not just values/scales). For dot.modular this is
HARD, and the reason is the copula itself:
- Our randomness isn't a free uniform stream a user can just substitute — it's Philox draws passed through
  PhiInv into a correlated z-domain, with the dependence structure (slew/spread/CA) and REVERSIBILITY
  (position-keyed, 1e-15 round-trip) baked in. The marginals and the copula are coupled through the whole
  pipeline.
- A user-supplied probability/value stream would have to enter WHERE in that pipeline? If pre-copula (as
  the raw uniforms), it breaks reversibility (external stream isn't position-addressable / re-derivable) and
  the copula would correlate arbitrary external values in ways with no guarantees. If post-copula, it's just
  a blend (QMIX already does that for the quantised source). The hard part is injecting EXTERNAL randomness
  that the copula then correlates WHILE keeping marginals + reversibility — external data has no Philox
  counter, so scrub/reverse/offset can't regenerate it.
- So: importing external SCALES/values (done) and BLENDING external quantised material (QMIX, done) are
  tractable; importing external PROBABILITY DISTRIBUTIONS into the correlational engine is hard because it
  fights the counter-based reversibility + the marginal/copula coupling. TBD if there's a clean entry point
  (maybe: external stream as an ALTERNATIVE marginal the copula maps onto, accepting it's non-reversible
  while external-driven — a "live, non-scrubbable" mode). Park as a hard open question.
