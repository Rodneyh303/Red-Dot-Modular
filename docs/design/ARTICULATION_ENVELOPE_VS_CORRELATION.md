# ARTICULATION: "within an envelope" (containment) vs correlation — a design question (Rodney)

NEEDS MORE THOUGHT/DISCUSSION — note only, not decided. The first time "mono not special" has been
considered MUSICALLY (not just mechanically — indices/decisions/timing). Opened by the MONO_SPECIAL_IN_
LOGIC_AUDIT + the EAST_EXTRA_LANES history.

## LINEAGE (why the current behaviour exists)
Articulation rules originally came from Vermona Melodicer + mex3: "All three voices share NOTE VALUE,
VARIATION and LEGATO; but REST is set individually per voice." → poly voices were SUBTRACTIVE from the
MONO (V1) voice — they articulate WITHIN V1's envelope. This carried into dot.modular; then EAST_EXTRA_
LANES progressively made things per-voice-flexible; eventually FULL equality across all random sources
(every lane). But the "voices articulate within V1's envelope" behaviour was never RE-EXAMINED against
the sophistication we now have (CA + copula correlation). It's a Vermona RESIDUE whose V1-specialness was
never chosen for THIS instrument.

## THE KEY DISTINCTION (the crux)
"Voices articulate within V1's ENVELOPE" (CONTAINMENT/nesting) ≠ "voices CORRELATED to V1" (copula).
- CONTAINMENT (nested/subtractive): V1 defines a gate WINDOW; poly voices can only articulate INSIDE it —
  rest/shorten/subdivide WITHIN V1's note, but CANNOT sound when V1 is silent, CANNOT extend past V1's
  gate. A hard SUPPORT constraint. Lets a voice be rhythmically active (ratchet/subdivide) INSIDE V1's
  held note, guaranteed bounded by it = a specific texture (sustained envelope + inner voices dancing
  within it).
- CORRELATION (copula/CA): each voice draws its OWN rest/legato/length; the DRAWS are statistically
  dependent (tight/loose/anti). A ρ≈1 voice TENDS to rest with V1 but CAN sound when V1 is silent, and
  draws its own gate length. A tendency, not a boundary.
