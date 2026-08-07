# dot.modular -- patchability & what's genuinely novel (pitch-ready material)

For the manual's "why this system" front matter and any reviewer pitch (Omri Cohen / CDM Peter Kirn).
Written as claims dot.modular can stand behind, with the honest reasoning.

## 1. Navigable probability space -- the headline novelty
CLAIM: no other generative sequencer (Rack) or MIDI-generation VST lets you freely ROAM probability
space back and forth, REPRODUCIBLY.
Why it's true: the dice/scatter RNG is an ADDRESSABLE Philox counter -- the random stream is a
COORDINATE space, not a one-way flow. Forward AND backward; land on a position and get the byte-
identical draw every time (dice scrub knob, reverse buttons, counter model). Contrast the field:
- Turing Machine / shift-register seqs: LOCK or loop a pattern; randomness knob PERTURBS. No reversible
  addressable position.
- Marbles: deja-vu = probability of REPEAT (a tendency), not a coordinate you traverse.
- MIDI-gen VSTs (Scaler/Riffer/etc): generate FORWARD; regenerate=new, undo=old. No continuous
  reproducible position IN the generative space.
This is architectural, not marketing: treating the RNG as addressable-by-POSITION (vs a stream you can
only advance) is what makes "roam back and forth reproducibly" possible. We know of no equivalent.

## 2. Modulate the pattern GENERATION itself, not just surface controls
Practically everything is modulatable -- the de-param arc existed because there were 1000+ mods to
begin with; modulation was the design SUBSTRATE, not a bolt-on. Sands exposes PROBABILITY, all of
LOR (length / offset / rotation), SPREAD, and DIRECTION as CV targets. Most gen seqs expose maybe
pattern/density/scale and stop. dot.modular exposes the SHAPE-GENERATION parameters, so you modulate
how the pattern is CONSTRUCTED, not just what plays. Qualitatively deeper input surface.

## 3. Phase-drivable, polymetric timing with external sync
Not just gate-clocked stepping: PHASE drive makes playback position a continuous, modulatable,
externally-syncable quantity. Drivable by Rack phase modules, LFOs, and EXTERNAL phase sources (e.g.
the Bitwig phase-in plugin). This timing model speaks to the growing phase-sequencing niche and is
distinctive and compelling in motion -- LEAD WITH THIS in a demo.

### Polymeter -- natively and deeply supported
Definition (Hetrick): two sequences with DIFFERENT LENGTHS running at the SAME clock speed; they share
a clock but diverge in effective length, realigning after LCM steps. This is a FIRST-CLASS feature:
- WITHIN ONE MONSOON: Sands' per-lane LOR (length/offset/rotation) gives each lane its own effective
  step-count. A 5-step melody lane against a 7-step rhythm lane against a 9-step accent lane -- all
  driven by the same 1/16 clock, realigning at different rates. One host, true multi-voice polymeter
  without patching multiple sequencers.
- AT THE ARRANGEMENT LEVEL: Intertropical's per-scene membership + scene sequencing means different
  scenes can represent different polymetric states -- the arrangement layer itself is polymetric.
- ACROSS MULTIPLE MONSOONS: gate or phase output of one drives another -> polymetric interaction
  between independent engines, with Change Alley correlation linking their content.
Pitch this explicitly and confidently. "Polymetric" is often what people mean when they say complex
generative rhythm, and most sequencers fake it; yours does it natively.

### Polyrhythm -- honest bound (cross-instance, available NOW)
Definition: different TIME BASES (a 3:2 ratio, e.g. triplets against straight eighths -- the two parts
are clocked differently, not just differently-lengthed). This is distinct from polymeter.

CROSS-INSTANCE, RIGHT NOW -- and richer than plain polyrhythm:
Two Monsoons fed different gate/phase inputs (e.g. 1/16 straight + 1/16-triplet from the same master
clock) = genuine polyrhythm. Each applies full generative articulation within its own metric grid.
But the more interesting capability: CORRELATED polyrhythm, available today with no new features:
- Give both Monsoons the SAME EXTERNAL SEED -- they start from the same position in the Philox
  probability space.
