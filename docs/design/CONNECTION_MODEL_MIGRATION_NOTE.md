# NOTE — Connection/binding model migration: status + how subpanels & warp depend on it

Added 2026-10-18b. Reconstructed by reading the code against CONNECTION_MODEL_SPEC.md, so a
future session doesn't re-derive it. This is a STATUS + DEPENDENCY note, not a new design —
the design is CONNECTION_MODEL_SPEC.md (locked) and CONNECTION_UI_MODEL.md (scoping/inventory).

## The problem (general, not bespoke)
Most suite modules are expanders. Rack positions modules purely by physical x-order and gives
NO API to bind modules together or forbid a user inserting a module between a host and its
expanders. So any mechanism that relies on adjacency (walk left/right to find your host/
children) BREAKS when something is inserted between. This is the root of:
  - the "Macro interspersed between Straits subpanels" issue (why subpanels are deferred),
  - the old order-dependent connect-mark lighting (CONNECTION_UI_MODEL §4 bug class),
  - and the SAME exposure in NEW STRAITS: StraitsBase.cpp spawns lane expanders and tracks
    them by walking the rightExpander chain (StraitsBase.cpp ~L51/57/266, findMonsoonEitherSide
    ~L241; StraitsLaneExpander.hpp). Insert a module between base and its lanes → the walk
    loses them. (It persists lane slugs + re-spawns on load, a partial id mechanism, but the
    LIVE binding is still adjacency.)

## The fix already exists and is PARTLY SHIPPED
There are (historically) THREE+ overlapping discovery mechanisms: the MonsoonExpanderManager
adjacency scan, the IntertropicalPairing id-templates, and assorted ad-hoc getModuleIds scans.
CONNECTION_MODEL_SPEC.md collapses them into ONE authority (the boundary-aware segment-rule
scan) + ONE identity layer (pairId via the generic templates). Key decisions in that spec:
  - node-anchored identity: each Monsoon owns a persisted pairId (1..8, lowest-unused);
  - segment rule (order-independent): an expander binds to the Monsoon whose boundary-aware
    scan claims it (stop at foreign module; a 2nd Monsoon is a boundary);
  - CA is the one SHAREABLE type (two Monsoons across it may both claim it) — the mechanism is
    generic (per-type `shareable` flag) for future shared types;
  - binding = adjacency default (zero-config, single-Monsoon rigs unchanged) + explicit
    hostPairId override when ambiguous; graceful unbind when the host disappears;
  - identity persisted by pairId, NEVER by module address/row position.

The id-pairing machinery is GENERIC + TESTED: ui/IntertropicalPairing.hpp templates
(assignPairIdT / resolveFollowedT / presentPairIdsT / findPairHubEitherSide), already used by
Lantern, Changi T3, MicroTuning, Change Alley. Rate discipline is established: the rack-wide
getModuleIds() scan runs at CONTROL rate behind a ClockDivider, result cached (Lantern.cpp:234,
Monsoon.cpp:116) — safe, not per-sample. resolveFollowedT returns nullptr on missing host and
every caller null-checks (graceful unbind). So reuse, don't reinvent.

## Migration status (CONNECTION_MODEL_SPEC §7 stages)
  - Stage 1 (fix the primitive: boundary-aware segment-rule findMonsoonEitherSide, repoint
    isConnectedAndClaimed) — appears DONE: VisualExpanderHelpers.hpp:34 delegates to
    MonsoonDiscovery.hpp (the templated, unit-tested segment walk; test_monsoon_discovery.cpp).
  - Stage 2 (Monsoon pairId identity + shared HostBadge widget) — appears DONE/near:
    Monsoon.hpp:692 has persisted `int pairId` with the spec's semantics; ui/HostBadge.hpp exists.
  - Stage 3 (explicit multi-host selection: hostPairId, "Bind to Monsoon #N", graceful unbind) —
    PENDING.
  - Stage 4 (CA primary/secondary filled-hollow badge) — PENDING; unblocks reseed Part 3.
(Verify current state in code before acting — stages may have advanced since this note.)

## How subpanels and warp depend on this
- The SUBPANEL-loses-adjacency fix IS Stage 3 (bind by hostPairId instead of pure adjacency;
  pin a subpanel to its host, survive insertion). Do NOT write a bespoke subpanel fix — it
  would be a 5th mechanism, exactly what the spec forbids (§4 "do not re-invent per module").
  New-Straits base↔lane tracking wants the same Stage-3 treatment (same exposure).
- One open sub-question for subpanels specifically (spec §9 open items): a subpanel follows a
  MACRO (itself an expander), not the Monsoon host directly — decide whether it binds to the
  Macro's identity or inherits the Monsoon's through the Macro. Plus per-panel HostBadge
  geometry. These are decisions for when Stage 3 resumes, not blockers to name now.
- WARP (prompts 01-04) is DELIBERATELY INDEPENDENT: it ships on WIDE PANELS, so it needs none
  of this. The warp modulation-tier → subpanel migration is GATED on (a) connection-model
  stages 3-4 DONE and (b) warp proven in Rack. Only then migrate the WHOLE modulation tier at
  once (param mods + LOR jacks/attenuverters + w-mod columns), for a coherent rule and uniform
  degradation — never piecemeal.

## Recommended sequencing
Two decoupled tracks:
  1. Warp feature — wide panels, prompts 01-04. Independent. Ships when it ships.
  2. Connection-model migration — resume Stages 3-4 as SUITE INFRASTRUCTURE on its own branch,
     reusing the IntertropicalPairing templates + HostBadge. Its own focused session + tests
     (the discovery layer is where the spread/QMIX crash cluster lived — contract-test it).
When both are done, the subpanel migration becomes safe. Not before.
