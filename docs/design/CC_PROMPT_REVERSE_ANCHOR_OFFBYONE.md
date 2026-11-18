# CC PROMPT — reverse voice/lane anchors one cell left of rotation (off-by-one)

## The bug (verified against code)
In cycle mode the rotation-anchored read is `wnow = (laneTickFor(vd, phrasePos+1, L) + R)
mod L`. At global step 1 (`phrasePos=0`):
- Forward: `laneTickFor(Forward,1,L)=0` → `wnow=R` ✓
- **Reverse: `laneTickFor(Reverse,1,L)=−1` → wraps to `L−1` → `wnow=(L−1+R) mod L = R−1`** ✗

So a **reversed** voice/lane sits **one cell left of rotation** at global step 1 and walks
`R−1, R−2, …`. This contradicts the UAS definition "R is the cell the voice is at when the
global is at its first step, *any* direction" (see
`CC_FIX_GLOBAL_BELOW_VOICE_PHASE_DESYNC.md`, Consequence 1).

Confirmed empirically (`laneTickFor(d, pp+1, 8)+0 mod 8`, R=0):
```
Forward   0 1 2 3 4 5 6 7   anchor 0 (= R)  OK
Reverse   7 6 5 4 3 2 1 0   anchor 7 (= R−1) BUG
Pendulum  0 1 2 3 4 5 6 7   anchor 0 (= R)  OK
PingPong  0 1 2 3 4 5 6 7   anchor 0 (= R)  OK
```
**Reverse only.** Pendulum and PingPong already return `0` at `t=1` and anchor at R — leave
them untouched.

## Target behaviour (Rodney, confirmed)
Reverse anchors at **R** at global step 1 and walks backward **`R, R−1, R−2, …`**:
- `R=0, G=4, L=8` → **`0,7,6,5`** (currently `7,6,5,4`)
- `R=0, G=8, L=8` → **`0,7,6,5,4,3,2,1`** (currently `7,6,5,4,3,2,1,0`)

## The fix
At the **rotation-anchored READ sites** — the cycle read in `voiceFrameTickAt` (CYCLE
branch), the **abs1** read, and the **probability-lane** reverse reads — apply a
reverse-only correction so the anchor lands at R: `… + R + 1` for Reverse, i.e. the reverse
walk starts at `0` (matching Forward's `0`) before `+ R`.

- **Prefer ONE small helper**, not scattered `+1`s — e.g. an `anchorBias(dir)` returning
  `{Forward:0, Reverse:+1, Pendulum:0, PingPong:0}`, applied uniformly at every
  rotation-anchored read. This is CA5-adjacent (the same law lives in several read sites);
  a single helper keeps them from drifting.
- **Apply it to BOTH twins at every tier — all ten sites.** The sign-law analysis settles
  this: the bias goes on `wnow` AND its `wprev` twin at each tier (macro frame `:1297`/`:1303`,
  voice frame `:1406`/`:1409`), on `laneRecomp`'s `cur`/`prv` (`:1426`/`:1431`), on the two
  `voiceFrameTickAt` seeds (`:883`, `:899`), and on the `macroLaneDisplay` display twin. The
  seeds must carry the same bias as the reads so the raw-advance phase stays consistently
  anchored; the display twin so the Rack visual moves with the audio, not one cell behind it.
- **The bias goes INSIDE the window fold, `(laneTickFor(d,t,L) + rho + anchorBias(d)) % L`,
  feeding `frameReadCell(wnow, …)` — it is NOT the pre-existing `wnow + 1`.** That `+1` at
  `:885`/`:901`/`:1407` is the 1-based driver representative, a different quantity. If the
  anchor bias is added as a second *post-mod* `+1` on the driver, the edge/sign cue moves but
  the READ cell stays at R−1 — the fix lands half-wrong (sign fixed, cell not). Every one of
  the ten edits must be pre-mod.
- **Keep the `(dd > 1 || dd < −1)` guard at every biased edge site** (`:1306`, `:1414`). The
  bias cancels in the difference **only in the no-wrap regime** (`|dd| ≤ 1`, same constant on
  both operands before `% L`). At the wrap step — the re-anchor edge — the cue stays exact
  because the guard discards the literal `dd` and substitutes `signMotion · dirSign`
  (bias-independent), and a +1 ring-translation relocates the wrap without shrinking its
  ±(L−1) magnitude, so the guard still trips. `laneRecomp`'s sign cancels algebraically
  (its `delta` is pre-mod). Do not "simplify" the guard away thinking the bias made it
  redundant — that silently breaks the reverse re-anchor sign.
- **Do NOT modify `laneTickFor`.** `laneTickFor(Reverse)=−t` is the time-symmetric primitive
  the bounce walks and the STEPS_32 goldens depend on; the comment says so. The correction
  belongs where rotation is applied, not in the walk primitive.
- **Do NOT touch Pendulum/PingPong** — they already anchor at R (verified above), and
  `anchorBias` returns 0 for them, so their twins are byte-unchanged.

## Tests (reproduce-first)
Add a headless test (register in `run_all.sh`) that FAILS before, PASSES after:
- Reverse voice read at `phrasePos=0` equals **R** for several `R` (e.g. 0,2,6), both
  windowed (`G<L`) and full (`G=L`); and the full sequence matches `R, R−1, R−2, …`
  (`R=0,G=4,L=8 → 0,7,6,5`).
- Same for the **abs1** reverse read and a **probability-lane** reverse read (the correction
  reached every site).
- **Regression (collateral):** Forward, Pendulum, PingPong still anchor at **R** at
  `phrasePos=0` (unchanged) — guards that the fix is Reverse-only.
- **Regression (bounces byte-identical):** assert the Pendulum and PingPong **full
  two-period walks** are bit-for-bit identical pre/post fix (dump the whole walk, not just
  `t=1`). This exercises the `anchorBias=0` path and proves the ten-site edit touched nothing
  the bounces consume. Reference walks (L=8, R=0): Pendulum
  `0,1,2,3,4,5,6,7,6,5,4,3,2,1,0,1`; PingPong `0,1,2,3,4,5,6,7,7,6,5,4,3,2,1,0`.
- **Regression (sign-law, the load-bearing one):** for a reverse re-anchored read, assert
  **both** that the read cell moves `R−1 → R` **and** that the per-step sign/edge cue is
  identical to pre-fix at **every** step, the wrap step included. Capture the pre-fix sign
  sequence as the fixture. This is what actually tests the guard-substitution argument rather
  than trusting it — if a biased site dropped its `|dd|>1` guard, this row fails at the wrap
  step while the cell-position assertions still pass.

## Goldens
Check whether any STEPS_32 fixture (`test_stage2b_pregates` / `test_stage5_combined` and
their `steps32_gate1_cap16_ref.txt`) drives a **reverse** voice/lane direction. If so, the
read changes for those rows — **recompute the golden to the new (correct) reverse**, do not
revert. Forward/bounce fixtures are unaffected (`laneTickFor` unchanged). If no fixture uses
reverse, the goldens don't move.

## Scope
Independent of the landed cycle fix (which ran Forward, so never exercised this) and of the
abs1/mask ring prompt. Branch off `lane-expander-refactor`, PR into it. "Needs Rack
feel-test" for the reversed-pattern feel once headless-green.

## Done when
`run_all.sh` green with the reverse-anchor assertions added (reverse reads `R` at step 1,
sequence `R,R−1,…`); Forward/Pendulum/PingPong unchanged; `laneTickFor` untouched; any
reverse-driven golden recomputed, not reverted.
