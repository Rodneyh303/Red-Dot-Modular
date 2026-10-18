#pragma once
// ── dotModular::WarpF — the per-voice warp-F variant: value type, IDENTITY default ─────────────
// (CC_PROMPT_02 / docs/design/CC_PROMPT_02_lanedist_START.md — the authority for this design.)
//
// WHY A VARIANT (not a POD switch, not unique_ptr): a unique_ptr<Distribution> per slot
// allocates when a curve is chosen and breaks the flat, copyable, memcpy-persistable
// house pattern (spread[112] & co.); a POD {kind; params[4]} + switch(kind) needs the
// math in TWO places — the switch AND the Distribution subclasses — the
// lying-abstraction failure. std::variant held BY VALUE is a value type, trivially
// copyable (pinned by test_warp_f_bank.cpp's static_assert), sits in a flat array like
// spread[112], and there is ONE implementation per curve — the concrete type's own
// body. No switch over formulas, no parallel math, nothing to keep in sync.
//
// IDENTITY IS THE DEFAULT F — uniform is NOT a special case. The warp mix is ALWAYS
// evaluated, branchlessly:
//     warped(u) = (1-w)*u + w * F.invCDF(u)
// "No warp / uniform" is F = IdentityWarp (invCDF(u)=u), the variant's alternative 0:
// a default-constructed bank is all-identity = all-uniform, and the mix runs the SAME
// code over every slot — no "is warp on?" branch anywhere. Identity is strictly
// increasing (slope 1), so Sklar-safety holds for the default exactly as for any
// curve; w·(identity,identity) = identity; no corner cases.
//
// SINGLE IMPLEMENTATION — the holders, and one documented deviation from the prompt's
// option (a): CC_PROMPT_02 sketched the wrappers holding the existing Distribution
// SUBCLASS by value. Two facts make that literal form unworkable: (1)
// TruncatedNormalDistribution has const members (a_, b_, Pa_, Pb_, Z_, ...) → its
// copy-assignment is deleted → std::variant's assignment operators would not exist;
// (2) every Distribution subclass is polymorphic (vptr + virtual destructor) → WarpF
// would not be trivially copyable, breaking the flat, memcpy-persistable bank. So each
// wrapper stores ONLY its plain params and forwards by CONSTRUCTING the existing
// subclass on the fly — the prompt's own fallback for "not cheaply value-holdable".
// The subclass's method body is still the ONE implementation per curve (the wrapper
// contains zero formulas), Distribution.hpp is untouched, and WarpF stays trivially
// copyable. Construction cost is a handful of double writes (TruncatedNormal's four
// Phi evals the worst case) — and cadence makes it moot (verified, CC_PROMPT_02):
// warp is evaluated on the MIRROR rebuild path (dirty events: curve/param edit, new
// draw, scrub), never per audio sample — the per-sample path reads a stored float.
// A voice/lane holds ONE F evaluated at 16 steps; a full regen re-evaluates the held
// Fs, it does not reconstruct them per cell.
//
// DRAWN curves are pool-indexed, NOT inline: MonotoneCurve stores a std::vector<Point>
// (heap-backed, MAX_POINTS=32) → NOT trivially copyable → must not sit in the variant.
// DrawnWarp holds a lightweight uint16_t index into the OWNING bank's set-time pool
// (WarpBank, below) and forwards to pool[idx] — the SAME MonotoneCurve methods
// PchipDistribution calls. The pool is threaded by the caller (never a global: a rack
// can hold several Monsoon instances, each bank owning its own curves). Modulation
// never touches drawn curves (no scalar param) → the pool is set-time only, no
// control-path allocation; only voices using a drawn curve consume a pool slot.
//
// Pure module: no Rack/nanovg/UI (jansson enters only via the Distribution layer for
// the bank's persistence round-trip, added in CC_PROMPT_02 step 4).

#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <variant>

