# Connection / binding model — scoping doc (Rodney)

> **SUPERSEDED (in part) by CONNECTION_MODEL_SPEC.md** — Q1 (node-anchored identity), Q2 (segment rule /
> claim-by-scan; CA shareable), and Q6 (badge suppressed for single-Monsoon rigs) are now DECIDED there.
> This doc remains the inventory/scoping record and the source for the still-[OPEN] items. Where the two
> disagree, the SPEC wins.

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

---

## 12. Ecosystem precedent — monome-rack (Dewb), and the ID lesson

Verified from the monome-rack README (github.com/Dewb/monome-rack). Its module↔grid binding is the
pattern that fixes our bug class:

**Mechanism (TAKE this):** right-click a module (e.g. white whale) → SELECT a grid device from a list →
"it should light up". Select a hardware grid instead → the virtual one "goes dark". So binding is:
- EXPLICIT (user picks target from a context menu), not spatial/adjacency
- POSITION-INDEPENDENT (reordering modules cannot change it — position was never the binding)
- CONFIRMED VISUALLY (light up / go dark = which device this consumer is bound to)

This directly answers our Q2 for the ambiguous cases: replace "walk left/right, first Monsoon wins" with
"consumer chose its host". MSIC-vs-MSCI and C-M-M-C both dissolve because position stops being the rule.

