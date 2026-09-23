# Connection / ownership rework — Phase 0 report (read-only)

Branch: `feat/sands-qmix-geometry`. This is the pre-code report the task asked for: the migration plan
for the discovery call sites, plus every place the DOCS contradict the CODE. **The docs predate recent
work — I checked the code, and a large part of what the brief describes as "to build" is already built.**

---

## TL;DR — the single most important finding

**Phase 1 (the order-independence bug fix) has ALREADY LANDED.** `findMonsoonEitherSide` is no longer the
old right-first, walk-through-anything, boundary-blind walk the brief describes. It was rewritten to the
segment rule in [`src/ui/MonsoonDiscovery.hpp`](Red-Dot-Modular/src/ui/MonsoonDiscovery.hpp:61) and
[`src/ui/VisualExpanderHelpers.hpp`](Red-Dot-Modular/src/ui/VisualExpanderHelpers.hpp:34), with a passing
pure-logic test at [`test/test_monsoon_discovery.cpp`](Red-Dot-Modular/test/test_monsoon_discovery.cpp:1)
that asserts the exact CMMC order-independence the brief calls the bug.

So Phase 1 is **NOT a green-field build** — it is an **audit + gap-fix** of an existing implementation.
That completely changes the risk profile the brief assumed ("a wrong primitive applied everywhere is worse
than the bug"): the primitive is already migrated behind one function; the 122 sites already call the fixed
version. What remains in Phase 1 is finding the *bugs in the new primitive*, of which I found two concrete
ones (below).

---

## 1. What already exists (verified in code, contradicting the brief's premises)

| Brief assumes… | Code reality | Evidence |
|---|---|---|
| `findMonsoonEitherSide` is the broken right-first walk | It's the **segment-rule** walk via `findHostBothSides` | [VisualExpanderHelpers.hpp:34](Red-Dot-Modular/src/ui/VisualExpanderHelpers.hpp:34) |
| Phase 1 primitive fix is unbuilt | Built + unit-tested (order-independent) | [MonsoonDiscovery.hpp](Red-Dot-Modular/src/ui/MonsoonDiscovery.hpp:1), [test_monsoon_discovery.cpp](Red-Dot-Modular/test/test_monsoon_discovery.cpp:1) |
| `isConnectedAndClaimed` finds host via broken walk | Already repointed to the segment rule | [VisualExpanderHelpers.hpp:72](Red-Dot-Modular/src/ui/VisualExpanderHelpers.hpp:72) |
| CA-share (`followCA`) is Phase-3/4 work | **Built**: field, persistence, menu, manager override | [Monsoon.cpp:89](Red-Dot-Modular/src/Monsoon.cpp:89), [Monsoon.hpp:628](Red-Dot-Modular/src/Monsoon.hpp:628), [MonsoonPersistenceManager.cpp:63](Red-Dot-Modular/src/dsp/managers/MonsoonPersistenceManager.cpp:63), [MonsoonWidget.cpp:1116](Red-Dot-Modular/src/MonsoonWidget.cpp:1116) |
| Pairing helpers are templated for reuse | Done (`assignPairIdT`/`resolveFollowedT`/`presentPairIdsT`) + IT aliases | [IntertropicalPairing.hpp:27](Red-Dot-Modular/src/ui/IntertropicalPairing.hpp:27) |
| `drawPairBadge` lives in the pairing header | It lives in **Intertropical.cpp** (a method, reads `module->pairId`) | [Intertropical.cpp:402](Red-Dot-Modular/src/Intertropical.cpp:402) |

### Still genuinely unbuilt (the real remaining work)
- **Monsoon has NO `pairId`.** Only Intertropical and CA V2 carry one. Phase 2 (host identity on Monsoon)
  is real work. [Monsoon.hpp](Red-Dot-Modular/src/Monsoon.hpp:628) has `followCA` but no `pairId`.
- **No shared `HostBadge` widget.** Only Intertropical's private `drawPairBadge` exists.
- **No 8-Monsoon cap** anywhere (grep for cap/limit/participating = 0 hits). Phase 2's cap is unbuilt.
- **No CA 8-slot mark row / primary selection** (Phase 3 CA half).
- **Lantern badge** (Phase 3 Lantern half) — selection state exists (`sourceMode`/`followIT`), display absent.

---

## 2. Contradictions between docs and code (check the code, not the docs)

1. **CA-share is NOT implemented as the "segment-walk shareable exception" the SPEC describes.**
   `CONNECTION_MODEL_SPEC.md` §1 says the boundary walk has a CA exception (both Monsoons across a CA may
   claim it). In code there is **no shareable flag in the walk** — the walk treats a second Monsoon as a
   hard boundary for *every* type including CA. CA sharing is instead a **separate rack-wide follow
   override**: [`Monsoon.cpp:89`](Red-Dot-Modular/src/Monsoon.cpp:89) runs, AFTER the adjacency scan, a
   `resolveFollowedT<MonsoonChangeAlleyV2>(this, followCA)` and overwrites `cachedChangeAlleyV2`. So CA
   sharing works via **explicit `followCA` selection (pairId)**, not via a walk exception. **The SPEC's
   §1 CA-exception language is stale.** This is fine (arguably cleaner), but the spec should be corrected,
   and Phase 1 must NOT "add a shareable flag to the walk" — that would duplicate a mechanism that already
   exists elsewhere.

2. **`isSuiteChainModel` is OUT OF LOCKSTEP with the manager scan — two real bugs.**
   The comment in [MonsoonDiscovery.hpp:28](Red-Dot-Modular/src/ui/MonsoonDiscovery.hpp:28) states the two
   lists MUST agree on suite-vs-foreign. They don't:
   - **ChangiT2 missing.** The manager scan caches `modelMonsoonChangiT2Expander`
     ([MonsoonExpanderManager.hpp:177](Red-Dot-Modular/src/dsp/managers/MonsoonExpanderManager.hpp:177)) but
     `isSuiteChainModel` omits it → a ChangiT2 placed between an expander and its Monsoon acts as a FOREIGN
     boundary and un-binds everything past it. This is the §4 bug class, still live for ChangiT2.
   - **ChangiT3 missing.** ChangiT3 is a suite follower (reads via IntertropicalPairing) and is not a
     boundary, but `isSuiteChainModel` omits it → same false-boundary bug when a ChangiT3 sits mid-chain.
   Both are one-line additions, but they are exactly the kind of gap the brief's "audit the primitive"
   warning targets. **These are the concrete Phase-1 fixes.**

3. **`CONNECTION_UI_MODEL.md` is largely superseded.** Its own header says so (SPEC wins on Q1/Q2/Q6). Its
   §1 "four mechanisms, 122 naive call sites, right-first walk" inventory is now historically inaccurate:
   the walk is fixed and the naive-walk problem is gone. Treat §4 (bug class), §9 (three models + cardinality
   table), §10 (primary predicate), §13–16 as the still-valid design; treat §1's code description as history.

4. **The "122 call sites" figure is stale and misleading about blast radius.** Current grep for
   `findMonsoonEitherSide` = ~90 real call expressions across ~20 files. **The overwhelming majority are
   NOT connect-mark logic** — see §3. Only a handful gate lit-state on discovery. So "a rule change touches
   122 connect marks" is false; the rule already changed, behind one function, and almost nothing needs
   per-site edits.

---

## 3. Migration plan for the discovery call sites (the part the brief says matters most)

**The migration is already done** (the primitive was swapped behind `findMonsoonEitherSide`). What's left is
to (a) fix the two suite-list gaps so the primitive is CORRECT, and (b) confirm no call site needs to change.
I categorised every call by what it uses the returned Monsoon FOR:

| Category | Count (approx) | What it does with the host | Needs a per-site change? |
|---|---|---|---|
| **Theme read** (`m->lightTheme`) | ~35 | picks light/dark for a port/panel | **No** — host-or-null is all it needs |
| **Mod-arc getters** (getSetNorm/getModNorm/isActive) | ~20 | reads a param / modViz flag off host | **No** — null-guarded already |
| **Field accessors** (getMacroSend/getMonoOwner/getLaneDir/…) | ~20 | reads/writes Monsoon store for Sands topology | **No** — correctness = "my host or null" |
| **Cached host** (Intertropical/Straits/Macro/East cache on a divider) | ~8 | perf cache of the lookup | **No** |
| **Connect-mark lit-state** | ~5 | the actual bound/unbound signal | **No code change; benefits from the fix** |
| **Observer reachability** (Intertropical/Lantern) | ~3 | lit iff any host reachable | **No** — intentionally not `isConnectedAndClaimed` |

**Conclusion:** there is no 90-site migration. Every call wants the same contract — "the Monsoon in my
segment, or null" — which the fixed primitive now provides order-independently. The single behavioural
lever is the primitive itself. **Do NOT introduce a second primitive and migrate site-by-site** (the brief's
alternative); the correct move is the opposite — keep the one primitive, fix its suite list, and let all
~90 sites inherit the fix. A parallel primitive would re-fragment discovery, the exact problem the SPEC ends.

### The one nuance to preserve
Observers (Intertropical [Intertropical.cpp:582](Red-Dot-Modular/src/Intertropical.cpp:582), Lantern) light on
**reachability** (`findMonsoonEitherSide != null`), NOT `isConnectedAndClaimed`. That distinction is correct
(Model C) and must stay — do not "unify" observers onto the claimed predicate.

---

## 4. Recommended Phase sequencing (revised for what's already built)

- **Phase 1 (small, do first):** Add `modelMonsoonChangiT2Expander` and `modelMonsoonChangiT3Expander` to
  `isSuiteChainModel`. Extend [test_monsoon_discovery.cpp](Red-Dot-Modular/test/test_monsoon_discovery.cpp:1)
  with a "suite follower mid-chain doesn't un-bind" case (and a case proving a *foreign* module still does).
  Audit the ~5 connect-mark sites + the per-module "additional adjacency check" worry (§4 bullet 3 of the UI
  doc) — with the fixed primitive several may already be redundant. Correct the SPEC §1 CA-exception wording
  to describe the `followCA` override that actually ships. Rack-verify M-S-I-C == M-S-C-I and C-M-M-C.
  Confirm graceful unbind (host deleted → null → dim) — already the null path, just verify.
- **Phase 2:** Monsoon `pairId` (instantiate the `*T` templates on Monsoon; needs `int pairId`), the shared
  `redDot::HostBadge` widget (colour+number, suppressed <2 Monsoons), and the 8-cap enforced on
  participation (9th Monsoon: no colour/slot, grey marks, never reuse colour 1). 8-cap ↔ 8-palette locked.
- **Phase 3:** Lantern badge (four states off existing `sourceMode`/`followIT`); CA 8-slot mark row (via
  `gen_change_alley_v2.py`, never hand-editing SVG) + context-menu primary (radio, persisted, auto-promote).
- **Phase 4 (optional, last):** Interchange target selector + Duo half indicator; do not change summing.

### Boundary-semantics decision the brief asks for (Phase 1)
"Does a second Monsoon stop the walk?" — **Yes, it must** (it already does), and this does NOT break shared
CA, because shared CA does not rely on the walk crossing a Monsoon — it uses the separate `followCA`
rack-wide override. So the strict boundary and shared CA coexist by construction. (This is the resolution to
the brief's §Q3 worry, and it's already how the code works.)

---

## 5. Anchors / generators note
No panel work in Phase 1. Phase 3's CA mark row is the only generated-panel change and MUST go through
`gen_change_alley_v2.py`. I did not find any anchor the kit can't resolve in the current discovery path;
the Phase-2/3 badges are net-new and will define their own anchors when built.

---

## 6. Open questions to confirm before Phase 1 commit
1. OK to treat Phase 1 as **audit + the two suite-list fixes** (not a rewrite), given the primitive already
   landed? (Strongly recommended.)
2. OK to **correct SPEC §1** so it documents the `followCA` override as the CA-share mechanism instead of the
   never-implemented walk exception?
3. Any real patch today that relies on an expander binding ACROSS a second Monsoon for a NON-CA type? (SPEC
   §9 flags this as the one thing the strict boundary would un-bind. I found none in code, but it's a
   rig-level question only you can confirm.)
