#pragma once
#include <algorithm>
#include <concepts>

// ─────────────────────────────────────────────────────────────────────────────
// SandsModifierResolver — the ONE shared resolution skeleton for every
// per-(voice,lane) modifier that delegates between Macro and East.
//
// THE MEMBERSHIP RULE (one sentence — inclusion AND exclusion):
//   A modifier belongs in this resolver IFF it VARIES PER (voice, lane) AND
//   DELEGATES between Macro and East.
//   IN : spread, LOR, direction, warp, θ (distribution params), clamp (lo/hi),
//        and the future offset/mask lane.
//   OUT: slew-rho, A/B mix — UNIFORM across all voices/lanes (one value for the
//        group; no per-voice divergence, no delegation). Forcing them through
//        this resolver would MANUFACTURE the divergence surface it exists to
//        remove. Change Alley too (a different layer). And the delegation
//        CONTROL itself — WHO owns a (voice,lane) — is SandsTopology's job:
//        this resolver CONSUMES the resolved `delegated` boolean and never
//        re-derives it. One topology authority feeds many value resolvers.
//
// The skeleton is RIGID where the delegation bugs lived (delegation flow, the
// P9b per-voice send-tap, skip-if-absent, end-clamp-once — written and tested
// ONCE); a Policy supplies ONLY the value math through the SandsModifierPolicy
// concept (Value type, how base/CV/send combine, the constraint, extra
// inputs). An incomplete policy is a LOCATED compile error at its declaration
// ("does not satisfy SandsModifierPolicy"), not a deep instantiation error
// inside effective().
//
// Canonical arithmetic (generalized from the spread original; P = the policy):
//   free      : v = base (+ownCv) (+eastCv) (+tappedDelta·macroSend if present),
//               then constrain ONCE at the end (end-clamp)
//   delegated : constrain(applyDelegated(macroBase, tappedDelta, extra))
// Contributions are SUMMED then constrained once — order-independent, and an
// intermediate that overflows the constraint is not pinned before a later term
// can pull it back, so the net of a large-then-opposite pair is preserved
// (which per-term constraining would destroy). Absent terms (unconnected CV /
// macro absent) are skipped entirely — a true no-op.
//
// Step 1 (this header's first rider): SpreadPolicy, extracted from
// SpreadResolver — the ONE tested delegation authority — so the skeleton is
// proven by being BYTE-IDENTICAL to what ships. Gate:
// test_resolver_spread_bridge.cpp fuzzes ≥100k seeded Inputs against a
// verbatim copy of the OLD SpreadResolver::effective and asserts same-BITS
// equality for both SandsModifierResolver<SpreadPolicy> and the compat shell
// SpreadResolver. Later steps (warp/LOR/direction) extend the same pin.
// ─────────────────────────────────────────────────────────────────────────────

namespace redDot {

// One additive CV/attenuverter modulation term — shared, policy-independent.
// connected gates it (an unconnected jack contributes nothing). Voltage is
// already /10 (unit-scaled) by the caller, matching the applyMono/MacroSprCV
// convention. contribution() is the RAW uV·atten product; any per-modifier
// scaling (spread's ×2 bipolar span) is the POLICY's business
// (SpreadPolicy::scaleCv), so CvTerm stays identical for every policy.
struct CvTerm {
    bool  connected = false;
    float unitVoltage = 0.f;   // jack voltage / 10
    float atten       = 0.f;   // attenuverter -1..1
    float contribution() const { return connected ? unitVoltage * atten : 0.f; }
};

// ── the POLICY CONTRACT (a real C++20 concept, not a static_assert fallback) ──
// What a modifier must supply to ride the skeleton. Everything else — the
// delegation flow, the tap, skip-if-absent, end-clamp-once — is shared.
template <class P>
concept SandsModifierPolicy = requires(typename P::Value v, float f,
                                       typename P::Value mb, typename P::Extra e) {
    typename P::Value;
    typename P::Extra;
    { P::scaleCv(f) }               -> std::convertible_to<float>;      // raw uV·atten → this modifier's units
    { P::addCv(v, f) }              -> std::same_as<typename P::Value>; // fold a CV term into the value
    { P::addSend(v, f) }            -> std::same_as<typename P::Value>; // fold the Macro send into it
    { P::applyDelegated(mb, f, e) } -> std::same_as<typename P::Value>; // the ceded branch
    { P::constrain(v, e) }          -> std::same_as<typename P::Value>; // end-clamp / valid region
};

// ── the skeleton: templated on a (constrained) Policy ───────────────────────
template <SandsModifierPolicy Policy>
struct SandsModifierResolver {
    using Value = typename Policy::Value;

