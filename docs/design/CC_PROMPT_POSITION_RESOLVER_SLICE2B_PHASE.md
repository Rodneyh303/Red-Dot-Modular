# CC PROMPT — Position Resolver SLICE 2b: fix the PHASE-mode regression (direction from the phase sweep)

Follow-on to slice 2 (`be1fc3b5`, CC_PROMPT_POSITION_RESOLVER_SLICE2_DERIVE). Slice 2 made the
derive read the resolver's SIGNED phrase coordinate and fixed gate/clock reverse rotation — but
it REGRESSED phase mode (Mode E). This slice fixes phase. It lands on the SAME branch,
`fix/position-resolver-slice2-derive`, BEFORE that branch merges into `lane-expander-refactor`
(slice 2 must not merge while phase is broken).

## CARDINAL RULE (non-negotiable)
VERIFY AGAINST CODE AND TESTS, never a plausible narrative. Prove the fix with a test that FAILS
before and PASSES after. You CANNOT verify Rack/behavioural things (no Rack here) — give yourself
a TESTED, MECHANICAL oracle; Rodney feel-tests in Rack with the manual phase knob. When in doubt,
read the code.

## THE BUG — ROOT CAUSE (verified in code)
Slice 2 changed `deriveHolders`'s cycle branches to
`ft0 = sPos = (pos_.direction() > 0) ? +pos_.phrasePos() : -pos_.phrasePos()`.
That is correct for GATE/CLOCK, where `pos_.advance(dir)` maintains BOTH the column and the
direction. But PHASE mode (Mode E) does not advance — it SNAPS:
`snapToPhaseStep` (SequencerEngine.cpp ~:1255) → `pos_.snapTo(stepIndex)`. And `snapTo` is:

    void snapTo(int col) { col_ = col; }      // PositionResolver.hpp ~:97 — sets the COLUMN ONLY

It never sets `dir_`, and `col_` is the already-windowed `phaseStep`, not the raw phrase
position. `phrasePos()` is defined off `dir_`
(`(dir_>0) ? wrapRing(col_-startStep_) : wrapRing(endStep_-col_)`), so in phase mode:
- **direction is stale** — whatever the last gate/clock `advance` left (or `+1` from reset),
  NOT the phase sweep → decreasing the knob "jumps forward" instead of reversing;
- **phrasePos ≠ wG** — the old derive used `ft0 = wG` (`globalWalk_`, which `anchorTransport`
  maintains correctly in EVERY mode); `pos_.phrasePos()` off the snapped windowed column differs
  by the window rotation → the voice lane sits one step past R at phase 0.

Pre-slice-2 `ft0 = wG` was correct in phase (confirmed: lane-expander-refactor :1044/:1145). The
identity harness (166/166) and the reverse oracle only exercise `advancePlayhead`; the `snapTo`
(phase) path had NO coverage, which is why this slipped through.

## THE SPEC (Rodney — the correct phase behaviour)
The voice lane playhead in phase mode:
- snaps to ROTATION **R at BOTH extremes**, phase 0% AND 100% (both are phrase entries — the
  0% entry and one full cycle later);
- as phase advances FORWARD, advances FORWARD from R (R, R+1, … through the window);
- as phase advances BACKWARD, advances BACKWARD from R (R, R−1, …);
- the **jump back to rotation comes LAST** in whichever direction is being played (the
  phrase-boundary wrap at the far end of the sweep).
This is EXACTLY slice 2's signed rule — forward enters at R and counts up; reverse enters at R
and counts down; `phrasePos == 0` is R in both directions — now driven by the phase sweep rather
than tick advances. The law in `deriveHolders` is already right; only its INPUTS are wrong in
phase.

KEY CONSTRAINT (Rodney): **phase direction is known only between two consecutive PPQN
quantisations** — i.e. per snap, as the sign of the change since the previous snap. Do not try to
infer it from the column alone (the ring wrap makes a column delta ambiguous).