ASYMMETRY (why they're NOT redundant): even ρ=1 correlation makes voices AGREE, it does NOT make a voice
UNABLE to sound outside another's gate, nor let it freely subdivide INSIDE while hard-bounded OUTSIDE.
Containment is a support-nesting constraint; correlation operates on the decisions. CORRELATION CANNOT
REPRODUCE CONTAINMENT. They are COMPLEMENTARY, composable axes.

## THE TWO QUESTIONS (Rodney)
1. Keep / flex / remove "within V1 envelope"?
   LEANING (not decided): KEEP the containment CAPABILITY (unique — correlation can't do it), but STRIP
   the V1-SPECIALNESS (the Vermona residue). Don't remove (loses the sustained-envelope+inner-voices
   texture); don't keep-as-is (V1-hardcoded = the mono-special being eradicated). → DECOUPLE the envelope
   from V1.
2. Allow "within ANOTHER envelope" (CA source, arbitrary)?
   LEANING (not decided): YES — make the ARTICULATION ENVELOPE a routable per-voice/per-group SOURCE:
   within V1 (current, now an OPTION) / within a CA-routed source (→ NESTED VOICE GROUPS: voices 2-4
   within v1's envelope, 5-7 within v5's) / within an external CV or another lane's gate / within NONE
   (independent — the full-equality case we have). Same "stop hardcoding V1, make it a routed input"
   pattern as spread/warp/direction.

## RESOLUTION OF "MONO NOT SPECIAL" (the musical meaning)
"Mono not special" MUSICALLY = V1 is not PRIVILEGED, but CAN be CHOSEN as a reference. Vermona made V1
the MANDATORY envelope; equality makes V1 one SELECTABLE envelope among sources (itself / CA / external /
none). So "mono not special" = "the envelope is ROUTABLE, defaulting to V1 for compatibility", NOT "no
envelopes". Equality DE-PRIVILEGES the envelope into a routing choice; it does not abolish it.

## HOW IT FOLDS INTO THE ARCHITECTURE (if pursued)
- Envelope SOURCE = a routing decision (CA-like, or the resolver's ownership/delegation generalized from
  "whose VALUE" to "whose ENVELOPE").
- CONTAINMENT = a DECISION-MANAGER rule: "this voice's gate decision is BOUNDED by {envelope source}'s
  gate window" — fires at step edge, mode-independent; a decide() input:
  decide(prob, draw, lane, ownership, ENVELOPE_SOURCE) → clamp the voice's gate to the envelope's window.
- V1-default = just the default routing (envelope = V1), preserving current behaviour until re-routed.
- Composes with correlation: a voice can be CONTAINED within V1 AND correlated in its within-envelope
  articulation — nested AND statistically related.
So it's NOT new machinery — it's the envelope-source routed through the decision-manager + CA authorities.

## THEORY CONNECTION (for REACHABLE_STRUCTURES_THEORY)
CONTAINMENT is a dependence relation that is NOT a copula — it is a hard SUPPORT-NESTING constraint on the
gate (copulas describe the DECISION dependence; containment constrains the SUPPORT). So it sits OUTSIDE
the copula framework as a separate structural operator. Worth adding to the reachable-structures map as
"the non-copula dependence: support nesting (containment), composable with the copula decision-dependence".

STATUS: note only, needs discussion. Decide (1) keep+generalize vs remove, (2) which envelope sources,
when the decision-manager + CA work is on the table (its natural vehicle). The containment-vs-correlation
distinction is the thing to hold onto: they are complementary, not substitutes.

────────────────────────────────────────────────────────────────────────────────
## ADVANCING THE DESIGN (Rodney) — CA source as envelope, chaining, gate mode, the control

Following the CA-spread fix (resolver computes / rate manager schedules / convergence-at-step), the
envelope can ride the SAME resolver+CA machinery. Resolved points:

### 1. The RHYTHM CA source IS the natural envelope — but containment is a SEPARATE toggle from spread
The articulation envelope is a RHYTHM concept (gate windows) → route it via the RHYTHM CA source. BUT the
rhythm CA source currently drives CORRELATION (spread-follow-CA). Using it ALSO as the CONTAINMENT envelope
means ONE routing carries TWO relations: "correlated to the source" (spread) AND "contained within the
source's gate" (envelope). They COMPOSE (complementary, established above) but must be SEPARATELY
switchable. → ONE routing (which CA source), TWO independent relations.

### 2. CHAINING gives TRANSITIVE NESTING for free (CA tree = containment hierarchy)
Does it matter if the CA source is identity-mapped or chained? YES, and it's a GIFT:
- IDENTITY source: v2 contained in v3, v3 is its own gate → one level (v2 ⊂ v3).
- CHAINED source: v2 in v3, v3 itself in v1 → containment is a SUPPORT constraint (can't sound outside the
  gate), so v3's ACTUAL gate is already v3∩v1 (it's clamped before v2 reads it). v2 ⊂ v3 ⊂ v1 holds
  TRANSITIVELY BY CONSTRUCTION — each level clamps before the next reads. So deep CA chains = deep nesting
  (inner voices within inner voices within an outer envelope) FOR FREE. The CA ROUTING TREE *is* the
  CONTAINMENT HIERARCHY, composing with the copula correlation on the same rank-1-per-block tree.
- NO CYCLES (Rodney, confirmed): a routing cycle makes containment ill-defined (mutual clamp). The CA tree
  must be ACYCLIC — containment makes cycle-prevention LOAD-BEARING (it already is for correlation).

