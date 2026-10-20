# SandsModifierResolver — INTERFACE SKETCH (design target, reconcile with code during extraction)

STATUS: a DESIGN PROPOSAL of the shape, grounded in the REAL SpreadResolver.hpp (read in full).
NOT code to ship verbatim — CC extracts against the actual SpreadResolver and the byte-identical
contract test is the arbiter. Purpose: validate that ONE skeleton admits all six foreseen policies
(spread, LOR, direction, warp, θ, clamp) BEFORE CC commits the skeleton — the one place a wrong
shape propagates into every policy and a byte-identical test would NOT catch it.

## WHAT SPREAD ALREADY IS (verified, the template)
SpreadResolver is already the skeleton for ONE policy. Its real shape:
  - `CvTerm { connected, unitVoltage, atten; contribution() = connected ? uV·atten·2 : 0 }`
  - `Inputs { base; delegated, macroPresent; ownCv, eastCv (CvTerm); macroBase, macroSend;
              macroPreDelta, macroPostDelta, macroTap }`
  - `tappedDelta(in) = macroPreDelta + (macroPostDelta - macroPreDelta)·macroTap`  (P9b per-voice tap)
  - `effective(in)`: `if delegated → clamp(macroBase + tappedDelta); else eff = base (+ownCv)
                      (+eastCv) (+tappedDelta·macroSend if macroPresent); clamp`
  - clamp = [-1,1]. Pure. Caller fills topology booleans; resolver never re-derives them.
The GENERALIZATION keeps ALL of this structure and parameterizes ONLY: the value TYPE, the combine
OPS (how base/cv/send combine), the CONSTRAINT (end-clamp), and per-policy EXTRA inputs.

## THE SKETCH

```cpp
// ── shared, policy-independent ──────────────────────────────────────────────
// CvTerm is IDENTICAL for every modifier (float contribution) — keep it as-is, shared.
struct CvTerm {
    bool  connected = false;
    float unitVoltage = 0.f;   // jack V / 10
    float atten       = 0.f;   // -1..1
    float contribution() const { return connected ? unitVoltage * atten : 0.f; }
    // NOTE: spread multiplies by kBipolarSpan(2). That ×2 is SPREAD-SPECIFIC (bipolar reach), so it
    // moves OUT of CvTerm into SpreadPolicy::scaleCv. CvTerm stays the raw uV·atten; the policy scales.
};

// ── the POLICY CONTRACT as a C++20 CONCEPT (the project moved to C++20 2026-10,
//    CI-green all platforms — so this is a real concept, NOT a static_assert fallback).
//    A malformed policy now fails AT THE DECLARATION with a located, readable
//    "BrokenPolicy does not satisfy SandsModifierPolicy: ..." instead of a deep
//    cryptic instantiation error inside effective(). This is the policy-layer analogue
//    of the strong-type/contract-test discipline: make an incomplete policy a BUILD
//    error, located, not a comment nobody is forced to honour (the polyBankIndex-lie
//    failure mode, one level up).
template <class P>
concept SandsModifierPolicy = requires(typename P::Value v, float f,
                                        typename P::Value mb, typename P::Extra e) {
    typename P::Value;
    typename P::Extra;
    { P::scaleCv(f) }               -> std::convertible_to<float>;
    { P::addCv(v, f) }              -> std::same_as<typename P::Value>;
    { P::addSend(v, f) }            -> std::same_as<typename P::Value>;
    { P::applyDelegated(mb, f, e) } -> std::same_as<typename P::Value>;
    { P::constrain(v, e) }          -> std::same_as<typename P::Value>;
};

// ── the skeleton: templated on a (CONSTRAINED) Policy ───────────────────────
template <SandsModifierPolicy Policy>
struct SandsModifierResolver {
    using Value = typename Policy::Value;      // float|triple|enum|(WarpF,w)|θ|(lo,hi)|(off,mask)

    struct Inputs {
        Value  base{};                         // the lane's base value for THIS modifier
        bool   delegated   = false;            // topology: ceded to Macro (never re-derived here)
        bool   macroPresent= false;
        CvTerm ownCv, eastCv;                  // shared shape; policy decides how (or whether) to fold
        Value  macroBase{};                    // Macro's value for the delegated override
        float  macroSend = 0.f;                // 0..1 send amount
        float  macroPreDelta = 0.f, macroPostDelta = 0.f, macroTap = 1.f;   // P9b per-voice tap (shared)
        typename Policy::Extra extra{};        // policy-declared EXTRA inputs (see below; empty for most)
    };

    // P9b tap lerp — SHARED, identical for every modifier (the send-tap contract is universal).
    static float tappedDelta(const Inputs& in) {
        return in.macroPreDelta + (in.macroPostDelta - in.macroPreDelta) * in.macroTap;
    }

    // THE SINGLE AUTHORITY — control flow identical to spread; ops are Policy's.
    static Value effective(const Inputs& in) {
        if (in.delegated)
            return Policy::constrain(Policy::applyDelegated(in.macroBase, tappedDelta(in), in.extra),
                                     in.extra);
        Value v = in.base;
        if (in.ownCv.connected)  v = Policy::addCv  (v, Policy::scaleCv(in.ownCv.contribution()));
        if (in.eastCv.connected) v = Policy::addCv  (v, Policy::scaleCv(in.eastCv.contribution()));
        if (in.macroPresent)     v = Policy::addSend(v, tappedDelta(in) * in.macroSend);
        return Policy::constrain(v, in.extra);
    }
};

// ── Policy trait each modifier supplies ─────────────────────────────────────
// struct SomePolicy {
//   using Value = ...;                 // the resolved value type
//   struct Extra { ... };              // per-policy extra inputs (default: struct Extra {};)
//   static float scaleCv(float raw);                       // raw uV·atten → this modifier's units
//   static Value addCv  (Value v, float cvContribution);   // fold a CV term into the running value
//   static Value addSend(Value v, float tappedSend);       // fold the Macro send into it
//   static Value applyDelegated(Value macroBase, float tappedDelta, const Extra&);  // the ceded branch
//   static Value constrain(Value v, const Extra&);         // end-clamp / valid-region / joint
// };
```

