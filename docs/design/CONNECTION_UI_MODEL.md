# Connection / binding model — scoping doc (Rodney)

STATUS: SCOPING. Not a spec yet. Written to STOP piecemeal growth of connection UI and define it
systematically before more connection code (including the shared-CA "PRIMARY MONSOON" badge in
CA_SHARED_EXPANDER_BUILD.md, which should become a CONSUMER of the model defined here, not its own
local invention). Everything below marked **[OPEN]** is a decision for Rodney, not pre-decided.

This doc has two jobs: (1) honestly inventory what exists today and what breaks, (2) lay out the
questions a systematic model must answer. It deliberately does NOT choose yet.

---

## 1. What "connection" means today — FOUR partial mechanisms, never unified

Connection grew feature-by-feature when there was "one of everything" in a row. It breaks as soon as
there are multiple Monsoons (or the expander order changes). There are at least four separate mechanisms,
with different rules:

1. **`findMonsoonEitherSide(self, maxDepth=12)`** — `src/ui/VisualExpanderHelpers.hpp`. The WIDESPREAD
   one: **122 call sites** across nearly every module. Walks `rightExpander` up to 12 hops, then
   `leftExpander` up to 12. Returns the FIRST Monsoon found. **Right-first.**
   - Does NOT stop at foreign modules — walks THROUGH anything.
   - Does NOT treat another Monsoon as a boundary.
   - Connect mark lights iff this returns non-null.
2. **`MonsoonExpanderManager` scan** — richer: caches typed pointers, has boundary rules (stops at a
   foreign module; treats another Monsoon as a boundary). Used by ~18 files. This is the scan the
   earlier design notes described — but it is NOT what most connect marks use.
3. **pairId / pairColour** — `src/ui/IntertropicalPairing.hpp`. A rack-wide group token + 8-colour
   palette (`pairColour(id)`, `(id-1)%8`). `assignPairId`/`presentPairIds`. Used by Intertropical,
   Lantern, CA, ChangiT3, Interchange, MonsoonWidget. Built for Intertropical's Follow (group identity).
4. **ConnectMark rendering** — inconsistent per module:
   - Intertropical: DRAWS a filled `drawPairBadge` (colour + number). The only true pair badge.
   - Colonnades (MicroTuning base): tints its ConnectMark with `pairColour`.
   - CA: computes `pairId` (line 260) but DRAWS no badge.
   - Monsoon: no pairId badge, no pairColour.
   - Most others: a plain boolean connect dot (lit = host found).

**Root problem:** four mechanisms, three rendering styles, two discovery rules (naive walk vs manager
scan), and the naive walk is the common one. "Connection" is not one system.

---

## 2. Modules that participate in binding (inventory)

Bind to a Monsoon (host), or to each other, via discovery above. Live (non-deprecated):
- **Straits** (poly voice expander), **Sands** mono/east/macro (visual), **Causeway** (poly CV),
  **Raffles** (dice mod; not registered in plugin.cpp), **Interchange**, **Junction**,
  **Changi / ChangiT2 / ChangiT3**, **Shophouse** + **Shophouse Micro**.
- **Change Alley** (correlation; can be SHARED by 2+ Monsoons — see CA_SHARED_EXPANDER_BUILD.md).
- **Intertropical** (arranger; has pairId Follow; re-exposes host via getHost() for transitive
  consumers like Lantern).
- **Colonnades / Colonnades Duo** (MicroTuning base; CLAIM Monsoon tuning source).
- **Lantern** (pure observer), **Sikit** (observer).
- **Keppel** (CV→MPE; binds for gate+pitch source).
Deprecated (ignore): MonsoonChangeAlleyExpander, MonsoonSandsExpander, StraitWest, StraitsEast, Temasek.

---

## 3. What multiple connections are possible TODAY (and where each breaks)

- **1 Monsoon, N expanders, one row** — the original "one of everything" case. Works. Connect marks
  meaningful because there's only one host to bind to.
- **2+ Monsoons in one row** — BREAKS visibility. `findMonsoonEitherSide` is right-first + walk-through
  + no-Monsoon-boundary, so which host an expander binds to depends on ORDER and SIDE, and the connect
  mark (boolean) can't show which. Only reliable today if you STRICTLY ENFORCE same-row, one-Monsoon.
