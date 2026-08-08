# Shared Change Alley — build plan (Option A, pairId rack-wide, TEMPLATE helpers)

Rodney rulings (Aug 2026): BUILD shared CA. Seed sharing is NOT needed (dropped). Use the pairId
rack-wide scan (the UPDATE path in CA_SHARED_EXPANDER_OPTION_A.md), and **template** the pairing
helpers (generalize IntertropicalPairing.hpp), don't copy them.

## Goal
A second Monsoon reads the SAME Change Alley V2's pin matrix (`rhythmSrc`/`melodySrc`). Both Monsoons
run their own engine/Sands/clock; only the CA correlation structure is shared. Enables cross-row
correlated polyrhythm. NO seed unification (each Monsoon keeps its own draw space; only the pin
CORRELATION is shared — which is the useful, testable half).

## Existing pattern to reuse (verified in code)
`ui/IntertropicalPairing.hpp` provides (all typed to `Intertropical*`):
- `assignPairId(self)` — lowest-free id across instances (self-excluded), called in Intertropical
  `process()` on a divider (NOT ctor — getModuleIds re-lock deadlock).
- `resolveFollowedIT(self, followId)` — 0 => nearest-either-side adjacency; >0 => rack-wide by pairId.
- `presentPairIds()` — sorted ids present (for Follow menus).
- Consumers (Lantern:120, ChangiT3) hold `int followIT` (0=Auto), persisted, and a control-rate
  `resolveFollowedIT` cache on a divider. Intertropical.cpp:62 self-assigns pairId in process().

## Design decisions (resolved)
- **Discovery:** BOTH modes (Auto adjacency default + pairId rack-wide override), mirroring IT/Lantern.
- **Seed:** none. Pins-only sharing.
- **Helpers:** TEMPLATE. Generalize the four functions onto a hub type `T` requiring `T::pairId`.

---

## STEP 1 — Template the pairing helpers (behaviour-neutral refactor)
In `ui/IntertropicalPairing.hpp`, add generic templates alongside the existing functions:

```cpp
template <class T> T* findPairHubEitherSide(rack::Module* self, int maxDepth = 12) { /* same walk, dynamic_cast<T*> */ }
template <class T> int assignPairIdT(rack::Module* self)  { /* same, dynamic_cast<T*>, read m->pairId */ }
template <class T> T* resolveFollowedT(rack::Module* self, int followId) { /* 0 => findPairHubEitherSide<T>; else rack-wide pairId==followId */ }
template <class T> std::vector<int> presentPairIdsT()     { /* same, dynamic_cast<T*> */ }
```
Requirement on `T`: a public `int pairId` member (Intertropical already has it; CA gets one in Step 2).

Keep the existing NON-template functions as **thin aliases** so IT/Lantern/ChangiT3 are untouched:
```cpp
inline Intertropical* findIntertropicalEitherSide(rack::Module* s, int d=12){ return findPairHubEitherSide<Intertropical>(s,d); }
inline int  assignPairId(rack::Module* s){ return assignPairIdT<Intertropical>(s); }
inline Intertropical* resolveFollowedIT(rack::Module* s, int f){ return resolveFollowedT<Intertropical>(s,f); }
inline std::vector<int> presentPairIds(){ return presentPairIdsT<Intertropical>(); }
```
`pairColour(id)` stays as-is (shared palette). Guard rail: build must be bit-identical for IT/Lantern/
ChangiT3 (they call the aliases, which forward to the templates instantiated on Intertropical).

NOTE the include shape: the templates dynamic_cast to `T`, so a translation unit using
`resolveFollowedT<MonsoonChangeAlleyV2>` must have CA's full type visible. The header should stay
type-agnostic (templates only need `T` at the instantiation site), so CA's .cpp includes both the
pairing header and MonsoonChangeAlleyV2.hpp. Do NOT add a CA include to the pairing header (keep it
decoupled; Intertropical.hpp include there is only for the aliases — consider moving the aliases to a
tiny IntertropicalPairing_IT.inl if the coupling bites, but first try leaving them).

## STEP 2 — CA V2: pairId + owner-guard flag
`MonsoonChangeAlleyV2.hpp`:
- Add `int pairId = 0;` (persisted). Self-assign in `process()` (mirror Intertropical.cpp:52-63:
  lazy assign if 0 or clashing; NOT ctor). Immutable once set, gaps ok.
- Add `bool transformsAppliedThisBlock = false;` (runtime only, NOT persisted). Reset at the TOP of
  `process()` each block.
- `applyPendingTransforms(active, axisMask)` stays as-is (the manager guards the CALL, not the method).

