# Sands consolidation — Phase 0 recon (write nothing, report)

Spec (authority): `docs/design/SANDS_CONSOLIDATION.md` (final decision = kill Mono, keep+extend
Macro, sends right; East→mono later; full dissolve deferred). Build plan:
`plans/sands_consolidation_build.md`. This file is the Phase 0 recon record — no code, just the
confirmed map. Steps 1+ build against these facts.

The "3-module latch at Monsoon.hpp:450" referenced in the brief is NOT a Sands-ownership latch —
line 450 is a STEP_LEGATO_GATE comment (gate emission). The actual Sands ownership coordination
lives in `SandsTopology.hpp` + `MonsoonSandsManager.cpp` + the three visual headers (below).

---

## 1. Where the Sands topology / ownership logic lives + the 3-module-only rules

**The ownership class:** [`SandsTopology.hpp`](src/dsp/SandsTopology.hpp) — the single authority for
who owns/edits/writes a lane cell. `Role { NONE, MONO, EAST, MACRO }`; `Config` enumerates ALL 8
reachable combos; `owner(voice, editorLane)` is the single ownership resolver.

**The live site:** [`MonsoonSandsManager.cpp`](src/dsp/managers/MonsoonSandsManager.cpp) —
`processDNA()` reads the three visuals, builds the topology, drives spread/remap/lock. `hasMonoVisual`
(:94), `monoVis` reads, and the "resolve the Monsoon store owner from Macro OR Mono" fallback (:107-109)
are the Mono-coordination surface.

**The three visual modules:**
- **Mono** = [`MonsoonSandsVisualExpander.hpp`](src/MonsoonSandsVisualExpander.hpp) — the V1/reference editor. 7 lanes (MONO_LANES). PROB_OUT ×7, direction ×7 + dir-mod ×7, V1-owner ×5 (poly lanes), delegation ×5 + deleg-mod ×5, spread-base trimpots ×5.
- **East** = [`StraitsEastSandsVisual.hpp`](src/StraitsEastSandsVisual.hpp) — per-voice editor, ALL voices (V1..V16). 7 lanes (EAST_LANES; VAR/LEG display-only until poly-LOR lands). direction ×7 + dir-mod ×7, delegation ×7 + deleg-mod ×7 (ALL lanes incl VAR/LEG), V1 owner ×5, poly owner ×15×5.
- **Macro** = [`StraitsSandsMacroVisual.hpp`](src/StraitsSandsMacroVisual.hpp) — global editor. 5 lanes (POLY_LANES). direction-mod, PROB_OUT ×5, spread/sends/taps.

**The lane grid:** [`SandsGrid.hpp`](src/ui/SandsGrid.hpp) — `MONO_LANES=7`, `POLY_LANES=5`,
`EAST_LANES=7`. Cross-header static_asserts pin these to `LaneMapping.hpp`'s
`POLY_LANE_COUNT`/`EDITOR_LANE_COUNT`. **Step 1 = `POLY_LANES` 5→7** (the lane-uniformity gate).

**Rules that exist ONLY to coordinate three modules (collapse when Mono dies — Step 7 deletes these):**
1. `SandsTopology::Config` — 4 of 8 configs involve MONO: `MONO`, `MONO_PLUS_EAST`,
   `MONO_PLUS_MACRO`, `MONO_EAST_MACRO`. With Mono gone only `EMPTY/EAST/MACRO_SOLE/EAST_PLUS_MACRO`
   remain (4 configs, 2 modules).
2. `Role::MONO` — the enum value + every `Role::MONO` return in `owner()`.
3. `owner()` Mono-present branches — V1→MONO (unless ceded to Macro), VAR/LEG→MONO
   (SandsTopology.hpp:109, 111-115). With Mono gone, V1→EAST (East is the V1 editor) and VAR/LEG
   become poly-delegable (once Step 1 makes them POLY lanes).
