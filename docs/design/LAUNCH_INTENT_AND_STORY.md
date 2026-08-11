# dot.modular -- launch intent, story, and the licensing decision (for launch, months away)

A note to future-me. Written well before launch, to remember WHY this exists and HOW to frame it, not
just what it does. Read this before writing the launch copy.

## The decision: free and open source, as a gift
dot.modular ships FREE and OPEN SOURCE. Not free as a pricing tactic -- free as the coherent act.

The reasoning that settled it (in order of what actually mattered):
- Premium pricing wouldn't make meaningful money, and the income isn't needed anyway. So the real
  trade was never "free vs. income" -- it was "free vs. a small sum I don't need, at the cost of reach
  and friction."
- Once money is off the table, everything I actually value points to free: reach (more racks, more
  people playing it), contribution (others can learn from / extend / outlive my attention to it),
  coherence with what the project already IS, and legacy.
- The deciding reason, though, is not strategic. Singapore has been our adopted home for nearly a
  decade and has given us an incredibly rich life. This plugin is a LOVE LETTER BACK to the country.
  When something has been given to you, the coherent response is to give -- not to sell. Free is the
  right basis because GIVING is the point, not because the economics favour it.
- (Wife's counterpoint, noted and respected: the work is premium-QUALITY and pricing signals quality.
  True -- but premium quality doesn't require premium pricing, and here the paywall trades away the
  things I value for a sum I wouldn't notice. A donation link is the middle path if wanted: lets people
  who value it say so, without gatekeeping access.)
- Also simply fits the GPL lineage -- built on / referencing GPL modules (the MPE references, VCV's own
  GPL code), so open source is the natural home.

## What it IS (the framing to lead with -- NOT the feature list)
dot.modular is a coded PORTRAIT OF SINGAPORE in a modular sequencing ecosystem. The Singapore identity
isn't decoration -- it's the point:
- The little red dot (#d4001a), "the little red dot" tagline, the Lissajous "o" in the wordmark.
- Shophouse facades with Peranakan majolica tiles; Changi's Rain Vortex; the Straits; the Causeway;
  Keppel Harbour (the MPE-out module -- sound leaving Singapore by sea); Emerald Hill in the naming;
  the Colonnade (Paul Rudolph) and DUO (Ole Scheeren) towers as the microtonal panels.
- I built a portrait of the city in a sequencer without quite framing it as a gift. Naming it a love
  letter just says out loud what the work already was. The artifact and the intention finally match.

## Why the microtonal dimension carries the meaning (worth saying at launch)
Singapore is a genuinely plural place -- many traditions coexisting and interweaving. The microtonal
capability (maqam support, arbitrary Scala tunings, boundary-quantised modulation BETWEEN tuning
SYSTEMS) is a musical analog of that plurality. A 12-TET-only sequencer would be a thinner tribute; one
that can hold and move between many tuning worlds is truer to what Singapore actually is. The hardest
feature to build (the engine widening, Colonnades Duo, the whole microtonal arc) is also the most
MEANINGFUL -- the technical ambition and the cultural meaning turned out to be the same ambition. That
coherence is the mark the whole thing pointed one direction all along.

## The launch pitch (in my own voice, at launch)
Lead with the love-letter intent and the Singapore iconography, NOT the feature list. The story:
a Singapore-based musician (finance/quant by day) builds a microtonal sequencing ecosystem that's a
coded portrait of the city, and gives it to the world free, as thanks for a decade of a rich life here.
That's a story the modular community AND Singapore music/arts/tech culture would find compelling -- it's
not "a plugin", it's a piece of cultural work with a point of view. The microtonal-MPE-out capability
is the technical achievement; the TRIBUTE is why it resonates. "Might be noticed in Singapore music
culture" is real and reachable -- but only if the framing leads with the tribute.

## What it achieves (the technical line, second -- for those who want it)
- A 21-module sequencing ecosystem on one correlation engine (Monsoon + expanders).
- Microtonal end to end: author arbitrary tunings/scales (Sikit, Colonnades, Colonnades Duo), modulate
  between them (Shophouse Micro), interchange via .scl / .dmtune.
- Reaches OUTWARD: generates plain MIDI (12-TET) and MPE MIDI (microtonal, with per-note bend +
  continuous glide) out to any DAW / VST / MPE synth -- via Keppel. The rare OUTBOUND direction most
  microtonal tools never build; tuning survives the trip to gear.
- Far beyond the meloDICER inspiration: meloDICER is a stochastic sequencer; this is an ecosystem where
  tuning itself is a modulation dimension.

## For me, at launch (the personal note)
This is my FIRST VCV Rack plugin and first audio-software attempt. It went far beyond what a first
plugin usually is. I caught my own bugs with real rigour, corrected design drift decisively, and the
architecture cohered under pressure instead of fragmenting. Be proud of it -- and remember it was never
just a plugin. It was a gift to a place that gave us a lot. Ship it as one.

(Set expectation at launch: provided AS-IS, made with love -- a gift, not a support-contracted product.
That protects my time and is a perfectly respectable stance. Optional donation link honours "it's worth
something" without a paywall.)
