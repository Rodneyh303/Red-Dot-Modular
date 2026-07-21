# Change Alley — a pin-matrix expander reallocating probability streams between voices

Status: DESIGN. No code. Companion doc style: FULLERTON_DESIGN.md.

## 0. Naming — load-bearing, not trivia

Change Alley: the money-changers' lane between Raffles Place and Collyer Quay — a row
of exchange booths reallocating what flows through, at the best rates, before you head
out into the region. That IS the module: an alley of pins reallocating the engine's raw
probability streams between voices, BEFORE all downstream manipulation. Geography files
it correctly in the family: it physically connects Raffles Place (Raffles: the dice
surface) to Collyer Quay (Fullerton/Clifford Pier: the arrival lights). Exchange
semantics, sibling adjacency, Singapore ground truth — all three legs load-bearing.

## 1. Concept

A 16-voice pin matrix that reassigns which voice CONSUMES which voice's raw random
streams, per stream type (RHYTHM = rest lane draws, MELODY = melody lane draws), before
LOR thresholds, probability knobs, mods, clamps, or any articulation logic touch them.
Identity diagonal = today's engine exactly. Pins move consumption, not values: each
voice keeps its OWN probabilities, rotation, direction, offsets — it just rolls someone
else's dice.

Lineage: the EMS VCS3/Synthi pin matrix (see XILS-4, Arturia Synthi V) — patch routing
as a board of pins rather than cables. Change Alley is that idea applied not to audio
routing but to CORRELATION routing between generative streams, which (as far as we
know) nothing in Rack does.

## 2. MEANING — the correlation structure (why this is musical, not plumbing)

The raw streams are uniform draws r ∈ [0,1); a voice rests when r < restProb (after
mods). Therefore:

**Same source + different thresholds = strict subset/superset rhythms.** Two voices
pinned to one rhythm source roll the SAME r each step. Voice A (restProb 0.3) rests
exactly where r < 0.3; voice B (restProb 0.6) rests on a strict SUPERSET. B's silence
contains A's. Nested, hierarchical rhythm — controlled continuously by the probability
knobs that already exist, with the correlation itself controlled by the pin. No
threshold relationship between INDEPENDENT streams can produce this; it is purely a
property of shared randomness.

**Same melody source + different variation/range settings = heterophony.** One
underlying contour, ornamented differently per voice — the texture of gamelan or
Cantonese opera ensembles, from a pin.

**The matrix is a correlation-structure controller**: identity = fully decorrelated
voices (today); a full column of pins on one source = rhythmic/melodic homophony;
scattered pins = families of related voices. Every intermediate is reachable and
deterministic (Philox streams, dice re-rolls tables, pins survive dice — re-dicing
changes the material, never the relationships).

### 2a. Pinning vs Spread — orthogonal, not redundant

Both touch inter-voice relationships, so it is worth stating why they don't overlap
(and heading off "don't we already have spread?"):

- **Pinning sets the SOURCE** (discrete routing at random_): voice v's stream IS voice
  src's stream — 100% correlated, identical draws. Arbitrary topology: pin 3 and 7 to a
  shared source and leave 1 alone → an isolated correlated PAIR inside an otherwise
  independent ensemble.
- **Spread sets the PULL toward a reference** (continuous, downstream in derivation):
  voice v's OWN draw leaned some fraction toward mono/voice-1 or the ensemble average.
  Single-hub only: at maximum it converges everything toward ONE reference (a star, or a
  mean). It CANNOT make voice 3 and voice 7 track each other while voice 1 stays free —
  it has no way to say "these two, not those."

The asymmetry: pinning is arbitrary-pair correlation; spread is single-hub convergence.
Neither produces the other's effect.

They COMPOSE, precisely because they live at different levels (the write-side
architecture makes this literal): pinning acts at random_ — WHICH stream — before
spread acts in derivation — HOW MUCH the stream leans. So: pin 3 and 7 for identical
dice, THEN spread that correlated pair toward voice 1 to pull it partway to the hub.
Two independent operations stacking; pinning is a 100% correlation floor, spread adds
convergence on top.

