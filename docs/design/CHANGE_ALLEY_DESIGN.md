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

> SUPERSEDED — WRITE-SIDE REVERTED (race + non-idempotence, found in Rack):
> The write-side in-place remap FLASHED the East visual on off-diagonal pins. Root cause,
> measured: random_ has 73 WRITE SITES across 7 files INCLUDING GUI-THREAD parameter
> managers (Mono/Poly/PolyVoiceSandsParameterManager write finals from editor save
> paths). Writers between remaps expose unmapped transients (flash), and in-place remap
> is NON-IDEMPOTENT under pin chains (remap twice = mapping composed twice — any bank
> not refilled between remaps WALKS through the chain). Making it safe needs tail-calls
> at all 73 writers (fragile forever) or true double-buffering (bigger, own hazards).
> The enumeration argument in fact runs the OTHER way in this codebase: READS funnel
> through 5 accessors (proven complete — East/Macro/CV/articulation agreed); WRITES
> scatter across 73 sites on two threads. Read-side mapping is race-free BY CONSTRUCTION
> (atomic at each read, no transient ever visible, no writer cooperation).
> RESOLUTION: read-side mapping (39360f2 semantics + the Macro slewed-base srcRow patch)
> is the FINAL architecture. The read accessors are the narrow waist; the write surface
> is the wide one. test_change_alley_remap removed with the reverted seam.
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

### The structure: FOUR transforms × a BLOCK-SIZE modifier

