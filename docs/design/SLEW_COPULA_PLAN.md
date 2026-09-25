# Plan: `SlewedDraw` — stateless, distribution-preserving slew (proof of concept)

## Goal

Build a standalone, header-only C++17 class that demonstrates the new slew design, plus a unit-test suite and a small demo. **No integration into any module in this pass.**

Slew is meant to control how far the next draw can move from the last one, subject to three constraints:

- **Stateless / reversible.** The output for step `n` is a pure function of `n`. Stepping forward, backward, scrubbing and jumping must all give identical results.
- **Distribution-preserving.** The lane's note distribution must be exactly unchanged at every slew setting.
- **Controlled motion.** Slew sets the serial correlation between consecutive draws.

## Design (source of truth for the tests)

Let `u(n)` be the existing per-step uniform draw. It is a black box: whatever the Philox pipeline currently produces (including Philox-keyed-by-Philox). The only assumption is that it is a pure function of `n`.

```
if r == 0:  return u(n)                                   // legacy, bit-identical
z(n) = sum_{j=0}^{K-1} w_j(r) * PhiInv(u(n - j))
return Phi(z(n))                                           // exactly Uniform(0,1)
```

**Weights.** `w_j ∝ r^j` for `j < K`, normalized so that `sum w_j^2 = 1`. Because z is then a unit-norm sum of i.i.d. N(0,1) variables, z(n) is exactly N(0,1), the output is exactly uniform, and the lane's existing quantile reproduces its note weights exactly.

**Motion.** The lag-1 correlation of z is `rho1(r) = sum_j w_j * w_{j+1}`, computed analytically and exposed by the class. The correlation at lag ≥ K is exactly 0.

**Parameters.**
- `K = 64` (`constexpr`).
- `r` is clamped to `[0, 0.97]`. A non-finite `r` is treated as 0.

**Optional period `L`.** When `L > 0`, index the draws as `u(((n - j) mod L + L) mod L)`, using positive modulo. The output is then exactly periodic, and the window wraps smoothly across the loop point.

**Phi and PhiInv.**
- `Phi(z) = 0.5 * erfc(-z / sqrt(2))`.
- `PhiInv` uses Wichura AS241 in double precision.
- Clamp u to `[2^-53, 1 - 2^-53]` before `PhiInv`.

## API sketch

```cpp
namespace dotmod {
double Phi(double z);
double PhiInv(double u);

class SlewedDraw {
public:
    static constexpr int K = 64;
    static constexpr double kRMax = 0.97;
    void   setSlew(double r);           // clamp + precompute weights
    void   setPeriod(int64_t L);        // 0 = no period
    double lag1() const;                // analytic rho1
    const std::array<double, K>& weights() const;
    template <class U> double operator()(int64_t n, U&& u) const; // u: int64_t -> double
};
}
```

The class must have no mutable state that affects output, and no caching in v1.

## Steps for Claude Code

0. **Recon first, write nothing.** On `feat/microtonal`, locate:
   - the Philox implementation;
   - the current per-step draw path (how the counter is formed, and which output word and bits-to-uniform conversion are used);
   - the existing test harness and its conventions.

   Report back, and flag it if the counter is a draw count rather than a step index.
1. **Branch.** Create `feat/slew-copula` off `feat/microtonal`.
2. **`Phi` / `PhiInv`.** Implement them with their tests, then commit (`feat(engine): add Phi/PhiInv ...`).
3. **`SlewedDraw`.** Implement the class with its tests, then commit.
4. **Demo.** Write `slew_demo`: for r ∈ {0, 0.5, 0.9, 0.97}, print a 64-step note sequence using a 7-note weighted scale, the note histogram, and the empirical versus analytic lag-1 correlation. Add a CSV output option for plotting. Commit.
5. **Report.** Give test results, ns per call at K = 64, and any deviations from this plan. Stop there.

Constraints:
- Plain `g++` / MinGW, C++17, and no Rack includes.
- In tests, `u(n)` should use the repo's Philox if it builds standalone; otherwise use a clearly marked pure-hash test double.
- Use conventional commit prefixes with detailed bodies.

## Tests

Write each test once, from the design above. If a test fails, investigate the source truth. Do not loosen a threshold without a written justification.

1. **Phi / PhiInv reference values.**
   - Φ(0) = 0.5.
   - Φ(1.959963984540054) = 0.975, and PhiInv(0.975) = 1.959963984540054 to within 1e-14.
   - PhiInv(1e-10) = -6.361340902404056 to within 1e-12.
   - Round trip `|Phi(PhiInv(u)) - u| ≤ 1e-14 * max(u, 1-u)` on a grid that includes the tails.
   - Both functions are strictly monotone on the grid.
2. **Weights.**
   - `sum w^2 = 1` to within 1e-15 for r on a grid.
   - At r = 0, w = [1, 0, …].
   - `lag1()` is strictly increasing in r.
   - Clamping works: r < 0 gives 0, r > kRMax gives kRMax, and NaN gives 0.
3. **Legacy identity.** At r = 0, the output is **bit-identical** to `u(n)` for n in [-1000, 100000].
4. **Reversibility.**
   - Evaluating forward (0..N), backward (N..0) and in a shuffled order gives bitwise-equal results.
   - Two independent instances give equal results.
   - Large n near 2^62 and negative n both work.
5. **Distribution preservation.** For r ∈ {0, 0.3, 0.6, 0.9, 0.97}:
   - Run 10^6 steps and **thin by K**. MA(K) samples at least K apart are exactly independent, so a standard KS test at α = 0.01 is valid.
   - Run a chi-square test on 100 bins.
   - Map through a lumpy 7-note weighted quantile and chi-square the note histogram against the weights.
6. **Motion.**
   - The empirical lag-1 correlation of `PhiInv(output)` matches `lag1()` to within 4 standard errors.
   - The empirical lag-K correlation is ≈ 0.
   - The mean |Δnote| is non-increasing in r across the grid.
7. **Periodicity.** With L set, `out(n) == out(n + L)` bitwise, and there is no discontinuity in the lag-1 statistic at the loop seam.

## Open decisions (leave as-is, flag in report)

- Whether K = 64 and rMax = 0.97 are the right values. Truncation makes ρ1 < r near the top of the range.
- How the knob maps to r: linear in r, or inverted so the knob sets the target ρ1 directly.
- Whether the lane's pitch ordering in the quantile gives the perceived motion we want.