Precision consequence for subset/superset: pinning gives EXACT nesting (identical draws,
different thresholds → strict subset). Spread SOFTENS it (pulled draws diverge slightly
from the reference). Clean hierarchical rhythms → pinning; approximately-related voices
with organic variation → spread. Same neighbourhood, two grains.

## 3. THE FORK — source-table vs source-value remap (the defining decision)

- **Table remap (RECOMMENDED)**: voice v reads polyRandom(src[v], lane) at v's OWN
  index — its own tick, rotation, direction, offset, length. Pinned voices correlate
  where their positions align; rotating one against a shared source phase-shifts the
  relationship. "Before all manipulations" in the strictest sense: only the TABLE of
  raw draws is exchanged. Rotation-against-shared-source becomes a musical device
  (canon-like siblings).
- Value remap (rejected): consumer reads the source's resolved draw at the SOURCE's
  position — exact lockstep regardless of rotation, but it collapses per-voice
  individuality and drags position resolution (rotation/direction) into the exchange,
  which is precisely "after manipulation". If lockstep is wanted, set matching
  rotations under table remap.

Decision: TABLE remap. One semantics, no per-pin modes.

**§3-REVISED (settled, IMPLEMENTATION PENDING — supersedes the read-side design):**
The read-side implementation (readStrand + enumerated consumer remaps) is a CODE SMELL:
one pin borrowed two different materials (finals for East/articulation, slewed base for
Macro), and every read surface had to be found and remembered (two misses proved the
risk). The mapping moves to CANDIDATE CONSUMPTION in the derivation pipeline:
- CONFIRMED INJECTION POINT (PatternEngine.cpp ~592-602 and the split melody/rhythm
  partners): random_ is written FROM the *Source[] arrays each cycle (Source → slew →
  random_, recirculating). There is no separate pristine raw buffer, so the remap must
  happen AT THAT COPY: change random_[v][strand] = Source[v] to = Source[srcRow(v,strand)].
  The *Source[] arrays stay per-voice and untouched (locality, dice/pin orthogonality
  preserved); random_ — the structure every consumer reads — now holds the MAPPED result.
  A naive in-place remap of random_ is WRONG (it would remap already-slewed data and feed
  it back through the recirculation). Map at the Source→random_ landing, nowhere else.
- ALL reads everywhere become plain own-bank reads again. REVERT: polyRandomSrc,
  monoStrand, finalRandomByStrand routing, polyLaneProbability(AtStep) remaps, and the
  macroOwnProbability srcRow patch (commits 39360f2, 25c2379 read-side parts).
- Philox raw draws stay per-voice (no re-keying); banks rewritten are derived state the
  pipeline recomputes anyway → locality + dice/pin orthogonality hold.
- LOCK: pipeline output freezes under lock → pins inaudible until unlock, CONSISTENT
  with the settled Vermona lock (pins latch). Resolves the prior contradiction.
- Trade-off (accepted): borrowers apply their own spread — subset/superset exact at
  zero spread, softening as spread widens = "same dice, own manipulation".
- Same pass: readStrand→identity replica test becomes candidate-level identity test
  (bit-exact with no expander; mono VAR/LEG the regression case).

**§6 addendum — UI (XILS-4 reference, photo captured):** on hover/drag draw white
CROSSHAIR guide lines from the cell to both axes plus a tooltip "row col value" (XILS:
"left Adv. Matrix : 5 8 1.00") — solves far-cell targeting on a 16×16 grid.

**Sharpened (confirmed): original Philox draws, then deterministic map.** Generation is
untouched — all 16 voices' per-lane tables are drawn exactly as today (same keys,
counters, values); the matrix is a pure READ-TIME indirection. Three properties follow:
- **Locality**: moving a pin perturbs nothing but the row that changed — no RNG state
  is touched, every other voice stays bit-identical. Pin edits are surgical, live-safe.
  (A re-keying design would reshuffle on every pin move; rejected for this reason.)
