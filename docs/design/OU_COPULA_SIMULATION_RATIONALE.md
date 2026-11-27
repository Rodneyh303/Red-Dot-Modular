# The copula engine's process identity, and why it is not an Euler scheme

Status: design rationale, written alongside PR #388 (`feat/bridge-per-cell-xi`).
Audience: future maintainers (and future us) asking "why is the OU process simulated
this strange way?" — because from first principles one would start with an Euler
scheme or the exact AR(1) recursion, and the reason we did neither is the
instrument's defining requirement, not numerical taste.

## What the process is (defensible names)

Per stream, per cell (step × lane × voice), the scrub position axis carries a
latent stationary Gaussian process pushed through Φ to a uniform — a Gaussian
copula process. Its temporal kernel, with r = slewKnobToR(slew):

- **ρ > 0: an Ornstein–Uhlenbeck Gaussian copula process.** Autocorrelation
  |r|^τ (exponential; θ = −ln r). Integer skeleton: sampled OU up to the K-tap
  truncation below. Fractional infill: the *exact* OU bridge conditional
  (scalar OU is Markov, so conditioning on the bracketing pair IS the full
  disintegration).
- **ρ < 0: the oscillatory extension** — the real part of a two-component
  (complex) OU rotating at phase π per step; autocorrelation |r|^τ·cos(πτ).
  Markov only in the 2-D state (in-phase + quadrature); the observed scalar is
  NOT Markov (Doob: a stationary scalar Gaussian Markov process must have pure
  exponential autocorrelation). Our infill conditions on the bracketing pair
  only: exact single-time marginal and exact covariance to the two neighbours,
  but deliberately not the full conditional given the whole skeleton (farther
  integer points carry residual information about the hidden quadrature phase).
  Avoid the phrase "2-state Markov" — it reads as a discrete flip-flop chain.
- **Innovation sharing is the #387 / #388 A/B.** Same marginal process in both:
  on `feat/scrub32-bidir` (#387) all cells share one bridge innovation per
  interval pair — an exchangeable one-factor coupling (cross-cell corr
  v = sqrtV², = (1−r)/(1+r) at frac ½) that exists only at fractional
  positions; on `feat/bridge-per-cell-xi` (#388) the pattern is a **field of
  independent copies**, ξ keyed per cell. Which is more musical is decided by
  ear, not by this document.

## The first-principles route, and why it fails here

Doing this fresh, the natural simulators are:

1. **Euler–Maruyama** on dX = −θX dt + σ dW — approximate, needs a step grid;
2. **exact AR(1) recursion** x_{n+1} = r·x_n + √(1−r²)·ε_n — exact and cheaper
   than what we built.

Both are **recursive**: state depends on history. That is fatal to the
instrument, whose defining requirements on the scrub/jog axis are:

- **Random access**: evaluate any position N cold, without replaying a path;
- **Time reversal**: scrub backwards as cheaply as forwards — a recursion has
  a preferred direction, and reversing it means either storing the path or
  re-deriving it;
- **Bitwise determinism**: two engines (or one engine revisiting) must agree
  exactly from (seed, position) alone — the crab-canon law (test_scrub_bidir
  G1: equal n−f ⇒ bitwise-equal buffers) cannot be stated for a recursion
  without "and identical replay history".

An Euler scheme is not wrong mathematics; it answers a different question
("integrate this path forward") than the one the instrument asks ("what is the
pattern *at* this position, approached from anywhere").

## What we built instead

- **Integer skeleton = the truncated Wold representation.** AR(1)'s causal
  MA(∞) form, truncated at K taps and **L2-renormalised**
  (`MovingAverageCopula::weightsK`). The renormalisation puts the entire
  truncation error into the correlation structure (vanishing like r^K) while
  keeping the marginal *exactly* N(0,1) at every position — the right trade for
  a copula, whose uniformity is what the musical layer consumes. Because the
  representation is a pure function of a window of **addressable** iid Philox
  draws, every position is O(K) cold, bidirectional, and reproducible from
  (seed, position). The crab-canon law is the Wold form's addressability.
  (Runtime depth K is a voiced parameter, 1..64 — at small K the skeleton's
  correlation is the truncated kernel, by design; the marginal stays exact.)
- **Fractional infill = exact conditional simulation** between skeleton points
  (`OUBridgeCoeffs`): 2×2 normal equations against ρA = |r|^f·cos(ωf),
  ρB = |r|^{1−f}·cos(ω(1−f)), ω = π for r < 0. Unit marginal at every frac;
  sqrtV = 0 at integers (no innovation drawn there — integer positions are
  bit-identical to the pre-bridge engine). Continuous position at constant
  cost, no finer lattice.
- **Caches change when you pay, never what you get.** The draw cache spans the
  scrub budget because sumZ needs K draws per window at every reachable
  position; the bridge ξ memo is single-pair (inside `ZWindowCache`, same key)
  because only the pair you stand in is ever consumed, and a pair crossing is
  already the expensive z-miss. Philox is the archive: any revisited position
  replays identical values (test_bridge_per_cell X2/X3).

## One-line summary for papers/manual

> An OU Gaussian copula process simulated via its truncated Wold
> representation (variance-exact; correlation error O(r^K)) with exact
> OU-bridge infill — chosen over recursive (Euler/AR(1)) simulation because
> the instrument requires random access: any position, either direction,
> bitwise-deterministic. For ρ < 0, the oscillatory (complex-OU real part)
> extension with bracketing-pair infill.

## Where the claims are pinned

- Crab-canon / addressability: `test_scrub_bidir` G1 (routes, signs, reverse rolls).
- Exact marginals at any frac: `test_scrub_bidir` G6, `test_scrub_depth` D5 (KS, family-wise 1%).
- Integer positions untouched by the bridge: sqrtV==0 path; `test_bridge_per_cell` X4.
- Revisit determinism / cache-shape irrelevance: `test_bridge_per_cell` X2, X3.
- Shared vs per-cell innovation (the A/B discriminator): `test_bridge_per_cell` X1
  (fails on #387 with corr ≈ v; passes on #388 with corr ≈ 0).
- Truncation/renormalisation: `MovingAverageCopula::weightsK` + `test_scrub_depth` D1–D4.
