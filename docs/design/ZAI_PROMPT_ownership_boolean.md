# ZAI PROMPT — Ownership-Boolean Mini-Brief: one named predicate kills the scattered `> 0.5f`

**Status:** ready for execution. **Size:** ~30 minutes. **Behaviour change: ZERO (byte-identical).**
**Base:** `lane-expander-refactor` @ 09830b3c (the feat/warp-ui merge). Line anchors are from THIS tree — verify each before editing, but no further steps will drift them.

## Why (the campaign catch, one paragraph)

The macroOwn store is a float whose name does not tell you what `> 0.5` means. Ground truth
(verified against three independent production uses during resolver step 4): **the store holds
1.0 = EAST owns the lane, 0.0 = Macro owns** (the East owner menu writes `setToEast ? 1.f : 0.f`,
StraitsEastSandsVisual.cpp:907). The step-4 brief shipped with `delegated = getMacroOwn > 0.5f` —
INVERTED; the executor caught it and shipped the complement. Rodney's directive: "the ownership not
being boolean is something to earmark to fix" (design doc EARMARKS (1), f9fbe68a). This brief is
the fix: ONE named predicate, every threshold read routed through it, zero behaviour change.

## THE CHANGE

### 1. The predicate — Monsoon.hpp, immediately after setMacroOwn (:1002)

```cpp
    // THE polarity predicate (post-campaign earmark): the macroOwn float store writes
    // 1.0 = EAST owns the lane (East UI owner menu / OwnerCell), 0.0 = Macro owns — the
    // name getMacroOwn does NOT say which side "> 0.5" selects. EVERY threshold read of
    // the store goes through this from now on; hand-rolled `getMacroOwn(...) > 0.5f`
    // comparisons are forbidden (review-enforced). Same name as SandsTopology::Inputs's
    // fillOwners parameter (SandsTopology.hpp:71) — one vocabulary, store to topology.
    bool ownerIsEast(int v, int lane) const { return getMacroOwn(v, lane) > 0.5f; }
```

### 2. Migrate the 13 threshold sites (byte-identical: each is the SAME comparison, named)

