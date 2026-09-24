# Connection / binding model — SPEC (Rodney)

STATUS: **SPEC.** Supersedes the [OPEN] questions Q1, Q2, Q6 in CONNECTION_UI_MODEL.md (that doc
remains the scoping/inventory record; this doc is the decisions + the implementable rule). Where the two
disagree, THIS doc wins.

Decisions locked this session:
- **Q1 → node-anchored identity.** One identity token (pairId + colour) per HOST Monsoon. Binding is by
  explicit consumer→host selection ONLY when ambiguous; adjacency is the zero-config default. The
  edge-anchored/directed-multigraph option is REJECTED (its sole strong driver — Sands cross-feed — was
  retired in favour of the Seeder; see SEEDER_EXPANDER_CONCEPT.md §"What this does NOT give you").
- **Q2 → segment rule (claim-by-scan, order-independent).** An expander is bound to a Monsoon iff that
  Monsoon's boundary-aware scan claimed it. Boundaries: stop at a foreign module; a second Monsoon is a
  boundary. EXCEPTION: Change Alley is a **shareable type** — both Monsoons adjacent across the CA may
  claim it. The boundary stays strict for every other type.
- **Q6 → badge suppression.** With exactly ONE Monsoon in the patch, participating modules render a PLAIN
  connect dot (bound/unbound), no colour/number. The colour+number badge appears ONLY when 2+ Monsoons
  exist. Zero visual noise for the common single-Monsoon rig.

---

## 1. The single discovery rule (replaces the four mechanisms)

There is ONE authority for "which expander is bound to which Monsoon": the **Monsoon-outward
boundary-aware scan** already implemented in `MonsoonExpanderManager` (MonsoonExpanderManager.hpp
`scan` lambda, ~line 141). It is order-stable, stops at foreign modules (Rule 1), treats another Monsoon
as a boundary (Rule 3), and hops through recognised non-claiming modules (Lantern/Sikit).

**Rule (canonical):**
> An expander E is BOUND to Monsoon M iff E lies within M's SEGMENT — the maximal run of modules reachable
> from M by left/right expander hops, terminated on each side by the first foreign module OR the next
> Monsoon. Within a segment, each CLAIMABLE type binds the FIRST instance found (left side before right);
> later duplicates of that type are bound-but-unclaimed (grey).

**CA exception (shareable type):** Change Alley is not consumed exclusively. When a CA sits at or across a
segment boundary between two Monsoons, BOTH may claim it. CA is the only shareable type today; the
mechanism is generic (a per-type `shareable` flag) so a future shared mutator (e.g. shared Colonnades,
§10 of the scoping doc) can opt in without new code.

### 1.1 What changes vs today
- `findMonsoonEitherSide` (VisualExpanderHelpers.hpp:26) — the broken, expander-outward, right-first,
  boundary-blind walk used by 122 call sites and every naive connect dot — is DEMOTED. It must no longer
  be the authority for connect-mark lighting. It may survive as a thin shim that delegates to the segment
  rule (see §7 migration) so the 122 sites don't all change at once.
- The connect mark's truth becomes: **"is there a Monsoon whose scan claimed me?"** — which is what
  `isConnectedAndClaimed` (VisualExpanderHelpers.hpp:68) already asks, EXCEPT it currently finds the
  Monsoon via the broken walk. Repoint it to the segment rule and it becomes correct.

### 1.2 Reachability from the expander side
The scan runs Monsoon-outward, but an expander needs to find ITS Monsoon. Two equivalent
implementations; pick per migration cost:
- (a) **Reverse-query:** expander walks out with the SAME boundary rules (stop at foreign, stop at another
  Monsoon), returning the first Monsoon whose segment it is in. This is `findMonsoonEitherSide` FIXED
  (add the two boundary conditions). Cheapest change, no new state.
- (b) **Back-pointer:** the Monsoon's scan writes a back-pointer (hostId) into each claimed expander each
  frame; the expander reads it. More state, but makes "which host" a stored fact (useful for the badge
  and for explicit selection persistence).
- **DECISION:** implement (a) as the immediate fix (order-independence), and layer (b)'s stored hostId
  only where the badge/explicit-selection needs it (multi-Monsoon). Single-Monsoon rigs never pay for (b).

---

## 2. Host identity (node-anchored)

