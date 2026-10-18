# CC PROMPT 02 — Phase 3 step 1: per-voice warp-F bank (variant, identity-default) + tests

PREREQUISITE: CC_PROMPT_01 (per-voice send-tap) is merged and green (it is). This does NOT
depend on the tap code; it's the next build step and assumes a clean trunk.

## READ FIRST
1. `CLAUDE.md` (root) — auto-context + non-negotiables.
2. `docs/design/CHAT_HANDOFF.md` — primer + working model + current state.
3. `docs/design/SESSION_LOG_2026-10-18b.md` §1 — the ownership reasoning (POD-vs-polymorphic,
   why not unique_ptr). NOTE: the design BELOW SUPERSEDES the POD-switch-plus-distmath scheme
   described there — later discussion (this prompt) simplified it to a value-variant with an
   identity default. Read §1 for the WHY (allocation on the control path is the thing to avoid);
   trust THIS prompt for the HOW.
4. `src/dsp/Distribution.hpp` — the existing polymorphic palette (8 concrete types).

Branch: off `lane-expander-refactor`, `git checkout -b feat/warp-f-bank`. PR into it.
Rules: verify against tests, red→green for any claim, header-only tests where possible, you
can't test Rack.

## GOAL
Give the engine a per-voice-per-lane "warp F" bank that is: value-type (no heap on the
modulation/draw path), trivially copyable, persistable, and SINGLE-IMPLEMENTATION (no duplicated
math). The default is IDENTITY (= uniform / no warp), so the mix formula is BRANCHLESS.

## THE DESIGN (converged — this is the authority)

### Why a variant, not a POD-switch, not unique_ptr
- unique_ptr<Distribution> per slot → allocates when a curve is chosen, and breaks the flat,
  copyable, memcpy-persistable house pattern. Rejected.
- A POD {kind; params[4]} + a switch(kind) → needs the math in TWO places (the switch AND the
  Distribution subclasses) → divergence risk (the lying-abstraction failure). Rejected — it was
  the earlier plan; the variant removes the duplication entirely.
- std::variant<...> held BY VALUE → value-type (no heap for the parametric types), trivially
  copyable, sits in a flat array like spread[112], and there is ONE implementation per curve
  (the concrete type's own invCDF). No switch, no parallel math, nothing to keep in sync.

### Cadence makes this safe (verified this session)
Warp is evaluated in the MIRROR, rebuilt on a DIRTY event (curve/param edit, new draw, scrub) —
NOT per audio sample (the per-sample path reads a float out of warped_). And a voice/lane holds
ONE F evaluated at 16 steps; a full regen re-evaluates the HELD Fs, it does not reconstruct them
(regen changes uniforms, not curves). Construction happens only on a user curve edit (UI rate).
So "construct where needed" is fine — you HOLD the F and mutate its params in place on
modulation; you never `new` one per cell. Value-variant has zero heap for parametric types, so
even construction is alloc-free.

### IDENTITY is the default F — uniform is not a special case
The warp mix is ALWAYS evaluated, branchlessly:
    warped(u) = (1-w)*u + w * F.invCDF(u)
For "no warp / uniform", F must be IDENTITY (invCDF(u)=u), so the formula yields u with no
branch. Provide identity as a REAL F (the default variant alternative), NOT as an empty/monostate
slot to test for:
    struct IdentityWarp {
        double invCDF(double u) const { return u; }
        double cdf  (double x) const { return x; }
        double pdf  (double /*x*/) const { return 1.0; }
        const char* typeName() const { return "identity"; }
    };
A default-constructed bank is all-IdentityWarp = all-uniform = no warp, and the mirror rebuild
runs the SAME code over every slot. Identity is strictly increasing (slope 1) so Sklar-safety
holds for the default exactly as for any curve; w·(identity,identity)=identity; no corner cases.

### The variant
    using WarpF = std::variant<
        IdentityWarp,              // index 0 = default = uniform / no warp
        TriangularWarp, PowerWarp, KumaraswamyWarp, LogitNormalWarp,
        LogitSigmoidWarp, MobiusWarp, TruncNormalWarp, ReversedPowerWarp,  // NB reversed-power:
        DrawnWarp >;               //   added to Distribution.hpp by CC_PROMPT_P1 sub-task C
Each alternative exposes invCDF/cdf/pdf. Dispatch via a thin forwarder (std::visit or a small
switch-on-index that calls the ONE implementation each type owns — there is no second copy of
any formula, so this switch cannot diverge: it only forwards). Prefer a forwarding member:
    inline double invCDF(const WarpF& f, double u) {
        return std::visit([u](auto& d){ return d.invCDF(u); }, f);
    }
(and cdf/pdf likewise). std::visit over value types is non-allocating.

### Relationship to Distribution.hpp (single implementation, no distmath layer)
The concrete warp types ARE the single implementation. Two clean options — pick the lower-churn:
  (a) Make the WarpF alternatives THIN WRAPPERS that each hold the existing Distribution subclass
      (by value) and forward invCDF/cdf/pdf to it. Distribution.hpp is untouched; the variant is
      a value-type holder over the existing classes. LOWEST churn — do this unless it's awkward.
  (b) If the Distribution subclasses aren't cheaply value-holdable (check: they're small structs
      with double params + an inline method body — they ARE, so (a) should work), fall back to
      the subclasses being the impl and the variant holding them.
Either way there is ONE body per curve's math. The "distmath:: refactor" from the older plan is
NOT needed and should NOT be created — it only existed to de-duplicate a POD-switch that no
longer exists.

