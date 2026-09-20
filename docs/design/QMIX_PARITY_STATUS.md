# q-mix parity — checklist vs. shipped (status as of `feat/sands-qmix-geometry`)

Cross-reference of [`QMIX_LANE_PARITY_CHECKLIST.md`](QMIX_LANE_PARITY_CHECKLIST.md) against what is committed
(94bc2a5 … f7537bd). Legend: ✅ done · ⚠️ done-but-diverges-from-doc · ❌ not done.

## Count constants + the accent anti-pattern (the mechanical, bug-prone layer)
- ✅ `NUM_STRANDS` 6→7; `PL_LANES` 4→5 + `PL_QMIX` added in **both** enum copies
  ([`PatternEngine.hpp:112`](../../src/dsp/engines/PatternEngine.hpp:112),
  [`SequencerEngine.hpp:303`](../../src/dsp/engines/SequencerEngine.hpp:303)). The doc flagged the duplicated
  enum as an anti-pattern (line 24) — it is still duplicated, but both now carry PL_QMIX so it is consistent.
  *Residual tech-debt: the two enums are still copy-paste twins (not de-duped); not a q-mix bug, pre-existing.*
- ✅ `SandsGrid` MONO/EAST 6→7, POLY 4→5; `LaneMapping` editor order (QMIX@2) + tables.
- ✅ **Hardening beyond the doc**: `lorStoreBank`/`varlegStoreBank` + SandsGrid↔LaneMapping and editor-enum
  `static_assert`s now tie the duplicate lane constants together (0196a10, 5ccd6a9) — this closes the
  "hardcoded bound didn't scale" class the doc warns about (the accent `l < 3` bug), for LOR banks + editor lanes.
- ❌ **CONFIRMED BUG — the exact accent-anti-pattern trap the doc named.** Three `[NUM_STRANDS]` (=7) arrays in
  [`SequencerEngine.hpp`](../../src/dsp/engines/SequencerEngine.hpp:177) still have **6-element** brace initializers:
  `laneTick_ = {0,0,0,0,0,0}` (:176, harmless — 0 is the fill), `laneSign_ = {1,1,1,1,1,1}` (:177),
  `laneSignPending_ = {1,1,1,1,1,1}` (:178), and `macroLaneSign_ = {1,1,1,1,1,1}` (:231). For the three SIGN
  arrays, element 6 (strand LEGATO) value-inits to **0**, not +1 → LEGATO has a zero direction-sign until the
  first `reset()` (which loops `s<NUM_STRANDS` and sets 1, :218-220). Post-reset it's corrected, so it's latent
  (masked by reset on construct), but it's precisely the "6-element init that didn't scale" the checklist calls
  out (lines 25/37). FIX (trivial, Code mode): make the three sign braces 7-element `{1,1,1,1,1,1,1}` (and
  laneTick 7-element for uniformity). Prepared but NOT applied — Architect mode can only edit Markdown.

## Per-subsystem parity
- ✅ **Engine generation**: q-mix drawn/LOR-shaped like melody, own STREAM_SOURCE_SELECT; per-voice arrays
  `polyRandom(v, PL_QMIX)`; seed/reseed/mix/slew mirrored (Task 4a/4b).
- ✅ **Sands UI editor**: QMIX is a normal lane at slot 2 (label "QMIX"); enum fixed + guarded (5ccd6a9).
- ✅ **Spread**: SpreadManager 4→5, SpreadInterp QMIX case, per-lane apply (Task 4c).
- ✅ **Persistence**: `qmixSeedFloat/…Pending/…PendingFloat`, `qmixRandom`, `drawCtrQ`, `slLatchedQ/slFirstQ`,
  `mixLatchedQ` round-trip (Task 4d) — mirrors rhythm/melody.
- ✅ **Panel generators**: Sands East/Macro/Mono full q-mix control lane + Macro 5th MIX-IN group.
- ✅ **Monsoon controls**: mono q-mix Level knob + `DICE_Q_PARAM` / `LAST_DICE_Q_PARAM` / `QMIX_MIX_PARAM` /
  `DICE_SLEW_Q_PARAM` + Dice Q pending light.