Each Monsoon owns a **pairId** (small integer, 1..N, lowest-unused, persisted) exactly like Intertropical
today (IntertropicalPairing.hpp `assignPairIdT`). Colour = `pairColour(pairId)` (the existing 8-hue
palette, IntertropicalPairing.hpp:111) — SHARED so host and every bound expander show the same hue for a
given number.

- The Monsoon self-assigns its pairId on creation (lowest free across all Monsoons), persists it, keeps it
  across reload.
- A bound expander DISPLAYS its host's pairId/colour; it does not own identity.
- Reuse the generic `assignPairIdT<T>` / `presentPairIdsT<T>` / `resolveFollowedT<T>` templates already in
  IntertropicalPairing.hpp — instantiate them on `Monsoon` (Monsoon needs a public `int pairId`). No new
  pairing machinery; this is the whole point of those being templates.

---

## 3. Binding: adjacency default + explicit override

- **0 or 1 Monsoon reachable in the segment → adjacency (zero-config).** Bind it. No menu, no badge
  number. This is every simple rig; nothing to configure, matches today's behaviour for the common case.
- **>1 Monsoon reachable (only possible via the CA shareable exception, or a future shareable type) →
  explicit selection.** The consumer stores a chosen `hostPairId` (0 = auto/nearest). Right-click →
  "Bind to Monsoon #N (colour)". Resolves via `resolveFollowedT<Monsoon>`. Position-independent.
- **Bound host disappears (deleted/unplugged) → unbind gracefully.** Fall back to auto (hostPairId
  behaves as 0); never crash, never silently mis-bind. This is the monome Added/Removed lifecycle lesson
  (scoping doc §13.2) applied to "Monsoon added/removed from patch."

Persistence: store `hostPairId` (the stable identity), NOT a module address or row position. DISPLAY the
colour/number. (Scoping doc §13.3.)

---

## 4. UI vocabulary (fixed, shared — every participating module renders the SAME way)

| Element | Meaning | When shown |
|---|---|---|
| connect DOT (lit/dim) | bound / unbound | always |
| colour of dot+badge | `pairColour(hostPairId)` | only when 2+ Monsoons present |
| small integer badge | host Monsoon's pairId | only when 2+ Monsoons present |
| filled vs hollow badge | primary vs secondary | only for shareable types with 2+ claimants (CA) |

- **Single Monsoon → plain dot only** (Q6). No colour, no number.
- The colour+number badge is NET-NEW rendering on most modules. Template it off Intertropical's
  `drawPairBadge` (filled circle + number) and Colonnades' ConnectMark tint. Provide it ONCE as a shared
  widget (e.g. `redDot::HostBadge` in ui/) so every module draws identically — do NOT re-invent per module
  (that re-creates the piecemeal problem this spec exists to end).
- **Primary/secondary (filled/hollow)** is meaningful ONLY for shareable mutators reachable by 2+ hosts.
  Today that is CA alone. The predicate for "needs primary" is unchanged from scoping doc §10: a single
  instance reachable by multiple Monsoons performing an asymmetric op. This makes the CA "PRIMARY MONSOON"
  badge in CA_SHARED_EXPANDER_BUILD.md a CONSUMER of this vocabulary, not its own invention.

---

## 5. Per-module relationship types (who renders what)

Three models from scoping doc §9 map onto the vocabulary:

- **Model A — claimed singleton** (Straits, Sands mono/east/macro, Causeway, Junction, Changi/T2/T3,
  Shophouse, Interchange/Scale): plain dot (bound/unbound via segment rule). Colour+number only when 2+
  Monsoons. Never a primary marker (opposite topology — the Monsoon picks the claimant).
- **Model B — contended resource** (Sikit vs Colonnades/Duo for the tuning slot): plain dot; the LOSER of
  the contention greys (already implemented via single-claimant resolution in updateExpanderPointers).
  Colour+number only when 2+ Monsoons. No primary marker unless tuning becomes shareable (deferred, §10
  scoping doc).
- **Model C — observer** (Lantern, Sikit-as-observer, Intertropical): reachability dot; each instance
  binds its own host; MANY instances fine. Colour+number when 2+ Monsoons so you can see which system each
  observes.
- **Shareable mutator** (CA): dot + (2+ Monsoons) colour+number + filled/hollow primary marker. The only
  type using the primary marker today.

---

## 6. Interaction with adjacent specs
- **CA_SHARED_EXPANDER_BUILD.md "PRIMARY MONSOON" badge** — now a consumer of §4's filled/hollow marker +
  §2 identity. Build it AFTER the shared `HostBadge` widget exists; do not hand-roll it.