### 3. GATE MODE IS FINE — the envelope is about DECISIONS, not the timing grid (Rodney corrected)
Earlier (wrong): "gate mode → envelope = V1-only, because V1 sources the gates". CORRECTION: V1 sources
the gate/subgate TIMING GRID (when gate events happen), but EACH VOICE makes its OWN rest/legato/accent
decisions at those gate AND subgate events (eventually with own-knob response — the planned 2nd knobs).
Containment operates on the ARTICULATION DECISIONS (whose rest/legato nests whom), NOT on who generates
the timing grid. So a voice CAN be contained within another's articulation envelope in gate mode (its
rest/legato decisions nested within the source's) even though both articulate on V1's shared grid.
=> FOLLOW-CA ENVELOPE IS VALID IN ALL MODES (clock/phase/gate). The envelope routing is NOT mode-
dependent; only the TIMING-GRID source differs by mode (per-voice gates in clock/phase, V1's grid in gate),
and the grid isn't the envelope. (This is cleaner than the mode-dependent version — no special-casing.)

### 4. THE CONTROL — a SEPARATE articulation-envelope context menu (Rodney)
A new context menu for the ARTICULATION ENVELOPE: NONE / FOLLOW V1 / FOLLOW CA — INDEPENDENT from the
existing SPREAD-target menu (None/Follow-V1/Follow-CA). Same three options, same routing targets, TWO
independent relations:
  SPREAD menu   = CORRELATION (does my probability track the source's).
  ENVELOPE menu = CONTAINMENT (am I nested within the source's gate window).
→ the full 3×3 of containment × correlation, independently chosen. E.g. envelope=Follow-CA + spread=None =
nested but UNcorrelated (contained, independent decisions within); envelope=None + spread=Follow-CA =
correlated but UNcontained (tend to agree, can sound outside). ENVELOPE=NONE (Rodney): no containment, but
STILL full copula correlation + atoms + everything — just not nested gating. That's the full-equality case
(likely the DEFAULT: uncontained unless routed). ENVELOPE=FOLLOW-V1 = the Vermona "within V1" behaviour —
now ONE option among three, not the law. ("Mono not special" = the envelope is a routing CHOICE, default
uncontained; V1 selectable, not mandatory.)

### STATUS
Design substantially advanced; still future work (with the decision-manager/modifier-resolver + the
planned 2nd-knob poly articulation). To build: the envelope as a resolver modifier policy (clamp gate to
{None/V1/CA-source} window), the separate context menu, transitive nesting via the acyclic CA tree, valid
in all modes. The containment-vs-correlation two-axis structure (two menus) is the core.

────────────────────────────────────────────────────────────────────────────────
## WHERE ENVELOPE ROUTING BELONGS — CA, as a FOURTH PINNING PLANE (Rodney) — conceptually settled

Earlier tension ("reuse CA's one correlation tree = couples containment+correlation structure" vs "a
separate routing system = flexible but reinvents acyclic enforcement") is RESOLVED by recognizing what
CA actually IS and how it's already structured.

### CA is the ACYCLIC PER-VOICE SOURCE-ROUTING authority — and hosts MULTIPLE PLANES
CA = "for each voice, which other voice is its source, constrained ACYCLIC, forming a tree/forest." That
primitive is RELATION-AGNOSTIC — it means "who points at whom without cycles", not inherently
"correlation". CA already hosts THREE independent pinning planes: RHYTHM, MELODY, QMIX (each its own
caRhythmSrc/… tree). Containment is STRUCTURALLY THE SAME THING: a per-voice directed source assignment
(v2's envelope source = v3), acyclic (no mutual clamp), chaining → transitive nesting. So the envelope
routing is naturally a FOURTH PINNING PLANE after rhythm/melody/qmix.

### Why CA, not a separate system (the acyclic point is a REASON-FOR, not just a constraint)
Cycle-prevention is HARD/subtle (dynamic per-voice routing, re-check on change, partial-cycle transients).
CA ALREADY solves it (correlation needed it). A SEPARATE envelope-routing system would REIMPLEMENT acyclic
enforcement — the exact duplication-of-a-hard-thing every authority here exists to prevent. So the acyclic
requirement is a REASON to route the envelope through CA, not merely a constraint to satisfy.

### The PLANE model RESOLVES the coupling weakness (Rodney's "joins correlation + hierarchy")
Reusing CA's RHYTHM-correlation tree for the envelope would COUPLE the two axes' STRUCTURE (same source for
both) — partially undoing the independence we established (containment ⟂ correlation). A FOURTH PLANE fixes
it: correlation (rhythm/mel/qmix planes) and containment (the envelope plane) are SEPARATE independent
trees sharing CA's ONE acyclic machinery. So "correlate to A, be contained in B" IS expressible (different
planes). Independent TARGETS, not just independent strengths — the full independence the two axes deserve.

### ONE DISTINCTION for the UI refactor
The three existing planes route CORRELATION (whose draws you track); the envelope plane routes CONTAINMENT
(whose gate you're nested in) — a different KIND of relation (binary nest-or-not per voice, vs
source-to-track). Same routing PRIMITIVE (acyclic per-voice source), different CONSUMER. The UI may want
slightly different affordances for the containment plane.

### COST = UI, not concept
Conceptually SETTLED: envelope routing = a fourth CA pinning plane, shared acyclic authority, independent
tree, resolves the coupling, avoids reinventing cycle-prevention. The only cost is a CA UI REFACTOR to
accommodate a fourth plane (CA's panel is built for three) — a PRESENTATION problem (how to surface four
planes without overcrowding), NOT a where-does-it-belong problem. Arguably LESS total UI than a separate
envelope system (which needs its own full UI + its own acyclic machinery).

STATUS: conceptual home settled (CA, 4th pinning plane). Build: after the decision-manager/modifier-
resolver + the 2nd-knob poly articulation; the CA UI refactor (3→4 planes) is the gating practical work.

────────────────────────────────────────────────────────────────────────────────
## THE FOUR-OPTION MENU + CA UI DIRECTION + PIPELINE LAYERING (Rodney) — settled

### The envelope menu — FOUR options (Rodney)
  NONE                     — uncontained (full copula correlation + atoms still apply; no nesting). DEFAULT.
  FOLLOW V1                — nested in V1's envelope (the Vermona behaviour, as one preset).
  FOLLOW CA (rhythm source)— containment rides the RHYTHM-CORRELATION pin = COUPLED-DRAMATIC mode: a CA
                             phrase-reroute moves BOTH correlation AND nesting together → whole-architecture
                             reshuffle. (The reroute-amplification is the point — expose as intentional.)
  FOLLOW CA (containment pin)— containment on its OWN 4th pin = STABLE-SKELETON mode: independent tree, its
                             OWN cadence (slower / phrase-boundary), correlations breathe within a held
                             structure.
The last two are DISTINCT musical modes (coupled-dramatic vs stable-skeleton), surfaced as menu entries,
not implementation detail. Reroute CADENCE is separable from routing STRUCTURE (a rate-manager concern):
the containment pin can change more slowly than CA's per-phrase correlation reroutes.

### CA UI refactor — the direction (Rodney; needs it anyway)
CA (3 planes: rhythm/mel/qmix) is already crowded; the 4th plane is the OCCASION for a refactor CA needs
regardless, not a burden it imposes. Options weighed:
- REJECT: strip CA to 1-D + separate small CAs per plane — DESTROYS "all on one pin matrix" (the unified
  cross-plane view is a real feature: see voice 2's rhythm+melody+qmix+envelope pins in ONE picture).
- LEAN: 4th COLOUR (yellow) = containment, ON the unified matrix (it IS a 4th pinning plane, belongs on the
  matrix like mel/qmix) + a PLANE-FOCUS control (active plane bright, others ghosted / tab to edit one) to
  manage 4-colour overload. Matches the "4th pinning plane" concept; preserves the unified matrix.
- OPTIONAL/combinable: relocate the matrix to an optional SUBPANEL (like new Straits) if the main panel is
  too tight — orthogonal to the colour choice (subpanel = WHERE the matrix lives; colour = HOW the plane
  shows). Can do both.

### PIPELINE LAYERING — what "follow CA" actually clamps to (Rodney)
Full chain to a gate: UNIFORM probability → WARP (final per-voice marginal) → REACTION SURFACE (legato/
rest/variation responding to the warped prob) → REALIZED GATES (gate open/close, length, ties, subdivs).
- CA ROUTING operates at the LATENT/UNIFORM layer (cheap, acyclic) — correlation is uniform→uniform;
  containment ROUTING (which source) is also here.
- WARP is the per-voice MARGINAL (final prob shaping).
- CONTAINMENT ("follow CA for envelope") clamps a voice to the SOURCE's REALIZED GATES — the END PRODUCT of
  the source's FULL pipeline (its uniform + ITS warp + ITS reaction surface). So the envelope reflects the
  source's COMPLETE musical character (sparse-rest source → tight envelope; legato source → wide envelope).
=> CORRELATION and CONTAINMENT share CA ROUTING but act at DIFFERENT PIPELINE STAGES: correlation couples
the DRAWS (uniform→uniform); containment nests the REALIZED GATE SUPPORTS (gate→gate, post-reaction-surface).
That is the DEEPEST form of containment≠correlation: not just hard-vs-soft, but different pipeline layers.
FOLDS into the authorities at the right stages: routing = CA; the gate CLAMP = the decision/gate layer
(decision manager). NON-BUG NOTE: two FOLLOW-CA voices with DIFFERENT warps legitimately show DIFFERENT
bars (same latent, own marginal) — the warp-marginal, NOT the follow-CA display bug we just fixed; the
resolver includes per-voice warp so the bars correctly differ.

────────────────────────────────────────────────────────────────────────────────
## ENVELOPE PLANE CONSTRAINT — same functional acyclic forest as CA (Rodney)

CA constraint: each DESTINATION ← exactly ONE source; one SOURCE → MANY destinations = a FUNCTION from
destinations to sources, which (with acyclicity) is a FOREST (each node one parent, parents many children,
no cycles). The envelope (containment) plane has the SAME constraint — and that identity is WHY it fits as
a fourth CA plane (not merely "similar" — the same constraint SHAPE; CA's machinery enforces all four
planes uniformly):
- ONE source per destination: "I'm nested in ONE envelope." ✓
- MANY destinations per source: one voice's envelope contains many inner voices. ✓
- NESTING via CHAINING: v2 ⊂ v3 ⊂ v1 holds transitively (each level clamps before the next reads) — covers
  the nested-depth case. ✓
- NO CYCLES, enforced PER-PLANE: the envelope tree is independently acyclic; it and the rhythm/mel/qmix
  trees can have DIFFERENT shapes with no cross-plane concern (independent relations). CA already does
  per-plane cycle checks for its three independent planes → the 4th inherits the same. ✓

### DELIBERATELY EXCLUDED — multi-parent containment (gate-intersection)
Containment has a coherent multi-source meaning correlation LACKS: contained in A AND B = sound only where
BOTH gates open (intersection). But keep the plane SINGLE-source anyway:
1. STRUCTURAL UNIFORMITY — single-source keeps the envelope plane the SAME constraint (forest) as the
   other three; multi-parent would make it a DAG, structurally different, breaking the uniform CA/UI
   handling. Exception cost > gain.
2. CHAINING already gives nesting DEPTH (hierarchical intersection: nest in A, A nested in B → in A∩B
   transitively). The only thing lost is NON-hierarchical multi-parent intersection (A,B unrelated, both
   constrain you) — niche.
3. If that niche ever matters it is a SEPARATE later extension (multi-parent = DAG), explicitly flagged as
   breaking forest uniformity — NOT the default.
So: envelope plane = single-source-per-destination, many-dest-per-source, nesting via chaining, no cycles
(per-plane) — identical to CA's correlation planes. Multi-parent gate-intersection excluded by choice to
preserve uniformity; deferred niche.

────────────────────────────────────────────────────────────────────────────────
## CYCLES: valid for CORRELATION, forbidden for CONTAINMENT — the mechanistic reason (Rodney)

CORRECTS the earlier "envelope inherits CA's acyclic check". The SAME routing ring is VALID as correlation
but INVALID as containment. The reason (Rodney): correlation READS the underlying DRAW; containment
COMPOSES a derived gate.

### Worked ring: 4→1, 1→2, 2→3, 3→4
AS CORRELATION — VALID. "1 spreads to 2's underlying DRAW; 4 spreads to 1's underlying DRAW." The DRAWS
are INDEPENDENT, already-SAMPLED values that PRE-EXIST the spread blending. Spread just READS other voices'
draws to blend toward them. A cycle is only in WHICH draw you read — and all draws already exist — so
nothing depends on a not-yet-computed value. Nothing to regress into → well-defined in ANY pattern,
cycles included. (Implementation: the CA target reads pubSlewed* snapshots, SpreadInterp.hpp:98-111 — the
realization of "read a pre-existing value", effectively a one-generation-delayed feedback that converges.)

AS CONTAINMENT — INVALID (cycle). Containment does NOT read a pre-existing value — it COMPUTES a DERIVED
constraint: gate(X) = clamp(own_gate(X), gate(container(X))), where gate(container) is ITSELF derived
(clamped) one level up. A chain of DERIVATIONS, not reads. 4⊂1⊂2⊂3⊂4 (follow the chain) has NO base case
(no root with no container) and NO valid evaluation order → non-terminating recursion. A hard support
constraint CANNOT be one-generation-delayed (stale container gate = you sound in a window the container has
since closed = wrong). It needs the container's CURRENT-step clamped gate → in-order eval → acyclic.

### THE FUNDAMENTAL DISTINCTION
- CORRELATION = READING pre-existing independent DRAWS (any pattern, cycles fine — nothing to compute in
  order).
- CONTAINMENT = COMPOSING derived clamped GATES up a dependency chain (needs order + base case → acyclic).
Reading pre-existing values is cycle-safe by nature; composing derived values up a chain is not.

### CONSEQUENCE (corrects the inherited-check claim)
- CA CORRELATION planes (rhythm/mel/qmix): cycles ALLOWED (read pre-existing draws / pub snapshot).
- The ENVELOPE/CONTAINMENT plane: cycles FORBIDDEN — needs a STRICTER per-plane acyclic check the
  correlation planes do NOT have. So the 4th plane shares CA's ROUTING STRUCTURE (single-source functional
  mapping, the pin matrix) but imposes an ADDITIONAL acyclicity constraint. The envelope routing must
  REJECT a pin that would close a containment cycle (while the correlation planes allow the analogous
  ring). Single-source makes the check O(depth): follow the one container-pointer, reject if it returns to
  start. Runs at routing-change (phrase/deliberate), not per-sample.
This is WHY the decision-manager recursion up the containment tree is well-founded: the acyclic check
guarantees a base case (a root envelope) and finite depth.

────────────────────────────────────────────────────────────────────────────────
## RESOLUTION — containment = a FIXED acyclic forest, CA-transform-free (Rodney)

The whole Follow-CA-containment exploration FAILS at CA TRANSFORM MODULATION, and that failure is the
answer. Supersedes the fourth-plane / flat-subgroup / external-leader attempts above.

### Why Follow-CA containment is ILL-CONCEIVED at root
CA correlation is CYCLE-SAFE (reads pre-existing draws), so CA is FREE to modulate into ANY configuration
INCLUDING cyclic ones. Containment is CYCLE-FATAL (composes derived gates — non-terminating on a cycle,
and a hard support constraint can't be stale-delayed). A STATIC acyclic check is insufficient: CA transform
modulation re-partitions subgroups and reroutes leaders at RUNTIME → reaches containment-cyclic states the
user never configured (e.g. 1-4 follow 6∈{5-8}, then CA modulates 5-8 to follow into 1-4 → cycle mid-
performance = non-terminating gate resolution). You CANNOT put a cycle-fatal relation on a routing layer
designed to freely reach cyclic states. So containment canNOT ride CA — not nesting specifically, the WHOLE
idea of containment-following-(modulatable)-CA.

### The resolution: containment is its OWN FIXED routing
Containment = a dedicated, FIXED (NOT modulation-driven; user-EDITABLE by hand), acyclic FOREST over the
16 voices — each voice: container ∈ {none | a voice}, single-parent, acyclic, CHECKED ONCE at config
(no modulation to invalidate). "Fixed" = not on the modulation bus, NOT immutable (edit the skeleton
deliberately).
VALID ARRANGEMENTS = ANY FOREST (single container/voice, acyclic). Musically useful subset:
- FLAT-SINGLE-ROOT (universal envelope — Follow-V1 generalized to any chosen voice).
- GROUPED-FLAT (a leader per group, members contained in their leader) — the common case.
- TWO-LEVEL / NESTED (nested voice groups) — the rich texture; RECOVERED safely BECAUSE fixed (nesting
  wasn't the problem — MODULATABLE nesting was; fixed-check-once makes arbitrary depth valid). [Rodney:
  not sure we want nesting — but it's AVAILABLE if fixed.]
- deeper chains if ever wanted.

### FIXED-NESS IS THE STABLE-SKELETON DESIGN (not a limitation)
Containment being fixed while CA correlation is modulatable IS the containment-vs-correlation distinction
made concrete: DIFFERENT MUTABILITY BY DESIGN. Containment = the SLOW/FIXED skeleton; correlation (CA) =
the FAST/EVOLVING layer breathing WITHIN the fixed skeleton. CA transforms evolve the draw-dependencies
inside a held containment structure. The skeleton holds; the correlations reconfigure within it.

### CA TRANSFORMS vs containment — none drive it
- CA STRUCTURAL transforms (reroute who-pins-whom, re-partition subgroups): canNOT drive containment
  (reach cyclic states it can't have). These are the interesting CA transforms → so effectively NO CA
  transform drives containment.
- CA NON-structural transforms (correlation strength/weight/output, graph unchanged): IRRELEVANT to
  containment (modulate correlation; containment is a hard clamp, ignores them).
=> Containment is CA-TRANSFORM-FREE. Fixed forest, held; CA transforms the correlations within it.

### FUTURE ESCAPE HATCH (if dynamic containment ever wanted) — NOT CA
Dynamic containment would need a BESPOKE ACYCLIC-PRESERVING modulation — e.g. modulate a voice PRIORITY-
ORDER and derive the containment forest from the order (a total order can NEVER cycle → always acyclic).
That's a NEW mechanism, not CA. Flagged, not committed — alongside nesting as "consider later".

────────────────────────────────────────────────────────────────────────────────
## WHAT CONTAINMENT BUYS MUSICALLY (Rodney — verified in Rack: "fun messing with basslines by flipping containment")

The module is BUILT + merged (PR #297; multi-parent DAG with intersection, disconnected vertices valid,
DAG-closed gesture verbs, gate mode tested 109 refs, goldens byte-exact). What it buys:

1. STRUCTURAL VARIATION WITHOUT NOTE VARIATION (the strongest, the bassline fun). INVERT/ROTATE/REFLECT
   change WHO CONTAINS WHOM while draws + correlations are untouched → a recognizably-same-but-restructured
   phrase. INVERT on a bassline re-assigns which voice holds the rhythmic floor (the sustained root becomes
   a busy inner voice; a constrained voice becomes the envelope) — one trigger, the whole rhythmic
   architecture re-pivots, notes unchanged. Theme-and-variation where the variation is HIERARCHICAL, not
   melodic. Nothing else in the instrument does structural variation.
2. INTERSECTION-NESTING (the multi-parent payoff). A voice in TWO parents sounds only where BOTH gates open
   = a sparse PUNCTUATION voice firing at the COINCIDENCES of two envelopes. More parents → sparser (triple
   coincidence). A controllable rhythmic-density gradient via parent count — "voices that sound at the
   intersection of others' activity".
3. NESTED VOICE GROUPS = arrangement density. Deep chains (v2⊂v3⊂v1) give inner-voices-within-inner-voices;
   COLLAPSE/EXPAND modulate the arrangement busyness (flatten→all active in one envelope; expand→nested
   sparseness). A density macro via structural depth.
4. DE-PRIVILEGING as a creative affordance ("mono not special" made real). V1 can be a LEAF (bass contained
   within a pad's envelope; no privileged voice) — arrangements the old Vermona-V1-envelope model FORBADE.

WHAT IT IS NOT: no pitch/harmony change (that's correlation/warp), no added notes (it gates/subtracts). The
space is rhythmic texture, density, and which-voice-is-the-floor — exactly right for an articulation/
envelope feature, and a space the instrument didn't address before.
THE DISTINCTIVE AXIS (paper point): STRUCTURAL MODULATION — modulate the dependency GRAPH (DAG-closed
transform verbs, safe + gestural + phrase-boundary) rather than the VALUES. Most synths modulate
parameters; this modulates RELATIONSHIPS. INVERT-on-a-bassline being fun is the proof: a single structural
trigger yields a satisfying, non-obvious, musical transformation — a genuinely novel modulation axis.