`MonsoonChangeAlleyV2.cpp` / widget:
- Persist `pairId` in dataToJson/dataFromJson (integer; missing => 0 => reassigned in process()).
- Widget: optional pair badge (reuse Intertropical's drawPairBadge idea + `pairColour(pairId)`), and a
  right-click "Shared access" hint is NOT needed — presence of a pairId + a Monsoon following it is the
  contract. (No sharedAccess bool — superseded by pairId.)

## STEP 3 — Monsoon: followCA field + menu
`Monsoon.hpp`: add `int followCA = 0;` (0 = Auto adjacency; >0 = the CA whose pairId matches). Persist
in PersistenceManager (mirror `locked`/`lockScope` lines).
`MonsoonWidget.cpp` context menu: a "Follow Change Alley" submenu built from
`presentPairIdsT<MonsoonChangeAlleyV2>()` — "Auto (nearest)" + one item per present CA pairId. Mirror
the Lantern/ChangiT3 Follow menu exactly (createCheckMenuItem).

## STEP 4 — ExpanderManager: resolve shared CA + owner guard
Two edits, both in the CA area:
1. **Discovery override (in `update(module)` or a small post-step):** after the adjacency walk sets
   `cachedChangeAlleyV2`, if the owning Monsoon's `followCA > 0`, OVERRIDE with the rack-wide match:
   `cachedChangeAlleyV2 = resolveFollowedT<MonsoonChangeAlleyV2>(monsoonModule, followCA)`. followCA==0
   keeps the adjacency result (today's behaviour). The manager already receives the Monsoon module in
   `update()`; read `followCA` off it (needs Monsoon full type — Monsoon.hpp is already included in the
   manager .cpp). Rate: resolveFollowedT rack-scans, so gate it on the SAME control-rate divider the
   expander scan already uses (update() runs at control rate) — no per-sample scan.
2. **Owner guard (sync(), the `if (cachedChangeAlleyV2)` block ~line 108):** before calling
   `applyPendingTransforms`, check the flag:
   ```cpp
   if (fireNow && !v2->transformsAppliedThisBlock) {
       v2->applyPendingTransforms(vActive, axisMask);
       v2->transformsAppliedThisBlock = true;   // first caller this block = the owner; others skip
   }
   ```
   The READ path (SandsManager::processDNA staging caRhythmSrc/caMelodySrc from v2->rhythmSrc/melodySrc)
   is unchanged and runs for BOTH Monsoons — so the reader gets the pins for free. Only the MUTATION is
   owner-gated. Owner's `vActive` (numPolyVoices+1) is the CA's operating voice count (documented).

## Owner/reader semantics (the one hazard, resolved)
- `rhythmSrc`/`melodySrc`: written once by CA process()/applyPendingTransforms, read-only from both
  Monsoons → SAFE.
- `applyPendingTransforms`: MUTATES → owner-only via `transformsAppliedThisBlock` (reset in CA process(),
  first sync() caller wins). Order-agnostic: whoever Rack runs first that block is owner; stable enough.
- Lock-scope interaction: the CA queue/axisMask logic (LOCK_SCOPE_MENU §6) is unchanged — it lives in
  the owner's sync() call. A reader under its own lock still just READS pins; it never fires transforms.

## Files to touch
- ui/IntertropicalPairing.hpp — add templates + keep IT aliases.
- MonsoonChangeAlleyV2.hpp — pairId, transformsAppliedThisBlock, process() assign+reset.
- MonsoonChangeAlleyV2.cpp / widget — persist pairId, pair badge (optional).
- Monsoon.hpp — followCA field.
- dsp/managers/MonsoonPersistenceManager.cpp — persist followCA.
- MonsoonWidget.cpp — "Follow Change Alley" submenu.
- dsp/managers/MonsoonExpanderManager.cpp — followCA override in discovery + owner guard in sync().
- (MonsoonExpanderManager.hpp — if the discovery override needs a cached CA-follow value; likely read
  from the Monsoon module directly, no new field.)

## Build order + guard rails
1. Template refactor (Step 1) — build IT/Lantern/ChangiT3, confirm bit-identical (neutral).
2. CA pairId + flag (Step 2) — CA self-numbers; no consumer yet → inert.
3. Monsoon followCA + menu (Step 3) — field defaults 0 (Auto) → inert.
4. Manager override + owner guard (Step 4) — the behaviour change. Rack-verify.

Guard rails:
- Steps 1-3 behaviour-NEUTRAL (defaults preserve today's single-CA adjacency).
- Only Step 4 changes behaviour, and only when followCA>0 OR two Monsoons reach one CA.
- Single-Monsoon patches: followCA=0, one Monsoon → adjacency as before, flag set once by the sole owner.

## Rack acceptance test (from the doc)
1. Monsoon A — CA V2 — Monsoon B. Set Monsoon B "Follow Change Alley → #<CA's id>".
2. Drive A 1/16 straight, B 1/16-triplet (different clocks).
3. Set pins in CA → BOTH Monsoons' voice correlation follows those pins.
4. Scatter from the CA → transform applies ONCE (owner only), BOTH see updated rhythmSrc/melodySrc.
5. Cross-row: put B on a different row, Follow by id → still binds (rack-wide scan).
6. Remove B → A unaffected (sole owner again). Remove CA → both fall back cleanly (cachedChangeAlleyV2 null).

## Cross-refs
- CA_SHARED_EXPANDER_OPTION_A.md — the spec (UPDATE section is the chosen path).
- ui/IntertropicalPairing.hpp — the mechanism being templated.
- Intertropical.cpp:52-63 — pairId self-assign reference.
- Lantern.cpp:117-230, MonsoonChangiT3Expander.cpp:20-112 — followId + menu reference.
- MonsoonExpanderManager.cpp:108 — the CA sync block (owner guard site).
- LOCK_SCOPE_MENU.md §6 — CA axisMask (unchanged; owner-side).

## Status
Plan only — not yet built. Seed sharing explicitly out of scope (Rodney).