- **SEEDER_EXPANDER_CONCEPT.md** — the Seeder is a Model-C one-to-many broadcaster (needs no primary). It
  renders the observer dot + (2+ Monsoons) colour/number so you can see which determinism domain each
  Monsoon is in. When specced, it becomes a HOST-LIKE identity source too (a domain has its own colour) —
  flagged as a follow-on, not in this spec's scope.
- **RESEED_ON_RESTART_INDEPENDENT_AXES.md Part 3** (CA panel reseed toggle across multiple Monsoons) — its
  a/b/c semantics question is now answerable: CA is shareable with a PRIMARY (§4). The CA reseed toggle
  drives the PRIMARY Monsoon by default (option a), with the primary marker showing which. This UNBLOCKS
  Part 3's open question #2.

---

## 7. Migration (staged; each stage independently verifiable)

The 122 `findMonsoonEitherSide` call sites mean a rule change is high-blast-radius. Sequence to keep each
step a clean, buildable, eyeballable commit:

**Stage 1 — Fix the primitive (order-independence), no UI change.**
- Add the two boundary conditions to `findMonsoonEitherSide` (stop at foreign module; stop at another
  Monsoon) — i.e. §1.2(a). Repoint `isConnectedAndClaimed` to use it.
- Result: the §4 CMMC bug class (Monsoon-Straits-Intertropical-Colonnades vs …-Colonnades-Intertropical)
  dissolves; lit set becomes order-independent. NO badge yet, NO explicit selection yet.
- Tests: a header-lite discovery test that builds mock chains and asserts the claimed set is identical
  under reordering (this is pure topology logic, engine-testable without Rack). Add cases for: single
  Monsoon; two Monsoons back-to-back (boundary); foreign module in the middle; CA shareable across two.
- Risk: some modules gate lit-state on an ADDITIONAL adjacency/claim check (scoping doc §4 bullet 3).
  Audit those per-module during this stage; the fixed primitive should let several drop their bespoke
  check.

**Stage 2 — Monsoon pairId identity + shared HostBadge widget (no behaviour change when 1 Monsoon).**
- Give Monsoon a persisted `pairId`; instantiate the `*T` pairing templates on Monsoon.
- Add `redDot::HostBadge` shared widget (colour+number, suppressed when <2 Monsoons).
- Convert connect marks to use HostBadge. Single-Monsoon rigs look identical to today (plain dot).
- Tests: pairId assignment (lowest-unused, stable across a mock add/remove), present-ids set.

**Stage 3 — Explicit selection for multi-host (CA + any 2+-Monsoon case).**
- Consumer stores `hostPairId` (0=auto); context menu "Bind to Monsoon #N"; resolve via
  `resolveFollowedT<Monsoon>`; graceful unbind on host disappearance.
- Persist `hostPairId`. Tests: resolve-by-id, fallback-to-auto on missing host, persistence round-trip.

**Stage 4 — CA primary marker (filled/hollow) as a HostBadge consumer.**
- Implement CA_SHARED_EXPANDER_BUILD's PRIMARY badge using §4. UNBLOCKS reseed Part 3.

Stages 1–2 are pure logic + a shared widget (low risk, high payoff). Stages 3–4 are the multi-Monsoon
features. Each is its own commit; each built + eyeballed by Rodney (container cannot build the plugin).

---

## 8. What this spec deliberately does NOT do
- No directed typed multigraph / edge roles (Q1 rejected; cross-feed retired).
- No cross-row host binding (pairId is rack-wide for IDENTITY/explicit-selection, but adjacency segment
  binding stays same-row; scoping doc Q4 stays deferred — revisit only if a real cross-row gesture appears).
- No change to the tuning contention (Model B) resolution; only its RENDERING joins the shared vocabulary.
- No Seeder implementation (separate concept doc; this spec only reserves its place in the vocabulary).

## 9. Open items intentionally left for their own decisions
- Exact `HostBadge` geometry/placement per module panel (each panel's ConnectMark position differs).
- Whether Stage 1's boundary tightening changes any CURRENTLY-WORKING rig (audit needed): specifically any
  patch that TODAY relies on an expander binding ACROSS a second Monsoon (other than CA). If such a rig is
  intended, that type must be marked shareable too — otherwise Stage 1 correctly un-binds it. Enumerate
  during the Stage 1 audit.