4. `Inputs::monoV1Owner[5]` — Mono's V1 owner array (the cede-to-Macro toggle).
5. `hasMonoVisual` + `monoVis` reads in `MonsoonSandsManager` + the "gMon from Macro OR Mono"
   fallback (:107-109).
6. The VAR/LEG "mono-only, never delegable" rule (SandsTopology.hpp:100,109,126 +
   SandsGrid EAST_LANES comment "display-only") — becomes false once Step 2 gives VAR/LEG poly
   buffers + spread.
7. SANDS_OWNERSHIP_SPEC §5 "if no Mono present, East may delegate lane 1" — a Mono-present/absent
   conditional; unreachable once Mono is gone.

**Before/after rule count (Step 7 deliverable):** ~7 coordination rules (4 configs + Role::MONO +
owner Mono-branches + Inputs::monoV1Owner + manager hasMonoVisual/gMon-fallback + VAR/LEG-mono-only)
collapse to the 2-module (East+Macro) reality. Exact count reported at Step 7 after the deletion.

---

## 2. What Mono does beyond V1 — THE key question (does killing Mono lose lane-owner / same-playhead?)

**Answer: Mono does NOTHING beyond V1 editing that East doesn't already cover. Killing Mono loses
NO capability — not lane-owner, not same-playhead, not blend. The deletion is clean. ✓**

Evidence (code, not docs):
- **Mono = V1 editor.** `MonsoonSandsVisualExpander` edits the mono/reference voice (V1). Its
  distinctive controls (PROB_OUT ×7, direction ×7 + dir-mod, V1-owner ×5, delegation ×5, spread ×5)
  are ALL present in East (which edits ALL voices incl V1) and/or Macro.
- **Lane-owner-across-playheads is EAST's, not Mono's.** The spec (§47) names it "East's distinctive
  `lane owner` + `blend`". No `laneOwner`/`lane_owner`/playhead-switch symbol exists in the Mono
  header — Mono edits one voice (V1) on the engine's single mono playhead. Killing Mono does not
  touch East's lane-owner. ✓
- **Same-playhead.** The spec: "the unified model assumes ONE mono playhead (Mono already uses the
  same playhead per voice)". Mono reads the engine's shared mono playhead; it has no separate
  playhead. Killing it loses no playhead capability. ✓
- **blend.** The spec's "blend (East ×9)" is East's. No `blend` symbol in the Mono header — the
  owner/delegation IS the blend mechanism (confirms spec audit (b): blend == reference/correlation
  blend, NOT a 2nd mechanism). ✓
- **East already covers V1.** Per the spec's settled model: "East already addresses ALL voices
  including V1 — V1 is just voice 1 of East." `StraitsEastSandsVisual` has a V1 owner store
  (:272) and edits voice 0. Mono existed ONLY to edit V1 as "the reference"; with Mono gone, V1
  editing is in East and the reference role is the follow/delegation target (voice 1). ✓

**Verdict: Step 6 (kill Mono) is safe — pure redundancy removal, no capability loss.** The one real
loss the spec feared (lane-owner-across-playheads) is East's and survives; Mono carries nothing
distinctive.

---

## 3. Lane feature complement — which lane has which of the full set

Full set (Step 2 target) = **direction** + **direction-mod** + **delegation** + **delegation-mod
[East]** + **spread** (poly buffers + per-voice slew/pre-remap) + **probability out**.

| lane (editor) | dir | dir-mod | deleg | deleg-mod | spread (poly) | prob-out | gap |
|---|---|---|---|---|---|---|---|
| MEL  | ✓ | ✓ | ✓ | ✓ (E 7) | ✓ (5) | ✓ | none |
| OCT  | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | none |
| QMIX | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | none |
| REST | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | none |
| ACC  | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | none |
| VAR  | ✓ (M7) | ✓ (M7) | ✗ mono-only | ✗ | ✗ mono slewed only | ✓ (M7) | **deleg + deleg-mod + spread** |
| LEG  | ✓ | ✓ | ✗ | ✗ | ✗ | ✓ | **deleg + deleg-mod + spread** |