### DRAWN curves — pool index, NOT inline (verified: MonotoneCurve holds a std::vector)
MonotoneCurve stores std::vector<Point> (heap-backed, MAX_POINTS=32) → NOT trivially copyable,
must NOT sit inline in the variant (would make WarpF heap-copying + non-POD). So DrawnWarp holds
a LIGHTWEIGHT POOL INDEX (uint16_t) into a set-time MonotoneCurve pool; its invCDF/cdf/pdf
forward to pool[idx]. Modulation never touches drawn curves (no scalar param) → the pool is
set-time only, no control-path alloc. Only voices using a drawn curve consume a pool slot.

## STEPS (test-first; suite green after each)

### STEP 1 — IdentityWarp + the WarpF variant + the forwarder
Add IdentityWarp, define WarpF (identity first = default), and the invCDF/cdf/pdf forwarders.
For the parametric alternatives, use option (a): thin value-holders over the existing
Distribution subclasses. DrawnWarp holds the pool index (pool in STEP 3).

### STEP 2 — the mix + the bank
    float warpW[112] = {};            // per-voice-per-lane depth, index v*7+lane, like spread[112]
    WarpF warpF[112];                 // default-constructed = all IdentityWarp = uniform
    // effective warp of uniform u for (v,lane):
    inline double warpedValue(int v, int lane, double u) {
        const int i = v*7+lane;
        return (1.0 - warpW[i]) * u + warpW[i] * invCDF(warpF[i], u);   // BRANCHLESS
    }
getWarpF/setWarpF, getWarpW/setWarpW. Delegation (Macro default / Sands override) reuses the
owner()/topo pattern — do NOT invent a new delegation mechanism.
(No mirror / engine wiring yet — that's CC_PROMPT_03. This prompt lands the bank + mix + tests.)

### STEP 3 — the drawn-curve pool
A set-time pool of MonotoneCurve (fixed-capacity array or a vector populated only at set-time/
load). DrawnWarp.idx indexes it; assert idx < capacity. Document the capacity.

### STEP 4 — persistence
warpW[112] (trivial floats) and warpF[112] round-trip in MonsoonPersistenceManager. For each
slot persist the variant's active type tag + its params (reuse each Distribution's existing
toJson/fromJson); drawn slots persist their MonotoneCurve (reuse the .dmcurve JSON) keyed by the
pool index. Missing key / default slot ⟹ IdentityWarp + w=0 (no warp — old patches unchanged).

### STEP 5 — tests  `test/test_warp_f_bank.cpp` (header-only, model on test_distribution.cpp)
  - DEFAULT: a default-constructed bank is all IdentityWarp; warpedValue(v,lane,u)==u for all u,
    for any w (identity makes the mix u regardless of w). This is the no-warp guarantee.
  - IDENTITY EXACT: invCDF(IdentityWarp,u)==u, cdf==x, pdf==1, over a dense grid.
  - MIX ENDPOINTS: w=0 ⟹ warpedValue==u exactly (any F); w=1 ⟹ warpedValue==invCDF(F,u) exactly.
  - SINGLE-IMPL AGREEMENT: for each parametric alternative, invCDF(WarpF,u) == the corresponding
    Distribution subclass invCDF(u) over a dense grid (proves the holder forwards, no divergence).
  - MONOTONE: for fixed (v,lane,F,w), u1<=u2 ⟹ warpedValue(u1)<=warpedValue(u2), dense grid, in
    the float the bank will store (Sklar-safety; strict where the editor guarantees strict).
  - DRAWN: a DrawnWarp over a pooled MonotoneCurve matches the curve's invCDF.
  - PERSISTENCE: save→load a mixed bank (identity, a parametric, a drawn, assorted w) → all
    survive exactly; a bank with no stored key loads all-identity + w=0.
Register `"test_warp_f_bank|"` in run_all.sh TESTS[]. GREEN.

## DONE =
- IdentityWarp + WarpF variant (identity default) + invCDF/cdf/pdf forwarders; ONE impl per
  curve (thin holders over the existing Distribution subclasses); NO distmath layer, NO POD
  switch, NO unique_ptr.
- warpW[112] + warpF[112] banks in editor with delegation; branchless warpedValue mix.
- Drawn-curve pool; DrawnWarp holds a pool index (MonotoneCurve is NOT inline — it's vector-backed).
- Persistence round-trips; default = identity + w=0 (old patches unchanged).
- test_warp_f_bank green (default no-op, identity exact, mix endpoints, single-impl agreement,
  monotone, drawn, persistence). `bash test/run_all.sh` ALL GREEN.
- No mirror, no engine hot-path, no UI (CC_PROMPT_03+). PR body: paste test output.

## NOT THIS PROMPT
- The warped_ mirror / scoped dirty mask / engine hot-path wiring (CC_PROMPT_03).
- Warp composition at genPitchLive (melody/octave) + finalRandomByStrand (CC_PROMPT_03).
- The UI columns / popup editor (CC_PROMPT_04).
- Parameter (theta) modulation + w send/tap routing (later; after 03/04).
- ReversedPowerDistribution is ALREADY in Distribution.hpp (P1 / PR #287 merged — verified:
  typeName "reversed-power", fromJson dispatch, battery coverage). ReversedPowerWarp wraps it
  exactly like the other 8 — no stub needed. (KumaraswamyDistribution also gained
  abFromModeConc/modeConcFromAb from P1 — that's a UI-authoring map, NOT used by the engine bank,
  which stays (a,b). Ignore it here; it's for CC_PROMPT_04.)