**IDs (REJECT this part — Rodney's observation):** monome-rack's device IDs are long, opaque,
hex-ish/random-looking strings — machine identity. Fine for monome (bind ONE module to ONE grid once, via
the menu, never look at the ID again). Does NOT scale to OUR case: multiple Monsoons where the user must
track which expander is bound to which AT A GLANCE, continuously, on the panel. An opaque ID can't be
eyeballed.

**Synthesis — best of both:** monome's SELECTION MECHANISM + our pairId/pairColour IDENTITY.
- Bind by explicit menu choice (monome), NOT adjacency.
- Identify by COLOUR + small integer (ours), NOT opaque hex. A colour badge is glanceable; a hex string
  is not. Our existing pairColour badge is exactly the human-friendly token monome lacks.
- Result: right-click Colonnades → pick "Monsoon (teal) / #2" → teal badge confirms. Position-independent,
  human-legible, continuously visible. Strictly better than either system alone.

This also informs Q1: selection-based binding is naturally EDGE-ish (a chosen consumer→host link), but the
IDENTITY shown can stay NODE-anchored (pairId/colour per host). So we can keep node-anchored identity
(simpler, and enough now that cross-feed is retired) while borrowing edge-style EXPLICIT binding for the
multi-host disambiguation. Adjacency stays as the zero-config default for the common one-Monsoon rig;
explicit selection is the override that appears only when there's ambiguity.

**Still to verify from monome-rack src/ (engineering, not UX):** device ENUMERATION (how the list is
built), PERSISTENCE of the chosen binding across save/load, and graceful handling when a bound device is
DELETED or missing on reload. These are the hard parts of any selection-based system and where the reusable
lessons live. [OPEN — read src/ before implementing.]

## 13. monome-rack ENGINEERING lessons (from source/API, not just UX)

Structure: a `GridConnection` abstraction with two impls — `SerialOscGridConnection` (hardware, via
serialosc) and `VirtualGridConnection` (in-Rack). Devices enumerated from a registry; each device carries
a serial-number identity; consumer selects one from the enumerated list. API shows `enumerate_devices()`,
`DeviceChangeEvent::Added/Removed` callbacks, and `MonomeDevice` (type + serial + port).

Four lessons:
1. **Registry/enumeration layer.** Binding is NOT module→module direct — a middle layer maintains "what
   targets exist now" and the consumer selects from it. For us: don't have each expander walk the chain;
   have a REGISTRY OF MONSOONS expanders select from. We already have the seed — MonsoonExpanderManager +
   presentPairIds() — so this formalises what partially exists.
2. **Add/removed lifecycle is first-class.** monome has device Added/Removed events because grids get
   plugged/unplugged. Our equivalent = Monsoons added/deleted from the patch. A bound expander MUST handle
   its host vanishing gracefully (fall back to unbound; never crash or silently mis-bind). Design the
   unbind path from the start.
3. **Stable-ID persistence.** Identity = device serial, stable/unique, persists across reload; that's what
   the saved patch stores. Our equivalent = pairId. Store the binding by stable ID; DISPLAY the colour/
   number, not the raw ID (monome shows the serial only because it has nothing better — we have colour).

4. **THE LESSON THAT DOES NOT TRANSFER (important):** monome's always-explicit selection exists because
   grids are EXTERNAL, discovered over a network protocol, with NO spatial relationship — adjacency was
   never available, so explicit selection was forced. WE HAVE ADJACENCY FOR FREE (modules are physically
   neighbours). So do NOT wholesale-adopt always-explicit binding — that forces config onto every simple
   one-Monsoon rig that currently works with zero setup.

**Refined model:** ADJACENCY as the zero-config default (which monome couldn't have) + explicit
REGISTRY-SELECTION as the disambiguation override only when >1 Monsoon is reachable (built the way monome
does it: registry, Added/Removed lifecycle, stable-ID persistence, graceful unbind). monome's rigour for
the hard case; adjacency's zero-config for the common case. Better than either alone.

Concretely for Q2 (one discovery rule): the rule becomes "if exactly one Monsoon reachable → bind it
(adjacency, zero-config); if >1 reachable → use the stored explicit selection, else prompt/most-recent;
if the bound one disappears → unbind gracefully and re-evaluate." Order-independent, and simple rigs never
see a menu.

---

## 14. Identity, group colour, the 8-Monsoon cap, and the two binding TIERS (Rodney)

### Identity scheme (decided direction)
- **Store bindings by a stable, hidden machine handle** (Rack module id / derived pairId). Survives
  save/load and reordering. NEVER shown to the user (monome's mistake was exposing the opaque serial).
- **Identify visually by GROUP COLOUR carried on the CONNECT MARK** — reuse the existing mark rather than
  adding a badge. The mark currently red = connected; generalise: **grey/hollow = unbound; filled in the
  GROUP COLOUR = bound to that group.** Colour specialises the existing connected/unbound signal, it does
  not replace the bound-vs-unbound distinction (filled vs hollow still carries that).
- **Keep the small group NUMBER on the mark as a secondary/fallback** — colour is primary and glanceable,
  the number disambiguates for colour-blind users. (With the 8-cap below, colour never WRAPS, so the
  number is purely an accessibility/text fallback, not needed for uniqueness.)
- **Optional user NAME override** (e.g. "Bass", "Lead") shown in menus in place of the auto label; falls
  back to the auto identity if unset. Selection menus show "Monsoon A (teal)" / "Lead (teal)" — never a
  raw id.
- **Group anchored on the HOST (Monsoon).** The Monsoon owns colour+id; expanders INHERIT it by binding.
  Rationale: the Monsoon is the stable centre; expanders come and go; "which host" is what users ask.
- **Shared module = the exception to one-mark-one-colour.** A CA bound to 2 Monsoons shows BOTH groups
  (split mark / two colours or two small numbers) — honest about being shared; ties to the PRIMARY spec.

### Hard cap: 8 connection-participating Monsoons
- **Cap at 8** — matches the 8-wide pairColour palette, so colour is GUARANTEED unique per group (no
  wrap). This is the main payoff: colour alone becomes a complete identifier.
- Also bounds everything count-scaled: primary selection, group enumeration, the registry, shared-CA reach.
- **8 is a ceiling far above real use** (8 sequencer cores = 128 voices), so users never hit it — it
  disciplines the implementation, not the user.
- **Enforce on connection PARTICIPATION, not module instantiation.** A 9th Monsoon may be PLACED (don't
  block Rack module creation) but does NOT get a group colour / connection-system slot: its connect marks
  stay grey, tooltip "connection limit reached". NEVER let the 9th silently reuse colour 1 — that
  reintroduces the ambiguity the cap exists to kill. Graceful, visible degradation.
- **The 8-cap and the 8-palette are LOCKED TOGETHER** — change one ⇒ change the other, or the
  "colour is unique" guarantee breaks. Record the dependency.

### Two binding TIERS — different UI defaults
1. **Adjacency-default, override-on-ambiguity.** Singleton expanders (Straits, Sands, Causeway, …): one
   per Monsoon, adjacency almost always correct; a selection menu appears ONLY when >1 Monsoon contends.
   Zero-config in the common rig.
2. **Selection-REQUIRED.** Observer/tap modules facing a FIELD OF SAME-TYPE PEERS — no default is correct
   even in principle, because the peers are equals by design:
   - **Lantern** — can observe a Straits OR an Intertropical, and (since MULTIPLE Intertropicals are
     allowed) WHICH instance. TWO-AXIS pick: first the SOURCE TYPE (Straits vs Intertropical), then the
     INSTANCE within that type. Structure the menu that way, not a flat mixed list. Lantern's own connect
     mark reads as "bound into group B, watching <arranger/voices>".
   - **Change Alley** — with multiple Intertropicals / arranged views, which view it reaches is an
     explicit pick, not adjacency.
   These are where the monome-style selection is the PRIMARY mechanism, not an override.

### Consequence for build order
The selection-REQUIRED tier is BROKEN without the mechanism (Lantern silently shows the wrong peer today);
the adjacency tier keeps working until reached. So build the selection UI FIRST for Lantern + CA. And it
MUST persist + degrade gracefully (monome lesson §13): a selected source that is deleted → the consumer
shows "source removed — pick another" (via stable id), never silently falls back to a wrong peer or blanks
without reason.