#include "Distribution.hpp"   // the parametric impls + loadDistribution (one body per curve)
#include "MonotoneCurve.hpp"  // the drawn case (via the bank's pool)

namespace dotModular {

// Drawn-curve pool capacity (WarpBank::pool is MonotoneCurve[kWarpPoolCap]).
// DOCUMENTED CAPACITY (CC_PROMPT_02 step 3): a bank has 112 (voice,lane) slots, but
// drawn curves are SHARED per identical curve (WarpBank::putCurve dedups), and a patch
// realistically carries a handful (one per lane being shaped, a few variants). 32
// covers two full voice-columns of distinct curves, or all 7 lanes × 4 variants, at
// ~5 KB per module. The UI (CC_PROMPT_04) is the arbiter of reuse/replacement beyond it.
constexpr uint16_t kWarpPoolCap = 32;

// ── IdentityWarp — THE default F: the uniform / no-warp case as a real curve ────────────────────
// invCDF(u)=u, cdf(x)=x, pdf=1: the mix (1-w)*u + w*u = u for ANY w — branchless, exact.
// Inputs are expected in [0,1] (the mix's uniforms always are); no clamps, no branches.
struct IdentityWarp {
    double invCDF(double u) const { return u; }
    double cdf  (double x) const { return x; }
    double pdf  (double /*x*/) const { return 1.0; }
    const char* typeName() const { return "identity"; }
    friend bool operator==(const IdentityWarp&, const IdentityWarp&) { return true; }
};

// ── The parametric holders ──────────────────────────────────────────────────────────────────────
// Each stores ONLY its plain params and forwards to a freshly constructed Distribution
// subclass — the subclass's body is the ONE implementation (see header note). Param
// defaults are the uniform/neutral point of each family; irrelevant to the bank's
// default (that is the variant's alternative 0, IdentityWarp).

struct TriangularWarp {
    double mode = 0.5;
    TriangularDistribution dist() const { return TriangularDistribution(mode); }
    double invCDF(double u) const { return dist().invCDF(u); }
    double cdf  (double x) const { return dist().cdf(x); }
    double pdf  (double x) const { return dist().pdf(x); }
    const char* typeName() const { return dist().typeName(); }
    friend bool operator==(const TriangularWarp& a, const TriangularWarp& b) { return a.mode == b.mode; }
};

struct PowerWarp {
    double k = 1.0;
    PowerDistribution dist() const { return PowerDistribution(k); }
    double invCDF(double u) const { return dist().invCDF(u); }
    double cdf  (double x) const { return dist().cdf(x); }
    double pdf  (double x) const { return dist().pdf(x); }
    const char* typeName() const { return dist().typeName(); }
    friend bool operator==(const PowerWarp& a, const PowerWarp& b) { return a.k == b.k; }
};

struct ReversedPowerWarp {
    double theta = 1.0;
    ReversedPowerDistribution dist() const { return ReversedPowerDistribution(theta); }
    double invCDF(double u) const { return dist().invCDF(u); }
    double cdf  (double x) const { return dist().cdf(x); }
    double pdf  (double x) const { return dist().pdf(x); }
    const char* typeName() const { return dist().typeName(); }
    friend bool operator==(const ReversedPowerWarp& a, const ReversedPowerWarp& b) { return a.theta == b.theta; }
};

struct KumaraswamyWarp {
    double a = 1.0, b = 1.0;
    KumaraswamyDistribution dist() const { return KumaraswamyDistribution(a, b); }
    double invCDF(double u) const { return dist().invCDF(u); }
    double cdf  (double x) const { return dist().cdf(x); }
    double pdf  (double x) const { return dist().pdf(x); }
    const char* typeName() const { return dist().typeName(); }
    friend bool operator==(const KumaraswamyWarp& x, const KumaraswamyWarp& y) { return x.a == y.a && x.b == y.b; }
};

struct MobiusWarp {
    double a = 1.0;
    MobiusDistribution dist() const { return MobiusDistribution(a); }
    double invCDF(double u) const { return dist().invCDF(u); }
    double cdf  (double x) const { return dist().cdf(x); }
    double pdf  (double x) const { return dist().pdf(x); }
    const char* typeName() const { return dist().typeName(); }
    friend bool operator==(const MobiusWarp& x, const MobiusWarp& y) { return x.a == y.a; }
};

struct LogitSigmoidWarp {
    double a = 1.0, b = 0.0;
    LogitSigmoidDistribution dist() const { return LogitSigmoidDistribution(a, b); }
    double invCDF(double u) const { return dist().invCDF(u); }
    double cdf  (double x) const { return dist().cdf(x); }
    double pdf  (double x) const { return dist().pdf(x); }
    const char* typeName() const { return dist().typeName(); }
    friend bool operator==(const LogitSigmoidWarp& x, const LogitSigmoidWarp& y) { return x.a == y.a && x.b == y.b; }
};

struct LogitNormalWarp {
    double mu = 0.0, sigma = 1.0;
    LogitNormalDistribution dist() const { return LogitNormalDistribution(mu, sigma); }
    double invCDF(double u) const { return dist().invCDF(u); }
    double cdf  (double x) const { return dist().cdf(x); }
    double pdf  (double x) const { return dist().pdf(x); }
    const char* typeName() const { return dist().typeName(); }
    friend bool operator==(const LogitNormalWarp& x, const LogitNormalWarp& y) { return x.mu == y.mu && x.sigma == y.sigma; }
};

struct TruncNormalWarp {
    double mu = 0.5, sigma = 0.2;
    TruncatedNormalDistribution dist() const { return TruncatedNormalDistribution(mu, sigma); }
    double invCDF(double u) const { return dist().invCDF(u); }
    double cdf  (double x) const { return dist().cdf(x); }
    double pdf  (double x) const { return dist().pdf(x); }
    const char* typeName() const { return dist().typeName(); }
    friend bool operator==(const TruncNormalWarp& x, const TruncNormalWarp& y) { return x.mu == y.mu && x.sigma == y.sigma; }
};

// ── DrawnWarp — the drawn case: a pool index, NOT an inline curve ───────────────────────────────
// MonotoneCurve is vector-backed → not trivially copyable → not inline in the variant.
// These forward to pool[idx] — the same MonotoneCurve methods PchipDistribution calls
// (one implementation). The pool pointer is threaded by the caller (the bank); it may
// be null for banks/slots that never evaluate a drawn curve.
struct DrawnWarp {
    uint16_t idx = 0;   // slot in the owning bank's drawn-curve pool
    double invCDF(double u, const MonotoneCurve* pool) const {
        assert(pool != nullptr && idx < kWarpPoolCap);
        return pool[idx].evalInverse(u);
    }
    double cdf(double x, const MonotoneCurve* pool) const {
        assert(pool != nullptr && idx < kWarpPoolCap);
        return pool[idx].evalCDF(x);
    }
    double pdf(double x, const MonotoneCurve* pool) const {
        assert(pool != nullptr && idx < kWarpPoolCap);
        return pool[idx].pdf(x);
    }
    const char* typeName() const { return "drawn"; }
    friend bool operator==(const DrawnWarp& a, const DrawnWarp& b) { return a.idx == b.idx; }
};

// ── The variant ────────────────────────────────────────────────────────────────────────────────
// IdentityWarp FIRST: alternative 0 is the default-constructed state — the no-warp
// bank. Trivially copyable (all alternatives are plain doubles/uint16_t), so the bank
// is a flat, memcpy-persistable array like spread[112].
using WarpF = std::variant<
    IdentityWarp,              // index 0 = default = uniform / no warp
    TriangularWarp, PowerWarp, KumaraswamyWarp, LogitNormalWarp,
    LogitSigmoidWarp, MobiusWarp, TruncNormalWarp, ReversedPowerWarp,  // NB reversed-power:
    DrawnWarp>;                //   added by CC_PROMPT_P1 (PR #287) — wrapped like the rest

// ── The thin forwarders ────────────────────────────────────────────────────────────────────────
// Dispatch to the ONE implementation each alternative owns (the subclass body /
// MonotoneCurve). std::visit over value types is non-allocating; the ONLY branch is the
// compile-time one that hands DrawnWarp its pool — there is no second copy of any
// formula, so this forwarder cannot diverge. `pool` is only read for drawn slots
// (null is fine elsewhere).
inline double invCDF(const WarpF& f, double u, const MonotoneCurve* pool) {
    return std::visit([u, pool](const auto& d) -> double {
        if constexpr (std::is_same_v<std::decay_t<decltype(d)>, DrawnWarp>)
            return d.invCDF(u, pool);
        else
            return d.invCDF(u);
    }, f);
}
inline double cdf(const WarpF& f, double x, const MonotoneCurve* pool) {
    return std::visit([x, pool](const auto& d) -> double {
        if constexpr (std::is_same_v<std::decay_t<decltype(d)>, DrawnWarp>)
            return d.cdf(x, pool);
        else
            return d.cdf(x);
    }, f);
}
inline double pdf(const WarpF& f, double x, const MonotoneCurve* pool) {
    return std::visit([x, pool](const auto& d) -> double {
        if constexpr (std::is_same_v<std::decay_t<decltype(d)>, DrawnWarp>)
            return d.pdf(x, pool);
        else
            return d.pdf(x);
    }, f);
}
inline const char* warpTypeName(const WarpF& f) {
    return std::visit([](const auto& d) { return d.typeName(); }, f);
}

// ── The lowering seam: authored (polymorphic) → bank (value) ────────────────────────────────────
// One-way lowering at the point an AUTHORED curve (the editor/.dmcurve form) enters
// per-voice engine state — the same shape as macroBase(authored) → published-deltas(hot).
// typeName() is the dispatch tag (each concrete type returns its own — it cannot lie);
// static_cast is safe on that basis. Unknown tags lower to IdentityWarp (safe default).
// The DRAWN case is deliberately NOT here: a Pchip needs a POOL slot, which is bank-level
// state — use WarpBank's drawn path (step 3), which lowers parametrics through this
// same function.
inline WarpF lowerWarp(const Distribution& d) {
    const char* t = d.typeName();
    if (std::strcmp(t, "triangular") == 0)
        return TriangularWarp{static_cast<const TriangularDistribution&>(d).mode};
    if (std::strcmp(t, "power") == 0)
        return PowerWarp{static_cast<const PowerDistribution&>(d).k};
    if (std::strcmp(t, "reversed-power") == 0)
        return ReversedPowerWarp{static_cast<const ReversedPowerDistribution&>(d).theta};
    if (std::strcmp(t, "kumaraswamy") == 0) {
        const auto& k = static_cast<const KumaraswamyDistribution&>(d);
        return KumaraswamyWarp{k.a, k.b};
    }
    if (std::strcmp(t, "mobius") == 0)
        return MobiusWarp{static_cast<const MobiusDistribution&>(d).a};
    if (std::strcmp(t, "logit-sigmoid") == 0) {
        const auto& s = static_cast<const LogitSigmoidDistribution&>(d);
        return LogitSigmoidWarp{s.a, s.b};
    }
    if (std::strcmp(t, "logit-normal") == 0) {
        const auto& n = static_cast<const LogitNormalDistribution&>(d);
        return LogitNormalWarp{n.mu, n.sigma};
    }
    if (std::strcmp(t, "truncated-normal") == 0) {
        const auto& n = static_cast<const TruncatedNormalDistribution&>(d);
        return TruncNormalWarp{n.mu, n.sigma};
    }
    return IdentityWarp{};   // "drawn" (bank-level pool) or unknown → the safe default
}

} // namespace dotModular
