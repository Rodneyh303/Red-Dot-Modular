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
