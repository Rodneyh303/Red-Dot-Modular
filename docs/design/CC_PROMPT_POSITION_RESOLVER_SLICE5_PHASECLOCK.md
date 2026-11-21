# CC PROMPT — Position Resolver SLICE 5: retire globalWalk_ — the phase clock as a resolver VIEW (option a)

Follow-on to slices 1–4 (merged on `lane-expander-refactor`). Slice 4 made `stepIndex` a derived
mirror of `pos_.playedColumn()`. This slice retires the OTHER position quantity — `globalWalk_`,
the abs-mode read coordinate — the same way: make `pos_` the single authority and DERIVE the phase
coordinate from it, deleting the redundant engine field.

This is option **(a)** from `DESIGN_POSITION_UNIFICATION.md` → section "globalWalk_ — WHY TWO
QUANTITIES …": the resolver coordinates both quantities coherently. It is DECIDED and
behaviour-preserving (safe before C3). Option **(b)** (collapse abs onto the column) is PARKED on
the C3 holder-model decision — this slice must NOT move toward it (see PARKED).

## CARDINAL RULE (non-negotiable)
VERIFY AGAINST CODE AND TESTS, never a plausible narrative. This is a PURE REFACTOR — BIT-IDENTICAL.
`test_position_resolver_identity` is the oracle. No Rack here.

## BRANCH
Off `lane-expander-refactor`. Branch `fix/position-resolver-slice5-phaseclock`. PR back into it.

## THE INVARIANT (verified — this is what makes the field redundant)
`globalWalk_` is a cache of a closed form. It is `≡ wrapMod(transportTick_ − 1, cachedLength)` at
EVERY write site — confirmed in code:
- **tickHolders** (SequencerEngine.cpp ~:830/:836): `transportTick_ += transportMotion;` then
  `globalWalk_ = wrapMod(globalWalk_ + transportMotion, lenG)` — both move together, congruence held.
- **anchorTransport** (~:768/:771): `transportTick_ = tick1based;` then
  `globalWalk_ = wrapMod(tick1based − 1, lenG)` — congruence seeded.
- **setWindow** (~:293): `globalWalk_ = wrapMod(transportTick_ − 1, newLen)` — `transportTick_`
  unchanged, the congruence simply re-taken at the new modulus (the re-author re-bind).

So `globalWalk_` carries NO state that `transportTick_` + `cachedLength` do not. It has exactly ONE
real reader — the abs derive at ~:999 `const int wG = globalWalk_;` (feeds `ft0 = wG + offG`,
`L = kMaskRing`) — plus two debug `printf`s (~:2532, ~:2588). NO external readers, no reference
alias (unlike `stepIndex`): `grep -rn globalWalk_ src/` is engine-internal only.

`cachedLength` == the resolver's own `len_` by construction (both set by the same `setWindow`
`length`). So the closed form can live on `pos_` using `len_`.

## THE SLICE (exactly this)
1. **Add the view to `PositionResolver`** — a const method returning the closed form from the
   resolver's own length and the engine's one clock:
   ```cpp
   // The abs-mode read coordinate: the transport's absolute phase in the window,
   // ≡ the retired globalWalk_. A VIEW (no stored state) — len_ is the resolver's,
   // transportTick the engine's one clock. (b) parked: this is the TICK phase, which
   // survives a window re-author; it is NOT the column.
   int64_t phaseClock(int64_t transportTick) const { return wrapMod(transportTick - 1, len_); }
   ```
   (`wrapMod` — use the resolver's existing modulo helper / mirror `SequencerEngine::wrapMod`'s
   definition; `len_` is already `max(1, length)` from `setWindow`.)
2. **Migrate the reader** — the abs derive at ~:999:
   `const int wG = globalWalk_;` → `const int wG = (int)pos_.phaseClock(transportTick_);`
   The two debug `printf`s (~:2532, ~:2588): same substitution.
3. **Delete the field + all three write sites** — remove the `globalWalk_` declaration
   (SequencerEngine.hpp ~:497) and the three assignment lines (`setWindow` ~:293,
   `anchorTransport` ~:771, `tickHolders` ~:836). At each site DELETE ONLY the `globalWalk_` line —
   every surrounding warm-phase seed stays (see KEEP).

## KEEP (do NOT delete, do NOT touch)
- **`transportTick_`** — the one absolute clock. The warm-phase seeds and Monsoon's Mode-E drift
  check consume it. It STAYS; `phaseClock` reads it.
- **The warm phases** seeded alongside `globalWalk_` at `anchorTransport` (`globalT0Phase_`,
  `lanePhase_[]`, `macroLanePhase_[]`, `macroFramePhase_`, their `*Key_`) and the raw-clock
  congruences in `tickHolders` — SEPARATE holders, out of scope. Only the `globalWalk_` line goes.
- **The window re-author on `col_`** (`pos_.setWindow` park) — slice-4 behaviour, untouched.

## PARKED (option b — do NOT drift toward it)
`phaseClock` reproduces `globalWalk_` EXACTLY: the transport-phase that SURVIVES a window
re-author (re-bound from the tick, not reset to the window top). Do NOT redefine abs on
`playedColumn()`/the window top — that is option (b), a behaviour change gated on the C3 holder
model (the "unit of absoluteness"). This slice is transport-phase, bit-identical, full stop.

## NON-SCOPE (separate threads)
- **`stepIndex` FIELD deletion (migration step 4-final)** — BLOCKED on the widget consumers
  (`Monsoon.hpp`'s `int& stepIndex` alias, `LanternRecordCore`, `VisualExpanderHelpers`); that is
  Rodney's Rack leg (step 3b), not this slice.
- **The cadence slice (C1/C2/C4 input latching)** — a behaviour change, its own slice.
- **C3 per-level direction** — rides the holder-model migration.

## ACCEPTANCE (bit-identical — reproduce-first)
1. **Pin the identity BEFORE deleting** — in `test_position_resolver_identity`, add an assertion
   that `pos_.phaseClock(transportTick_) == globalWalk_` across the full sweep: forward, reverse,
   every `LaneDir`, the non-divisive lengths, offsets, a MID-RUN `setWindow` re-author (the third
   desync case — the re-bind must match), and a phase snap/anchor. Confirm GREEN with the field
   still present (proves the closed form == the accumulator everywhere).
2. **Then delete** `globalWalk_` and repoint that assertion at the closed form directly (or drop it,
   the abs-read goldens now cover it).
3. `bash test/run_all.sh` — all green. The march goldens (GOLDEN_A/B/BS, UA_NONDIV) BYTE-IDENTICAL.
   `test_reverse_rotation_anchor`, `test_phase_rotation_anchor` green. The abs-mode (`MASK_DYNAMIC`)
   mapped reads unchanged (that is the one reader that moved — a drift there fails a golden).
4. `test_16_triage` (GATE 1 grep-pin) still green — no new anonymous literals.

## DELIVERABLE
The engine change + the identity-harness assertion, on `fix/position-resolver-slice5-phaseclock`,
PR into `lane-expander-refactor`. One commit, bit-identical, the identity harness as proof.
