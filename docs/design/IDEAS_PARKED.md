
## MRT line colours for the 6 Sands strands (parked — try some other time)

Observation (Rodney, morning commute): the MRT map has 6 lines; Sands has 6 strands.
Map the six official SMRT/LTA line colours onto the six strands as a per-lane identity
palette — deeply Singapore, recognisable, and each colour already carries strong
standalone identity (they were designed to be told apart at a glance on a map).

The six lines and their official colours (for when we try this):
- North South Line (NSL)      — red      #d42e12  (brand-adjacent — careful vs our #d4001a)
- East West Line (EWL)        — green    #009645
- North East Line (NEL)       — purple   #9900aa
- Circle Line (CCL)           — orange   #fa9e0d
- Downtown Line (DTL)         — blue     #005ec4
- Thomson-East Coast (TEL)    — brown    #9d5b25

Strand→line assignment is open (aesthetic, not semantic). One natural cut: the two
"pitch" strands (MELODY, OCTAVE) get the two trunk lines (NSL red / EWL green), the
rhythm family (RHYTHM, ACCENT) get CCL orange / DTL blue, and the articulation pair
(VARIATION, LEGATO) get NEL purple / TEL brown. Revisit against the dark LCD ground —
purple and brown may need lifting for contrast on #0b0c0d.

Caveat: NSL red (#d42e12) sits very close to brand red (#d4001a); if used as a lane
colour it competes with accent/connection red. Either reassign NSL's slot or shift it.
Applies wherever lanes are colour-coded: Lantern lanes, Sands editors, Change Alley
pool tinting.

### Refinement (Rodney): two lane-palette THEMES, and the red-role conflict

Structure it as TWO selectable lane palettes, not a replacement:
- **Pastel** (current) — the existing muted lane hues. Stays the default identity.
- **MRT** — the six line colours as lane identity.
Palette becomes a theme choice (context menu / settings), like dark/light already is.
This keeps the pastel work and makes MRT opt-in.

The red-role conflict (the real design pin): brand red #d4001a currently carries a
FUNCTIONAL role — accent emphasis (Lantern top edge, accented notes) and expander
CONNECTION (the connect dot). In MRT theme the NSL lane wants red too, and it should be
BRAND red (#d4001a), not MRT's #d42e12 — one canonical red in the product. That means a
lane and a functional signal would share the exact colour.

Resolution: keep brand red for the NSL LANE in MRT theme, and move the
ACCENT/CONNECTION functional signal to a different colour SO THE FUNCTION STAYS
LEGIBLE against a red lane. Candidates for the functional accent/connection colour:
- amber/gold #c8960c (already a secondary brand colour; reads as "active/attention")
- bright white — but white is a Change Alley pin (rhythm), so context-dependent
Leaning amber: it's already in the palette, distinct from every MRT line, and "amber =
this is the live/accented/connected thing" is a clean second signal alongside red.

Note this makes the accent/connection colour a per-theme value too: pastel theme can
keep red-as-accent (no lane collision there), MRT theme uses amber-as-accent. So the
functional-signal colour is bound to the lane palette, not global. Decide together when
we do the colour pass.

## Transitive reduction / closure as graph verbs (parked — post-Stage-F)

Question (Rodney, 2026-10-27, after F-3 landed): is it worthwhile to add transitive
reduction and closure to the graph verbs later?

Banked answer: the two words earn OPPOSITE verdicts, and one fact decides it — which
mechanisms read the DIRECT edge set.

**The deciding fact.** The per-sample algebra self-closes: E(v) = Own(v) ∧ ∧ E(u)
closes transitively through the materialized topo fold (F-2), C3's nv folds max over
parents transitively, C2's rest propagates through the forced-rest cascade — so the
STATE-intersection mechanisms (C5 envelope, C2 forcedRest, C3 lengthCeiling) are
INVARIANT under redundant edges: an edge v→g implied by v→u→g adds nothing. But the
EVENT mechanisms read the direct edge set verbatim: C1's offer union — a grandparent
that onsets offers DIRECTLY through a redundant edge even when the intermediate
container doesn't (the F-2 gap-hosted refinement rides the same way); C4's hold-follow
picks its shape from the topologically-first SOUNDING container — a redundant edge adds
an earlier candidate whose held-gate waveform can differ; D4's first-onsetting container
names the gateType. A redundant edge is therefore NOT a no-op — it is a musical
feature: "the child hears the grandparent's onsets directly."

**Transitive closure as stored state: NO.** The fold already closes; storing closure
would double-store truth, bloat the raw uint16[16] that IS the serialization (the
byte-identity anchors pin it), and CHANGE C1/C4 semantics by manufacturing shortcut
offers nobody authored. As a QUERY it already exists (ancestorClosure, wouldCycle); as
a GESTURE it is 90% shipped (collapseAll = the closure onto roots; a full-ancestor
variant is a one-liner if ever wanted).

**Transitive reduction as AUTO-canonicalization: NO.** Not semantics-preserving (the
same C1/C4 argument, reversed) — it would silently delete deliberate shortcut offers.
Cleanup that changes sound is exactly what §8 forbids.

**Reduction as an EXPLICIT gesture ("DECLUTTER"/PRUNE): YES, later.** Where it earns
its slot: (a) legibility — COLLAPSE re-links grandparents, EXPAND restores from
trajectory memory, INVERT transposes; after a gesture march the drawn pin graph can
carry shadow edges, and a prune shows the load-bearing spine; (b) it is COLLAPSE's
natural companion — collapse makes implied structure explicit, reduction makes implied
structure implicit, and together they give the user full control of the offer/shape
topology without touching the envelope algebra; (c) the parked SEED verb wants it — a
random-forest generator can offer reduction as a post-pass (a flag, not forced: random
shortcut edges may be exactly the variety wanted). Cost: trivial — edge v→u is
redundant iff u ∈ ancestorClosure(w) ∪ {w} for some OTHER parent w of v; compute the
full redundant mask first, then strip (pure, order-free), edit-rate only, never
per-sample; the engine is untouched (materializeForest re-derives from the words at the
next graph change). Two property rows in the existing seeded-DAG harness:
closure(reduce(g)) == closure(g), and reduce is idempotent. Sits in
ContainmentTransforms.hpp next to collapse/collapseAll, gated by the same DAG checks.

Timing: NOT in the F ladder (F-4 ghost D5 + D6 VAR/LEG, F-5 property suite + compat
pin). Slots in post-F with the seed verb and the three deferred rulings (plan §3.4) —
module-layer only, zero engine surface.
