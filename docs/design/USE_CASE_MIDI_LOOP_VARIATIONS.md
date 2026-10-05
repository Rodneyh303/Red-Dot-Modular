# Use case / demo — "feed a MIDI loop, keep the groove, vary everything else"

A strong, self-explaining use case that composes from the existing architecture with NO new feature.
Likely the best way to LEAD when presenting the instrument (familiar input, obviously-musical output),
with the generative-quantiser depth as what people discover underneath.

## The chain
- A **poly MIDI loop** (drum pattern, riff, anything multi-channel) comes in.
- **One channel = the main GATE** → defines the rhythm/groove skeleton (the "when"). Gate mode preserves
  its timing, so the FEEL is kept.
- **Other channels = SUBGATES** (ratchet in-gate / ghost in-gap) → the loop's own internal detail
  drives the subdivision/fill layer.
- The **generative apparatus applies on top**: variation perturbs notes, rest/legato reshape
  articulation, pitch generate↔quantise (q-mix) decides the "what", correlation spreads across voices.
- **Intertropical remaps** the voices → same groove, mapped differently → ensemble/heterophony, not just
  a varied loop.

## Why it's compelling (the inversion)
Most generative tools make patterns FROM SCRATCH. This takes a groove you ALREADY HAVE and like, uses
it as the rhythmic ARMATURE, and generates variations ON it — keeping the feel (gate timing carried
through) while the notes/articulation/voice-mapping evolve. Humans are good at grooves; let the loop
supply the groove, let the instrument supply the variation + ensemble texture. Direct expression of the
thesis "makes any deterministic sequence stochastic" — a MIDI loop is the MOST deterministic input, and
q-mix lets it slide from "quantise the loop's pitches" to "generate new pitches on the loop's rhythm".

## Ways to get the subgates (demo/patch options)
1. **Other MIDI channels of the loop** — the loop's own internal detail as ratchet/ghost. Elegant;
   depends on those channels producing gate shapes the subgate inputs read well (verify once subgates
   are solid — may need conditioning).
2. **A Rack SEQ or function generator** — drive ratchet/ghost from a separate clock/sequencer/FG for
   deliberate subdivision independent of the loop.
3. **Rack GATE DELAYS / BURST GENERATORS on the incoming gate** (Rodney — the right answer, use these).
   The ecosystem already has excellent gate manipulators — burst generators, gate delays, clock
   dividers/multipliers, Bernoulli gates (Count Modula, Bogaudio, ML, Stoermelder, etc.). Feed the
   incoming gate into one to manufacture a ratchet/ghost subgate grid. This is BETTER than a bespoke
   module: less to build/maintain, meets users where they are (they own + know these), keeps
   dot.modular focused on what's unique (the generative-quantiser core), and composes naturally
   (burst gen -> ratchet subgate; gate delay -> ghost subgate).
   **Do NOT build a dot.modular gate chop/delay utility** — it's commodity functionality Rack does
   well; the external route is the intended answer, not a fallback. Same discipline as "gate mode
   takes external sequences": build what only we can (the copula generative-quantiser), use the
   ecosystem for commodity gate utilities.

## Demo value
Self-explaining: "here's a drum loop / MIDI riff you know — patch it into gate mode — now listen" and it
varies, grooves, spreads across voices, remaps. No model knowledge needed; input familiar, output
obviously MORE than the input. Reframes the instrument from "generative sequencer" (niche/intimidating)
to "turn your loops into living arrangements" (immediately desirable). Lead with this; let the
copula/quantiser depth be the discovery underneath.

## To verify before relying on it in materials
- Subgate-from-second-MIDI-channel produces a musically-sensible ratchet/ghost layer (or needs
  conditioning).
- The groove FEEL genuinely survives the variation layer (gate timing preserved, as designed).


## Standout patch — Befaco Burst (probability-driven ratchets) fed by Sands PROB-OUTS
Befaco Burst (VCV clone of the hardware; a trigger->burst generator) is an ideal subgate source: feed
it a gate, it emits a BURST of ratchet triggers, with a PROBABILITY input controlling how likely each
sub-trigger fires ("an organic chain of events"), plus count/time-shape.

**The clever bit (Rodney): feed Burst's PROBABILITY CV from Sands' PROBABILITY OUTS.** Then:
- Burst manufactures the ratchet subgate grid from the incoming gate, AND
- which sub-pulses fire is driven by the generative engine's OWN per-voice probability field (via the
  prob-outs added to every lane in the Sands consolidation).
So the ratchet DENSITY isn't a fixed grid or an independent thinning — it's coupled to the SAME
probability structure driving the pattern. The subgates breathe WITH the generative field.

This is a self-referential loop: dot.modular generates probabilities -> exports via prob-outs ->
modulates Burst -> Burst's ratchets feed back in as subgates -> engine subdivides/varies them. The
probability field reaches OUT into the ecosystem and shapes an external module that feeds back as
rhythmic material.

**Retroactively validates the prob-outs** (which looked like completeness-for-its-own-sake in the
consolidation): here's the concrete, musical, non-obvious payoff — the generative probability field
driving an external probability-based module. A distinctive patch, not commodity. Good demo material:
"our probability drives the ratchets, so the subdivisions thin and thicken with the pattern itself."


## Experiment — INVERT rest probability -> Burst (rests become fills)
Rodney, worth experimenting: feed Burst's probability from the INVERTED rest prob-out, so
`burst_prob = 1 - rest_prob`. Then **where the main pattern RESTS, the subgates BURST** — the gaps fill
with ratchet activity exactly when the main voice goes quiet. Self-balancing density: busy main line ->
sparse bursts; lots of rests -> busy bursts. This is how a drummer fills (main pattern lays out -> put
in a roll). Rests stop being just silence and become INVITATIONS for subgate activity.

Kin to GHOST PROTOCOL (which already fills gaps with ghost notes): two complementary gap-fillers —
ghosts (internal, variation-gated) and inverted-rest->Burst (external, ratchet bursts). Both turn
"where the main pattern rests" into "where other activity happens". They can reinforce or be split
across the gaps (a patching choice) — watch for dense double-fills if both fire in the same gaps.

General principle this reveals: **any exported probability can be INVERTED, and the inversion often
has an opposite-but-complementary musical meaning** ("do the opposite where X is likely"). A little
algebra of complementary density from a simple CV op.

Practical:
- **Invert externally** — 10V-x, an attenuverter at -1 + offset, or Burst's CV polarity. No build
  needed to TRY it. (A context-menu "invert" on the prob-out would be a nice small touch later, not
  required.)
- **Probability-driven vs actual-rest-driven:** burst density from rest PROBABILITY (likelihood,
  smooth) vs from actual RESTS (only where a rest really happened, precise gap-fill). Probability is
  just the CV (simple); actual-rest needs a "did this step rest" gate output. Try both.
- Watch the main-gate interaction: high rest prob but the step still FIRES -> main note + high burst
  together (maybe too much). Actual-rest-driven avoids this; probability-driven smooths it.
