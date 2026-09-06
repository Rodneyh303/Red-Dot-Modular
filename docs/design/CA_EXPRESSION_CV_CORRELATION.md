# Change Alley — correlated poly-CV expression pairs (feeds Keppel MPE Y/Z/vel)

**STATUS: PARKED SPEC — sequenced AFTER q-mix lands. No code. Design-only.**
Captured 2026-09-xx from a design conversation. This is version **B** (routing/permutation
correlation), deliberately chosen over version A (a new drawn expression stream inside the engine).
Rides existing pin/scatter machinery — "arrangement, not new engineering."

Related: [[RANDOM_VS_INPUT_MODULE_CONCEPT]] (q-mix = the 3rd stream this depends on),
[[CA_PANEL_THREE_STREAM_LAYOUT]] (this lands on Plan B — see below),
[[MPE_UTILITY_BUILD_SPEC]] (Keppel — the consumer of these CV pairs, its Y/Z/pressure/vel inputs).

## The idea (one line)
CA gains a fixed bank of poly-CV in/out pairs; each pair permutes its poly channels by ONE
stream's LIVE voice correlation, so external expression envelopes (patched in) come out reordered
exactly the way CA is reordering that stream's notes — then feed Keppel's per-voice MPE expression
inputs (velocity / Y=CC74 / Z=channel-pressure). Result: per-note MPE expression that is correlated
with the pitch material by the same order↔chaos machinery that governs the notes.

## Why version B (not A)
- **A (deep):** make expression a NEW per-voice Philox stream that CA scatters alongside
  rhythm/melody/q-mix. Most on-thesis, but real engine work — another stream, more CA draw
  dimensions, lives in Monsoon/CA. DEFERRED / maybe never.
- **B (chosen):** permute EXTERNAL poly CV by CA's EXISTING per-stream voice correlation. Cheap
  (a permutation lookup, no new draw), decoupled (Keppel just receives poly CV; correlation happens
  in CA), and demonstrates the thesis audibly. Correlates the ASSIGNMENT of externally-shaped
  contours, not the shape itself — voice 1's envelope goes where voice 3's note went. That's enough.

## Fixed per-stream pairs (NO selector)
Rather than a per-pair selector (which needs a mode param, UI, save/load, runtime indirection),
allocate pairs to streams at BUILD time. Example for an 8-row budget:

- **3 pairs → rhythm**
- **3 pairs → melody**
- **2 pairs → q-mix**

Rationale: fixed allocation is compile-time known, zero per-pair state, deterministic layout, and
cheaper to compute AND to build than a selector. The 3/3/2 split mirrors the streams' weight
(rhythm + melody are the full expression-worthy dimensions; q-mix is newer/narrower). Rebalancing is
a constant, not a redesign. Different expression dimensions can thus follow different musical
dimensions — e.g. pressure→melody (tracks pitch relationships), timbre→rhythm (tracks groove),
another→q-mix (tracks the composed-vs-generated gradient).

## LIVE correlation (follows the verbs, phrase-granular — NOT per-sample)
Each pair re-reads its stream's CURRENT voice permutation whenever CA's scatter/collapse/rotate/
reflect updates that stream — i.e. at PHRASE granularity, the cadence the pin/scatter state already
changes. NOT audio-rate. This keeps it cheap (a permutation lookup at scatter-change, not a
per-sample op) and avoids reassigning a contour mid-note. "Live" here = "follows the verbs", not
"audio-rate". Static-topology-follow was considered as a cheaper start but LIVE is the decision —
it's what makes it feel connected to the order/chaos verbs.

## Correctness discipline (get this exactly right at build time)
- **Same permutation object as Keppel consumes.** A pair follows its stream's voice permutation
  (which voice maps where); that permutation must be the SAME state Keppel's voice→channel (LRU) map
  ultimately consumes, or pitch and expression correlate to subtly different states. One correlation
  state, many consumers.
- **Permute the CV, exactly.** Poly-CV OUT = poly-CV IN with channels mapped through the stream's
  current voice permutation. Literally "your envelopes, reordered the way CA is reordering the notes."
- **Update at scatter-state change, not per note-in-flight** (see phrase-granular above).

## Panel — lands on CA Plan B (6-column reorg)
This is why it reinforces Plan B over Plan A (see [[CA_PANEL_THREE_STREAM_LAYOUT]]): Plan B is
(rhythm, melody, q-mix) × (intra, inter) columns in component-type sections. The expression-CV pairs
become another component-type row band, and the 3/3/2 split sits UNDER its own stream columns —
rhythm pairs beneath the rhythm column, etc. Topologically co-located with the stream each follows.
Plan B's regularity makes this trivial to lay out; Plan A's mirrored layout does not.
- **Check at build:** whether CA's existing (deferred) GRAIN / STEP_POLY_IN poly-mod inputs can be
  generalized into this expression-CV plumbing instead of adding all-new jacks — reuse over new HP,
  given CA's space crunch.

## Sequencing — AFTER q-mix
Hard dependency: q-mix IS the third correlation stream. The fixed pairs follow rhythm/melody/**q-mix**,
so the q-mix stream must exist and be settled first (its CA dimension expansion 8→12 = the 2×3×2
product, and the green source-select pin plane are the machinery this rides on). Building the
expression-CV pairs before q-mix lands = building against a moving target. Order: finish q-mix (+ its
Sands lane geometry) → then this drops in on the same stream structure.

## Parked one-liner (for the triage)
*CA (Plan B) gains a fixed bank of poly-CV in/out pairs — e.g. 3 rhythm / 3 melody / 2 q-mix on an
8-row budget — each permuting its poly channels by that stream's LIVE (phrase-granular) scatter
state, co-located under the stream columns, feeding Keppel's velocity/Y/Z inputs for correlated
per-note MPE expression. No selector, no per-pair state. Sequenced after q-mix.*