- Send a SIMULTANEOUS DICE GATE to both -- they roll forward together, staying locked in probability
  space even as their metric grids diverge.
- Drive them on DIFFERENT CLOCKS/GATES/PHASERS -- each steps at its own rate.
Result: two Monsoons drawing CORRELATED stochastic decisions (same probability space, same position)
but at different temporal positions. The same shared random voice expressed in two metric worlds
simultaneously -- not two independent patterns that happen to coexist, but one generative idea in
a 3:2 polyrhythmic relationship. This is the addressable Philox counter (point 1: navigable
probability space) making cross-instance correlation possible -- the same feature at two scales.
HONEST NOTE: Change Alley does NOT link separate Monsoon instances -- its correlation matrix operates
within one engine only. The cross-instance correlation here is probability-space correlation (shared
seed + simultaneous dice), not CA structural correlation.
FUTURE: allow multiple Monsoons to share one Change Alley expander's pin positions, adding
structural/timbral correlation on top of the probability correlation. Requires a change to the
adjacency-based expander discovery model (currently one host per CA).

WITHIN ONE MONSOON: currently the 1/16 grid is the constraint. The maybe-later triplet step model
would bring native in-engine polyrhythm between lanes. Deferred; cross-instance covers the use case.

Claim POLYMETER and POLYRHYTHM both. Don't conflate the terms; both are real and available today.

### External CLOCK vs external GATE
External CLOCK is table stakes. External GATE here is NOT: an incoming external gate still passes
through the engine's REST / LEGATO / ACCENT (mode B) articulation -- so an arbitrary/irregular/random
external gate stream (tested: Venom Rhythm Explorer) isn't just a metronome, it becomes raw rhythmic
material that Monsoon's own generative articulation layer rest-filters, legato-shapes and accents.
External structure and internal generation COMPOSE. [CODE-CHECK before publishing: confirm external-
gate-driven steps route through the SAME rest/legato/accent-mode-B path as internal steps, not a
reduced path.]

## 4. Self-feeding + ecosystem-citizen patching (PROVEN)
Gates from one Monsoon can drive another's gate/phase drive input -> self-modulating generative loops
(the emergent-evolving-patch pattern reviewers love). And it takes gates from ANY source: tested with
Venom Rhythm Explorer driving it. So it's not a walled garden -- it integrates into anyone's patch,
cross-brand. That Venom test says a big thing: genuine modular citizen, not a closed system.

## 5. Change Alley correlation textures (poly) -- the compositional payoff
The CA correlation matrix turns poly voices from decorrelated into FAMILIES of related voices, every
intermediate reachable:
- HETEROPHONY: same melody source + different variation/range per voice = one contour ornamented
  differently per voice -- the texture of GAMELAN / Cantonese opera ensembles, from a single pin.
  (Also: same random REST data shared across poly channels but different REST settings per channel ->
  related-but-distinct rhythmic articulation of one underlying stochastic skeleton.)
- HOMOPHONY: a full column of pins on one source = voices move together.
- CALL-AND-RESPONSE: cross-block source offset (block i sources from block i+k) -> section A follows
  section B between GROUPS of voices.
- NESTED SUB-SEQUENCES: re-dicing a nested pair yields a new rhythm with the SAME hierarchy --
  structured variation, not just noise.
Note the cultural rhyme: gamelan / Cantonese opera are the regional ensemble traditions -- the
capability and the Singapore identity reinforce each other (not engineered; the philosophy is
consistent).

## 6. Flexible output + arrangement routing
Per-voice CV/gate/accent/step-gate/step-legato broken out to jacks (Changi T1/T2) -> every voice to
its own chain, with ARTICULATION downstream (the phrasing layer most gen patches lack). Intertropical
adds poly-budget scene ARRANGEMENT (voice->slot->output, fan-out chords, per-output transpose), and
that arranged output is itself a source (Lantern visualises it; Changi T3 jacks it out). Generate ->
shape -> correlate -> arrange -> visualise -> break out. One coherent signal journey.

### Musical techniques the three-space mapping enables
Intertropical is really three independent mappings -- PER-SCENE membership (which voice sits in which
slot), GLOBAL slot->output (fixed structure), and per-output TRANSPOSE. That separation is what makes
these classic arrangement techniques fall out (they're consequences of the model, not bolted-on
features), all visible at a glance in the all-scenes voice->slot grid:

