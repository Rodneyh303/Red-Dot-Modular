
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

### Adjudication (2026-10-27, Rodney's questions): reduction and closure as QUERY / VERB / TRANSFORM / GESTURE

Rodney pulled both ideas forward and asked two questions directly: should we have a closure
gesture, and should collapseAll take parameters? Ruled on the axis the codebase already
speaks. QUERY = read-only predicates (ContainmentGraph.hpp). VERB = the per-edge edit
primitives (same header — addParent/removeParent/clearParents, the only deliberate routing
writers). TRANSFORM = the pure whole-graph rewrites (ContainmentTransforms.hpp). GESTURE =
the triggered row (ContainmentPending.hpp: armed, stamped, applied at the phrase boundary,
reversible through the EXPAND trajectory ring). Laying both ideas on the axis:

**REDUCTION, by layer:**
- VERB: NO — never an edit primitive. Reduction is a derived whole-graph rewrite; the edit
  verbs stay the only per-edge writers (closure-as-stored-state / reduction-as-auto are
  already NO above, and the same argument covers the verb slot).
- TRANSFORM: reduceTransitive(g) — YES, LANDED at this ruling next to collapse/collapseAll.
  The strip is the banked trivial cost made exact: v→u is redundant iff ANOTHER parent w of
  v reaches u upward in the ORIGINAL graph (all witness reads against the `before` snapshot,
  so the strip is PURE and ORDER-FREE — a witness path from w can never touch the deleted
  edge, since revisiting v would be a cycle). Root-set invariant (a strip needs a SECOND
  parent; the "stripped-by" relation among a vertex's parents is acyclic — a maximal parent
  always survives — reduction never orphans a voice). The two banked property rows landed
  with it in T11: closure(reduce(g)) == closure(g) MASK-EXACT, and reduce is idempotent (the
  result IS the unique DAG reduction), 300 random DAGs per run.
- GESTURE: DECLUTTER/PRUNE — YES, explicit trigger only, stamped, EXPAND-reversible, never a
  background pass (§8). The pure verb is landed; the trigger row parks with the panel work
  (the same seam as COLLAPSE-ALL-ONTO — row order is lockstep with the panel generator).

**CLOSURE, by layer:**
- QUERY: SHIPPED (ancestorClosure, wouldCycle).
- TRANSFORM: SHIPPED — collapse (peel one level, marches to empty) and collapseAll (flatten
  onto roots); now joined by collapseAllOnto (Ruling 2).
- GESTURE: SHIPPED as V_COLLAPSE_ALL — and ruled CLOSED as a vocabulary slot (Ruling 1).

**Ruling 1 — should we have a closure gesture? NO new verb; the gesture already exists.**
COLLAPSE and COLLAPSE-ALL already span the closure axis (one level / all the way onto
roots). A distinct CLOSER verb would be a third entry point over the same underlying
operation (new_parents[v] = closure(v) ∩ M for some mask M), differing only by its mask —
synonymy, not a new decision, and the CA trigger discipline is one trigger = one decision.
The gesture surface stays: ROTATE / REFLECT / INVERT / COLLAPSE / COLLAPSE-ALL / EXPAND.