## THE FIX (fix 1 — pos_ stays the ONE authority in every mode)
Feed `pos_` the phase sweep direction and a `phrasePos` that reproduces `wG`:
1. **Direction from the sweep.** `snapToPhaseStep` already computes `absStep` — UNWRAPPED and
   "grows across span wraps" (its own comment). Track the previous `absStep`; the sweep
   direction is `sign(absStep − prevAbsStep)` (hold the LAST direction when the delta is 0 — no
   PPQN crossing, direction unknown; at the extremes both ends anchor R, so a reversal there is
   harmless). Feed it to `pos_` — add `snapTo(int col, int dir)` (or a `setDirection(int)` called
   right before `snapTo`); set `dir_` to the sweep sign. Keep the existing `snapTo(col)` for any
   caller that must not change direction, or route them through the new form explicitly.
2. **phrasePos reproduces wG (forward bit-identity).** With `dir_` correct, `sPos` must equal the
   old `wG` on a FORWARD sweep — bit-identical to pre-slice-2 phase. Reconcile the snapped column
   so `pos_.phrasePos()` (forward `col_−startStep_`) equals the phrase position `anchorTransport`
   seeds as `globalWalk_`. If `phaseStep − startStep` already equals `wG`, the only fix needed is
   (1) and the off-by-one was the stale reverse direction; if it does NOT, make the snap carry
   the raw phrase position so `phrasePos == wG`. The reproduce-first test tells you which — do not
   assume.

Result: `deriveHolders`'s existing `sPos` is correct in phase — forward counts up from R
(bit-identical to before), backward counts down from R, both anchored at R, the wrap-to-R last.

## REPRODUCE-FIRST ORACLE (NEW — test/test_phase_rotation_anchor.cpp, register $SE $GS $PE $CE)
Drive the phase path directly (`snapToPhaseStep(pulsePos, PULSES_PER_STEP)` across a sweep; read
the voice/macro frame the way `test_reverse_rotation_anchor` reads `macroFrameRead()`), with a
dialed voice LOR (a non-dividing L, e.g. Rodney's {len 8, off 4, L 7, R 4} and the full-ring
default). Gates — these FAIL on the current branch (off-by-one at 0% + the stale-direction jump)
and PASS after:
1. **Both extremes = R**: at phase 0% (absStep 0) AND 100% (absStep == lenG) the voice read == R
   (= O + R%L, ring-wrapped — the same R the reverse oracle pins).
2. **Forward sweep counts up from R**: sweeping absStep 0→lenG forward, the read walks R, R+1, …
   in the window and never leaves it; the jump back to R lands at the far extreme.
3. **Backward sweep counts down from R**: sweeping lenG→0 backward (feed decreasing absStep so
   the per-snap direction is −1), the read walks R, R−1, …; the jump back to R lands last.
4. **Forward phase BIT-IDENTICAL**: capture the forward-sweep reads on pre-slice-2 `ft0 = wG`
   (or assert they equal the wG-fold directly) — the forward phase path is unchanged.

## INVARIANTS (the contract)
- Gate/clock reverse unaffected: `test_reverse_rotation_anchor` stays GREEN.
- Forward phase BIT-IDENTICAL (gate 4) — slice 2 claimed forward bit-identity; it must now hold
  on the snap path too.
- Goldens BIT-IDENTICAL (`GOLDEN_A/B/BS`, `UA_NONDIV`); `test_position_resolver_identity` stays
  166/166. Full headless battery green (`bash test/run_all.sh`), anchor/bind audit, array-guard.

## NON-SCOPE (do NOT touch)
- The gate/clock derive coordinate (slice 2's reverse fix) — only the PHASE path feeds `pos_`
  differently; `deriveHolders`'s `sPos` law stays as slice 2 wrote it.
- Migration step 3 (other `stepIndex` consumers → `playedColumn`), step 4 (field deletion),
  the cadence slice (C1/C2/C4), counter removal / C3, the drive-direction model plumbing
  (this fix is a concrete instance of "phase direction comes from the input" — implement only
  the phase-snap direction, nothing broader).

## DELIVERABLE
The `snapTo`-with-direction plumbing in `snapToPhaseStep` (+ the `phrasePos == wG` reconciliation
if gate 4 needs it), the new `test_phase_rotation_anchor` registered and green, the full battery
green with goldens and the reverse oracle bit-identical. In the PR: state whether the off-by-one
was direction-only or also needed the phrasePos reconciliation (what gate 4 showed).