| #  | Site (merged tree) | Current expression → becomes |
|----|--------------------|------------------------------|
| 1  | MonsoonExpanderManager.cpp:98 (fillOwners lambda, CC_PROMPT_07) | `mmTopo->getMacroOwn(v, l) > 0.5f` → `mmTopo->ownerIsEast(v, l)` |
| 2  | MonsoonExpanderManager.cpp:401 (setVarlegLocalEast VAR) | `mmOwn->getMacroOwn(v, PL::PL_VARIATION) > 0.5f` → `mmOwn->ownerIsEast(v, PL::PL_VARIATION)` |
| 3  | MonsoonExpanderManager.cpp:402 (setVarlegLocalEast LEG) | same shape, `PL::PL_LEGATO` |
| 4  | MonsoonExpanderManager.cpp:430 (poly direction push, step-4 complement) | `!(mmOwn->getMacroOwn(v, l) > 0.5f)` → `!mmOwn->ownerIsEast(v, l)`; refresh the :425-428 polarity comment to name the predicate |
| 5  | MonsoonSandsManager.cpp:144 (fillOwners lambda, Sands side) | `gMon->getMacroOwn(v, l) > 0.5f` → `gMon->ownerIsEast(v, l)` |
| 6  | MonsoonSandsManager.cpp:511 (monoOwnedByMacro — ⚠ THE ZERO-TEST, see §3) | `macroHere && !(mmV1 ? mmV1->getMacroOwn(Monsoon::kMonoMacroOwnRow, lane) : false)` → `macroHere && !(mmV1 && mmV1->ownerIsEast(Monsoon::kMonoMacroOwnRow, lane))` |
| 7  | StraitsEastSandsVisual.cpp:200 | `...->getMacroOwn(v, lane) > 0.5f` → `...->ownerIsEast(v, lane)` (keep the double `findMonsoonEitherSide` ternary shape verbatim) |
| 8  | StraitsEastSandsVisual.cpp:458 | `(vi >= 0 && vi < 16) ? (m->getMacroOwn(vi, lane) > 0.5f) : true` → `(vi >= 0 && vi < 16) ? m->ownerIsEast(vi, lane) : true` |
| 9  | StraitsEastSandsVisual.cpp:708 (visual topo fill, eastV1Owner) | `mmT->getMacroOwn(Monsoon::kMonoMacroOwnRow, el) > 0.5f` → `mmT->ownerIsEast(Monsoon::kMonoMacroOwnRow, el)` |
| 10 | StraitsEastSandsVisual.cpp:711 (visual topo fill, eastPolyOwner) | `mmT->getMacroOwn(pv, el) > 0.5f` → `mmT->ownerIsEast(pv, el)` |
| 11 | StraitsEastSandsVisual.cpp:846 (spread-arc isActive, V1) | `m->getMacroOwn(Monsoon::kMonoMacroOwnRow, engLane) > 0.5f` → `m->ownerIsEast(Monsoon::kMonoMacroOwnRow, engLane)` |
| 12 | StraitsEastSandsVisual.cpp:849 | `(vi >= 0 && vi < 16) ? (m->getMacroOwn(vi, engLane) > 0.5f) : true` → same shape with `m->ownerIsEast(vi, engLane)` |
| 13 | StraitsEastSandsVisual.cpp:1051-1053 (owner-delegation toggle RMW) | `const float nv = (cur > 0.5f) ? 0.f : 1.f;` → `const float nv = mm->ownerIsEast(ch, eng) ? 0.f : 1.f;` (keep the `cur` read ONLY if referenced below; else fold: `mm->setMacroOwn(ch, eng, mm->ownerIsEast(ch, eng) ? 0.f : 1.f);`) |
| 14 | StraitsEastSandsVisual.cpp:1609 | `(cachedMon_ && (cachedMon_->getMacroOwn(Monsoon::kMonoMacroOwnRow, lane) > 0.5f)) ? 1.f : 0.f` → `(cachedMon_ && cachedMon_->ownerIsEast(Monsoon::kMonoMacroOwnRow, lane)) ? 1.f : 0.f` |

### 3. The one semantic subtlety — site 6 is a ZERO-test, not a threshold test

The old text negates a FLOAT: `!(mmV1 ? getMacroOwn(...) : false)` — the ternary's common type is
float (false → 0.f), and `!(float)` is `value == 0`. That agrees with `!ownerIsEast()` (i.e.
`value <= 0.5`) **iff the store only ever holds 0.0 / 1.0**. It does: EVERY writer is 0/1-valued —
:463 `b ? 1.f : 0.f`, :470 `val > 0.5f` (bool→float), :907/:937 `setToEast ? 1.f : 0.f`,
:1053 `nv ∈ {0.f, 1.f}`. Persistence round-trips raw floats (MonsoonPersistenceManager.cpp:189/:471,
json key "editorMacroOwn") but only ever persisted what those writers wrote; only a HAND-EDITED
patch could inject a freak value, which is already outside the model. Document this in a one-line
comment at the migrated site: `// ==0 vs <=0.5: identical on the writable domain {0.0, 1.0} (all writers are 0/1; see ZAI_PROMPT_ownership_boolean.md §3)`.

### 4. Comment-only refresh — Monsoon.hpp:806-810 (WRONG on three counts)

The block above the array still says `INDEXING IS POLY-BANK ... v = 15 -> V1/mono (see
getMonoMacroOwn/setMonoMacroOwn) ... index = v*5 + lane`. Reality: index = `v*7 + lane` (the
accessors at :1001-1002), row **0** = V1 via `kMonoMacroOwnRow` (getMonoMacroOwn/setMonoMacroOwn
are DELETED — see :995-999), lanes 0..6. Rewrite the block to say exactly that, plus: values are
0.0/1.0 written by the East owner menu (Visual :907/:937/:463/:1053), read via `ownerIsEast`.

