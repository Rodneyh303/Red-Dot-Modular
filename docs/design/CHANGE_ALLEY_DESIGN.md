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
3. Pin CV: a trigger input that re-scatters / re-identities the board (performance
   gesture), or fully manual? (Dice already re-rolls material; a "scramble pins" button
   would be the correlation-space analogue of DNA scramble.)
4. Panel: booth/alley motif around the board — the matrix as the row of exchange
   counters; needs the render mock from §6 first.
