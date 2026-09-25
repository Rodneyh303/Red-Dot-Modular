# Distribution rework — master plan (slew, scrub/AB mix, spread)

ONE branch off master, PHASED. Each phase is its own commit, suite green and a LISTEN before the
next. Do not bundle: these change generated output, so a bad phase must be bisectable.

## Read first (in this order)
1. `UNIFORM_MARGINALS_COPULA_PLAN.md` — WHY. Three stages silently change the data distribution;
   it was never the intention. Contains the code-accurate description of SCRUB vs SLEW.
2. `SLEW_COPULA_PLAN.md` — slew implementation, revised for the CARRIED-STATE chain.
3. `SPREAD_TARGET_MODES.md` — spread semantics: pin says WHERE, spread says HOW MUCH, sign says
   opposition; the follow-CA no-op discovery and its fix.
4. `DICE_SCRUB_SLEW_B2.md` — the constraints that must survive: counter-addressable, no accumulated
   state, NO origin truncation (always read the full window into negative counters), reverse == forward.
Background only, NOT a spec for this work: `CA_DICE_COUNTER_MODEL.md` (dice counters / true reverse)
— read only for the reversal contract.

## Already done on master — do not re-implement
`src/dsp/GaussianCopula.hpp` (Phi, PhiInv, combine, mix2), `src/dsp/MovingAverageCopula.hpp`
(K=64, R_MAX=0.97, geometric L2-normalised weights, analytic lagCorr), and
`test/test_GaussianCopula.cpp` (in run_all.sh, green).

## Phase 0 — recon, write nothing
Report: how `rhythmDrawCtr`/`melodyDrawCtr`/`qmixDrawCtr` advance; exactly where the pre-remap draw
is available relative to the CA pin remap (`PatternEngine.hpp:139` says pins remap the slewed buffers
PRE-spread); and where per-voice spread amounts enter (`SpreadInterp`, `MonsoonSandsManager`).
Flag anything contradicting the docs. CHECK THE CODE, NOT THE DOCS.

## Phase 1 — slew readout in normal space
The existing `patternRhythmAt/MelodyAt/QmixAt` already have the right SHAPE: a weighted sum per STEP
INDEX across DRAWS (a "draw" is a whole 16-step pattern, so lag 1 IS draw-to-draw — this is the
intent). Change only the summation:
- linear `SUM w_j·u` normalised by `SUM w` → normal-space `SUM w_j·PhiInv(u)` normalised by
  `SQRT(SUM w^2)`, then one `Phi`. Use `MovingAverageCopula`.
- `r == 0` must stay BIT-IDENTICAL to the legacy draw, asserted through the engine path.
- Carried-state window (head + tail, `step(±1)` via the Philox bijection). **Recompute the full
  K-term sum every step — NEVER a running sum**, or reversal stops being bit-exact.
- Cache `PhiInv(u)` beside each carried draw (pure function of the draw ⇒ reversal-neutral). Without
  it this is ~16 steps x 64 taps of PhiInv per stream per draw.
- `K` and `R_MAX` are ONE decision: truncation is negligible while `K >= 2/(1-R_MAX)`. Keep K=64 /
  R_MAX=0.97 for now; if the top of the travel feels not-still-enough, move to K=128 / R_MAX=0.99
  TOGETHER. (Achievable lag-1 saturates at exactly (K-1)/K, so R_MAX alone just makes a dead zone.)
- Slew stays PER STREAM (R/M/Q), never per voice — see Phase 3 note.

## Phase 2 — SCRUB (the "A/B mix" knob) in normal space
Scrub spans 6 positions (`s = mix*6`) and interpolates two ADJACENT windows. Do that interpolation
on the two `z` values in NORMAL space and apply `Phi` ONCE at the end — interpolating the two uniform
outputs reintroduces a linear blend of uniforms at exactly the point we just fixed.
Note SCRUB_K (6, the scrub span) is NOT the copula K (64, the memory window). Do not conflate.

## Phase 3 — spread
Per `SPREAD_TARGET_MODES.md`:
- Replace the convex blend with `copula::mix2(own, leader, rho)`. The `1-p` mirror special case
  DISAPPEARS (rho = -1 gives exactly `1 - leader`).
- The knob becomes rho directly (today `k(a)=a/sqrt((1-a)^2+a^2)` — 0.5 reads 0.71).
- **follow-CA needs the voice's OWN PRE-REMAP draw kept available** at the spread stage; without it
  the mode is a NO-OP (the remap has already replaced the value with the leader's). One extra
  buffer. Polarity: `0` = own draw, `+1` = leader, `-1` = complement of leader.
- Per-lane mode (anchor V1 / follow CA) on Monsoon's context menu as the SINGLE SOURCE OF TRUTH
  mirrored to the engine — never per-visual flags (that is what diverged before). 5 rows incl. QMIX.
- On enabling follow-CA for a lane, initialise that lane's spread amounts to FULL, or pins appear to
  do nothing (open item in that doc).
- **Do NOT make slew per voice.** Cross-voice correlation after filtering is `rho x dot(w_A, w_B)`;
  identical weights give exactly rho, differing weights can only ATTENUATE it (measured: rho=0.7
  with r 0.8 vs 0.3 lands at 0.53). Shared r per stream is what keeps spread's rho truthful.

## Phase 4 — UI truth
Labels/tooltips: slew = "correlation with the previous draw"; spread = rho with landmarks
`oppose (-1) / independent (0) / follow (+1)`. Remove any text implying the old semantics.

## Tests (add with the phase that needs them)
- `r = 0` legacy BIT-IDENTITY through the engine path.
- Reversibility: random ± walks return BITWISE, including with r varying per step; forward vs
  backward-to-the-same-position identical; negative counters work.
- Distribution preserved: **thin by >= K** (samples K apart share no source draws, so they are
  exactly independent), then KS at 1% and a 100-bin chi-square; also through a lumpy weighted
  quantile. **NEVER loosen a KS threshold to make it pass** — check the VARIANCE first: near 1/12
  means the marginal is fine and you need thinning; well below 1/12 means real concentration.
- Motion: empirical lag-m == `MovingAverageCopula::lagCorr`; lag >= K is 0.
- Spread: measured pairwise correlation matches the rho knob; marginals unchanged as rho varies.

## Expected audible change (for the listen)
Biggest at LOW slew, where today's linear average of the window runs at ~11% of the correct variance
— patterns should regain contrast without changing their shape or timing. Mid-spread should stop
washing out. Nothing should change at r = 0 or at spread 0.
