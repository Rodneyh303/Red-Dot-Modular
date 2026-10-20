#pragma once
#include <algorithm>
#include <concepts>

#include "WarpF.hpp"   // dotModular::WarpF — WarpPolicy's curve member. The include pair
                       // is a GUARD-SAFE CYCLE: WarpF.hpp includes THIS header as its
                       // last line (see the WarpBank section at the bottom of this file
                       // for why that is safe in both entry orders).

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
// SpreadResolver.
//
// Step 2 (second rider): WarpPolicy — WarpBank's mix/effective members now
// resolve their (curve, depth) pair through this skeleton (the hand-rolled
// `if (delegated)` branch in WarpF.hpp is DELETED; the definitions live at the
// bottom of this file). Gate: test_resolver_warp_bridge.cpp fuzzes seeded
// banks against a verbatim copy of the OLD WarpBank mix/selection and asserts
// same-BITS mix outputs + the same resolved (f,w). Later steps
// (LOR/direction) extend the same pin.
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

// ── WARP, the second policy — the (curve, depth) pair the bank mixes ──────────
// Value is WarpCell{f, w}: the resolved curve + depth whose branchless mix
// (1-w)·u + w·F⁻¹(u) shapes a lane's probability. WarpF is a trivially
// copyable std::variant (pinned by test_warp_f_bank's static_assert); BY VALUE
// is correct — resolution runs on the mirror rebuild path, never per sample.
//
// CV/send/delegation modulate the DEPTH only; the curve is set-time (sketch §4
// — curve-PARAM modulation is a separate future θ policy that composes at
// apply-time, never fused here). Every op below touches v.w and leaves v.f
// untouched.
//
// applyDelegated's general form {mb.f, clamp01(mb.w + td)} is DORMANT today:
// no warp send/tap plumbing exists, so the bank wires no pre/post deltas and
// tappedDelta ≡ 0 — the resolved cell is bit-identical to the OLD pure
// selection (own slot vs Macro's global default; pinned same-BITS by
// test_resolver_warp_bridge). When warp grows the spread send/tap shape, this
// policy already implements it; only the Inputs wiring changes.
//
// constrain clamps the RESOLVED w to [0,1] — a different layer from the mix's
// documented no-clamp (WarpF.hpp: the MIX trusts its inputs; the RESOLVER
// clamps its output). Identity on valid inputs (authored w lives in [0,1]).
struct WarpCell {
    dotModular::WarpF f;   // the curve (set-time; never modulated here)
    float w = 0.f;         // the depth in [0,1] (0 = no warp; NSDMI so a
                           // default-constructed cell is the no-warp state)
};

struct WarpPolicy {
    using Value = WarpCell;
    struct Extra {};        // warp needs no extra inputs (yet)

    // [0,1] clamp in SpreadPolicy's max/min form (NOT std::clamp — they differ
    // on NaN; one convention for the whole resolver file).
    static float clamp01(float x) { return std::max(0.f, std::min(1.f, x)); }

    static float scaleCv(float raw) { return raw; }   // depth CV is already unit-scaled
    static WarpCell addCv  (WarpCell v, float c) { v.w = clamp01(v.w + c); return v; }
    static WarpCell addSend(WarpCell v, float s) { v.w = clamp01(v.w + s); return v; }
    static WarpCell applyDelegated(WarpCell mb, float td, const Extra&) {
        mb.w = clamp01(mb.w + td);   // DORMANT general form — td ≡ 0 today (section note)
        return mb;
    }
    static WarpCell constrain(WarpCell v, const Extra&) { v.w = clamp01(v.w); return v; }
};

// The concept must BITE here too (the located-error enforcement, same as spread).
static_assert(SandsModifierPolicy<WarpPolicy>);

} // namespace redDot

// ── WarpBank rides the resolver (STEP 2) — the bank's mix, defined HERE ───────
// WarpF.hpp DECLARES warpedValue/effectiveF/effectiveW and includes THIS header
// as its LAST line; the definitions live here because this is the only place
// where dotModular::WarpBank and the resolver skeleton are both complete. The
// include pair is a guard-safe cycle (this header → WarpF.hpp at the top;
// WarpF.hpp → this header at the bottom): whichever header a TU enters first,
// the definitions below always execute after WarpF.hpp's full body, so they
// see the complete bank. No resolver-dependent code may live in WarpF.hpp.
//
// The hand-rolled `if (delegated)` hand-copy this routing replaces was the
// design doc's INCOMPLETE warp delegation copy (0 CV/send/tap terms) — the
// Macro→East/V1 bug class dies by construction once those terms are wired;
// today the wiring is base/macroBase/delegated only. Signatures are FROZEN
// (the uniformity-harness U2 oracle calls the 4-arg form; test_warp_f_bank S4
// pins all four).
namespace dotModular {

// The one wiring: this voice's own slot is the base, Macro's global default is
// the macroBase, the topology-resolved flag is the delegation. No warp
// CV/send/tap plumbing exists yet, so every other Input stays absent — the
// resolver's output is PURE SELECTION, bit-identical to the old branch. When
// warp grows the spread send/tap shape, ONLY this wiring changes.
inline redDot::WarpCell resolveWarpCell(const WarpBank& b, int v, int lane, bool delegated) {
    redDot::SandsModifierResolver<redDot::WarpPolicy>::Inputs in;
    const int i = WarpBank::index(v, lane);
    in.base      = redDot::WarpCell{b.warpF[i], b.warpW[i]};
    in.macroBase = redDot::WarpCell{b.globalWarpF[lane], b.globalWarpW[lane]};
    in.delegated = delegated;
    return redDot::SandsModifierResolver<redDot::WarpPolicy>::effective(in);
}

// THE MIX, branchless, on the RESOLVED pair (see the declaration site in
// WarpF.hpp for the w=0/identity bit-exactness pins). The 4-arg form is the
// delegation-resolved entry (PatternEngine::warpMixCell — the ONLY production
// caller); the 2-arg form is the same authority with delegated=false.
inline double WarpBank::warpedValue(int v, int lane, double u, bool delegated) const {
    const redDot::WarpCell c = resolveWarpCell(*this, v, lane, delegated);
    return (1.0 - c.w) * u + c.w * invCDF(c.f, u);
}

inline double WarpBank::warpedValue(int v, int lane, double u) const {
    return warpedValue(v, lane, u, /*delegated=*/false);
}

// The resolved cell's members (no production callers; test_warp_f_bank S4c
// pins the selection semantics). effectiveF returns BY VALUE: the resolver
// produces a resolved cell, not a reference into the bank (WarpF is trivially
// copyable, pinned).
inline WarpF WarpBank::effectiveF(int v, int lane, bool delegated) const {
    return resolveWarpCell(*this, v, lane, delegated).f;
}
inline float WarpBank::effectiveW(int v, int lane, bool delegated) const {
    return resolveWarpCell(*this, v, lane, delegated).w;
}

} // namespace dotModular