**Gaps Step 2 must close (the real engine work):**
- **VAR/LEG spread** — the big one. They have mono `slewed` buffers but NO `slewedPoly*` buffers, no
  per-voice slew/pre-remap path, no spread UI. Needs: `slewedPolyVariation[15][16]` +
  `slewedPolyLegato[15][16]` (PatternEngine), `SpreadInterp` `N_SPREAD_LANES` 5→7 + mapping tables,
  `PL_VARIATION`/`PL_LEGATO` in the `PolyLane` enum (`PL_LANES` 5→7), `spreadTargetMode[5]`→`[7]`
  (Monsoon.hpp:793 + PatternEngine.hpp:144), `SpreadManager::spread[8][5]`→`[8][7]`
  (SpreadManager.hpp:62). This is NOT a table-widen — it needs the poly draw/slew path + pre-remap
  (spec §65-72).
- **VAR/LEG delegation + deleg-mod** — East already WIRES 7 (deleg-mod ×7 "all lanes",
  StraitsEastSandsVisual:111-112); the gap is the engine/topology treating VAR/LEG as DELEGABLE
  (Step 1 makes them POLY lanes → SandsTopology VAR/LEG-mono-only rule dies → delegable). Mono's
  VAR/LEG delegation was mono-only; East's already covers 7.
- **Macro prob-outs** — only 5 (`PROB_OUT_REST..QMIX`, StraitsSandsMacroVisual:127); needs 7 after
  Step 1.
- **Audit flag (plan §30):** "other lanes may also be missing direction-mod/prob-outs." From the
  headers: Mono has dir-mod ×7 + prob-out ×7 (full); East has dir-mod ×7 + deleg-mod ×7 (full);
  Macro has dir-mod + prob-out ×5. So the ONLY uniformity gap is VAR/LEG (deleg/spread) + Macro's
  5→7 widen. No other lane is missing a feature — the 5 poly lanes are already full.

**Engine sizing that Step 2 widens (all currently 5 → 7):** `PL_LANES` (PatternEngine + SequencerEngine),
`spreadTargetMode[]`, `SpreadManager::spread[][]`, `N_SPREAD_LANES`, Macro `PROB_OUT`/`NUM_OUTPUTS`,
`SandsGrid::POLY_LANES` (Step 1), `SandsTopology::kPolyLanes` + the owner arrays (`monoV1Owner[5]`/
`eastV1Owner[5]`/`eastPolyOwner[15][5]` → `[7]`). The `LaneMapping.hpp` `POLY_LANE_COUNT` +
`ENGINE_LANE_TO_EDITOR_QMIX` table widen in lockstep (cross-header asserts catch a miss).

---

## Step 5 flag — perVoiceArticulation
`perVoiceArticulation` (default OFF) is the binary "soup" flag (spec §74-80): the per-voice
VAR/LEG mechanism WITHOUT spread/correlation. Step 2 (spread+correlation on VAR/LEG) makes it
redundant → Step 5 removes the menu item + flag (Lantern.cpp:303/437 reference it). Do NOT promote
it ON before Step 2 (it gives uncontrolled soup).

## Conflicts (code vs docs)
- The brief's "3-module latch at Monsoon.hpp:450" — code says it's a STEP-gate comment, not Sands
  ownership. The real coordination mesh is `SandsTopology` + the manager (above). No conflict, just
  a misremembered line number.
- Spec §47 "blend (East ×9)" vs code: no `blend` symbol in East's header — the owner/delegation IS
  the blend. Confirms spec audit (b) (blend == reference/correlation blend). No conflict.

## Order confirmation
Step 1 (POLY_LANES 5→7) → Step 2 (full complement, the engine work, with tests) → 2b (East var/leg
lock symbols) → 3 (Macro sends RHS) → 4 (helix room) → 5 (perVoiceArticulation) → 6 (kill Mono,
LAST) → 7 (topology simplification, reported before/after). All on a fresh branch off master; suite
green + Rack check between each.