- VOICE SUBSTITUTION (the headline case): an output is a STABLE part (slot->output is global, so the
  output is always fed), but WHICH voice generates it changes per scene (membership reseats the slot).
  The harmonic/timbral skeleton holds while the source rotates. Read across a slot's row in the grid
  and you SEE the substitution. (Worked example: outputs 3 & 6 are fixed chord tones; voice 16 feeds
  them only in scenes 4 & 6, other voices elsewhere -- pure membership, no re-routing.)
- ORCHESTRATION / REGISTRATION CHANGE: because each output goes to its own synth voice, reseating a
  different global voice into the same output across scenes = reassigning a PART TO A DIFFERENT TIMBRE
  (flute -> oboe between sections; an organist changing stops). Arrangement as re-scoring.
- HOCKET: seat a slot in ALTERNATING scenes (voice A odd scenes, voice B even) -> one output LINE is
  passed back and forth between voices. The medieval/gamelan hocket technique, by scene pattern.
- ANTIPHONY / CALL-AND-RESPONSE: seat voices into output-GROUP A's slots in some scenes and group B's
  in others -> registral/spatial call-and-response between output banks (pairs with sending groups to
  different synth chains / pan positions).
- TEXTURE DYNAMICS (thin/build): a scene that seats FEWER voices leaves outputs unfed -> breakdowns,
  builds, drop-outs as an arrangement dimension. Density becomes a per-scene compositional control.