## THE FOUR HARD CASES — does the shape actually hold? (the point of the sketch)

### 1. SpreadPolicy — the proof it's a faithful generalization (must be byte-identical)
```cpp
struct SpreadPolicy {
  using Value = float; struct Extra {};
  static float scaleCv(float raw)              { return raw * 2.f; }          // kBipolarSpan
  static float addCv  (float v, float c)       { return v + c; }
  static float addSend(float v, float s)       { return v + s; }
  static float applyDelegated(float mb, float td, const Extra&) { return mb + td; }
  static float constrain(float v, const Extra&){ return std::clamp(v, -1.f, 1.f); }
};
// SandsModifierResolver<SpreadPolicy>::effective == SpreadResolver::effective, BYTE-IDENTICAL. The
// extraction's gate: the existing SpreadResolver test passes unchanged against the templated version.
```

### 2. DirectionPolicy — NO BLEND (owner selects; CV/send are no-ops). Fits via no-op ops.
```cpp
struct DirPolicy {
  using Value = Dir;  // enum: FWD/REV/PINGPONG/... (the PER-LEVEL sign+mode; NOT the composed product)
  struct Extra {};
  static float scaleCv(float raw)              { return raw; }               // unused
  static Dir   addCv  (Dir v, float)           { return v; }                 // direction doesn't blend CV
  static Dir   addSend(Dir v, float)           { return v; }                 // nor send
  static Dir   applyDelegated(Dir mb, float, const Extra&) { return mb; }    // ceded → Macro's direction
  static Dir   constrain(Dir v, const Extra&)  { return v; }
};
// KEY: the resolver resolves THIS LEVEL's direction (owner's pick). The global×voice×lane SIGN-PRODUCT
// composition (UNIFIED_ADDRESSING_SYSTEM) is the ADDRESSING SYSTEM's job, NOT here. Resolver = per-level
// value; composer = product. If a future "direction CV-nudge" is wanted, addCv stops being a no-op —
// the shape already admits it.
```

### 3. ClampPolicy — the JOINT constraint (lo<hi). Fits via Extra + constrain seeing both.
```cpp
struct ClampPolicy {
  using Value = LoHi;  // struct { float lo, hi; }
  struct Extra {};     // (lo<hi is intra-Value, so no extra field needed — see note)
  static float scaleCv(float raw)                 { return raw; }
  static LoHi  addCv  (LoHi v, float c)            { return { v.lo + c, v.hi + c }; } // or per-rail; TBD by owner
  static LoHi  addSend(LoHi v, float s)            { return { v.lo + s, v.hi + s }; }
  static LoHi  applyDelegated(LoHi mb, float td, const Extra&) { return { mb.lo+td, mb.hi+td }; }
  static LoHi  constrain(LoHi v, const Extra&) {
      v.lo = std::clamp(v.lo, 0.f, 1.f); v.hi = std::clamp(v.hi, 0.f, 1.f);
      if (v.lo > v.hi) v.lo = v.hi;       // enforce lo<=hi JOINTLY (the clamp wrinkle)
      return v;
  }
};
// INSIGHT from writing it: clamp's joint constraint is INTRA-Value (lo and hi are both in Value), so
// constrain() already sees both — NO separate jointConstrain hook needed. My earlier "jointConstrain"
// was over-engineering; a Value that carries both members + a constrain() that sees the whole Value
// handles it. The skeleton is SIMPLER than the design doc implied.
```