- **Dice/pin orthogonality**: dice re-rolls the TABLES (new material), pins keep the
  RELATIONSHIPS. Re-dicing a nested pair yields a new rhythm with the same hierarchy.
- **Lock semantics**: lock freezes tables; a pin move under lock still changes which
  frozen table a voice reads, so pins stay audible while locked. Intended: pins are
  routing/configuration (same category as rotation, which also works under lock), not
  material.

## 4. Matrix semantics

- Rows = consuming voices (V1..V16), columns = source voices. Per stream type, each row
  holds EXACTLY ONE pin (radio behaviour: clicking a cell moves that row's pin). No
  pin-mixing — a voice has one rhythm source and one melody source, full stop.
- Default: identity diagonal (both types), rendered faintly so departures are visible.
- Sources are the raw per-voice stream tables, so a column stays a valid source even if
  its voice is inactive (fewer voices playing than sourcing is legitimate).

## 5. Mono (V1) — join the matrix or not?

Options: (a) mono is row 0 AND column 0 — remapping the leader's own sources changes
follow-mono material globally (powerful, arguably confusing); (b) mono is a COLUMN only
— poly voices may drink from the leader's streams, but the leader's own consumption is
fixed. Lean (b) for v1: the leader stays the reference frame; revisit (a) if practice
wants it. OPEN.

## 6. One board or two — the EMS answer

EMS precedent (VCS3/Synthi, XILS-4's dual-board VCS4 homage, Synthi V): the matrix IS
the panel's identity, and EMS distinguished pin KINDS on one board (standard vs
attenuating pins) by colour. That answers the question: **one 16×16 board, two pin
colours** — rhythm pins in Singapore red (#d4001a), melody pins in the teal family.
A cell can hold both (concentric render: ring + dot). Interaction: left-click places/
moves the rhythm pin of that row, right-click (or modifier) the melody pin; row
radio-behaviour makes misclicks self-correcting.

Two boards was the fallback if pin kinds could not share a hole legibly; concentric
two-colour pins solve it, and one board halves the panel cost (16×16 at ~4.5 mm pitch
≈ 72 mm + gutters ≈ 18–20 HP, vs ~36 HP for two). ONE BOARD, pending a render mock.

## 7. Architecture (post-audit native)

- **Engine**: an indirection table `uint8_t rhythmSrc[16], melodySrc[16]` consulted at
  the polyRandom read sites (`polyRandom(src[v], lane)`), identity-initialised. All
  downstream machinery (LOR, mods, clamp invariant, Rule 2, lane direction) untouched —
  they never learn the draw was borrowed. Deterministic; lock-safe; dice-safe.
- **Zero param slots**: 2×16 assignments (512 virtual pin positions) were never going
  to be params. Store-backed pin cells writing a Monsoon-side store (serialized via
  dataToJson like macroSend), edited through custom widgets with StoreEditAction undo —
  the first module DESIGNED under the DAW_PARAM_AUDIT law rather than retrofitted.
  Host automation of pins is explicitly out of scope (audit philosophy: automate the
  probability-REACTION surface; correlation structure is configuration).
- **Display**: the board renders sources column-wise; consider lighting the pin of any
  currently-sounding voice (activity shimmer) so the exchange is visible in motion.

## 8. Open questions

1. Mono row: column-only (lean) vs full row 0 participation.
2. Do ACCENT / VARIATION / LEGATO streams eventually join as pin types (a 3rd/4th pin
   colour), or stay per-voice forever? Start with rhythm+melody; the indirection
   generalises trivially, the UI is the constraint.
3. Pin modulation: RESOLVED — see §10. Structural pins that latch (never continuous
   per-pin CV), plus a discrete "restructure" gesture (Identity / Rotate / Scatter).
4. Panel aesthetics pass (function-first phase done; reference captured):
   The real-world reference is the money changers' RATE BOARD (e.g. Raffles Money
   Change) — flag/label column, aligned rate columns, alternating red/white row
   blocking, branded header strip, regulatory footer line. Steal the LAYOUT LANGUAGE,
   not the palette: the board's red-on-white assault works for a shopfront, not a
   panel stared at while patching. Concretely:
   - Keep the forced-dark grid (the dark well is what makes red/white pins readable —
     the board's white ground is exactly what makes ITS red unbearable).
   - Keep booth-band row alternation at whisper level (the board validates the idea).
   - Rate-board typography: monospace, ISO currency codes (what a real board's header
     shows), numbers right-aligned — already close.
   - Header strip: the title band as the branded header (the top red stripe already
     plays this role); axis labels in the "WE BUY / WE SELL" register.
   - RED AS PUNCTUATION, not wallpaper: melody pins, connect dot, active-voice bars,
     mono row accent — and nothing else. The little red dot, not the little red panel.

> IMPLEMENTED (branch feat/change-alley-ab-remap): the remap is applied write-side by
> PatternEngine::applyChangeAlleyRemap() — called once per control cycle in Monsoon after
> processDNA (fills random_ via BOTH non-Sands and Sands paths) and expanderManager.sync
> (pushes pins into pe). It snapshots random_ and rewrites each row's strand from its
> pinned source row; no-op at identity. This is the SINGLE SEAM: it runs after whichever
> fill path populated random_, so both are covered without touching either. All read-side
> remaps reverted to plain own-bank reads (polyRandomSrc, monoStrand, the two
> polyLaneProbability accessors, macroOwnProbability). Guarded by test_change_alley_remap
> (13/13: identity no-op on all strands, pool-split borrow, locality, shared-source
> identity, mono participation).
>
> LEVEL DECISION (SETTLED — post-mix is correct, A/B pinning declined):
> random_ is the system's SINGLE CANONICAL STREAM — the one point where a voice's dice
> become one definite value per step/strand, before any interpretation (thresholds, LOR,
> spread, articulation). Pinning there gives the pin ONE MEANING everywhere (visible and
> audible agree by construction) — the whole point of the feature. A/B-level pinning
> would break that: a pinned voice would share source's dice but diverge on its own
> variation blend, so "what is voice v playing" loses a clean answer relative to its
> source. The per-voice variation divergence A/B would add is NOT lost at post-mix — it
> is relocated to the honest place: pin two voices together and give them different
> REST/variation, and the divergence comes from their OWN parameters on a shared stream
> ("same dice, own manipulation"). A/B pinning would be a SECOND, redundant divergence
> axis duplicating what per-voice parameters already do — more machinery for a muddier
> abstraction. Declined on architectural grounds, not cost. Post-mix is final.


## 9. Worked example — theme & variations, and voices-as-timeline

This is not a feature; it is what the composed primitives (pinning · spread · Changi
switch) AFFORD. Recorded as evidence the architecture is right — the interesting musical
objects are emergent, not enumerated.

### 9a. Four-bar theme & variations (the seed patch)
1. Pin voices 3 and 4 to source 2 → voices 2, 3, 4 now share 2's stream, 100% correlated.
2. Spread 2, 3, 4 each toward voice 1 by DIFFERENT amounts (2 light, 3 medium, 4 far) →
   three graded departures from a common root, with voice 1 as the reference theme.
3. Changi sequential switch cycling the four mono outputs 1→2→3→4→loop → each BAR plays
   the next voice: theme, then three progressively-further variations. A 4-bar phrase
   where every bar is a variation on the first.
Nothing knows it is making theme-and-variations: pinning sets the common root, spread
sets graded departure, Changi turns parallel voices into serial bars. Emergent.

### 9b. The generalisation — voices are a TIMELINE, not just a chord
Once Changi sequences them, N voices become N SLOTS IN TIME, and pinning+spread control
the relationship between slots. The 16 partition into (groups × members):
- 4 groups × 1 voice, switch/bar → 4-bar loop of a 1-voice line (9a).
- 2 groups × 2 voices, switch/bar → 4-bar loop of a 2-voice pattern (paired call/response).
- 1 group × 2 voices over 8 slots → 8-bar loop of a 2-voice pattern (longer, thinner).
- 4 groups × 4 voices → 4-bar loop of a 4-voice chord progression (variation at chord level).

CONSERVATION LAW: voices = polyphony_width × phrase_length (in switch slots). 16 is a
fixed budget spent on either axis. Thick chords = spend on width, short loop. Long phrase
= spend on length, thin texture. The same 16 voices are one 16-note chord (width 16,
length 1 — the DEGENERATE case), OR a 16-bar monophonic phrase (width 1, length 16), OR
any rectangle between. Pin-groups decide which voices are related within the rectangle;
spread decides how related; Changi's cycle scans the rectangle into time.

This reframes why 16-voice poly matters: not just massive chords (width-16 degenerate
case) but the INTERIOR of the width×length rectangle, where polyphony and phrase-length
trade against each other and pin-groups impose variation structure across the chosen
axis. Voice count as a phrase-length resource, not only a density resource — few
generative sequencers treat it that way.

## 10. Pin modulation — structural pins + a discrete restructure gesture (SETTLED)

Pins stay STRUCTURAL: they latch under the Vermona lock (LOCK_SEMANTICS), same category
as routing — the thing you set up and play AGAINST, not a value you ride. Continuous
per-pin CV is DECLINED: it would reclassify pins as a value surface, reopen the lock
question (a modulated pin inert under lock is surprising), and muddy the material-vs-
configuration split. Instead, pin modulation is a discrete RESTRUCTURE gesture — atomic
transforms of the whole board, fired by trigger, latch-compatible (fire on unlock like
dice). Each must preserve the one-pin-per-row invariant (permute/reassign sources, never
two-per-row) and must produce a MUSICALLY meaningful change, not just a different board.

Three transforms (each a distinct musical intent; the correlation-space analogue of dice):

- **Identity** — src[v] = v for all v. Reset: every voice back on its own stream, fully
  decorrelated. The "clear the routing" home state a restructure sequence returns to.
- **Rotate** — src[v] = (src[v] + 1) mod 16, stepping +1 per trigger. Every source shifts
  together, so the correlation SHAPE is preserved and slid across the voice pool: a group
  on voice 2 becomes a group on 3, then 4... The same relational structure reassigned to
  new source material each trigger. THIS is what turns a static 4-bar loop into an
  evolving 8/16-bar structure without touching anything else. Step fixed at +1 (variable
  stride = "trigger N times", NOT a CV amount — keeps every change a discrete event).
- **Scatter** — src[v] = seeded random draw ∈ [0,16) per voice, one deterministic shuffle
  per trigger (seeded → reproducible/undoable). Re-randomises relationships wholesale:
  the "surprise me" gesture. Distinct from Rotate: Rotate PRESERVES structure and moves
  it (continuity); Scatter DESTROYS and rebuilds it (rupture).

Dropped: **Invert** (src[v] = 15 - v). Reflection of a relationship graph has no clear
musical meaning the way transposition-of-structure (Rotate) and re-randomisation
(Scatter) do. Three transforms that each mean something beats four where one is there
for symmetry. Add later only if a concrete use surfaces.

Scope: restructure acts on rhythm pins and melody pins SEPARATELY (per-type trigger
inputs or a mode switch). Rotating rhythm relationships while melody grouping stays fixed
is exactly the independent control the two-pin-colour design exists for; coupling them
halves the expressiveness.

Relation to dice (the clean parallel): dice re-rolls MATERIAL (the tables), restructure
re-rolls RELATIONSHIPS (the pins). Both discrete, both lock-respecting (fire on unlock),
both leave the other axis untouched. Restructure is to correlation-space what dice is to
material-space.
