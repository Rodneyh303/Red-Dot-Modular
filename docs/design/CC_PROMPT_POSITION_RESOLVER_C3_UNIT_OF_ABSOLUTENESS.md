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
   on revert). FAILS before (stuck at the drifted phase), PASSES after.
2. **Reverse byte-identity** — `test_reverse_rotation_anchor`, `test_phase_rotation_anchor` green and
   unchanged (the reverse view reproduces `col_`'s retrace).
3. **March goldens** (GOLDEN_A/B/BS, UA_NONDIV) BYTE-IDENTICAL — the forward change is a no-op off the
   re-park path.
4. `bash test/run_all.sh` all green; `test_16_triage` grep-pin green.
5. **Rack feel-test (Rodney)** — the dice/slew redraw visibly lands at the window start and tracks the
   offset, reverting cleanly to zero.

## Deliverable

The PositionResolver change (column as a tick-based view with an entry-frame anchor; the forward edge
on `phaseClock`; the `setWindow` forward re-origin dropped) + the reproduce-first forward test, with
the reverse goldens as the byte-identity proof and the forward drift-fix as the single intended
behavioural delta. Branch off `master`, PR into `master`.