- FAN-OUT HARMONY: one voice -> multiple outputs (a slot's row lit at 2+), each with its own transpose
  -> chord/octave/unison DOUBLING from a single generated line (+0 unison, +12 octave, +7/+3/+4 chord
  tones). Turns the arranger into a harmoniser. (Trade-off: fan-out COUPLES the parts -- same line in
  lockstep -- intended for doubling, muddy if independence was wanted.)
- GENERATIVE vs COMPOSED arrangement: membership auto-packs voices to slots in order by default (let
  it arrange itself) OR you seat explicitly (compose the arrangement). Same grid, both workflows.

Together: Intertropical is a SCENE-BASED ARRANGER where the classic techniques of orchestration --
substitution, hocket, antiphony, doubling, textural dynamics -- are expressible as seat/route/transpose
choices over a polyphonic generative source. Few generative systems offer an arrangement layer at all;
fewer make it this legible.

## 7. The correlation matrix as a textural continuum -- the East/West axis
The single most important control (the CA correlation matrix) is a CONTINUOUS dial between textures,
not a set of modes:
- Full column of pins on one source = HOMOPHONY (voices move together; + Intertropical fan-out chords
  = melody-plus-accompaniment homophony).
- Shared source, different variation/range/rest settings per voice = HETEROPHONY (one contour,
  many simultaneous ornamentations).
- Identity / decorrelated, scale-constrained by Shophouse = INDEPENDENT POLYPHONY / contrapuntal
  TEXTURE (independent lines kept consonant by a shared scale -- modal-polyphonic, the way a lot of
  Renaissance/folk polyphony actually works).
HONEST BOUND: this produces the TEXTURE of counterpoint (independent, rhythmically distinct lines,
scale-constrained to consonance) -- NOT species counterpoint with enforced voice-leading. Claim the
texture continuum, not "it writes fugues"; a theory-literate reviewer will respect the precision.

The strong framing (better than a feature checklist): homophony, heterophony and independent polyphony
are ONE continuous control at different settings, traversable in REAL TIME. And the traditions sit at
different points on that axis -- heterophony is the gamelan / Southeast-Asian ensemble base; homophony
and independent polyphony lean Western common-practice. So the correlation dial SWEEPS BETWEEN AND
BEYOND the textural foundations of Eastern and Western ensemble music, through hybrid points no single
tradition parked at (so they have no names). Not "imitates two traditions" -- reveals they are
endpoints of one continuous space and lets you compose in between. (The East/West axis and the
Singapore identity are the same coherence again: the region's music IS the middle of the dial.)

CONCRETE BACKING -- the SCALE SET (openable in Rack, verifiable): 28 scales spanning Western
common-practice (Major, the 3 minors, all church modes, Whole Tone, Diminished, Blues, pentatonics)
AND the East/Southeast Asian traditions -- notably PELOG (the Indonesian GAMELAN scale, which pairs
directly with the heterophony/gamelan point above), the Japanese set (Hirojoshi, In-Sen, Iwato,
Kumoi), Bhairav (N. Indian raga), and the Eastern-European/Iberian seam (Hungarian Minor, Spanish).
So the pitch COLLECTIONS themselves span the same East/West axis the correlation matrix does -- two
independent expressions of one idea. For Singapore this reads as the city's musical demographics as a
scale list. And the scale system is PROBABILISTIC, not a hard quantiser: per-semitone probability
faders + a scale MASK that gates the READ (out-of-scale notes read zero prob when locked, faders keep
the user's values and merely DIM) -- so you can weight scale degrees, CV-modulate the weights, and
stay in-scale, which hard-quantiser scale systems can't do. HONEST NOTE: Pelog/Slendro etc. are 12-TET
APPROXIMATIONS of non-12-TET tunings (standard for a semitone-mask system). Label accurately.

## 8. Reversibility: Philox counter + phase drive = a scrubbable generative system
Two axes are independently reversible, and TOGETHER they make the generative output scrub like tape:
- PHASE drive makes TIME reversible/addressable (playback position is a continuous driven quantity --
  scrub it backward and forward).
- The addressable PHILOX counter makes RANDOMNESS reversible/addressable (position -> byte-identical
  draw, both directions).
Why the complement matters: normally these FIGHT. You can rewind a clock, but an ordinary RNG is a
one-way stream -- the random values are gone, so scrubbing backward yields DIFFERENT notes on the way
back than you heard going in; the music doesn't survive a rewind. dot.modular is the rare case where
BOTH are pinned to POSITION and phase IS position -- so rewinding phase and rewinding the counter stay
in lockstep, and the exact same stochastic material plays in reverse, note-for-note. A generative
sequence you can scrub, reverse, and replay deterministically. THIS is why "roam probability space
back and forth" (point 1) and phase drive (point 3) are not two features but one system: reversible
randomness + reversible time = a navigable generative timeline.
HONEST BOUND: this holds for the SAMPLING (RNG draw at a position) and phase. STATEFUL board transforms
(scatter's domain permutation) COMPOSE on the running board and are not automatically reversed by a
counter rewind -- true board-undo needs the inverse permutation (see feat/domain-reverse-inverse /
the CA reverse discussion). So: the sampling+time axes are fully reversible; the transform-composition
layer reverses with inverse-operation care. Claim the former cleanly; don't claim "the entire system
is perfectly reversible."

## 9. Cross-instance canon: correlated voices in transformed time (the contrapuntal payoff)
The primitives compose into something few generative systems can do: the classical CANONIC
transformations (retrograde, mensuration/augmentation, and inversion-adjacent devices) expressed on
LIVE generative material. Mechanism: multiple Monsoons sharing one Change Alley (correlation structure,
via the rack-wide pairing scan) + a unified seed (S / S+1 / S+2 Philox key offsets = same probability
space) + INDEPENDENT temporal transformation (forward/backward phase drive, different clock rates).
- Same seed + shared CA => the voices are correlated BY CONSTRUCTION (same stochastic source, same
  correlation topology) -- lawfully related, which is what makes it counterpoint and not two unrelated
  lines.
- Different time transformation => the derived voice is the original under retrograde (backward phase),
  augmentation (slower clock), or a metric ratio (3:2 polyrhythm).
- Result: a live, stochastic CANON -- strict in structure, improvised in content. The "theme" is a
  navigable probability distribution; the canon is a live derivation from it.

Worked examples:
- Two Monsoons, shared CA, same seed, one phase-driven FORWARD and one BACKWARD = a retrograde (crab)
  canon generated live from one stochastic source -- the Musical Offering device, generatively.
- Different clock rates sharing seed+CA = a mensuration/prolation canon (one voice augmented against
  another), each with full generative articulation.
- Dice on both at the polymetric re-align (LCM) point = the correlated system re-rolls together at the
  combined cycle's downbeat.

WHY THIS MATTERS (and the honest framing): the claim is NOT "it composes like Bach" -- that would be
hollow. The claim is that the ARCHITECTURE makes the canonic transformations EXPRESSIBLE on generative
material, because "shared source + shared correlation + independent transformation" is exactly the
structure a canon requires. Structurally grounded and checkable, not decorative.

This also closes the East/West axis from the WESTERN side: the correlation continuum (point 7) has a
gamelan/heterophony pole (Eastern) AND a canon/counterpoint pole (Western), and BOTH fall out of the
same mechanism -- correlated voices in different temporal relationships. The instrument sits on the
axis between Bach and gamelan because that axis is structurally real, not an applied theme. (Aptly,
the conceptual lineage here is pure Godel-Escher-Bach: a self-referential formal system whose
transformations generate the counterpoint -- the strange loop closing as the shared seed feeds
correlated derivations of itself.)
A polyphonic generative sequencing SYSTEM where you can scrub probability space in both directions
(addressable Philox counter + phase drive = a reversible, replayable generative timeline), modulate
the pattern-generation itself, sweep poly voices along one continuous dial from homophony through
heterophony to independent polyphony (the textural axis of Eastern and Western ensemble music), and
route the result freely -- built as one host + expander ecosystem, with a Singapore identity woven
through the naming.

## 10. Spread as the vertical mirror: inversion of the probability profile
Spread is `interpolated = original + (target - original) * spread`, target = active-voice average or
the mono draw. So spread is a CONVERGENCE/DIVERGENCE dial:
- spread -> +1: voices collapse onto the target (unison / homophony -- the ensemble "agrees").
- spread = 0: each voice keeps its own independent draw (maximum natural divergence).
- spread < 0 (negative): ANTI-convergence -- each value reflects THROUGH the target, past its
  original, in the opposite direction. A mirror around the centre (the average / mono value).

Two claims fall out, one textural and one that upgrades it to a compositional device:

TEXTURAL (poly): negative spread is a mirror around the ensemble CENTROID -- voices above the mean go
below and vice versa. Structurally this is INVERSION in the vertical/textural domain, the counterpart
to phase-reverse being RETROGRADE in the temporal domain. The instrument has BOTH classical mirror
operations, each in its natural domain: time-mirror (phase) and register/centroid-mirror (spread sign).
HONEST BOUND: this is inversion by structural analogy (mirror around a centre), NOT literal melodic
inversion (mirror of intervals around a fixed pitch axis). Real and elegant, but state it as analogy.

COMPOSITIONAL (even on a MONO line, via MODULATION -- the stronger claim): the spreadable lanes are
REST, MELODY (pitch), OCTAVE and ACCENT. Modulating spread negative FLIPS THE PROBABILITY PROFILE of
that lane around its centre:
- REST lane: rests become likely where they were unlikely -- RHYTHMIC inversion of the rest pattern.
- MELODY lane: high-probability notes become low-probability -- the melodic TENDENCY/contour inverts
  (close to melodic inversion, as a distribution).
- OCTAVE lane: register tendency flips.
- ACCENT lane: where accents fall inverts -- DYNAMIC inversion.
Each lane is an INDEPENDENT continuous axis, so "inversion" decomposes into its musical dimensions --
rhythmic, melodic, registral, dynamic -- as FOUR separable modulatable controls, sweeping continuously
from a profile (+) through neutral (0) to its mirror (-). No notated tradition treats these as
separable continuous parameters, because they are statistical operations on DISTRIBUTIONS, not
operations on notes. This is native to a statistical instrument and unavailable to a notated one.
HONEST BOUND: it inverts the probability TENDENCY, not a deterministic note-for-note mirror. With a
shared/locked seed it approaches deterministic; free-running it inverts the statistical CHARACTER
(arguably more musical than rigid inversion -- it flips the tendency while keeping the line alive).

Together with points 8-9: phase gives RETROGRADE (time mirror), spread-sign gives INVERSION (profile
mirror), clock-ratio gives AUGMENTATION. The three classical transformation families are each present,
each in the domain natural to a live statistical instrument, each modulatable and correlatable.

## 11. Composition of transformations: performable multi-layer canon
Points 8-10 give three transformation axes that operate at DIFFERENT levels and compose ORTHOGONALLY
(different structure, no interference), so they run simultaneously and MULTIPLY:
- BETWEEN instances: phase forward vs backward + shared CA + shared seed = retrograde CANON (voice B
  is voice A's retrograde, correlated by construction).
- WITHIN each instance, independently: spread-sign modulation inverts the probability profiles
  (rest=rhythmic, melody=melodic, octave=registral, accent=dynamic), each on its own LFO/CV.
- ACROSS the metric axis: different clock ratios = augmentation.

Stacked example: Monsoon A runs FORWARD with its rest-profile inverting under one LFO; Monsoon B runs
BACKWARD (retrograde of A's shared material) with ITS accent-profile inverting under a different LFO.
Result = a retrograde canon whose subject is itself a live-inverting distribution, each canonic voice
inverting along its own probability dimensions on its own schedule. In classical terms: canon by
retrograde STACKED with per-voice inversion -- the multi-transformation canonic device family (Art of
Fugue: retrograde-and-inverted canons, "per augmentationem in contrario motu").

WHAT'S CATEGORICALLY NEW (honest, not hype): Bach's multi-transformation canons are FIXED -- the
relationship is locked once written. Here every transformation is a CONTINUOUS MODULATABLE AXIS:
- degree of spread-inversion is a knob/CV (sweep profile -> neutral -> mirror DURING the canon)
- the phase relationship (how far forward/backward diverge) is drivable
- the shared seed is scrubbable (move the whole correlated system through probability space while the
  canonic relationships hold)
So it's not a fixed canon -- it's a canon whose TRANSFORMATION PARAMETERS ARE THEMSELVES PERFORMABLE.
You don't play the notes; you play the TRANSFORMATIONS, and the canon structure (shared source +
correlation) guarantees the voices stay lawfully related wherever you move the controls. A new
instrument category: structural counterpoint, continuously performable along the classical axes.

HONEST BOUNDS (inherit from 8-10):
- "Inversion" stays the tendency/structural-analogy sense (profile mirror, not literal interval
  inversion). The stacked claim inherits this.
- LEGIBILITY caveat (musical, not architectural): stacking retrograde + per-lane inversion +
  seed-scrub can get so dense it reads as TEXTURE, not audible counterpoint. The structure is real;
  the LEGIBLE demo is usually a SUBSET (expose 1-2 layers clearly). Full stack = possible; compelling
  demo = curated. An arrangement judgment for the ear, not an architecture limit.
- Framing: the architecture makes these multi-transformation canonic structures EXPRESSIBLE and
  PERFORMABLE on live generative material -- NOT "it reproduces Bach."

## 12. Cross-TUNING canon: shared Change Alley across differently-tuned Monsoons (post-microtonal)

[PATCH NOTE -- Rodney, for when Micro + shared Change Alley both exist.]

Once the microtonal expanders land AND Change Alley can be shared across instances, a new patch class
opens: **two Monsoons in DIFFERENT TUNINGS, sharing one Change Alley.**

### The patch
- Monsoon A: no tuning expander (or Sikit at defaults) -> 12-TET.
- Monsoon B: Micro-24 loaded with a maqam .scl -> quarter-tone Arabic modal tuning.
- Both share a Change Alley (correlation) + unified seed.
Result: the SAME generative decisions -- which degree, when, rest/accent/legato -- rendered through
TWO DIFFERENT TUNING SYSTEMS simultaneously.

### Why this works architecturally (and it's EMERGENT, not designed-in)
The orthogonality falls out of where things live:
- The TuningTable is PER-MONSOON (each has its own tuning source; see MICRO_TUNING_INTEGRATION_PLAN).
- Change Alley correlation operates at the RANDOM-DRAW level, ACROSS instances -- it correlates the
  *decisions* (which degree index gets picked, when a rest happens), not the *pitches*.
So a correlated draw of "degree 7" is degree 7 IN EACH INSTANCE'S OWN TUNING -- a perfect fifth in
12-TET, something else entirely in the maqam. Same structural decision, two tuning worlds. The
architecture didn't set out to do this; it's what you get when correlation lives at the decision layer
and tuning lives at the render layer.

### Musical meaning: the East/West axis made literal
Point 7 framed the correlation matrix as an East/West textural continuum; point 9 framed cross-instance
canon in transformed time. This is the THIRD axis and arguably the most literal one: the same line, in
two tuning traditions at once. Not "a Western line and an Eastern line played together" (which any two
sequencers can do) but "ONE musical decision-stream, heard simultaneously in two intonational worlds."
That's the East-West-and-places-between theme (which Change Alley already anchors historically) made
audible as a patch rather than stated as a concept.

### Works with SIKIT too -- less dramatic, still real
Doesn't need the full Micros. With Sikit alone:
- Monsoon A: Sikit at equal-division defaults (12-TET).
- Monsoon B: Sikit loaded with well-tempered / meantone / a stretch tuning.
- Shared Change Alley.
Result: correlated material in subtly different intonation -- like two instruments tuned differently
playing the same line. Historically that's exactly what an ensemble of period instruments sounds like.
Much subtler than the maqam case, but musically real, and available a whole phase earlier (Sikit is
Phase 1; Micros are Phase 2/3).

### Demo-patch candidate
Strong headline demo alongside the crab canon (MASTER_PLAN demo-patch item). Legibility caveat from
point 11 applies: expose ONE transformation layer (the tuning difference) rather than stacking it with
retrograde/mensuration/spread-inversion, or it reads as texture rather than as the point.

### Dependencies (both needed for the full version)
1. Micro-12/24 (Phase 2/3) for the dramatic version; Sikit (Phase 1) for the subtle version.
2. SHARED Change Alley across instances -- currently CA operates within one engine; sharing across
   separate Monsoons is the "shared CA" work (see SEED_OFFSET_DESIGN build order: offset first, shared
   CA second). Until then the cross-instance correlation route is shared-seed + simultaneous-dice.

### 12a. Concrete realisation: two Intertropicals, alternating correlated riffs, call-and-response
[Rodney's patch sketch -- the specific form point 12 should take in a demo.]

**Patch:**
- Monsoon A (12-TET) -> Intertropical A. Monsoon B (maqam via Micro-24) -> Intertropical B.
- Shared Change Alley + unified seed + shared clock/reset.
- IT A: alternating scenes, odd = full voice->output routing, even = EMPTY (no routing = silence).
- IT B: the inverse (odd empty, even full).
Result: A speaks in 12-TET, B answers the correlated phrase in maqam, alternating automatically as the
scenes advance. Call and response across two intonational worlds.

**Why it needs NO new features:** an empty IT scene routes nothing, which IS silence. So alternation is
just scene configuration -- no mute logic, no gating, no new module. The 8-scene sequencer already does
it.

**Why it stays locked:** both ITs reset to scene 1 / repeat 1 on Monsoon reset (the reset-sync fix,
commits e0cbce0 / ffaa7a2 / 26f0850 -- resetPulse in processResetGate + the justReset flag swallowing
the first post-reset wrap). Without that fix the two ITs would drift apart and the call-response would
decay into overlap. Verified in clock mode; PENDING verify under phase drive (Mode E).

**Why the FORM fits the material:** call-and-response is native to BOTH traditions being bridged --
antiphony in Western practice, responsorial forms in Arabic music. The patch structure matches the
content rather than imposing an arbitrary shape on it.

**Legibility:** this is a GOOD demo precisely because it exposes ONE transformation (the tuning
difference) and makes it maximally audible by ALTERNATING rather than overlaying. Point 11's warning
(stacked transformations read as texture) is dodged: separation in time is what makes the tuning
contrast legible. Overlay the same two tunings simultaneously and you get a blur; alternate them and
the ear hears the relationship.

**Variations worth trying:**
- Asymmetric scene lengths (A gets 2 scenes, B gets 1) -> uneven call/response, more speech-like.
- Both ITs partially full on some scenes -> overlap moments where the two tunings sound together,
  punctuating the alternation.
- Sikit version (Phase 1): same patch, well-tempered vs 12-TET. Much subtler, still real.
