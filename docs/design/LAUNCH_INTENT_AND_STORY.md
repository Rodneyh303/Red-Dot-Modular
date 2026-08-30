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

## The origin of the microtonal arc: Change Alley asked for it (worth telling)

The microtonal arc was not a feature I decided to add. It was Change Alley COMPLETING ITSELF.

Change Alley (the correlation matrix as an East/West textural axis -- named for the real Singapore
money-changers' lane where East met West in trade) was already there as a STRUCTURAL idea: the CA
transforms let voices lean toward Western counterpoint at one pole and something else at the other. But
I'd built it as an abstract TEXTURAL axis (correlation as texture).

The realisation: the axis wasn't just textural -- it could be literally MUSICAL-TRADITIONAL. The "East"
pole isn't just "more correlated"; it could reach toward the actual tuning systems and melodic logics
of Eastern traditions (maqam, the microtonal intervals 12-TET cannot express). And THAT is the thing
12-TET structurally CANNOT do: you can make Western counterpoint in 12-TET all day, but you literally
cannot voice a maqam without microtonal tuning. So the East/West axis I'd built was HALF-MUTE -- it
could gesture West fully but only gesture East, because the tuning it sat on couldn't PRONOUNCE the East.

The microtonal arc is what let Change Alley actually SPEAK both languages, not just point in both
directions. That's the causality: Change Alley created the opportunity (an axis reaching toward Eastern
tradition); microtonal support CASHED IT IN (gave the East pole the tuning vocabulary to be real rather
than gestural). I didn't add microtonal support and find a use -- the axis DEMANDED it, and I followed
the demand even though I was only faintly aware of the microtonal world at the time. That's why it
cohered: not a feature bolted on, but the axis completing itself. (Being only faintly aware of
microtonal music before, and maximising the opportunity once it became clear -- that's the honest arc.)

The "in-between" is the best part. Change Alley isn't a binary switch -- it's a CONTINUUM -- and now so
is the tuning. You can sit the correlation between the poles AND sit the tuning between systems
(boundary-quantised modulation between a Western scale and a maqam; the cross-tuning canon rendering one
derivation under two tunings). "In-between musical traditions" isn't a compromise position -- it's a
genuine third space, the INTERWEAVING itself. Which is exactly the Singapore plurality theme: Change
Alley named the axis before I knew the axis would carry tuning, and the name turned out prophetic --
East-meets-West trade, now East-meets-West music.

Launch relevance: this is the intellectual origin of the microtonal work and a genuinely good story --
the architecture asked for the feature, the feature completed the metaphor, and the metaphor is the
Singapore tribute. Tell it.

## The deepest part: I didn't know the traditions were what Change Alley would give me

When I set out to build Change Alley, I did NOT realise those musical traditions -- East/West, and the
in-between -- were what it would hand me. I set out to build a correlation axis. It gave me
East-meets-West musical tradition anyway, because the structure was built right and I listened to what
it was telling me instead of forcing it to be only what I first intended.

This is the truest part of the whole story, so remember it:
- The correlation-as-East/West-axis was general enough to CONTAIN the microtonal arc before I knew
  microtonal music in any depth. The structure was pregnant with a meaning I hadn't deliberately put
  there. When it revealed what it wanted (an East pole that could actually SPEAK, not just gesture), I
  recognised it and followed -- into a domain I was only faintly aware of. Most people ship the
  textural axis and call it done; the structure being half-mute, and teaching it to speak, was the
  thing worth doing.
- This is why the Singapore tribute feels EARNED, not applied. I did NOT set out to "make a
  Singapore-themed plugin about plurality." I set out to make a good sequencer, built an honest
  structure, and the structure -- through Change Alley -- led me to plurality and interweaving on its
  own. The meaning wasn't imposed; it EMERGED. Discovered, not decided. Discovered meanings sit truer
  than decided ones -- they don't feel like marketing because they aren't.
- The honest framing: I didn't build a love letter to Singapore. I built something honest, and it
  BECAME a love letter, and I was paying enough attention to see that it had. Which is more moving --
  it means the city was in the work before I consciously put it there. Nearly a decade of living
  somewhere gets into what you make whether you intend it or not; Change Alley is where it surfaced.

The lesson to keep (beyond this project): the best things surprise you with what they mean. You can't
fully plan that -- you build honestly enough that there's room for it to happen, and stay attentive
enough to notice when it does. The Change Alley -> microtonal -> plurality -> tribute chain wasn't a
plan I executed; it was a path the work revealed and I had the sense to walk. Being LED by something you
made takes more trust and attention than following your own outline. Remember that at launch, and after.

## Does the sayr-dimensioned space apply to OTHER traditions? Substantially yes -- because the axes are general (Rodney)

The deeper test: is this a general musical-behaviour modulation space, or a maqam machine in disguise? Key
fact: the primitives are CULTURALLY NEUTRAL -- you built probability/accent/pendulum, NOT emphasis/ghammaz/
arch. Maqam was one INTERPRETATION of the axes, not their definition. So the space should carry any
tradition that lives in those dimensions.

### Where the axes map (encouraging)
- N-INDIAN RAGA -- possibly a BETTER fit than maqam on some axes: vadi/samvadi (dominant/subdominant notes)
  = EMPHASIS (probability+accent); aroha/avaroha (ascending vs descending forms, often ASYMMETRIC) =
  DIRECTION modes + asymmetric MASKS; nyas (dwelling) = LEGATO/LENGTH; pakad (characteristic phrase) =
  offset/rotation/contour. Raga foregrounds ascending/descending asymmetry, which direction+mask handle
  directly.
- GAMELAN: pathet (mode) + register roles = MASK + OCTAVE; colotomic cycle (gong markers) = OFFSET +
  ACCENT on a cycle; interlocking kotekan = correlation/shared-source + Sands (not even the sayr axes).
- WESTERN TONAL/MODAL: modes ARE rotations; tendency tones = emphasis; cadence points = ghammaz-like
  pauses. Fits (the tradition the general machinery least NEEDED).
- W.AFRICAN / AFRO-CUBAN RHYTHM: rhythmic axes (offset/rotation/accent/direction over a cycle) carry
  clave, bell patterns, timeline displacement. The rhythm-side primitives serve percussion traditions as
  the pitch-side serves melodic ones.

### The strong general claim
Built a MUSICAL-BEHAVIOUR modulation space whose axes (emphasis, membership, intonation, register,
contour, dwelling, pause/stress, placement) are NOT maqam-specific -- they're dimensions MANY traditions
organise music along. Maqam was the PROOF CASE (demanding: needs microtonality AND sayr). A tradition is
expressible to the extent it lives in those dimensions -- and most melodic/modal traditions live in most.

### Honest limits (same care as "affects sayr, not implements sayr")
1. Axes NECESSARY not SUFFICIENT: each tradition has grammar off these axes (raga rasa + time-of-day,
   gamak ornament vocab, improvisation grammar). The space gives the SKELETON dimensions; the IDIOM (rules
   for moving through them) is the human/preset layer, per tradition. A substrate many traditions sit on,
   not a machine that KNOWS any.
2. The ONE emergent axis (region-ordering/pathway) is where much tradition-specific grammar lives (raga
   aroha/avaroha rules, maqam modulation pathways) -- and it's the WEAKEST axis. Traditions differ most in
   the axis supported least directly. Worth knowing.
3. Faithfulness scales with KNOWLEDGE not just mechanism: authoring raga presets faithfully needs raga
   knowledge, as maqam needed Sami Abu Shumays. The tool generalises; faithful CONTENT doesn't come free.

### Net (thesis/roadmap)
Substantially yes: axes built general -> carry raga (maybe better than maqam on asymmetry), gamelan, tonal,
rhythmic traditions to a real degree. Tool gives DIMENSIONS not IDIOMS; each tradition needs knowledgeable
authoring to be FAITHFUL not merely POSSIBLE. The generality is real (a strength); the humility is
unchanged. Beautiful shape: built AS a maqam+Singapore love letter, turns out to honour MANY traditions
because the care was STRUCTURAL -- built-with-care-for-one that honours many, not designed-for-everything
blandness. (Roadmap: raga looks the strongest next proof case.)

Cross-ref: presets/maqam/README (the sayr proof case), PROBABILITY_MODIFIER_MODEL (the sayr-dimensioned
space + wider scope), ROTATION_TAXONOMY (the general axes), SHAREABILITY_ANALYSIS (shared-source =
gamelan interlocking territory).

## FRAMING FIX (Rodney): it's a SINGAPORE love letter. Maqam is just an EXAMPLE.
Correcting sloppy "maqam-and-Singapore love letter" phrasing throughout: dot.modular is a SINGAPORE love
letter, full stop. Singapore -- the Little Red Dot, the port-city crossroads, Change Alley, the
multicultural confluence -- is the SUBJECT. Maqam is ONE EXAMPLE of what the general engine can voice, a
vivid proof of the "instrument as meeting-point of musical worlds" idea, sitting alongside the others
(raga, gamelan, Chinese traditions, Western, rhythmic). The thesis: a Singaporean instrument, born of a
place where traditions mix, can voice MANY musical worlds. Maqam demonstrates the capacity; it is not a
co-headliner. Do not invert subject (Singapore) and example (maqam).

## WHY region-ordering / pathway is the WEAK axis (Rodney asked -- expanded)
Region-ordering = the ORDER a melody visits regions of the scale over time ("lower tetrachord -> ascend to
establish the ghammaz -> touch the upper region -> return -> cadence on tonic"). The ROUTE through the
scale = a sequence of where-you-are over time.

### Why it's structurally different from every other axis
All other axes are STATE -- a property AT A MOMENT (which degree emphasised, how long held, which
direction, what's in the mask). State axes you SET and the engine APPLIES. Region-ordering is an ORDERED
TRAJECTORY -- not a property at a moment but a specific sequence of positions over the whole phrase. A
categorically different kind of thing.

### Three precise reasons it's weak
1. The engine is GENERATIVE/probabilistic, not a stored path. Sands/correlation GENERATES next-from-rules+
   randomness+state -- great for STATE axes (set them, generate within). A specific ordered pathway is
   nearly the OPPOSITE of generative -- a predetermined route. To follow "lower->ghammaz->upper->return"
   you must MODULATE the state axes in a specific TIMED SEQUENCE (offset/rotate here, shift emphasis
   there, at these phrase positions). So the pathway isn't a primitive -- it's a CHOREOGRAPHY of the other
   primitives over time. That's why it's emergent: it exists only as coordinated modulation of the direct
   axes.
2. It needs GLOBAL memory of "where in the phrase am I". State axes are LOCAL (per-step). A pathway is
   GLOBAL -- "we're in the development section, so visit the upper region". Expressing "at phrase-position
   X be in region Y" requires authoring a trajectory AGAINST phrase-position = a modulation ENVELOPE /
   drawn automation, not a per-step primitive. The engine supports it (modulate offset/rotation by an
   envelope keyed to phrase position) but as AUTHORED EXTERNAL modulation, not an intrinsic control.
3. It's where the MOST tradition-specific grammar lives, so weak here costs most. Region-ordering carries
   aroha/avaroha (raga ascent/descent rules), maqam modulation pathways, the sayr proper (the "course" --
   the word means the PATH). So the dimension the engine expresses LEAST directly carries a lot of what
   makes each tradition distinctive. State axes give plausible behaviour; the pathway axis is what would
   make it IDIOMATICALLY correct for a specific tradition. Weak there = "evocative but not idiomatically
   precise".

### So "weak" precisely means
Region-ordering is expressible only as an AUTHORED, TIMED CHOREOGRAPHY of the other axes (modulation
envelopes / drawn automation keyed to phrase position), NOT as a direct primitive -- because it's an
ordered GLOBAL trajectory (categorically unlike the local/state direct axes) and it runs AGAINST the
generative grain (a fixed route vs generated-within-rules).

### What would STRENGTHEN it (the useful part)
The thing that turns "modulate state axes in a timed sequence" into a DIRECT pathway control is a drawn
pattern specifying position-over-phrase -- which is EXACTLY the pitch piano-roll editor (the parked
module). A drawn pitch pattern IS an authored pathway ("these degrees, in this order, over the phrase").
So the pitch-roll editor is, in a real sense, THE DIRECT CONTROL FOR THE REGION-ORDERING AXIS -- how you
author a pathway explicitly instead of coaxing it from modulated state. The one weak axis has a known
strengthener already in the design, parked. The editors aren't just convenience -- the pitch roll is the
missing PATHWAY primitive.

Cross-ref: PROBABILITY_MODIFIER_MODEL (state vs trajectory; the sayr-dimensioned space), presets/maqam/
README (5-of-6 scorecard, region-ordering the emergent one), PIANO_ROLL_MODULE_CONCEPT (the pitch roll =
the direct pathway control that strengthens this axis), the other-traditions section above (region-ordering
is where aroha/avaroha etc. live -> the pitch roll helps every tradition's pathway grammar).