The key generalisation (Rodney): a BLOCK SIZE modifier makes each transform operate
WITHIN blocks rather than across all 16, and it restores Reflect (meaningless globally,
a retrograde when blocked). CORRECTION: Collapse is NOT Identity-per-block — Identity is
src[v]=v (self-mapping, blocks change nothing), whereas Collapse is src[v]=blockLeader(v)
(every voice sources its block's leader → homophony). They are different operations;
Collapse keeps its own slot. Block size answers "at what GRAIN does this relationship
operate."

Transforms (src[] operations; all preserve one-pin-per-row):
- **Identity** — src[v] = v. Full decorrelation; every voice on its own stream. (Block
  size irrelevant — self-mapping ignores grouping.) Identity IS Collapse at block size 1.
- **Collapse** — src[v] = blockLeader(v): every voice sources its block's first voice, so
  each block becomes HOMOPHONIC. Block 2 = pairs {0,1}→0, {2,3}→2…; block 4 = four
  quartets each locked to their leader; block 16 = total unison (all on voice 0). THE
  width-setting gesture and the one that builds theme-and-variation groups in one move.
  Its grain knob spans 1 (=Identity, full independence) → 16 (total homophony) — a single
  control from full decorrelation to full unison.
- **Rotate** — src[v] = (src[v] + 1) wrapped WITHIN its block. Block 16 = the shape marches
  across the whole pool (static loop → evolving 8/16-bar structure). Block 4 = each group
  of four rotates internally; the four-group partition holds while its internals evolve.
  Step fixed +1 (variable stride = trigger N times, not CV).
- **Scatter** — seeded shuffle WITHIN each block. Block 16 = global re-randomise (rupture).
  Block 4 = reshuffle inside each quartet; the coarse four-group architecture survives
  while who's-related inside each block changes. Blocked scatter is FAR more musical than
  global — it preserves phrase macro-structure. Seeded → reproducible/undoable.
- **Reflect** — src reversed WITHIN each block (block of 4: 0,1,2,3 ← 3,2,1,0). Globally
  meaningless (why we dropped "Invert"); BLOCKED it is RETROGRADE of the correlation
  structure at the phrase-group grain — in a Changi scan, the group plays its variations
  in reverse relatedness order. Odd partial blocks have a self-reflecting centre voice
  (correct, no special case: it's the mirror axis).

### Block size × active poly
Blocks tile the ACTIVE pool [0, polyCount] (0=mono always active), NOT raw 16 — sourcing
from an inactive voice borrows a stream nothing plays (musically dead; Scatter is the
worst offender). So:
- Automatic transforms draw/rotate/reflect only over LIVE voices; block size is capped at
  the active count (block-8 with poly-6 = one block of 6; the knob's top detent is
  effectively "whole active pool" regardless of label).
- Partial trailing blocks operate on their live members only (poly-6, block-4 → block
  {0-3} full + block {4-5} of two).
- Consequence (document, don't surprise): transform RESULTS depend on numPolyVoices — raise
  poly and a re-Scatter uses the newly-live voices. Correct, but state it. Transforms read
  the count the same way the widget does (cached Monsoon).
- The MANUAL pin grid stays full 16: you may deliberately pin to an inactive source (about
  to raise poly; routing persists like rotation/direction already do across poly changes).
  Only the AUTOMATIC transforms respect the live pool.

### Controls & scope
- Per pin TYPE, independently: rhythm and melody restructure separately (per-type trigger
  + per-type block-size knob). Rotate rhythm across 8 while Scatter melody within 4 — the
  independent control the two-pin-colour design exists for.
- Block size = a STEPPED knob per transform type (2/4/8/16, capped at active pool).
- Every transform is a discrete atomic event, latch-compatible (fires on unlock like dice).

Relation to dice (the clean parallel): dice re-rolls MATERIAL (the tables), restructure
re-rolls RELATIONSHIPS (the pins). Both discrete, both lock-respecting (fire on unlock),
both leave the other axis untouched. Restructure is to correlation-space what dice is to
material-space; block size is the grain at which the relationship operates.

## 11. Restructure queuing & controls (SETTLED spec)

### Queuing — adopt the dice machinery, one pending PER TYPE
Each transform is gate-triggerable like dice: fire a gate → the transform is QUEUED with
a pending light → applies at the next PHASE BOUNDARY (and on UNLOCK, like dice). Manual
button = same as a gate pulse.

One pending transform per pin type (rhythm has one pending slot, melody has one),
LATEST-OVERWRITES — exactly the dice UI (pending light per type). But the REASON differs
from dice, and it settles the "queue multiple?" question:
- Dice queues one because dice/trial/backwards are MUTUALLY EXCLUSIVE outcomes on the same
  material — a second would just overwrite the first.
- Transforms are COMPOSABLE and ORDER-DEPENDENT (Collapse∘Rotate ≠ Rotate∘Collapse). So
  the question is "single op or ordered chain per boundary?"
- DECISION: single op, latest-overwrites. Composition is reached ACROSS boundaries —
  trigger Collapse (fires boundary 1), then Rotate (fires boundary 2) = Collapse-then-
  Rotate as a two-bar EVOLUTION, which is MORE musical than batching (you hear each step)
  and matches the whole point of phase-boundary timing (structure develops in time). An
  ordered per-boundary chain would need a visible queue, chain undo, dup rules — more
  machinery for a less musical result.
- Rhythm and melody pending INDEPENDENTLY: they are parallel axes, not a sequence, so one
  pending-rhythm + one pending-melody firing at the same boundary is coherent and wanted.
  Two pending lights, one per type, each latest-overwrites within its type.

Block size: a STEPPED knob PER TRANSFORM (grain 1/2/4/8/16, capped at active pool).
Shared across R/M for that transform (one grain per transform; the per-type split is the
gate/trigger, not the grain). Applies at the same phase-boundary/unlock moment.

### Panel controls — COMMITTED layout (Rodney): 8×(knob+jack+button), grid claims the right

Identity is NOT a control (it is Collapse@block-1). So the controls are the FOUR real
transforms × TWO pin types = 8 control rows: 8 block-size knobs, 8 gate jacks, 8
illuminated buttons (button = manual trigger AND pending light in one element).

Row order (top→bottom), grouped by transform with a hairline between transforms:
  Collapse-R, Collapse-M, Rotate-R, Rotate-M, Scatter-R, Scatter-M, Reflect-R, Reflect-M

Per row, left→right: block-size knob · gate jack · button · light (button+light separate,
as dice).
Control column on the LEFT; the 16×16 grid claims the RIGHT (no longer centred).
Geometry (23HP→29HP): knobs ~x16mm, jacks ~x25mm, buttons ~x33mm; grid left edge ~x40mm,
grid 99.5mm wide (6.22mm cells kept), panel ~147mm = 29HP. 8 rows over the 99.5mm grid
height = 12.4mm pitch, control cluster vertically centred on the grid.

Block-size knob = stepped 1/2/4/8/16 (grain), capped at active pool. Per transform per
type (the 8 knobs) — so Collapse-R grain is independent of Collapse-M grain, Rotate, etc.
The LIGHT shows PENDING (queued, awaiting phase boundary); the BUTTON queues manually;
the jack queues on a gate — same button+light relationship as dice. Latest-overwrites per (transform,
type) — one pending each.

29HP is large but defensible: Change Alley is a centrepiece correlation controller (cf.
large matrix mixers / the EMS matrix it descends from). NOT built until the engine
transform set exists; then panel regenerates at 29HP with this cluster and the widget
wires 8 params (knobs) + 8 buttons + 8 inputs.

## NEXT-SESSION FIRST TASK — merge regression: East VAR/LEG lanes render transparent
Symptom (Rodney, screenshot 2026-07-21): on East (V1 tab), VARIATION and LEGATO lane
bars render faded/transparent; Sands mono renders them solid. Post f2925ac merge.
NOT the value fill: mirrorMonoExtraLanes exists and is called (cpp 616 onVoiceTabChanged,
1195 step) and finalRandomByStrand reads are correct. Suspect the STYLING path:
src/ui/SandsVisualEditorV4.hpp was a MASTER-side file in the merge (branch cpp mostly
won the East file) — likely an alpha/lane-style/locked-dim API the master editor expects
that the branch-era cpp doesn't call (or a default alpha change). Leads: diff de96cf4 vs
merge-base for SandsVisualEditorV4.hpp; check laneLockedFn/dim styling for lanes 4/5;
compare against Sands mono expander's calls (renders correctly, same editor class).

## BUG (Rodney, in-rack) + DESIGN QUESTION: pin off-diagonal makes spread inert

Symptom: with Change Alley pins OFF the identity diagonal, most spreads have no visible
effect on East, and AVERAGE_POLY vs MONO_DRAW (voice-1) targets look identical. On the
diagonal, spread works normally.

Root cause (traced): spread is applied PER BANK in MonsoonSandsManager (mono →
random_[0] via the *Random refs; poly → random_[v] via polyRandom(v,·)), each voice
pulled toward the SAME GLOBAL reference (AVERAGE_POLY = mono+Σpoly average, or MONO_DRAW
= voice 1's slewed draw). The read-side pin remap then makes voice v's display/articulation
read random_[caSrcRow(v)] — the SOURCE's bank, already spread-resolved toward the global
hub for the SOURCE, not the consumer. So:
- The consumer's OWN spread knob is invisible (its bank isn't the one being read).
- With spread high, every bank is pulled toward the same hub, so all banks look alike →
  pinning to any source shows ~the same thing, and switching the target barely changes it.

This is the read-side architecture's structural cost surfacing: PIN PICKS THE BANK, but
SPREAD WAS COMPUTED PER-BANK toward per-bank/global references. Reading a borrowed bank
borrows its spread; the consumer's spread is bypassed.

THE DESIGN QUESTION (decide before fixing — do NOT rush): when voice v is pinned to
source s, whose SPREAD applies?
  (A) Consumer's spread on borrowed DRAWS. Pin borrows s's raw/slewed draw, then v applies
      v's OWN spread toward the hub. Requires spread to run AFTER the pin remap, on the
      remapped pre-spread buffer — i.e. remap the SLEWED buffers (pre-spread), then spread.
      This is the candidate-level idea again, one stage earlier than random_. Matches "same
      dice, own manipulation" and makes the spread knob live for pinned voices.
  (B) Inherit source's spread (current accidental behaviour). Pin borrows s's fully-resolved
      stream including s's spread. Coherent IF documented, but makes the consumer's spread
      knob dead when pinned — surprising, and the reported bug.
  (C) Post-mix pin stays; spread re-applied per-CONSUMER after the remap on the borrowed
      value using the consumer's reference. More code, and double-spreads (source already
      spread) unless we remap PRE-spread — which collapses to (A).

Leaning (A): remap at the SLEWED (pre-spread) buffers so each voice spreads its borrowed
draw with its own knob toward the hub. This is where the "map right after the draw" instinct
was actually pointing. But it reopens the two-fill-path question (non-Sands slew copy vs
Sands SpreadInterp) and the GUI-thread-writer race — so it must be designed against the
GUI_THREAD_FINALS_MIGRATION plan, not bolted on. NEXT-SESSION DESIGN TASK, with build tests.