- **2 Monsoons sharing 1 CA** — intended feature (crab canon / polymeter). Correlation is symmetric so
  it works; but "which Monsoon is primary" for reseed/theme is invisible (the PRIMARY spec addresses
  behaviour; the badge to SHOW it is unbuilt — and depends on this model).
- **Cross-row** — pairId is rack-wide so Follow can cross rows; but `findMonsoonEitherSide` is
  same-row only (expander chain). So group identity and host binding use DIFFERENT reachability.
- **Transitive** (Monsoon→Straits→Intertropical→Lantern) — Lantern reads host via Intertropical's
  getHost(). Works when the chain re-exposes host; fragile when a middle module doesn't.

---

## 4. THE BUG CLASS — "connections you'd expect don't connect" (Rodney)

Same module set, different ORDER, different connect-mark result. Rodney's example:
- `Monsoon - Straits - Intertropical - Colonnades` → **Colonnades connect mark DIMMED**
- `Monsoon - Straits - Colonnades - Intertropical` → **all lit**

**Code-level cause (confirmed):** `findMonsoonEitherSide` walks by immediate `left/rightExpander` only,
right-first, 12-hop, through foreign modules, with NO Monsoon boundary. So whether a module "connects"
is a function of its DIRECTION to, and HOP DISTANCE from, the nearest Monsoon — i.e. its POSITION in the
row — not of whether a valid host exists in the patch. Reorder the row and the lit set changes. This is
position/direction-sensitivity where the user expects TOPOLOGY-sensitivity ("is there a Monsoon I can
reach?"). It is the SAME root as the multi-Monsoon visibility failure: the discovery primitive encodes
no notion of reachability-independent-of-order.

Likely contributing factors to audit per module (NOT yet fixed):
- right-first asymmetry (a host to the LEFT loses to a nearer/any host to the right, or vice-versa by
  distance)
- 12-hop cap silently truncating longer rows
- some modules gate their lit state on an ADDITIONAL same-row / adjacency / claim check beyond
  findMonsoon, inconsistently
- two discovery mechanisms (naive walk vs manager scan) disagreeing for the same physical layout

---

## 5. Relationship TYPES a systematic model must separate

These have been conflated into one dot. They are distinct:
- **(a) bound / unbound** — is this module driven by a host at all? (today: the connect dot)
- **(b) bound to WHICH host** — with 2+ Monsoons, which one? (today: invisible; the CMMC case)
- **(c) which host is PRIMARY** — for asymmetric ops when 2+ share one expander (today: spec'd for CA,
  badge unbuilt)
- **(d) group membership** — which members form one logical set, e.g. Follow #N (today: pairId/pairColour)

A single connect dot can carry (a) only. (b)/(c)/(d) each need MORE: identity token, direction, or colour.

---

## 6. Open design questions — [OPEN], for Rodney

1. **[OPEN] Group-anchored or edge-anchored identity?** pairColour colours a rack-wide GROUP (pairId).
   The CMMC "which Colonnades ↔ which Monsoon" problem is PAIRWISE (an edge). Follow wants groups;
   binding wants pairs. Is the unit the group, the edge, or both (and if both, do they share a token)?
2. **[OPEN] One discovery rule.** Collapse to a single mechanism. Should `findMonsoonEitherSide` be
   replaced by / made to honour the manager scan's boundary rules (stop at foreign module? treat another
   Monsoon as a boundary?)? Define reachability so it does NOT depend on row order.
