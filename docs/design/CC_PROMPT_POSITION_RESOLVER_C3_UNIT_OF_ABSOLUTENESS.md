# C3 — Unit of Absoluteness (finishing slice 5)

Status: **DECISION RECORDED, implementation pending.** This resolves the question slice 5
explicitly PARKED (`CC_PROMPT_POSITION_RESOLVER_SLICE5_PHASECLOCK.md`, the PARKED section:
*"option (b) — collapse abs onto the column — is PARKED on the C3 holder-model decision"*).

## The decision

**The unit of absoluteness is the signed transport tick (`transportTick_`).** The played
column stops being independent state and becomes a *view* of the tick plus a small
**entry-frame anchor** `(entryTick, entryDir)`.

- `transportTick_` is the one true position. Verified signed, not monotonic: `advancePlayhead(dir)`
  → `tickHolders(dir, …)` → the sole write `transportTick_ += transportMotion` (SequencerEngine.cpp
  :1018), so +1 forward, −1 reverse; scrub/phase re-anchors it absolutely via `anchorTransport`.
- `phaseClock(t) = (t−1) mod len` is already a pure view of it; slice 5 moved the **reads** onto it
  and retired `globalWalk_`.
- `col_`, `phrasePos()`, `readCoord()` become views of `(transportTick_, window, entryDir)`. No
  competing accumulator remains, so the phrase **edge** and the **reads** physically cannot
  disagree — the DRY seam closes, in both directions.

The entry anchor is a **frame origin, not a second position**: captured *from* the tick, constant
between entries, carrying no position of its own, so it cannot drift against the tick the way the
old `col_` accumulator did. One position authority, plus a pin saying where this phrase's
coordinate frame sits.

## Why this was the bug (the residual seam)

The reads already fold the tick-derived `phaseClock` (SequencerEngine.cpp :844, :1255, :1529). The
phrase **edge** was the last consumer still reading the column-derived `phrasePos()` — `wrapped`
(:506) and the LOR / voice-frame commit gates (:837, :939, :1030, :1227). Two derivations of "where
in the phrase are we," from two state variables with different update laws:

- `transportTick_` runs with the clock.
- `col_` **re-parks** to the window top in `setWindow` (PositionResolver.hpp :111) on a mid-phrase
  offset/length change.

