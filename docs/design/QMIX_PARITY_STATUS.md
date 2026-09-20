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

### 1. ❌ The blend is NOT routed through Change Alley (the doc's "one genuinely new bit")
The checklist §"The blend" (lines 95–120) specifies the per-voice mux sits **downstream of CA**, with BOTH
operands and the threshold being CA outputs:
- a **new green `qmixSrc[v]` source-select plane** on Change Alley (row-radio like white=rhythm/red=melody),
- the CA scatter streams growing **8→12** (the 2×3×2 product incl. q-mix, keyed `STREAM_CA + i`),
- `pick = philoxSourceSelect(v) < qmixProb_CA(v) ? quantizedInputCV_CA(v) : generatedPitch(v)`.

**What we shipped instead**: a simpler per-voice blend where the threshold is the **Straits per-voice q-mix
level knob** (+ the q-mix draw), and the input CV is the straight poly quantiser CV-in — **CA does not route the
q-mix probability, and there is no `qmixSrc[v]` plane** ([`MonsoonChangeAlleyV2.hpp`](../../src/MonsoonChangeAlleyV2.hpp:30)
has only `rhythmSrc`/`melodySrc`; `corrKey` still `SIDES*TYPES*2 = 8` streams, not 12).
- Musically: our version gives per-voice blend depth but NOT CA's per-voice *scattering* of which-voice's-q-mix
  each voice reads. The doc's heterophony payoff (q-mix probability permuted across voices by CA, like melody) is
  **not** achieved.
- Note: q-mix's *generated draw* already flows through CA's melody-side routing for its slewed buffers
  (`caSrcRow(row, STRAND_QMIX)` in [`PatternEngine.hpp:228/238`](../../src/dsp/engines/PatternEngine.hpp:228)),
  so the generated operand is CA-aware — but the **blend threshold plane** (green qmixSrc) and the **input-CV
  operand routing** are not.
- **Decision needed**: is the shipped per-voice-knob blend the intended final behaviour, or is the CA green-plane
  heterophony (8→12 scatter streams + `qmixSrc[v]` + downstream mux) still wanted? This is the largest open gap
  and a genuine feature, not a cleanup.

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

## Recommended next steps (priority order)
1. **Decide the CA-blend question (item 1)** — it's the doc's headline "genuinely new bit" and the only large gap.
   If wanted: spec the green `qmixSrc[v]` plane + 8→12 scatter streams + downstream mux, keeping the current
   knob-blend as the fallback threshold when no CA is attached.
2. **Grep the hardcoded-initializer traps (`laneSign_` etc.)** — cheap, closes the last accent-class risk.
3. **Raffles q-mix gate-redice (item 2)** — mechanical parity add.
4. **Junction/Causeway q-mix target routing (item 3)** — turns the `getEffectivePolyQmix` TODO into real per-voice
   q-mix probability modulation.
5. **Confirm Big6 UI intent (item 4)** — knob placement only; no engine change.
