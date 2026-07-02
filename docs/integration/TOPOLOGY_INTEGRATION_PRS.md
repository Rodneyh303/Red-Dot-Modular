# Topology integration — 3 PRs to close out (merge IN THIS ORDER)

Base of the stack: `integration/sands-topology` (tip bd1dd0b, step 5+6 merged).
The three branches are STACKED, so merge 1 → 2 → 3 in order. After #1 merges, #2's PR base
auto-updates; same for #3.

---

## PR 1 — step 5b part 1: poly write guards onto SandsTopology
**Branch:** `feature/sands-topology-step5b-poly-writes` → `integration/sands-topology`
**Merge type:** normal merge (1 clean commit, aa0b1c6)

Extends the topology write-ownership guards from V1 to poly voices — poly write decisions now
go through `SandsTopology::owner(v+1, lane)` like V1 already did. Behaviour-preserving
refactor; cross-check asserts confirmed equivalence before step 5+6 stripped them. Build-proven
earlier (no asserts hit in Rack).

---

## PR 2 — step 5b part 2: spread bug #1 (poly spread-follow) + diagnosis
**Branch:** `feature/sands-topology-step5b2-spread` → `integration/sands-topology`
**Merge type:** SQUASH recommended (8 commits incl. a wrong-turn dance that cancels out —
f3842cd/35fc28b/cfb817c + revert aadd28a; squash lands the clean end state as one commit)

Fixes spread bug #1: the spread-follow that shows Macro's global spread on a ceded lane was
V1-only; poly tabs showed the stored East value. Adds the follow to the poly path via
owner(currentVoice-1, lane)==MACRO. (Bug #1 build-confirmed.)

Also records the full diagnosis of bug #2 (cede→reclaim revert) — that spread lacks the
display/store separation LOR has — which is FIXED in PR 3. The intermediate commits here are
superseded wrong attempts; squashing keeps integration history clean. No engine behaviour
change beyond the poly display follow; topology test 26/26.

---

## PR 3 — spread base display/store split (cede/reclaim fix) + prob-leak diagnosis
**Branch:** `feature/spread-display-store-split` → `integration/sands-topology`
**Merge type:** SQUASH recommended (5 commits → one clean "spread base display/store split")

The real fix for spread cede/reclaim, mirroring LOR's three-layer separation
(STORE/DISPLAY/ENGINE — see docs/design/DISPLAY_STORE_ENGINE_SEPARATION.md):
- DimmableTrimpot gains displayValueFn: a ceded lane's spread knob RENDERS Macro's base spread
  while the SPREAD_* param (the store) stays East's value → reclaim reverts.
- Removes all the param-forcing (poly AND V1) and the store-guards it needed — net
  simplification.
- combineSpread already arbitrates playback (ceded→Macro, owned→East), so engine unchanged.
- **BUILD-CONFIRMED by user:** base spread cede/reclaim now round-trips for BOTH V1 and poly,
  even after Macro's global spread is moved; no display jitter (Rack Pro 2.6.6).

Also DIAGNOSES (not fixed here — deferred, own branch) the spread-mod → Macro prob-bar display
leak on East-owned lanes, and records DECISION (a): Macro must show its OWN probability, never
East's (avoids a Macro↔East feedback loop; downstream must never appear upstream).

### Known-open items riding along (documented, separate branches):
- Macro prob-bar leak fix — option (a), engine change (Macro-own probability source). NEXT.
- SpreadManager spread[][] is 3 lanes wide → ACCENT spread (lane 3) unstored; widen to 4.
- Modulation/arc display on ceded lanes (base values were the priority; deferred).
