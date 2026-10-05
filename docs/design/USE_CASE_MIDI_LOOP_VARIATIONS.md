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
