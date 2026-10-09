# Modulation canvas — the instrument as a CV-modulation target (design intent + demo direction)

NOT a feature to build — a CONSEQUENCE of the architecture worth recognising, demonstrating, and
ensuring stays possible: the instrument's controls are CV-modulatable, so external flexible modulators
(e.g. MindMeld ShapeMaster Pro — multi-channel drawable envelopes/LFOs synced to clock, or any
step/function sequencer) can AUTOMATE THE EVOLUTION of the generative music.

## Two modulation LAYERS
### 1. Generation axes — modulate WHAT IS DECIDED
- Spread / correlation -> VALUE relationships.
- LOR / voice-length -> TIMING / METER.
- Mask -> PRESENCE / structure.
Modulating these changes what happens (more radical; can shift coherence). The per-step mask-rotation
anchoring (see UNIFIED_ADDRESSING_SYSTEM.md) is exactly what makes ShapeMaster-driving-mask-rotation
behave coherently.

### 2. Realization controls — modulate HOW DECISIONS -> OUTPUT (often MORE coherent)
The probability-reactive / voicing stage. Modulating these keeps the generative SKELETON (the probability
decisions) stable while the TONAL / REGISTRAL / ARTICULATION SURFACE evolves — coherent development
(recognisable pattern) with harmonic/registral/textural motion.
- **note faders, octave faders — GLOBAL (NOT polyphonic):** modulating these moves the WHOLE ENSEMBLE's
  pitch-mapping / register TOGETHER -> coherent ensemble harmonic + registral motion on a shape.
  (Global is musically RIGHT here: you want ensemble pitch coherence, not voices in random keys/octaves.)
- **rest, legato, qmix — PER-VOICE (Straits poly versions), modulatable:** drive each voice's
  sparseness / articulation / quantise-mix on its OWN shape (ShapeMaster Pro channels) -> independent
  per-voice textural/articulation development.

So the split is musically sensible: **harmony/register evolve TOGETHER (global), articulation/density
evolve PER-VOICE (poly).** Over a stable generative skeleton -> the ensemble moves harmonically as one
while individual voices breathe in density/articulation.

## Three-level stack this enables
GENERATE (the engine) -> COMPOSE the structure (the four axes) -> AUTOMATE the composition over time
(external modulators on the axis + realization controls). A meta-composer for the generative ensemble.

## Why it's deeply MODULAR (on-brand)
No built-in automation/mod-sequencer (would bloat the module). Own the unique core (generative engine +
composed axes + realization), EXPOSE it as CV, let the ecosystem's best modulators (ShapeMaster Pro,
step/function sequencers) drive it. Same philosophy as length-via-Intertropical and long-notes-via-
linked-instances: compose from the ecosystem, don't build everything in.

## DESIGN-INTENT CHECK (keep possible through release)
- Ensure the musically-key controls stay CV-ADDRESSABLE with modulation-friendly ranges:
  generation (spread/LOR/mask), realization global (note/octave faders), realization per-voice
  (rest/legato/qmix poly).
- Confirm the Big 6 / quantiser-scale controls' musically-valuable parameters accept CV where sensible
  (some may be discrete/UI-only — check which matter).

## DEMO / MANUAL goldmine
"Patch ShapeMaster Pro into Monsoon/Straits: draw ensemble-wide harmonic/registral motion on the
note/octave faders, AND per-voice articulation/density evolution on the poly rest/legato/qmix — over a
stable generative pattern." Immediately shows depth + ecosystem fit (the params are real CV targets).