- ✅ **Straits poly**: `POLY_QMIX_PARAM_1..15` (15, pattern-consistent — matches the doc's line-76 note) + a 3rd
  per-voice knob bank; 34HP panel; per-voice poly source-select (f7537bd).
- ✅ **Quantiser source-select (mono + poly)**: `voicePitch(forceGenerated)` blend; the "one correctness rule"
  holds because generated notes flow through the normal `genPitchLive`→gate path that sets `lastSemitone`.

## Divergences worth a decision (⚠️/❌)

### 1. ✅ The blend IS now routed through Change Alley (RESOLVED — the doc's "one genuinely new bit")
> **UPDATE (feat/sands-qmix-geometry):** this gap is CLOSED. The green plane + CA-routed blend shipped after
> this status was first written. The description below is retained for history; the ✅ items are the current state.

The checklist §"The blend" (lines 95–120) specified the per-voice mux downstream of CA with both operands +
the threshold being CA outputs. **All three legs are now implemented:**
- ✅ **Green `qmixSrc[v]` source-select plane** on Change Alley — row-radio like white=rhythm/red=melody, with
  its own green pin (Shift+click), render, hover readout, undo/reset/persistence
  ([`MonsoonChangeAlleyV2.hpp`](../../src/MonsoonChangeAlleyV2.hpp:40) `qmixSrc[16]`).
- ✅ **CA scatter streams 8→12** — `N_SCATTER = SIDES*SCATTER_TYPES*2 = 12`, keyed `STREAM_CA + i`; q-mix is
  panel `TYPES`-dim 2 and gets full CA transform parity (collapse/rotate/reflect/scatter, dom/cod, intra/inter,
  Philox back-jacks + reverse), applied on `qmixSrc` in `applyPendingTransforms`.
- ✅ **Downstream per-voice mux** — `pick = (r_qmix < qmixLevel) ? generated : quantisedInputCV`, gated by
  `quantiserPitchSource`, for mono (voice 0) and all poly voices ([`SequencerEngine.cpp:456/847`](../../src/dsp/engines/SequencerEngine.cpp:456)):
  - THRESHOLD rides the green plane: `slewedQmix`/`slewedPolyQmix` remapped by `caSrcRow(row, STRAND_QMIX)` →
    `caQmixSrc` ([`PatternEngine.hpp:162`](../../src/dsp/engines/PatternEngine.hpp:162)).
  - INPUT-CV operand rides CA's MELODY plane: `voicePitch` reads `caInputCvSrcRow(vi)` (= `caMelodySrc`)
    ([`SequencerEngine.hpp:560`](../../src/dsp/engines/SequencerEngine.hpp:560), [`PatternEngine.hpp:175`](../../src/dsp/engines/PatternEngine.hpp:175)).
  - GENERATED operand = `genPitchLive`, unchanged.
- ✅ Staging: `engine.pe.caQmixSrc[v] = v2->qmixSrc[v]` each block ([`MonsoonSandsManager.cpp:42`](../../src/dsp/managers/MonsoonSandsManager.cpp:42)).
- ✅ Tests: [`test_ca_qmix_source_select.cpp`](../../test/test_ca_qmix_source_select.cpp) covers qmixSrc
  persistence, the 12-stream scatter (determinism/reversibility/independence), the blend mux + markSemi rule,
  the CA transforms applied on the q-mix plane, and the routing-plane invariants
  (`caSrcRow`/`caInputCvSrcRow`/`caQmixSrcRow`).

Musically the heterophony payoff (q-mix probability permuted across voices by CA, like melody) is now achieved.
The Straits per-voice q-mix Level knob remains the blend DEPTH; identity `qmixSrc` + level 0 → always quantised
input (legacy), level 1 → always generated.

### 2. ❌ Raffles q-mix gate-redice
Doc lines 85–87 want `RAFFLES_GATE_REDICE_Q` / `RAFFLES_GATE_LASTDICE_Q` so Raffles can fire q-mix dice like
rhythm/melody. Raffles currently has only R/M gate re-dice ids
([`Monsoon.hpp:382`](../../src/Monsoon.hpp:382), [`MonsoonRafflesExpander.hpp:25`](../../src/MonsoonRafflesExpander.hpp:25)).
**Not added.** (Straightforward parity add: id block + config + gate handler + Raffles panel marker.)

### 3. ❌ Junction + Causeway — q-mix as a modulation TARGET
Doc lines 88–90: modulators should be able to target q-mix's probability + re-dice. Causeway's per-lane CV plane
is `cv3Lane[5]` incl. QMIX ([`Monsoon.hpp:668`](../../src/Monsoon.hpp:668)) so the *slot* exists, but confirm
Junction/Causeway routing actually exposes q-mix as a selectable target (the mod-arc `cv3Lane[4]` is wired for
Slew/Mix Q display, but per-voice q-mix **probability** modulation via Causeway is the `getEffectivePolyQmix`
"ready for a future Causeway q-mix CV" TODO — i.e. **not yet routed**). **Decision/scope**: wire q-mix into
Junction/Causeway target lists, or leave as the documented TODO.

### 4. ⚠️ Big5 → Big6
Doc line 60 wants q-mix added to the headline-modulatable set. Code still says `big5Lane[5]`/`cv3Lane[5]` and
`anyBig5Modulated()` loops `i<5` ([`MonsoonParameterManager.hpp:65`](../../src/dsp/managers/MonsoonParameterManager.hpp:65)) —
those **5** are the mod lanes (note/var/leg/rest/accent), a different axis from the poly q-mix lane. Whether q-mix
becomes a 6th *headline* modulatable (its own big knob in the top row) vs. staying the bottom-right Level knob is a
**UI decision** — currently it's the standalone Level knob, not part of the Big5 group. Confirm intent.

### 5. ⚠️ Dice "trial / B→A" family
Doc line 79 lists `DICE_TRIAL_{R,M}` in the per-stream dice family. Those Trial slots were **repurposed** into
`DICE_Q_PARAM`/`LAST_DICE_Q_PARAM` ([`Monsoon.hpp:215`](../../src/Monsoon.hpp:215)) — i.e. Trial was removed for
ALL lanes, not just skipped for q-mix. So q-mix has the *current* full family (Dice/Last-Dice/Mix/Slew); there is
no q-mix Trial because Trial no longer exists. **Consistent — no action** (the checklist predates the Trial removal).

## Test-suite status (the 2 reds are STALE QMIX-lane tests, not regressions — but they ARE ours to fix)
`test/run_all.sh` = 37 pass / 2 fail. Both failures are **pre-existing relative to the RNG merge** (they fail
identically on pre-merge `0d46db3`), but they are **stale because of the QMIX lane renumber we shipped** — i.e.
QMIX-caused test debt, not "unrelated." The parity checklist explicitly requires "the whole unit suite stays
green" (line 67), so these should be updated to the QMIX lane order as part of finishing q-mix:
- **`test_SandsTopology.cpp`** — asserts the PRE-QMIX lane numbers: `owner(0,4)==MONO "VAR (lane4)"` and
  `owner(0,5)==MONO "LEG (lane5)"` (:50-51) and `owner(1,4)==NONE "poly has no VAR lane"` (:81). After QMIX,
  VAR=5/LEG=6 and lane 4 = ACCENT (a real poly lane). The TEST is wrong, the code is right. Fix: renumber the
  test's VAR→5/LEG→6 and update the "poly lane 4" case to ACCENT (poly) semantics.
- **`test_probmod_roundtrip.cpp`** — pins the pre-QMIX model: `kStrands[6]` with the old order comment
  `RHYTHM=2` (:50-55, now QMIX=2/RHYTHM=3), loops poly editor lanes `ed<4` while `PL_LANES` is now 5, and uses
  the OLD 4-lane `EDITOR_TO_ENGINE_LANE`/`ENGINE_LANE_TO_EDITOR` tables (:135/148/189) rather than the
  `*_QMIX` 5-lane tables. Fix: extend to 7 strands (incl. QMIX) + 5 poly lanes and the QMIX-aware tables, keeping
  it a faithful round-trip oracle. **Care**: this is the LOR/spread "regression oracle" — update it to the NEW
  correct model deliberately (don't just make it pass), so it still catches real leakage.
- These two are also the reason the parity doc's item-1 hardening note matters: they'd have caught lane drift if
  they'd been kept current. Recommend a dedicated "update QMIX-stale unit tests" commit, separate from the merge.

## Test-runner hygiene: 3 entries reference MISSING files (silently skipped = false coverage)
`test/run_all.sh`'s `TESTS` list names three tests whose `.cpp` does **not** exist in `test/`, so the runner
prints `? (missing file)` and skips them — they neither pass nor fail (vacuous), hiding lost coverage:
- `test_ScaleMaskArbiter` ([run_all.sh:39](../../test/run_all.sh:39)) — no `test/test_ScaleMaskArbiter.cpp`.
- `test_quantize_engine` ([run_all.sh:64](../../test/run_all.sh:64)) — no `test/test_quantize_engine.cpp`.
- `test_quantize_phrasing` ([run_all.sh:65](../../test/run_all.sh:65)) — no `test/test_quantize_phrasing.cpp`.
Unrelated to the RNG merge (the merge only ADDED `test_qmix_rng` to the list). Either these tests were
deleted/renamed without updating the runner, or listed-but-never-added. **Per file**: (a) restore if it existed
and is wanted — `git log --diff-filter=D -- test/<name>.cpp` to find when it vanished — or (b) drop the stale
`TESTS` entry so the runner stops advertising coverage it doesn't have. NOTE: the two `test_quantize_*` names
overlap the quantiser-mode pitch path that the q-mix source-select now modifies (Mode C/D/F) — worth confirming
they weren't lost right where q-mix needs coverage most.

## Recommended next steps (priority order)
1. **Decide the CA-blend question (item 1)** — it's the doc's headline "genuinely new bit" and the only large gap.
   If wanted: spec the green `qmixSrc[v]` plane + 8→12 scatter streams + downstream mux, keeping the current
   knob-blend as the fallback threshold when no CA is attached.
2. **Grep the hardcoded-initializer traps (`laneSign_` etc.)** — cheap, closes the last accent-class risk.
3. **Raffles q-mix gate-redice (item 2)** — mechanical parity add.
4. **Junction/Causeway q-mix target routing (item 3)** — turns the `getEffectivePolyQmix` TODO into real per-voice
   q-mix probability modulation.
5. **Confirm Big6 UI intent (item 4)** — knob placement only; no engine change.
