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
- **Do NOT modify `laneTickFor`.** `laneTickFor(Reverse)=−t` is the time-symmetric primitive
  the bounce walks and the STEPS_32 goldens depend on; the comment says so. The correction
  belongs where rotation is applied, not in the walk primitive.
- **Do NOT touch Pendulum/PingPong** — they already anchor at R (verified above).

## Tests (reproduce-first)
Add a headless test (register in `run_all.sh`) that FAILS before, PASSES after:
- Reverse voice read at `phrasePos=0` equals **R** for several `R` (e.g. 0,2,6), both
  windowed (`G<L`) and full (`G=L`); and the full sequence matches `R, R−1, R−2, …`
  (`R=0,G=4,L=8 → 0,7,6,5`).
- Same for the **abs1** reverse read and a **probability-lane** reverse read (the correction
  reached every site).
- **Regression:** Forward, Pendulum, PingPong still anchor at **R** at `phrasePos=0`
  (unchanged) — guards that the fix is Reverse-only.

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
