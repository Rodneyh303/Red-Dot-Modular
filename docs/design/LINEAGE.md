# Lineage — what dot.modular borrows, and the one move that unifies it

Written after the Change Alley design settled, because the shape is clearer now than it
will be in six months. Useful for the README and release copy.

## The four sources

- **Vermona meloDICER (+ its MEX expander)** — the stochastic core: per-lane probability
  for rest / pitch / octave / accent, dice as the gesture that re-rolls material, and the
  expander idiom of extending the instrument by attaching modules rather than growing one
  panel. Monsoon's engine and the whole expander chain descend from this.
- **EMS VCS3 / Synthi pin matrix** — the interface: a square grid where a pin is a routing
  decision you can see, and the board is a readable picture of the patch's topology.
  Change Alley is this, rendered as a 16×16 with two pin colours.
- **Reaktor Blocks Shift Sequencer** — the LANE concept: parallel lanes for note, octave,
  slider etc., each advancing with its own behaviour, so a "step" is a vertical slice
  across independent lanes rather than one value. Our six strands (melody, octave, rest,
  accent, variation, legato) are the same idea — but the lanes carry PROBABILITY, not
  values.
- **Clock/gate and phase drive** — the two ways a Rack sequencer is driven, both of which
  we support first-class (see §Determinism).

## The unifying move: every idiom lifted one level of abstraction

Each borrowed gesture operates one level up from where it normally lives — on
*probability* rather than on values:

| Idiom | Normally acts on | Here acts on |
|---|---|---|
| Pin matrix (EMS) | signal routing | which random STREAM a voice consumes |
| Shift/rotate (Reaktor) | values along a lane | which SOURCE each voice reads (the correlation topology) |
| Dice (meloDICER) | the notes produced | the probability TABLES behind them |
| Lanes (Reaktor) | note / octave / slider values | per-strand probability |

This is why dice and pins stay orthogonal: dice re-rolls MATERIAL, pins re-arrange
RELATIONSHIPS, and neither disturbs the other (CHANGE_ALLEY_DESIGN §3, §10). It is also
why the results don't feel derivative — at the value level these gestures give you a
specific pattern; at the probability level they give you CHARACTER THAT SURVIVES
RE-ROLLING. You compose tendencies, not notes.

## Determinism: Philox enables TWO independent axes of roaming

Counter-based (stateless, addressable) random is not an implementation detail — it is the
enabling condition for both of the ways you navigate this instrument. A sequential PRNG
supports neither, because it has no addressable position.

1. **Time — phase drive, reversible mode.** A DAW phase ramp can scrub FORWARD AND
   BACKWARD through the sequence, and the draws at any position are reproduced exactly,
   because each draw is indexed by counter rather than generated in sequence. Most
   stochastic sequencers cannot be scrubbed at all. (See LANE_DIRECTION_REVERSE.md; the
   per-stream signed-index design replaced the abandoned "audition tape" approach for
   exactly this reason.)
2. **Material — clock/gate drive, normal mode.** Dice forward and backward-dice let you
   ROAM the probability space itself, revisiting previous table states deterministically
   rather than losing them. Backwards-dice is only possible because a previous draw state
   is addressable, not consumed.

So: **one axis roams time, the other roams material, and both are reversible because the
randomness is addressable.** That pair is the plugin's real technical signature.

## Where the identity shows up in the panels

The Singapore frame (see the panel design docs) is not decoration bolted on afterward —
it maps onto the architecture: Change Alley (the money-changers' alley) for an exchange
matrix, the MRT map's six lines for six strands (IDEAS_PARKED), the Lantern for the
piano-roll readout. The instrument's structure and its imagery were designed together.