3. **[OPEN] Boundary semantics.** Does a second Monsoon block the walk (so each expander binds only
   within its own Monsoon's "segment")? That would make binding order-independent and give a clean
   ownership story — but must not break shared-CA (which intentionally reaches two Monsoons).
4. **[OPEN] Same-row vs rack-wide.** Host binding is same-row (expander chain); pairId is rack-wide.
   Should they unify, or stay deliberately different (physical binding vs logical grouping)?
5. **[OPEN] Which modules participate in which relationship types?** Not all need (b)/(c)/(d). Define
   per-module: observer (Lantern/Sikit) vs host-bound (Straits/Causeway/…) vs shareable (CA) vs
   grouped (Intertropical Follow).
6. **[OPEN] UI vocabulary — fixed and shared.** Pick a SMALL fixed vocabulary every module uses:
   e.g. connect dot = bound/unbound; pair number = identity; filled/hollow = primary/secondary;
   colour = pairId. Every participating module renders the SAME way (today only Intertropical draws the
   badge; CA computes but doesn't draw; Colonnades tints). The badge is NET-NEW rendering on CA and
   Monsoon — template off Intertropical `drawPairBadge` (filled+number) and Colonnades ConnectMark-tint.
7. **[OPEN] Migration.** 122 findMonsoon call sites — a rule change touches all. Sequence: fix the
   primitive once, or introduce a new primitive and migrate module-by-module behind it?

---

## 7. Relationship to existing specs
- **CA_SHARED_EXPANDER_BUILD.md "PRIMARY MONSOON"** — the primary/secondary badge there is a CONSUMER of
  this model's relationship type (c) + UI vocabulary (Q6). Do NOT build that badge until (b)/(c) UI is
  settled here, or it re-adds to the piecemeal problem.
- **INTERTROPICAL_SPEC.md / PAIRING_CROSS_ROW_NOTE.md** — origin of pairId/pairColour (type (d)).
- **CONTEXT_RECOVERY.md** — grain taxonomy of which expander hosts which input.

## 8. Suggested next step (not a decision)
Answer Q1–Q2 first — group-vs-edge and one-discovery-rule — because every other question depends on
them. Then Q6 (vocabulary). Then this becomes a spec and the CA primary badge falls out as a consumer.

---

## 9. THE THREE CONNECTION MODELS ALREADY IN THE CODE (inventory finding, Rodney)

Connection isn't one model implemented inconsistently — it's THREE distinct models. Unification means
naming them and making discovery + UI consistent WITHIN each, not collapsing them into one.

**Model A — Claimed singleton per type (many expanders → one Monsoon).**
`isClaimedExpander` (VisualExpanderHelpers.hpp) checks identity against ONE cached slot per type in
`MonsoonExpanderManager` (cachedPolyVoiceExpander, cachedSandsVisualExpander, cachedCausewayPolyExpander,
cachedChangiExpander, cachedChangeAlleyV2, …). A second of the same type reaches the Monsoon but
`isConnectedAndClaimed` returns false → greys out. Enforces "one of each type per Monsoon."
Members: Straits, Sands (mono/east/macro — plus the Sands TOPOLOGY class for their interactions),
Causeway, Junction, Changi/T2/T3, Shophouse, Scale.

**Model B — Contended exclusive resource (several types → one shared slot, one winner).**
The TUNING SOURCE. Sikit AND Colonnades/Duo compete for ONE claim via `claimAsTuningSource`; loser greys.
Resolution: order-of-discovery, Sikit preferred (`cachedSikit ? sikit : colonnades`, Monsoon.cpp ~102).
`maskAuthored` cleared when the claimant isn't the mask-authoring Micro. This is why "one Colonnades OR
Duo, not both, and not alongside Sikit" — they all contend for a single tuning authority.
Members: Sikit, Colonnades, Colonnades Duo (and Interchange half-claim: first-bound claims a half,
later ones on that half inert — a sub-variant).

**Model C — Observer / reachability (no claim).**
Lights on REACHABILITY ("can I see a Monsoon/Straits system"), NOT on claim — observers have no claim
slot and "can't scale to N pairs" (Intertropical.cpp ~579). This is why MULTIPLE instances work.
Members: Intertropical (many — many arrangements), Lantern, Sikit-as-observer paths.

### Cardinality table (per Monsoon unless noted)
| Module | Cardinality | Model | Notes |
|---|---|---|---|
| Straits | 1 | A | |
| Sands mono / east / macro | 1 each | A | + Sands topology resolver class |
| Causeway | 1 | A | |
| Junction / Changi / T2 / T3 / Shophouse | 1 each | A | |
| Sikit | 1 winner \ | B | contends tuning w/ Colonnades |
| Colonnades / Duo | 1 winner, not both | B | contends tuning w/ Sikit |
| Interchange | 1 per half (2) | B-variant | first-bound claims a half |
| Intertropical | MANY | C | many arrangements |
| Lantern | MANY | C | pure observer |
| Change Alley | SHARED across N Monsoons | A-inverted | one CA, many Monsoons → needs PRIMARY |

## 10. WHO NEEDS A DESIGNATED PRIMARY — the predicate (Rodney's question)

**Primary is needed iff a SINGLE instance is reachable by MULTIPLE Monsoons AND performs an ASYMMETRIC
operation (mutates shared state, or reads a value back FROM a host).**

Applying it:
- **Models A and B** are the OPPOSITE topology (many expanders → one Monsoon). Exclusivity runs the other
  way (the Monsoon picks one claimant), so there is no primary question — there's a CLAIMANT question,
  already solved. Not primary.
- **Model C observers** never need a primary — read-only, each instance binds its own host. Permanently
  exempt.
- **Shared mutators** (one instance, many Monsoons) — the ONLY case. Today **CA is the only such module.**

So: **CA is the only module needing a primary today.** The rule generalises to any future shared mutator.

### Colonnades sharing — [OPEN, worth deciding]
Colonnades is Model B (contended, one winner per Monsoon) today, NOT shared. But two Monsoons on ONE
tuning authority is plausible (shared microtonal scale across a polymeter rig). IF allowed, tuning
publish is a MUTATION, so shared-Colonnades would need a primary exactly like CA — or the same
"both read, one writes" split. Decision: is tuning a shareable resource? If yes, it's the second module
in the "shared mutator → needs primary" class, and the primary machinery should be built generic, not
CA-specific.

## 11. "NICE TO HAVE" connections — expressiveness wishlist (Rodney)

Cases the current models DON'T express, worth weighing for musical value vs complexity. NOT commitments.

>   **RESOLVED (Rodney): do NOT build cross-feed or shared Sands.** Two Monsoons seeded identically
>   produce identical probabilities by construction (deterministic Philox spine), so each can keep its
>   OWN Sands reading its own local copy and modulate it same or differently — correlation WITHOUT
>   shared mutable state, no primary, no asymmetric edge. Establish the shared determinism domain with a
>   SEEDER expander (see SEEDER_EXPANDER_CONCEPT.md). Live coupling (A's runtime deviation shows up in B)
>   is the only thing seed-sharing can't express — rarer/less musical, left unbuilt. This retires the
>   cross-feed wish below; kept for the record.

- **[WISH] Cross-feed: an expander bound to a PRIMARY Monsoon but ALSO feeding a SECOND Monsoon**
  (e.g. Sands feeding probabilities to another Monsoon while owned by its primary). This breaks Model A's
  one-expander→one-host assumption: the expander now has a primary host (full claim) AND a secondary host
  (partial, read-only feed). It's a DIRECTED, TYPED, ASYMMETRIC edge — "feeds probabilities to" is not the
  same edge as "is claimed by." Musically real (share a generative dimension across two sequencers without
  duplicating the source). Complexity: MODERATE-HIGH — needs (a) per-edge role/type, not per-node identity
  (reinforces the edge-anchored answer to Q1), (b) a rule for what a secondary may READ vs DRIVE, (c) UI to
  show a node with two differently-roled edges. This is the strongest argument that the underlying model is
  a directed typed multigraph, with the singleton/claim cases as a constrained subset.
- **[WISH] Shared Colonnades** (see §10) — one tuning authority, many Monsoons. Same shape as shared CA.
- **[WISH] Expander bound to a system reachable only across rows** — pairId is rack-wide but host binding
  is same-row; a nice-to-have is binding a generation expander to a Monsoon on another row (Q4).

### Is the Sands cross-feed too complex?
Not conceptually — it's the same "one instance, asymmetric edges to multiple hosts" shape as shared CA,
just with the multiplicity on the EXPANDER side (feeds many) rather than the host side (owned by many).
Both point at the SAME underlying model: directed, typed, asymmetric edges; primary/claim as one edge
role among several. So the honest read: don't special-case it. If the connection model is built
edge-anchored (Q1) with typed roles (claim / feed / observe) and a primary predicate (§10), Sands
cross-feed and shared CA and shared Colonnades are all the SAME feature seen from different sides —
and the model expresses all three without bespoke code. If instead the model stays node-anchored
(one pairId per module), every one of these is a special case and the complexity is real. The
cross-feed wish is therefore a strong vote for edge-anchored — it's the test case that decides Q1.