### 4. WarpPolicy + the future PER-VOICE-LANE lane — the Extra field earns its keep
```cpp
struct WarpPolicy {
  using Value = WarpCell;  // struct { WarpF curve; float w; }  (curve + depth)
  struct Extra {};
  static float   scaleCv(float raw)                 { return raw; }
  static WarpCell addCv  (WarpCell v, float c)      { v.w = std::clamp(v.w + c, 0.f, 1.f); return v; } // CV modulates DEPTH
  static WarpCell addSend(WarpCell v, float s)      { v.w = std::clamp(v.w + s, 0.f, 1.f); return v; }
  static WarpCell applyDelegated(WarpCell mb, float td, const Extra&) { mb.w = std::clamp(mb.w+td,0.f,1.f); return mb; }
  static WarpCell constrain(WarpCell v, const Extra&){ v.w = std::clamp(v.w, 0.f, 1.f); return v; }
};
// NOTE: warp's Value is (curve,w) but CV/send/tap modulate ONLY w (the curve is set-time, per the
// theory: param-mod of the curve itself is a separate θ policy). So addCv/addSend touch v.w only.
// θ (DistributionPolicy) is the one that modulates the curve's PARAMS → its own policy, Value=θ-vec,
// composed with warp (θ resolves the curve warp applies). Two policies, they compose at apply-time.

// THE FUTURE OFFSET/MASK (per-voice LOR level, UNIFIED_ADDRESSING_SYSTEM):
struct VoiceLorPolicy {
  using Value = LorTriple;              // { int len, off, rot }  (+ a mask, TBD shape)
  struct Extra { int globalPlayhead; }; // <-- THIS is why Extra exists: reference-relative resolution
  // ... offset is resolved RELATIVE to extra.globalPlayhead + the voice equivalent.
};
// CONFIRMS: Extra is the mechanism for a policy that needs an input the others don't (the playhead
// reference). Most policies have `struct Extra {};` (empty). The skeleton doesn't change.
```

## WHAT THE SKETCH PROVED / CHANGED (feed these into the extraction)
1. The shape HOLDS for all six — one skeleton, each policy ~6 small static fns. The abstraction is
   right-sized: rigid where bugs live (delegation flow, tap, topology-consumption), flexible only in
   value-math.
2. SIMPLIFICATION vs the design doc: NO separate `jointConstrain` hook — clamp's lo<hi is intra-Value,
   so `constrain(Value, Extra)` seeing the whole Value handles it. Drop jointConstrain.
3. The `×2 kBipolarSpan` is spread-specific → moves OUT of CvTerm into SpreadPolicy::scaleCv. CvTerm
   becomes truly shared (raw uV·atten).
4. `Extra` (per-policy inputs, empty for most) is the clean mechanism for the future lane's playhead
   reference and any future per-policy input — avoids bloating the shared Inputs.
5. Direction is pure-selection (no-op addCv/addSend) AND its cross-level sign-product is the ADDRESSING
   SYSTEM's job, not the resolver's. Resolver resolves per-level direction only.
6. Warp modulates only `w`; curve-param modulation is a SEPARATE θ policy that composes. Keep them
   distinct — don't fold θ into WarpPolicy.
7. CONTRACT ENFORCEMENT = a C++20 CONCEPT (SandsModifierPolicy), not comments and not a static_assert
   fallback — the project moved to C++20 (CI-green all platforms, 2026-10). An incomplete policy is a
   LOCATED build error at the declaration. This is the whole-point enforcement: the resolver exists to
   stop half-implemented delegation; the concept stops a half-implemented POLICY from compiling.

## FOR CC (the extraction task, when it runs)
- Reconcile THIS sketch with the REAL SpreadResolver.hpp (field names, the 4-lane kSpreadLanes note,
  the CvTerm ×2). The sketch is the TARGET SHAPE; the real file is the SOURCE OF TRUTH for spread's math.
- Gate: SandsModifierResolver<SpreadPolicy> must be BYTE-IDENTICAL to today's SpreadResolver (the
  existing spread test passes unchanged). That proves the skeleton is a faithful generalization before
  any other policy rides on it.
- Then WarpPolicy; then the uniformity harness green for warp; then LOR/direction policies.