**Ruling 2 — should collapseAll take parameters? YES, exactly one: the anchor mask.
LANDED at this ruling. Nothing else, ever:**
- collapseAllOnto(g, anchors) — flatten every voice UNDER the anchor set ONTO the set:
  new_parents[v] = closure(v) ∩ anchors when that is non-empty; every other voice keeps its
  wiring VERBATIM. The membership key is load-bearing, not a nicety: the naive mask form
  (out[v] = closure(v) & A for ALL v) would silently DETACH any voice whose ancestors miss
  A — 0<1<2<3 plus a separate component 9<10 with A={3} would orphan 9. Detaching is the
  edit verbs' deliberate business, never a flatten's side effect. The anchor's own
  containment is untouched (A={2} in 0<1<2<3 keeps 2<3 — "flatten the cluster under 2 onto
  2"). CLOSED and IDEMPOTENT by the mask-uniform argument (every new edge was an old
  ancestor, so reachability only shrinks; self-bits need a cycle). collapseAll is now
  DERIVED — the same function with the derived root mask (every upward path in a DAG
  terminates at a parentless voice, so the membership key always holds there), byte-identical
  to its pre-anchoring body, compat-pinned per seed in test_containment_transforms T10.
  Honest-edge contract pinned alongside: a flatten can only PROMOTE existing ancestors to
  direct parents, never invent a containment relation. One parameter is inside the house
  idiom (ROTATE's "<=1 param: step, default 1" precedent in the same header).
- Depth/level count: NO, permanently. Iterated collapse already composes per trigger — the
  performer owns the counting; a depth knob would re-open the step-0 smoke adjudication
  (flatten-toward-root is collapseAll's job, NOT iterated collapse) as a hidden mode.
- Predicate/kind filters: NO, permanently. Cleanup-with-knobs is the §8 trap, and the graph
  has no kinds — every edge is a deliberate offer.
- Tie-break modes: NO, permanently. R1 killed tie-breaks; the OR-union needs none.

**Still parked after this ruling:**
- The full-ancestor form (anchors = 0xFFFF — every voice gains ALL its ancestors as direct
  parents) is the flooding case: C1's offer union and C4's hold-follow candidates multiply
  by the closure size — closure-as-stored-state arriving through a gesture's back door. It
  stays ONE CALL away (collapseAllOnto(g, 0xFFFF); T10 proves it is still a valid, loadable
  DAG) but no verb row, no default, no panel button ships it. Revisit only when a
  performance need demonstrates itself.
- The trigger wiring: which panel row / context voice feeds the mask is module layer,
  parked with the containment panel work (row order is lockstep with the panel generator —
  a new row is a three-file change). The pure verb is ready.
- The seed verb's reduction post-pass flag — now ONE CALL: the flag invokes
  reduceTransitive(g) when the seed verb lands (a flag, not forced: random shortcut edges
  may be exactly the variety wanted).

Update 2026-10-27 (twice over): BOTH sides of this entry pulled forward from post-F in the
same stroke — closure first (collapseAllOnto + T10), then reduction (reduceTransitive + T11)
per Rodney's "reduction as an EXPLICIT gesture: YES". Both are pure transforms + property
rows + this ledger: module-layer only, zero engine surface, goldens untouched. The gesture
vocabulary stays SIX rows (ROTATE / REFLECT / INVERT / COLLAPSE / COLLAPSE-ALL / EXPAND);
neither new verb occupies a trigger slot — their trigger wiring parks with the containment
panel work. Reduction keeps its EXPLICIT-only rule: the strip deletes deliberate shortcut
offers (C1/C4 read direct edges), so it runs only when the user pulls the trigger, and the
EXPAND trajectory ring makes that trigger reversible.

### RANDOM uncloses the gesture rows (ruled 2026-10-27, Rodney) — the seed verb becomes row 7

Rodney's ruling at seed-verb pull-forward: "Random needs to unclose the 6 gesture rows. It
needs to be on panel with jack and button." The closure recorded above was an anti-SYNONYMY
ruling — no third entry point over an operation the rows already span. RANDOM is not
synonymy: it is a genuinely new decision class (introduce randomness into the graph), so the
vocabulary accepts it. The closure stands against synonymy; it does not stand against new
decisions.

**The row: V_RANDOM = 6, N_VERBS 6 -> 7.** Same shape as the six: trig jack (left flank) +
fire button + pending lamp, armed through the single choke point (armVerb — the CA latchRow
rule), stamped on the unarmed->armed edge, applied at the phrase boundary in stamp order,
committed (pre-push held state, post-push result), consumed. Row order stays lockstep with
the panel flank (gen_containment.py + MonsoonContainment.hpp + ContainmentPending.hpp — the
three-file change). ZERO param slots preserved (the DAW_PARAM_AUDIT / ChangeAlleyV2 rule):
draw configuration lives in context menus, like ROTATE's step.

**Two variations (Rodney), and they split cleanly along CA's own state fork**
(CA_DICE_COUNTER_MODEL.md, state-dependent vs stateless/absolute):
- OUTRIGHT NEW FOREST — the STATELESS/ABSOLUTE model: the draw specifies the graph
  absolutely, the graph IS f(counter, key), so reverse-draw IS an exact state restore
  (at(N-1) re-derives the previous forest bit-for-bit). This is the CA doc's own "when
  stateless would be right" case: a teleport to fresh configs, counter-alone
  reproducibility, reverse-as-undo for free. Proven core already in the tree: the F-5
  generator recipe (Fisher-Yates topological permutation of the 16 slots, parents only from
  earlier positions — acyclic by construction — density plus lone-parent connectivity
  boost), lifted from the test harness into the module layer with the stream position in
  place of the test seed.
- MODIFY FOREST (state-dependent scatter) — "more like change alley scatter which randomly
  modified pins state dependent." The STATE-DEPENDENT model exactly: the draw transforms
  the CURRENT graph (re-pin edges dependent on where the pins are), path-dependent,
  successive scatters evolve a trajectory, composes with the other six rows. Wanted
  CONDITIONALLY ("if a sensible method available") — the sensible method exists and is the
  CA scatter shape applied to edges: per-voice draw under wouldCycle gating (add/remove/
  re-source parent bits), validity preserved by the QUERY layer the pin plane already
  trusts. The exact method (which bits, the anti-knob budget — rate knobs/filters/
  tie-breaks stay NO per the collapseAllOnto precedent) parks with the wiring work; the
  requirement is recorded now.

**The four either-way requirements (Rodney), each grounded in the CA model:**
1. OWN PHILox DRAW — a domain-separated Philox stream of its own (fixed key encodes the
   domain, the CA dice-stream rule), counter = signed addressable POSITION in the stream
   (rng.at(position)), NOT seed-keying (the CA doc's "before" anti-pattern: a counter used
   as a key jumps to a different sequence per count instead of stepping the draw).
2. REVERSE-DRAW ACTION like change alley — counter-- -> at(N-1) is EXACTLY the previous
   draw; a generative scrub (apply the previous randomisation to the current graph), NOT an
   undo. For OUTRIGHT the scrub is also the exact state restore (stateless); for MODIFY it
   is not (state-dependent) — which is exactly why requirement 3 exists.
3. INCLUDED IN THE GLOBAL REVERSE — "like change alley, philox key reverse is not enough to
   fully reverse a state dependent modification": draw-reversible != state-restorable. The
   global reverse (the CA true-reverse proposal; LOR_BACKLOG §5's transport-level direction
   — advancePlayhead(dir) + the draw-index rewind, named MIRROR if built) must replay
   containment's committed-state trajectory backward. Containment already owns the
   ingredient: ContainmentTrajectory (65,536 committed phrase-boundary states, sparse via
   pushIfChanged, stepBack, verb-agnostic — built true-reverse-shaped on purpose; the
   EXPAND note cites CA's true-reverse as its precedent). RANDOM commits enter that
   trajectory like any row, so a state-dependent MODIFY steps back through the trajectory,
   not through the counter.
4. UNDO PARITY — "if change alley supports undo so should containment." ALREADY LANDED at
   step 4: ContainmentModuleCore publishes before/after snapshots into the 16-slot SPSC
   undo ring at every phrase-boundary commit; the widget drains them into Rack history
   (ContainmentTransformUndoAction — one commit = one undo step, no-op batches publish
   nothing, drop-if-full the CA rule); pin edits ride StoreEditAction. A V_RANDOM row
   inherits all of it BY CONSTRUCTION — its commit is just another verb batch through
   applyAll. Nothing new to build for this requirement; the parity Rodney asked for is the
   step-4 work already in the tree.

**Engine surface: ZERO.** Both variations write the graph through the same module-layer
choke point the verbs use; the engine re-derives (materializeForest) at the next graph
change; the transport (containmentParents staged every block) is the producer interface.
The Philox stream is module-layer — the engine's own draws are untouched (domain
separation is the point).

**Still parked with the row:**
- The three-file wiring itself (V_RANDOM = 6: panel row + module param/input/lamp + pending
  dispatch + the two pure verbs) — parked with the containment panel work, alongside the
  COLLAPSE-ALL-ONTO / DECLUTTER trigger rows.
- MODIFY's exact scatter method and its parameter budget (density only, as in the F-5
  recipe; no shape presets, no per-edge kinds — the graph has no kinds).
- The reverse-draw affordance shape (a back jack like CA's, a context-menu action, or a
  modifier — CA's back-jack is the precedent).
- The global reverse itself (requirement 3) is cross-module work tracked by
  CA_DICE_COUNTER_MODEL.md and LOR_BACKLOG.md; containment's obligation is recorded here so
  the row lands reverse-ready — its trajectory is the data the global reverse replays.
