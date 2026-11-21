# ZAI PROMPT — Delegation-Decision Unification (layer 2): ONE store→topo path, rig-pinned owner flip

**Status:** READY — §3 DECIDED: option B (Rodney, 2026-10-21; ruling block in §3). No open decision points.
**Size:** ~45 minutes + one mandatory feel-test (§3 = option B, protocol in exit contract #5). **Behaviour change: ZERO except §3 option B (deliberate, one state).**
**Base:** `lane-expander-refactor` @ ac8cfd34 (ownerIsEast landed). Re-verify anchors before editing.

## Why (the ruling, one paragraph)

Rodney asked whether delegation should move INTO the resolver. Ruling: NO — the resolver CONSUMES
the `delegated` boolean; the byte-identical bridge methodology and the skeleton's rig-visibility
both depend on that seam (design doc: "ONE topology authority feeds MANY value resolvers"). But the
DECISION side never got the campaign's discipline: three derivation paths exist (two manager
fillOwners sites, the visual's private fill), the direction push re-derives from the RAW STORE
instead of the topology, and none of it is rig-testable — which is exactly why the live
"warp wrong with macro following east" symptom has no rig proof. Layer 2 gives the decision side
the one-home treatment. Three named layers, each testable at its own level:

| Layer | Home | Rig-visible? | Status |
|---|---|---|---|
| 1. Store polarity | `Monsoon::ownerIsEast(v,l)` | no | LANDED (ac8cfd34) |
| 2. Derivation | ONE store→topo fill + `topo.owner()` | **yes** | THIS BRIEF |
| 3. Selection | the policies, `delegated` stays an input | yes | landed, unchanged |

## THE CHANGE

### 1. The one fill — `SandsTopology::Inputs::fillOwnersFromStore` (rig-visible, plain-data)

Add to `SandsTopology.hpp` (next to `fillOwners`):

```cpp
        // Layer-2 home of the store→topo fill (ZAI_PROMPT_delegation_decision.md): the ONE
        // derivation every consumer uses. Plain-data in — the 112-float macroOwn store, no
        // Monsoon — so the rig can pin the whole owner-flip matrix (test_ownership_flip).
        // Same predicate the manager lambdas hand-wrote: !macroPresent || store > 0.5.
        void fillOwnersFromStore(const float* macroOwn112, bool eastPresent, bool macroPresent) {
            fillOwners(eastPresent, [macroOwn112, macroPresent](int v, int l) {
                return !macroPresent || macroOwn112[v * 7 + l] > 0.5f;
            });
        }
```

(The 112 = 16×7 voiceSlot-major convention is the store's own — Monsoon.hpp macroOwn comment.)

### 2. Migrate the three store→topo fills (byte-identical)

| # | Site | Becomes |
|---|------|---------|
| A | MonsoonExpanderManager.cpp :97-99 (fillOwners lambda, CC_PROMPT_07) | `topoIn.fillOwnersFromStore(gMon->editor.macroOwn, /*eastPresent=*/true, hasMacro);` — keep the `mmTopo`/`noMacro` guards' EFFECT: the lambda's `noMacro` term becomes `macroPresent`; `mmTopo` null cannot happen at this site (guarded above — verify, else keep the lambda form and STOP) |
| B | MonsoonSandsManager.cpp :142-145 (fillOwners lambda, Sands side) | `topoIn.fillOwnersFromStore(gMon->editor.macroOwn, eastPresent, hasMacro);` where `eastPresent` is the existing cachedEastSandsVisual test; note the Sands lambda lacks the mmTopo-style null guard because `gMon` is lambda-captured — helper takes the array address, caller keeps the `gMon &&` guard |
| C | StraitsEastSandsVisual.cpp :707-712 (buildTopo base fill) | same helper; **the live-proxy overlay (:717-724, eastOwnsLane) STAYS** — it is UI-live semantics, not store state, and stays exactly as is |

Byte-identity note: every migrated site must produce the SAME eastV1Owner/eastPolyOwner bits as
before for ALL 112 cells. The helper's `> 0.5f` is the raw threshold — NOT `ownerIsEast` (the
helper cannot call Monsoon; that is what makes it rig-testable). Document that in one line.

### 3. DECISION POINT — the direction push's second derivation path (NOT byte-identical either way)

MonsoonExpanderManager.cpp :431: `const bool delegated = !mmOwn->ownerIsEast(v, l);` reads the RAW
store — no `noMacro` term. The topology answer (`topo.owner(v,l) == Role::MACRO`) differs in ONE
state: **no Macro visual attached AND store says Macro-owns (0)** — reachable (store defaults 0).
There the two behaviors DISAGREE TODAY:

- WARP (topology): no Macro ⇒ owner=EAST ⇒ own-slot warp plays.
- DIRECTION (raw store): no Macro ⇒ delegated=TRUE ⇒ Macro's getGlobalDir plays.

One of them is wrong per product intent. **Option A (byte-identical, safe):** leave :431 as is;
pin the raw read as the direction authority with a comment; the inconsistency stays but is now
DOCUMENTED and grep-visible. **Option B (unify, behavior change in the no-Macro state only):**
`:431` → `const bool delegated = (topo.owner(v, l) == dotModular::SandsTopology::Role::MACRO);`
— direction joins warp's semantics; requires Rodney's feel-test of the no-Macro patch shape.
Equivalence when Macro IS attached: `macroPresent` parity verified (fillPresence :299 ==
hasMacro), so Option B is byte-identical in every Macro-attached state. Rodney decides; if B,
the exit report must name the changed state verbatim.

**DECIDED — Rodney, 2026-10-21: OPTION B.** Verbatim rationale: "Direction can't get anything
from a missing module." That elevates the answer from preference to SPEC. The canonical rule for
everything resolver-routed (warp, spread, direction, LOR), Rodney's words:

> 1. East only → then east plays.
> 2. Macro only → macro plays.
> 3. Both → east plays for voice and lane delegated to east, otherwise macro plays for the
>    voice and lane.

Ear-verified by Rodney on the 2026-10-21 rack: case 1 "seems ok" (warp), and "Spread gets it
right" — SpreadPolicy::applyDelegated (resolver :196) consumes the SAME delegated seam, so Spread
is the ear-calibrated reference for delegation semantics, not a counter-example. Under option B,
direction agrees with warp everywhere, and the one changed state (no-Macro ∧ store-0) is exactly
the state the rule's case 1 calls wrong. §4's pin asserts THIS RULE on the full grid — rule
violations must fail a suite, not fail in ears.

### 4. The rig pin — new suite `test_ownership_flip.cpp` (register in run_all.sh, 72nd)

- 16×7 × {eastPresent × macroPresent} grid: store 0→1 flip ⇒ `fillOwnersFromStore` output flips
  ⇒ `topo.owner()` flips MACRO↔EAST; no-Macro ⇒ all-EAST (rule case 1); Macro-present ∧ store-0
  ⇒ MACRO (rule case 2). The grid asserts Rodney's 3-case rule (§3 ruling block) end-to-end.
  Honesty clause: if east-present gating makes a rule case unreachable in the plain-data fill,
  pin the reachable subset and DOCUMENT the unreachable case in the suite header — do not
  silently drop it.
- Store-domain pin: only 0.0/1.0 are meaningful (mirrors ownership-brief §3).
- Warp-derivation pin at pattern level: `warpDelegated_[v][l] == (topo.owner(v,l)==MACRO)` given
  the same Inputs (the harness already derives warpDelegated_ this way — assert the identity).

## Intentionally UNTOUCHED

- The resolver and all four policies (layer 3 unchanged — the ruling).
- The OwnerCell write path, varlegDeleg, persistence format (ownership brief's untouched list).
- Stale comment visual :1604-1605 (cites deleted getMonoMacroOwn) — fold into this brief's
  comment pass ONLY if §3 option B touches the same file anyway.

## Exit contract

1. Rig: **72/72** + anchors (east 113 / macro 140 / keppel 19 / causeway 55).
2. `UNIFORMITY_STRICT=1` → **1494/1494**, manifest EMPTY, exit 0 (no new seams; the new suite is
   a positive pin, not a seam).
3. Grep: zero `fillOwners(` lambdas with hand-written `ownerIsEast` bodies remain in managers;
   the visual's base fill greps through the helper.
4. Plugin build SUCCESS (executor-side gate — the visual/manager hunks are rig-invisible).
5. §3 = option B (DECIDED): feel-test protocol REQUIRED in the exit report (no-Macro patch:
   East lane-dir knob must respond — case 1 of the rule; with Macro attached: unchanged).

## Honesty block

§1/§2/§4 are byte-identical or additive. §3 is the ONLY behavior-relevant hunk and ONLY in the
no-Macro-and-store-0 state, and only under option B — say which option shipped, verbatim, in the
exit report. If any suite moves unexpectedly, STOP and report instead of adapting the brief.

## Not this task

- Bool STORE migration + versioned persistence (Rodney-decidable, patch-load story).
- varlegDeleg predicate (sibling store, own brief).
- Unifying store-flag vs topo.owner AUTHORITIES beyond the fill (the earmark constraint).
- lane>=4 monoDirAuthority, tap ternary, mono LOR copies.