Also (one line): SandsTopology.hpp:71 — change "the store read (getMacroOwn(v,l) > 0.5)" to
"the store read (Monsoon::ownerIsEast(v,l), i.e. getMacroOwn(v,l) > 0.5)".

## Intentionally UNTOUCHED

- **Writers** (:463, :470, :907, :937, :1053): they write 0.f/1.f through setMacroOwn — correct as-is. A write-side helper adds risk for zero safety.
- **StraitsEastSandsVisual.cpp:470** `setMacroOwn(vi, lane, val > 0.5f)`: the threshold is on the INCOMING widget value, not a store read — not this predicate's job.
- **varlegDeleg** (:1056, :401-402 `getVarlegDeleg(...) > 0.5f`): a SIBLING float store with the same idiom and its own polarity story. Same-class future candidate — its own mini-brief if Rodney wants it. Do NOT migrate here.
- **Persistence format** (editorMacroOwn, json_real 112-array): untouched — patch compatibility is preserved by keeping the store float.

## Verified facts (merged tree @ 09830b3c, code-verified)

- Store: `float macroOwn[112]` (Monsoon.hpp:811), accessors :1001-1002, `kMonoMacroOwnRow = 0` (:1000). 14 threshold/zero-test sites, ALL listed above — a repo grep for `getMacroOwn` finds no others (remaining hits are comments/accessor-def/writes).
- The visual keeps its OWN topology fill (:708/:711) — CC_PROMPT_07 unified the MANAGERS onto fillOwners but not the visual. Out of scope here; noted for the ledger.
- `StraitsSandsMacroVisual.cpp` reads no ownership — the Macro side never greps the store.
- Polarity pins that BITE if you break it: test_warp_voice_isolation (delegation-SOURCE contract), harness U8 (topology owner matrix), test_resolver_dir_bridge (poly-push polarity, :21 documents the polarity), test_warp_all_lanes (A1-A5).
- **THE WHOLE BRIEF IS RIG-INVISIBLE**: the rig compiles only PatternEngine/SequencerEngine/GateState — both manager cpps, both visuals and Monsoon.hpp's consumers are plugin-build-only. The plugin build is the ONLY compile proof. Do not claim done without it.

## Exit contract

1. Ladder: `export RACK_DIR=<sdk>` then `bash test/run_all.sh` → **71/71** + anchors (east 113 / macro 140 / keppel 19 / causeway 55).
2. `UNIFORMITY_STRICT=1` harness run → **1494/1494, exit 0**, manifest EMPTY (unchanged — no new seams, no manifest edits).
3. NO new test suite (this is a rename-level change; the four polarity pins above are the proof).
4. Plugin build SUCCESS, warning-clean. **This is the mandatory gate** (see verified facts).
5. Grep check: `getMacroOwn(... ) > 0.5f` → ZERO hits in src/ (comments excepted). `grep -rn '> 0.5f' src/ | grep -i macroown` must be empty.

## Honesty block

ZERO deliberate behaviour changes. Byte-identical on the writable value domain. The ONLY textual
semantic movement is site 6 (zero-test → threshold-test), equivalent on {0.0, 1.0} per §3 — say so
in the exit report verbatim. If ANY suite or the plugin build moves unexpectedly, STOP and report
instead of adapting the brief.

## Not this task

- Bool STORE migration + versioned persistence format (Rodney-decidable; needs a patch-load story).
- Unifying store-flag vs topo.owner AUTHORITIES (earmark constraint — never in the same breath as this rename).
- varlegDeleg predicate (sibling store, own brief).
- lane>=4 monoDirAuthority semantics (CC_PROMPT_08), tap ternary residue, mono LOR copies.