    struct Inputs {
        Value base{};               // the lane's base value for THIS modifier

        // Topology-derived (caller fills from SandsTopology — NOT re-derived here):
        bool delegated    = false;  // this (voice,lane) ceded to Macro (owner()==MACRO) → track Macro
        bool macroPresent = false;

        // Own CV (Mono's jack, or Macro's own — same shape) and the East V1 add
        // (only meaningful on the mono path; leave default off elsewhere):
        CvTerm ownCv;
        CvTerm eastCv;

        // Macro contributions (used both for the delegated override and the additive send):
        Value  macroBase{};         // Macro's base value for the modifier
        float  macroSend = 0.f;     // the send amount (0..1)

        // PER-VOICE SEND-TAP (the P9b fix): the publish carries BOTH endpoints and
        // each receiving VOICE selects its own tap — the pre/post choice stays
        // downstream of Macro's publish (which can no longer bake one
        // globally-tapped delta).
        //   macroPreDelta  = raw cv·(hi-lo)          (tap=0 endpoint, PRE the attenuverter)
        //   macroPostDelta = cv·att·(hi-lo)          (tap=1 endpoint, POST — the old tappedDelta)
        //   macroTap       = THIS voice's tap (0..1; default 1.0 = POST, the old behaviour)
        float macroPreDelta  = 0.f;
        float macroPostDelta = 0.f;
        float macroTap       = 1.f;

        typename Policy::Extra extra{};   // policy-declared EXTRA inputs (empty for most)
    };

    // The per-voice tap selection: lerp between the two published endpoints. Pure;
    // the reader-side half of the publish-both-endpoints contract (the send term is
    // tappedDelta·send). SHARED — the send-tap contract is universal.
    static float tappedDelta(const Inputs& in) {
        return in.macroPreDelta + (in.macroPostDelta - in.macroPreDelta) * in.macroTap;
    }

    // THE SINGLE AUTHORITY. Pure: same inputs → same output, no hidden state.
    // Control flow is identical for every policy; only the ops are the policy's.
    // Sums the present contributions and constrains ONCE at the end; a term that
    // isn't present (unconnected CV / macro absent) is skipped entirely.
    static Value effective(const Inputs& in) {
        if (in.delegated) {
            // Ceded cell: mirror Macro entirely, ignore own base/CV. The ceding
            // VOICE's tap selects the delta (P9b).
            return Policy::constrain(Policy::applyDelegated(in.macroBase, tappedDelta(in), in.extra),
                                     in.extra);
        }
        Value v = in.base;
        if (in.ownCv.connected)  v = Policy::addCv(v, Policy::scaleCv(in.ownCv.contribution()));
        if (in.eastCv.connected) v = Policy::addCv(v, Policy::scaleCv(in.eastCv.contribution()));
        if (in.macroPresent)     v = Policy::addSend(v, tappedDelta(in) * in.macroSend);
        return Policy::constrain(v, in.extra);
    }
};

// ── SPREAD, the first policy — extracted from the old SpreadResolver ─────────
// The bipolar spread amount in [-1,1] that feeds redDot::SpreadInterp::apply.
// Every op below is the old header's arithmetic, verbatim: CV contributions are
// scaled by the bipolar span, everything adds, the end-clamp is the old
// clampSpread's max/min form (NOT std::clamp — they differ on NaN, and the old
// header is the math truth).
struct SpreadPolicy {
    using Value = float;         // the bipolar spread amount in [-1,1]
    struct Extra {};             // spread needs no extra inputs

    // ±1 range → ×2 so full-depth CV reaches the ends. SINGLE DEFINITION — the
    // compat shell SpreadResolver aliases this; nothing else may re-define it.
    static constexpr float kBipolarSpan = 2.f;

    static float scaleCv(float raw)                                { return raw * kBipolarSpan; }
    static float addCv  (float v, float c)                         { return v + c; }
    static float addSend(float v, float s)                         { return v + s; }
    static float applyDelegated(float mb, float td, const Extra&)  { return mb + td; }
    static float constrain(float v, const Extra&)                  { return std::max(-1.f, std::min(1.f, v)); }
};

// The concept must BITE: spread is a member (an incomplete policy is a build
// error at its declaration — this assert fails loudly if the contract drifts).
static_assert(SandsModifierPolicy<SpreadPolicy>);

} // namespace redDot