Under a mid-phrase window re-author the two diverge — exactly the *"third desync case"* slice 5
pinned its identity harness against **for the reads**, which the edge never got. Observable result
(Rodney's Rack feel-test): the dice/slew redraw fired at the drifted column (`phaseClock` stuck at
~8/14/30) instead of at the window start — *"not when transport is at zero, more like 8"* — and it
affected **redice too**, because dice and slew share the one edge still on `col_`. (The separate
"mix has no effect at slew 0" issue was the dead-scrub `if (r==0) return` bug, already fixed in
PR #361 — NOT this.)

## The entry-anchor cadence (the write law)

Today (verified, PositionResolver.hpp): `entryDir_` is cleared by `reset()` (:100) and by any real
window change in `setWindow` (:104), and set on the first motion after a clear to the sign of that
motion (`advance`, :121); a mid-phrase direction flip never touches it (the true-retrace anchor).

**That `setWindow` entry-clear is the drift in disguise.** Re-capturing the entry on every offset
twiddle re-origins the phrase to *when you moved the knob* — history-dependent, which is the
stickiness. So the one cadence change:

- **A window change must NOT re-origin the forward phase.** Forward's entry is the clock's own zero
  (`phaseClock == 0`), which is deterministic and survives a window re-author by construction. In
  forward `entryDir == +1`, the `setWindow` entry-clear is a no-op, and `col = startStep + phaseClock`.
  Offset becomes **pure placement** (it moves `startStep`), never a phase re-origin.
- **`entryDir` carries real information only in reverse** — the reverse entry edge plus the ±1 the
  bare `phaseClock` gets wrong (PositionResolver.hpp header :22–27). It is captured at a **direction
  entry** (the first motion after reset, and entering the reverse leg), because a mid-phrase reverse
  entry is genuinely history-dependent (the retrace feature exists for exactly that).

So: a **new `entryDir` is set at a direction entry, not at a window change.**

## Forward / reverse split

- **Forward** — phase origin = the clock zero. The phrase edge becomes `phaseClock(transportTick_) == 0`.
  Deterministic absolute position; moving the global offset moves only the window placement, and the
  boundary **snaps back** when the offset reverts. This is the one **intended behavioural delta**.
- **Reverse** — phase measured from the captured entry (`entryTick`, `entryDir`); retrace preserved.
  This is C3's reverse semantics, and we **preserve today's observable behaviour** — a preservation
  choice, not an unmade decision.

Because forward-linear satisfies `col_ == startStep + phaseClock`, the edge-on-tick is
**byte-identical** to the edge-on-column everywhere **except** the `setWindow` re-park path — i.e.
the change bites only on the bug.

## Edge case — directionlessness / stall (Rodney 2026-11-24)

A step can have **no motion and no direction**: gate mode with Variation at the centre (neither
forward nor back) is a HOLD, and a phase snap can stall at the same position. The view needs **no
special directionless branch**, because the tick is the unit of absoluteness: a *position* exists
even when a *direction* does not. Verified routing:

- **Gate centre** → `scanHoldFor_` runs, `advancePlayhead` is NOT called, and the refresh goes through
  `recomputeLaneTicks` → `tickHolders(0, lastPlayDir, …)`. `transportTick_ += 0` (:1018, *"the
  offsets-only refresh (motion 0) leaves it verbatim"*): the tick **freezes**, and `lastPlayDir` /
  the resolver's `dir_` retain the last real direction. So the forward/reverse edge classification and
  the `phaseClock` origin are both still well-defined — the held step reads the same edge it read
  before, byte-identical on the aligned path.
- **Stalled phase snap** → the phase-override path (`phraseOverridden_`) keeps `phraseOverride_`; the
  forward/reverse `phaseClock` split does not touch it (the edge predicate returns `phraseOverride_ == 0`
  first when overridden). Unchanged.

So the rule the resolver already enforces — `advance()` touches `entryDir_`/`dir_` only when
`motion != 0` — is exactly right: a hold is "tick held, last direction retained," never a new entry
or a flip. The tick-as-unit design inherits this for free; that direction can be absent while position
cannot is itself a small confirmation that the tick is the right unit.

## Scope / non-scope

- The phase-override / scrub path (`snapTo`, `phraseOverride_`) is unchanged — it keeps its own raw
  phrase.
- Reverse *behaviour* is preserved, not changed. If the tick-based view could not reproduce reverse
  without a behaviour change, THAT would be a new decision — not expected, since the view is a
  re-expression of `col_`'s own traversal, not a new law.

## Acceptance (reproduce-first, slice-5 pattern)

1. **Reproduce-first forward test** — drive the engine forward, perturb the global offset mid-phrase
   and revert it; assert the forward phrase edge coincides with `phaseClock(transportTick_) == 0` at
   every started step (equivalently: the boundary's absolute position is deterministic and snaps back
   on revert). FAILS before (stuck at the drifted phase), PASSES after. Include a **hold-at-boundary**
   case: a directionless/held step (tick frozen) must not spuriously fire or flip the edge — it reads
   the same edge as the step before it.
2. **Reverse byte-identity** — `test_reverse_rotation_anchor`, `test_phase_rotation_anchor` green and
   unchanged (the reverse view reproduces `col_`'s retrace).
3. **March goldens** (GOLDEN_A/B/BS, UA_NONDIV) BYTE-IDENTICAL — the forward change is a no-op off the
   re-park path.
4. `bash test/run_all.sh` all green; `test_16_triage` grep-pin green.
5. **Rack feel-test (Rodney)** — the dice/slew redraw visibly lands at the window start and tracks the
   offset, reverting cleanly to zero.

## Implementation outcome (2026-11-24, PR #365)

Shipped the forward **draw edge** on the tick; the battery corrected the plan's "point the 5 edge
sites at the predicate" to **one site**:

- `PositionResolver::atPhraseEdge(tick)` added (forward `phaseClock(tick)==0`, reverse `phrasePos()==0`,
  phase-snap raw, pre-start true). Only `advancePlayhead`'s `wrapped` (the draw edge — the reported
  dice/slew bug) moved onto it.
- The other four edge sites — `voiceFrameEdgeLive_`'s identity row and the LOR latch gates in
  `anchorTransport` / `tickHolders` — were moved too, then **reverted**: they failed
  `test_global_dip_differential`. Those are per-voice **commit** edges that fold the voice frame and
  `advanceOffset`; the bare global `phaseClock` can't express that anchor, so they stay on
  `phrasePos()==0`. Unifying them is the holder-model migration, not this fix.
- Reverse `entryTick` question, settled against the code: a fresh reverse entry (`entryDir:=−1`) CAN
  land mid-run at an arbitrary tick (a window re-author then a reverse step parks `col_` to `endStep`),
  so a full "col as a pure view off (tick, entryDir)" — with `col_` deleted — can't be byte-identical
  for reverse without a 4th piece of state (the entry tick) OR keeping the column accumulator for
  reverse. Since the decision already keeps the column for reverse, `col_` is **not** deleted; it
  survives as reverse's coordinate. So the minimal state stands as tick + current dir + entryDir, with
  `col_` retained for reverse rather than a separate entry tick.
- Proof: `test_phrase_boundary_absolute` (reproduce-first) red→green; reverse/position goldens
  byte-identical; full battery 163/163. Rack feel-test outstanding (dice/slew redraw at the window
  start, reverts to zero).

Open: the per-voice commit edges + the full column-as-view (entry-tick or retained column) remain for
the holder-model migration — a separate arc, not required by the dice/slew fix.

## Deliverable

The PositionResolver change (column as a tick-based view with an entry-frame anchor; the forward edge
on `phaseClock`; the `setWindow` forward re-origin dropped) + the reproduce-first forward test, with
the reverse goldens as the byte-identity proof and the forward drift-fix as the single intended
behavioural delta. Branch off `master`, PR into `master`.

## Follow-up — the net-steps model and resolver ownership (Rodney 2026-11-25)

A few minutes in Excel produced the whole model: a single SIGNED counter (+1 forward, −1 back, 0 hold;
first motion enters; reset re-inits; never re-parks). The read is `read = O + ((netSteps + R +
advanceOffset) mod L)`. Pinned against the live engine in `test_netsteps_model`: it reproduces the
engine for forward, mid-phrase flip (retrace), holds, rotation (static + mid-run), static voice/global
offset, per-voice advanceOffset, and cold reverse from zero — diverging ONLY on a re-anchor (reverse
right after a window change; a mid-run OFFSET change), which is the same re-park #365 removed on the
forward edge, and the deliberate C3 choice.

The offset law, verified: **R** and **advanceOffset** are window-relative PHASE (added INSIDE mod L);
the voice LOR **O** is window PLACEMENT (OUTSIDE mod L); the GLOBAL transport offset is NOT in the read
value at all — it places the played COLUMN. `advanceOffset_[v]` is per-VOICE (`netSteps +
advanceOffset_[v]` = voice v's own net steps), so net steps is tracked POLYPHONICALLY; the per-lane
walk (`laneTickV_[v][strand]`, per-lane direction) is finer still.

Key architectural finding: **`getStrandIdx(tick, L, O, R) = O + ((tick + R) mod L)` IS the net-steps
read law** (SequencerEngine.cpp :3222), and every probability lane already reads through it. So the
probability side already runs the model. The ONE hold-out is the voice FRAME (`voiceRead_`, the strip
playhead), which still goes through `readCoord()`/`col_`/`entryDir` + `walkFold`. The lanes are NOT
independently drift-free — `laneRecomp` recomputes `laneTickV_` each derive from the frame's seed
(`frameT0`, from `wnow`, from `readCoord`, from `col_`), so the ONE upstream coordinate (`col_`) feeds
the frame and all seven lanes per voice. That is the real DRY/ODR issue: one position truth, many
representations.

### Target shape

The resolver should OWN the position state and the ONE read law; the engine keeps POLICY (LOR latching,
commit gates, draw timing) and asks the resolver where things are. Today the resolver owns only the
transport column, while the engine holds `transportTick_` (the real unit of absoluteness),
`advanceOffset_`/`advOffSeen_`/`scanOfsV_` (per-voice divergence), `laneTick_`/`laneTickV_`/`laneSign_`/
`laneSignV_` (per-lane positions + directions), `lanePhase_`/`macroFramePhase_` (holder phases),
`voiceDriver_`/`voiceRead_`/`macroFrameRead_` (the reads), and `startStep`/`cachedLength`/`cachedOffset`/
`lastPlayDir` (duplicates of resolver state). Consolidating these into the resolver makes the next
disagreement visible as two methods in one file instead of two counters in two files.

### Slicing order (each slice byte-identical except where noted)

1. **Widen the net-steps pins** — non-Forward lane directions, poly advance, sub-windows (lane L <
   global L). If the counter still matches except the re-anchor cases, proceed.
2. **Move `transportTick_` + the per-voice offsets into the resolver** as-is, no behaviour change.
3. **Move the lane + frame derivation behind resolver methods**, still byte-identical.
4. **Swap the coordinate source to net steps**; retire `col_` + `entryDir`. The ONE deliberate
   behavioural delta, on the re-anchor cases (feel-tested; the re-author march in
   `test_position_resolver_identity` and the reverse-rotation anchor goldens get a conscious update).

Not a no-brainer to execute: phase mode (`snapToPhaseStep` sets position directly, counter not wired);
the cold-reverse played COLUMN (engine seeds at `endStep`, the counter at 0 then wraps — a visible
display/mask choice); and the call-site/test count touching these members directly (slices 2–3 may be
wider than they look). Resolver owns STATE + the read law, never policy — else it becomes a second engine.
