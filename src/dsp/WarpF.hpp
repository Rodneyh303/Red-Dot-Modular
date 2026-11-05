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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>

#include "Distribution.hpp"   // the parametric impls + loadDistribution (one body per curve)
#include "MonotoneCurve.hpp"  // the drawn case (via the bank's pool)
#include "WarpStages.hpp"     // FEEL-TEST: the MO rail-tilt + injected-atom stages (pure, Rack-free)

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

// ── The parametric variant ── (the BASE palette — the dual's member type) ──
// IdentityWarp FIRST: alternative 0 is the default-constructed state — the no-warp
// bank. Trivially copyable (all alternatives are plain doubles/uint16_t), so the bank
// is a flat, memcpy-persistable array like spread[112].
// (the BASE palette — the dual's member type) — named ParamF so the dual below can hold
// it BY VALUE without recursion (a variant cannot contain itself; ParamF is the same
// list MINUS the dual alternative).
using ParamF = std::variant<
    IdentityWarp,              // index 0 = default = uniform / no warp
    TriangularWarp, PowerWarp, KumaraswamyWarp, LogitNormalWarp,
    LogitSigmoidWarp, MobiusWarp, TruncNormalWarp, ReversedPowerWarp,  // NB reversed-power:
    DrawnWarp>;                //   added by CC_PROMPT_P1 (PR #287) — wrapped like the rest

// ── DualWarp — THE DUAL READING (PHASE_DISTRIBUTION_SPEC §1/§6, Rodney: "For each of our
// distributions ... add the dual ... So we double the number of distribution choices") ──
// ONE implementation: the same curve object read with CDF and quantile swapped —
//   dual.cdf(x)    = member.invCDF(x)      (the member's quantile, read as a CDF)
//   dual.invCDF(u) = member.cdf(u)         (the member's CDF, read as a quantile)
//   dual.pdf(x)    = 1 / member.pdf(dual.cdf(x))   (the reciprocal density; the finite
//                    DIST_EDGE_EPS proxy where the member's density vanishes — the
//                    house divergent-edge rule, mirrored inward)
// The menu entry fixes the reading (the spec's "a single class with a reading parameter
// fixed by the menu entry"); the chain stages (tilt/gap/atom) live OUTSIDE the curve on
// the WarpCell, and the DUAL SWAPS THE COMPOSED PAIR at eval time (WarpBank::warpedValue),
// so an atom on K reads as a gap on DK and vice versa (spec §6/§8.8 — Rodney: "Atom dual
// is a gap and vice versa"). The member is a ParamF — NEVER a WarpF — so duals cannot
// nest (dual∘dual = the original reading; the menu just flips between K and DK), and the
// type stays trivially copyable (a flat ParamF value inside).
struct DualWarp;

// The FULL variant: the base palette with the dual reading appended LAST — indices 0..9
// are UNTOUCHED (the fold's kind dispatch, warpParamSpec's tables, the test pin on
// "IdentityWarp is alternative 0" all keep their numbers; the dual is index 10).
using WarpF = std::variant<
    IdentityWarp,              // index 0 = default = uniform / no warp
    TriangularWarp, PowerWarp, KumaraswamyWarp, LogitNormalWarp,
    LogitSigmoidWarp, MobiusWarp, TruncNormalWarp, ReversedPowerWarp,
    DrawnWarp,
    DualWarp>;                 // index 10 = the dual reading (PHASE_DISTRIBUTION_SPEC)

// The member-level dispatch the DualWarp swap forwards to (defined beside the thin
// forwarders, below — the ParamF visitors the WarpF visitors delegate into).
inline double invCDFOf(const ParamF& m, double u, const MonotoneCurve* pool);
inline double cdfOf  (const ParamF& m, double x, const MonotoneCurve* pool);
inline double pdfOf  (const ParamF& m, double x, const MonotoneCurve* pool);

// DualWarp — the reading swap, pool-threaded exactly like DrawnWarp (a drawn member
// evaluates through the SAME pool pointer the outer call received).
struct DualWarp {
    ParamF member;   // the wrapped curve (never a DualWarp — no nesting, by construction)
    double invCDF(double u, const MonotoneCurve* pool) const { return cdfOf(member, u, pool); }
    double cdf  (double x, const MonotoneCurve* pool) const { return invCDFOf(member, x, pool); }
    double pdf  (double x, const MonotoneCurve* pool) const;
    const char* typeName() const { return "dual"; }
    friend bool operator==(const DualWarp& a, const DualWarp& b) { return a.member == b.member; }
};

// ── the ParamF ↔ WarpF seams ───────────────────────────────────────────────────────────
// Lift a base-palette value into the full variant (one visit, one copy); and the narrow
// direction used when BUILDING a dual's member: a plain alternative copies, a DualWarp
// input is DEFENSIVE (callers never build members from full-variant values) and falls
// back to the safe identity.
inline WarpF toWarpF(const ParamF& m) {
    return std::visit([](const auto& d) -> WarpF { return WarpF(d); }, m);
}
inline ParamF toParamF(const WarpF& f) {
    return std::visit([](const auto& d) -> ParamF {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, DualWarp>) return ParamF{IdentityWarp{}};
        else return ParamF(d);
    }, f);
}
// Wrap a (base-palette) curve as the dual reading — the menu's commit path. An IdentityWarp
// member is refused (the uniform IS its own dual; the menu never offers it) and returns
// the member untouched, so a stray wrap can never manufacture a second uniform entry.
inline WarpF dualWrap(const WarpF& member) {
    if (std::holds_alternative<IdentityWarp>(member)) return member;
    return WarpF{DualWarp{toParamF(member)}};
}
inline bool isDualWarp(const WarpF& f) { return std::holds_alternative<DualWarp>(f); }
// The member as a full-variant value (the eval/display paths need the bare curve to
// compose the stages over — the dual's swap happens AROUND the chain, not inside it).
inline WarpF dualMember(const WarpF& f) {
    if (const auto* d = std::get_if<DualWarp>(&f)) return toWarpF(d->member);
    return f;
}
// The EFFECTIVE param kind for the per-kind spec tables: a dual's params are the
// MEMBER's params (the popup's p1/p2 knobs and the mod-jack fold re-mint the member,
// then the dual re-wraps) — so every kind-keyed table lookup resolves through here.
inline int warpEffKind(const WarpF& f) {
    if (const auto* d = std::get_if<DualWarp>(&f)) return (int)d->member.index();
    return (int)f.index();
}

// ── The thin forwarders ────────────────────────────────────────────────────────
// Dispatch to the ONE implementation each alternative owns (the subclass body /
// MonotoneCurve). std::visit over value types is non-allocating; the ONLY branch is the
// compile-time one that hands DrawnWarp — directly, or as a DUAL's member — its pool;
// there is no second copy of any formula, so this forwarder cannot diverge. `pool` is
// only read for drawn slots (null is fine elsewhere). The ParamF-level trio is the
// member dispatch the DUAL's swap calls (§6: dual.invCDF = member.cdf, dual.cdf =
// member.invCDF); the WarpF-level trio adds the DualWarp branches on top.
inline double invCDFOf(const ParamF& m, double u, const MonotoneCurve* pool) {
    return std::visit([u, pool](const auto& d) -> double {
        if constexpr (std::is_same_v<std::decay_t<decltype(d)>, DrawnWarp>)
            return d.invCDF(u, pool);
        else
            return d.invCDF(u);
    }, m);
}
inline double cdfOf(const ParamF& m, double x, const MonotoneCurve* pool) {
    return std::visit([x, pool](const auto& d) -> double {
        if constexpr (std::is_same_v<std::decay_t<decltype(d)>, DrawnWarp>)
            return d.cdf(x, pool);
        else
            return d.cdf(x);
    }, m);
}
inline double pdfOf(const ParamF& m, double x, const MonotoneCurve* pool) {
    return std::visit([x, pool](const auto& d) -> double {
        if constexpr (std::is_same_v<std::decay_t<decltype(d)>, DrawnWarp>)
            return d.pdf(x, pool);
        else
            return d.pdf(x);
    }, m);
}
// DualWarp::pdf — defined here (pdfOf is complete above): the reciprocal density
// 1/f(F⁻¹(x)) with the house finite proxy. Where the member's density vanishes the
// dual's density DIVERGES (the interior mirror of a divergent rail edge); the proxy
// keeps callers finite exactly like the palette's edge handling.
inline double DualWarp::pdf(double x, const MonotoneCurve* pool) const {
    const double y = invCDFOf(member, x, pool);      // = dual.cdf(x) = F⁻¹(x)
    const double d = pdfOf(member, y, pool);         // f(F⁻¹(x))
    return 1.0 / (d < DIST_EDGE_EPS ? DIST_EDGE_EPS : d);
}
inline double invCDF(const WarpF& f, double u, const MonotoneCurve* pool) {
    return std::visit([u, pool](const auto& d) -> double {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, DrawnWarp>)     return d.invCDF(u, pool);
        else if constexpr (std::is_same_v<T, DualWarp>) return d.invCDF(u, pool);
        else                                            return d.invCDF(u);
    }, f);
}
inline double cdf(const WarpF& f, double x, const MonotoneCurve* pool) {
    return std::visit([x, pool](const auto& d) -> double {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, DrawnWarp>)     return d.cdf(x, pool);
        else if constexpr (std::is_same_v<T, DualWarp>) return d.cdf(x, pool);
        else                                            return d.cdf(x);
    }, f);
}
inline double pdf(const WarpF& f, double x, const MonotoneCurve* pool) {
    return std::visit([x, pool](const auto& d) -> double {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, DrawnWarp>)     return d.pdf(x, pool);
        else if constexpr (std::is_same_v<T, DualWarp>) return d.pdf(x, pool);
        else                                            return d.pdf(x);
    }, f);
}
inline const char* warpTypeName(const WarpF& f) {
    return std::visit([](const auto& d) { return d.typeName(); }, f);
}
// The display name: a stored dual reads as "dual-<member>" (the popup header's
// "%s" slot — a static-string return cannot compose, so the caller's buffer is
// threaded; a null buf or a non-dual falls back to warpTypeName).
inline const char* warpDisplayName(const WarpF& f, char* buf, size_t cap) {
    if (buf && std::get_if<DualWarp>(&f)) {
        std::snprintf(buf, cap, "dual-%s",
                      std::visit([](const auto& m) { return m.typeName(); },
                                 std::get<DualWarp>(f).member));
        return buf;
    }
    return warpTypeName(f);
}
// THE ACTIVE READING's display name (the bidirectional warp — the amended spec's
// sign law): production state is always a BARE member, and the SIGN of the depth
// picks the reading, so the readout names what the engine hears — negative w reads
// "dual-<kind>" (the same curve with CDF and quantile exchanged), non-negative w
// reads the kind itself. The identity takes no dual name on the negative side (the
// uniform IS its own dual — F(x) = x swaps to itself; naming it would manufacture
// a second uniform entry, exactly what the menu refuses).
inline const char* warpReadingName(const WarpF& f, float w, char* buf, size_t cap) {
    if (w < 0.f && buf && !std::holds_alternative<IdentityWarp>(f)) {
        std::snprintf(buf, cap, "dual-%s", warpTypeName(f));
        return buf;
    }
    return warpTypeName(f);
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

// ── THE PER-KIND PARAM AUTHORITY (CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC) ────────────────
// Moved here from WarpCurvePopup.hpp (UI) so the DSP fold can share it: the popup's
// editor knobs AND the SandsDistributionExpander's permanent knobs AND the mod-jack
// delta fold all bind from THIS table (the popup delegates; one authority, three
// consumers). { label, lo, hi, def } per param; present=false = the kind has no such
// param — the spec's unused-param ruling ("modulation sent to a param a distribution
// doesn't use is HARMLESS — a no-op") reads the same table, so a fold and a lock can
// never disagree about what a kind owns.
struct WarpParamSpec { const char* label; float lo, hi, def; bool present; };
inline WarpParamSpec warpParamSpec(int kind, int which) {
    static const WarpParamSpec s1[] = {
        /* identity    */ { "",        0.f,  1.f, 0.f,   false },
        /* triangular  */ { "mode",    0.f,  1.f, 0.5f,  true  },
        /* power       */ { "k",       0.2f, 5.f, 2.f,   true  },
        /* kumaraswamy */ { "mode", 0.02f, 0.98f, 0.5f,  true  },
        /* logit-norm  */ { "mu",     -3.f,  3.f, 0.f,   true  },
        /* logit-sig   */ { "a",      0.25f, 4.f, 1.5f,  true  },
        /* mobius      */ { "a",      0.25f, 4.f, 1.5f,  true  },
        /* trunc-norm  */ { "mu",    -0.5f, 1.5f, 0.5f,  true  },
        /* rev-power   */ { "theta",  0.2f, 5.f, 2.f,   true  },
        /* drawn       */ { "",        0.f,  1.f, 0.f,   false },
    };
    static const WarpParamSpec s2[] = {
        /* identity    */ { "",        0.f,  1.f, 0.f,   false },
        /* triangular  */ { "",        0.f,  1.f, 0.f,   false },
        /* power       */ { "",        0.f,  1.f, 0.f,   false },
        /* kumaraswamy */ { "conc",    1.f,  5.f, 2.f,   true  },   // CONC_MAX=5 (load-bearing)
        /* logit-norm  */ { "sigma",  0.2f,  3.f, 1.f,   true  },
        /* logit-sig   */ { "b",      -3.f,  3.f, 0.f,   true  },
        /* mobius      */ { "",        0.f,  1.f, 0.f,   false },
        /* trunc-norm  */ { "sigma", 0.05f, 1.f, 0.2f,  true  },
        /* rev-power   */ { "",        0.f,  1.f, 0.f,   false },
        /* drawn       */ { "",        0.f,  1.f, 0.f,   false },
    };
    const int k = kind < 0 ? 0 : (kind > 9 ? 9 : kind);
    return (which == 0 ? s1 : s2)[k];
}

// The authored param value on the (mode, conc) authoring basis where the kind needs it
// (kumaraswamy: conc:=a, mode solved closed-form — the popup's convention verbatim).
inline double warpParamValue(const WarpF& f, int which) {
    return std::visit([which](const auto& d) -> double {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, DualWarp>) {
            // A dual's params are the MEMBER's params (the reading swap doesn't touch
            // the shape) — read through the member (toWarpF lift; the member is never
            // a dual, so this recurses exactly one level).
            return warpParamValue(toWarpF(d.member), which);
        }
        else if constexpr (std::is_same_v<T, TriangularWarp>)        return which == 0 ? d.mode : 0.0;
        if constexpr (std::is_same_v<T, TriangularWarp>)        return which == 0 ? d.mode : 0.0;
        else if constexpr (std::is_same_v<T, PowerWarp>)        return which == 0 ? d.k : 0.0;
        else if constexpr (std::is_same_v<T, ReversedPowerWarp>)return which == 0 ? d.theta : 0.0;
        else if constexpr (std::is_same_v<T, KumaraswamyWarp>) {
            double mode = 0.5, conc = 2.0;
            KumaraswamyDistribution::modeConcFromAb(d.a, d.b, mode, conc);
            return which == 0 ? mode : conc;
        }
        else if constexpr (std::is_same_v<T, LogitNormalWarp>)  return which == 0 ? d.mu : d.sigma;
        else if constexpr (std::is_same_v<T, LogitSigmoidWarp>) return which == 0 ? d.a : d.b;
        else if constexpr (std::is_same_v<T, MobiusWarp>)       return which == 0 ? d.a : 0.0;
        else if constexpr (std::is_same_v<T, TruncNormalWarp>)  return which == 0 ? d.mu : d.sigma;
        else return 0.0;   // IdentityWarp / DrawnWarp: no parametric params
    }, f);
}

// Re-mint the member with one param replaced — the fold's p1/p2 path. Parametrics
// lower through lowerWarp exactly as the set-time commit does (no pool touch — drawn
// has no params and is unreachable here); identity/drawn return f unchanged. The
// caller window-clamps v against warpParamSpec BEFORE calling (the fold does; the
// popup's knob path is window-bound by the same table).
inline WarpF warpWithParam(const WarpF& f, int which, double v) {
    return std::visit([&f, which, v](const auto& d) -> WarpF {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, DualWarp>) {
            // Re-mint the MEMBER with the param replaced, then re-wrap the reading —
            // ONE re-mint implementation (the member path below), no parallel mint.
            // dualWrap refuses an identity member (dual-of-uniform = uniform), which
            // is unreachable here: a dual's member is never identity by construction.
            return dualWrap(warpWithParam(toWarpF(d.member), which, v));
        }
        else if constexpr (std::is_same_v<T, TriangularWarp>)
            return lowerWarp(TriangularDistribution(which == 0 ? v : d.mode));
        else if constexpr (std::is_same_v<T, PowerWarp>)
            return lowerWarp(PowerDistribution(which == 0 ? v : d.k));
        else if constexpr (std::is_same_v<T, ReversedPowerWarp>)
            return lowerWarp(ReversedPowerDistribution(which == 0 ? v : d.theta));
        else if constexpr (std::is_same_v<T, KumaraswamyWarp>) {
            double mode = 0.5, conc = 2.0;
            KumaraswamyDistribution::modeConcFromAb(d.a, d.b, mode, conc);
            if (which == 0) mode = v; else conc = v;
            double a, b;
            KumaraswamyDistribution::abFromModeConc(mode, conc, a, b);
            return lowerWarp(KumaraswamyDistribution(a, b));
        }
        else if constexpr (std::is_same_v<T, LogitNormalWarp>)
            return lowerWarp(LogitNormalDistribution(which == 0 ? v : d.mu,
                                                     which == 1 ? v : d.sigma));
        else if constexpr (std::is_same_v<T, LogitSigmoidWarp>)
            return lowerWarp(LogitSigmoidDistribution(which == 0 ? v : d.a,
                                                      which == 1 ? v : d.b));
        else if constexpr (std::is_same_v<T, MobiusWarp>)
            return lowerWarp(MobiusDistribution(which == 0 ? v : d.a));
        else if constexpr (std::is_same_v<T, TruncNormalWarp>)
            return lowerWarp(TruncatedNormalDistribution(which == 0 ? v : d.mu,
                                                         which == 1 ? v : d.sigma));
        else { (void)d; return f; }   // Identity / Drawn: no params — the no-op
    }, f);
}

// ── Slot-level JSON codec (CC_PROMPT_02 step 4) ───────────────────────────────────────────────
// Identity serializes as JSON null (default slots stay compact); a parametric as the
// Distribution subclass's own .dmcurve toJson VERBATIM (one serializer per curve, the
// same reuse the prompt mandates); a drawn slot as a pool REFERENCE
// {"format":"dmcurve","version":1,"type":"drawn","pool":idx} — the curve itself persists
// in the bank's pool array, keyed by that index. Inverse: null / absent / malformed /
// unknown ⟹ IdentityWarp (the safe default — old patches and corrupt slots load as
// no-warp, never as garbage).
inline json_t* warpToJson(const WarpF& f) {
    return std::visit([](const auto& d) -> json_t* {
        using T = std::decay_t<decltype(d)>;
        if constexpr (std::is_same_v<T, IdentityWarp>) {
            return json_null();
        } else if constexpr (std::is_same_v<T, DrawnWarp>) {
            json_t* root = json_object();
            json_object_set_new(root, "format", json_string("dmcurve"));
            json_object_set_new(root, "version", json_integer(1));
            json_object_set_new(root, "type", json_string("drawn"));
            json_object_set_new(root, "pool", json_integer(d.idx));
            return root;
        } else if constexpr (std::is_same_v<T, DualWarp>) {
            // PHASE_DISTRIBUTION_SPEC: the dual persists as its member's own codec
            // wrapped in a "dual" envelope (the member is a ParamF — the nested call
            // can never emit a nested dual; no recursion hazard).
            json_t* root = json_object();
            json_object_set_new(root, "format", json_string("dmcurve"));
            json_object_set_new(root, "version", json_integer(1));
            json_object_set_new(root, "type", json_string("dual"));
            json_object_set_new(root, "member", warpToJson(toWarpF(d.member)));
            return root;
        } else {
            return d.dist().toJson();   // the ONE serializer: the subclass's own
        }
    }, f);
}
inline WarpF warpFromJson(json_t* j) {
    if (!j || json_is_null(j) || !json_is_object(j)) return IdentityWarp{};
    json_t* jt = json_object_get(j, "type");
    if (json_is_string(jt) && std::strcmp(json_string_value(jt), "drawn") == 0) {
        json_t* jp = json_object_get(j, "pool");
        if (json_is_integer(jp)) {
            const long long idx = json_integer_value(jp);
            if (idx >= 0 && idx < (long long)kWarpPoolCap) return DrawnWarp{(uint16_t)idx};
        }
        return IdentityWarp{};   // bad/missing pool ref
    }
    if (json_is_string(jt) && std::strcmp(json_string_value(jt), "dual") == 0) {
        // The dual envelope: parse the member with the SAME codec (it may be a drawn
        // pool reference). A member that fails to load (or is the IDENTITY — a
        // dual-of-uniform IS the uniform) collapses to the member; a hand-nested
        // dual collapses by the dual∘dual = original reading law.
        const WarpF member = warpFromJson(json_object_get(j, "member"));
        if (std::holds_alternative<IdentityWarp>(member)) return member;
        if (const auto* nested = std::get_if<DualWarp>(&member)) return toWarpF(nested->member);
        return WarpF{DualWarp{toParamF(member)}};
    }
    std::unique_ptr<Distribution> d = loadDistribution(j);   // the ONE loader + validation
    return d ? lowerWarp(*d) : IdentityWarp{};
}

// ── WarpBank — the per-voice-per-lane warp bank (CC_PROMPT_02 step 2) ──────────────────────────
// The STORE the mirror and UI will read (wired in CC_PROMPT_03/04 — this prompt lands the
// bank + mix + tests only). Lives in Monsoon's editor slice beside spread[112]; the same
// flat stride (v*7 + lane), the same slot convention (VoiceResolver::voiceSlot; mono = 0).
//
// The GLOBAL members are Macro's delegation DEFAULT — the warp twin of
// globalSpread[7]/globalTap[14]: a lane ceded to Macro (SandsTopology owner()==MACRO)
// tracks those instead of the voice's own slot. Delegation is NOT re-derived here — the
// caller wires the topology-resolved `delegated` flag, exactly the SpreadResolver::Inputs
// shape (do NOT invent a new delegation mechanism).
//
// The POOL is the set-time store of drawn curves: fixed-capacity (kWarpPoolCap —
// documented there), touched only at set-time/load. Modulation never touches drawn
// curves (no scalar param) → no control-path allocation; only voices using a drawn
// curve consume a pool slot.
struct WarpBank {
    static constexpr int N      = 112;   // 16 voice-slots × 7 poly lanes
    static constexpr int VOICES = 16;
    static constexpr int LANES  = 7;

    // (v*7 + lane) — the spread[112] stride.
    static int index(int v, int lane) { return v * LANES + lane; }

    float warpW[N]          = {};   // per-voice-per-lane SIGNED depth [−1,+1]
                                    // (0 = no warp; the sign selects the reading —
                                    // + the member's quantile, − the DUAL's, the
                                    // bidirectional warp's amplitude law: the depth
                                    // is |w|, the blend (1−|w|)·u + |w|·Q(u))
    WarpF warpF[N];                 // default-constructed = all IdentityWarp = uniform
                                    // (BARE members only — the reading lives in the
                                    // sign of warpW; a legacy persisted dual envelope
                                    // is migrated at parse, see the migration pass
                                    // at the bottom of parse())
    float globalWarpW[LANES] = {};  // Macro's default depth (the delegation source; signed, same law)
    WarpF globalWarpF[LANES];       // Macro's default curve (default = identity; bare members)
    MonotoneCurve pool[kWarpPoolCap];   // set-time drawn-curve pool (DrawnWarp.idx indexes it)

    // ── FEEL-TEST STAGES (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START): rail tilt + injected
    // atom, per (voice,lane), beside the (curve, depth) pair — ADDITIVE flat arrays
    // following the bank's own shape (warpW/warpF), plus Macro's global delegation
    // defaults (the globalWarpW/globalWarpF twin). NEUTRAL defaults: tilt 0, mass 0 ⟹
    // warpedValue's early-outs keep the mix BIT-IDENTICAL to the pre-feel-test line, so
    // existing patches and every pinned bridge are unaffected. The atom LEVEL's interior
    // default (0.5) is only reachable while mass > 0 — the level store is inert at m=0.
    // Eval-side clamps live in WarpStages.hpp (never here — the STORE trusts its writer).
    float warpTilt[N]        = {};   // rail tilt p (neutral 0; authored |p| ≤ 0.95)
    float warpAtomLevel[N]   = {};   // atom location ℓ (interior default 0.5 via the ctor)
    float warpAtomMass[N]    = {};   // atom mass m (neutral 0; authored m ≤ 0.9)
    float globalWarpTilt[LANES] = {};          // Macro's delegated-default tilt
    float globalWarpAtomLevel[LANES] = {};     // Macro's delegated-default atom location
    float globalWarpAtomMass[LANES] = {};      // Macro's delegated-default atom mass

    // ── THE GAP STAGE (PHASE_DISTRIBUTION_SPEC §4/§9 — Rodney: "We also add zero
    // density gaps with controls and modulation similar to atoms"): the gap's
    // AUTHORED (location, size) per (voice,lane), the atom arrays' twin in the SAME
    // block shape, plus Macro's global delegation defaults. NEUTRAL default: size 0
    // (width zero IS the neutral body — the gap's own p=0 early-out); the LOCATION's
    // interior default (0.5) is only reachable while size > 0 — the location store is
    // inert at width zero, exactly like the atom level at mass 0. Eval-side clamps
    // (clamp01 bounds + the 1−ε mass cap) live in WarpStages.hpp (the STORE trusts
    // its writer). The dual reading swaps the COMPOSED pair at eval time, so these
    // same params drive the dual's gap (the atom↔gap exchange is the reading's
    // property, not a second store).
    float warpGapLoc[N]        = {};   // gap location a (interior default 0.5 via the ctor; inert at size 0)
    float warpGapSize[N]       = {};   // gap size b−a (neutral 0; authored ≤ 1, eval clamps b ≤ 1)
    float globalWarpGapLoc[LANES]  = {};   // Macro's delegated-default gap location
    float globalWarpGapSize[LANES] = {};   // Macro's delegated-default gap size

    // ── THE DISTRIBUTION SEND-BUS PLANES (CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC:
    // "Macro distribution mods flow through per-voice sends; East blends
    // global+per-voice") — NOT store state, NOT persisted (the JSON codec below is
    // field-wise and never touches these): they are CV-published every block by the
    // SandsDistributionExpander's process() (unit deltas, jack volts /10 — the CvTerm
    // convention) and are re-published from live jacks, so nothing latches. The
    // fold site is resolveWarpCell (SandsModifierResolver.hpp — the ONE wiring, gated
    // on distDeltasActive so the zero-delta path stays BIT-IDENTICAL: the U2 oracle
    // and the test_warp_f_bank S4 pins run flag-off banks).
    //   distGlobalDelta[lane][p] — the CROSS-VOICE publisher's deltas (the Macro-side
    //     expander): reaches EVERY voice, delegated or not (it is Macro-side
    //     machinery — on delegated cells it modulates Macro's default; on own cells
    //     it arrives as the Macro-sent send). The future Macro send subpanel's
    //     per-voice amounts scale this term at the fold (identity until that
    //     subpanel lands — the spec's "per-voice scaling lives in the receiving
    //     side's send amounts").
    //   distOwnDelta[v][lane][p] — the PER-VOICE publisher's rows (the East-side
    //     expander addressing the shared-selected voice): reaches only that voice's
    //     own (non-delegated) cells — delegated cells mirror Macro entirely, the
    //     resolver's standing law. Param index order = the expander's
    //     P1/P2/TILT/LEVEL/MASS/WARP (0..5).
    //   distPublishSeq[2] — the publishers' per-block heartbeats (index 0 = the
    //     macro-role publisher, 1 = the per-voice publisher): Monsoon::process
    //     watches them and DROPS a publisher's plane when its heartbeat freezes
    //     (module deleted / bypassed — a vanished expander must not leave deltas
    //     latched; the publish overwrites, nothing latches, including death).
    //   distDeltasActive — the fold's single gate, maintained by Monsoon's set/clear
    //     accessors (true on any nonzero write; recomputed on every clear).
    //   distSend[v][lane][p] — the MACRO SEND SUBPANEL's per-voice send amounts (the
    //     spec's "per-voice scaling lives in the receiving side's send amounts"): how
    //     much of the Macro-sent global component voice v's (lane,p) cell receives.
    //     Authored store state (the subpanel's store-backed knobs write it; persisted
    //     via the DistSend key below) — NOT a CV plane, no watchdog. Default 1.0 =
    //     IDENTITY: the fold's pre-subpanel behaviour is preserved bit-exactly for
    //     every patch that never touches the subpanel (x·1.0f is exact IEEE, and a
    //     patch without the subpanel never writes the store). 0 = the voice receives
    //     none of the global distribution mod. The OWN component is NOT scaled — it
    //     is already per-voice by construction (the receiving side's own expander).
    // PHASE_DISTRIBUTION_SPEC §9 (Rodney: "zero density gaps with controls and
    // modulation similar to atoms"): the two gap params APPENDED to the fold axis —
    // 0 P1, 1 P2, 2 TILT, 3 LEVEL, 4 MASS, 5 WARP (the pre-gap order UNTOUCHED: the
    // delta planes' indices, the expander's mod-jack ids 0..41 and every pinned fold
    // test keep their meaning), 6 GAPLOC, 7 GAPSIZE. The only legacy surface is the
    // PERSISTED DistSend stride (6 -> 8 per (voice,lane) group) — WarpBank::parse
    // detects the old 672-entry layout and maps it per-group (identity on 0..5,
    // identity defaults on 6,7).
    static constexpr int DPARS = 8;
    static constexpr float kDistSendDefault = 1.0f;
    float    distGlobalDelta[LANES][DPARS]          = {};
    float    distOwnDelta[VOICES][LANES][DPARS]     = {};
    float    distSend[VOICES][LANES][DPARS];
    uint32_t distPublishSeq[2]                      = {};
    bool     distDeltasActive                       = false;

    // The neutral-default constructor: everything zero/identity except the atom LEVEL,
    // whose authored default is INTERIOR (warpStages::kAtomLevelDefault — the prompt's
    // "interior default; 0/1 = rail atom allowed"). A user-provided default ctor does
    // NOT affect trivial copyability (copy/move/assign/dtor stay trivial — the flat,
    // memcpy-persistable contract test_warp_f_bank pins), and `WarpBank{}` value-init
    // runs it, so every construction path (Monsoon's editor slice, parse's reset, the
    // tests) starts from the same documented neutral state.
    WarpBank() {
        for (int i = 0; i < N; ++i)     warpAtomLevel[i]     = (float)warpStages::kAtomLevelDefault;
        for (int l = 0; l < LANES; ++l) globalWarpAtomLevel[l] = (float)warpStages::kAtomLevelDefault;
        // the gap LOCATION's interior default (the atom level's twin; the SIZE stays
        // 0 = the neutral body, so a fresh bank is bit-identically gap-free):
        for (int i = 0; i < N; ++i)     warpGapLoc[i]     = (float)warpStages::kGapLocDefault;
        for (int l = 0; l < LANES; ++l) globalWarpGapLoc[l] = (float)warpStages::kGapLocDefault;
        // the send amounts' IDENTITY default (see the distSend block above): every
        // construction path — Monsoon's editor slice, parse's reset, the tests —
        // starts at full send, so the fold is the pre-subpanel fold until authored.
        for (int v = 0; v < VOICES; ++v)
            for (int l = 0; l < LANES; ++l)
                for (int p = 0; p < DPARS; ++p) distSend[v][l][p] = kDistSendDefault;
    }

    // The bank's 2-arg forwarder form: threads THIS bank's pool for drawn slots.
    double invCDF(const WarpF& f, double u) const { return dotModular::invCDF(f, u, pool); }
    double cdf  (const WarpF& f, double x) const { return dotModular::cdf(f, x, pool); }
    double pdf  (const WarpF& f, double x) const { return dotModular::pdf(f, x, pool); }

    // THE MIX — branchless. The default F (identity) and w=0 both make this u with no
    // branch; w=0 is a BIT-EXACT no-op in the stored float and w=1 is exactly
    // invCDF(F,u) (both pinned by tests). w ∈ [0,1] is the caller's contract — no clamp
    // in the MIX itself (clamping would add the very branch this exists to avoid); the
    // RESOLVER clamps the resolved w — a different layer (SandsModifierResolver.hpp).
    // Evaluated on the rebuild path only; the per-sample path reads a stored float.
    //
    // RESOLVER STEP 2: these four members are DECLARED here and DEFINED
    // out-of-line in SandsModifierResolver.hpp — the (f,w) pair they mix onto is
    // resolved by SandsModifierResolver<WarpPolicy> (base = this voice's own
    // slot, macroBase = Macro's global default, delegated = the topology-resolved
    // flag — see the struct comment above), and the definitions must live in the
    // one header where the bank AND the skeleton are both complete (the
    // include-order note there). The old hand-rolled `if (delegated)` hand-copy
    // is DELETED: value resolution has ONE authority. Signatures frozen (the
    // uniformity-harness U2 oracle + test_warp_f_bank S4 call them); effectiveF
    // now returns BY VALUE (the resolver produces a resolved cell; WarpF is
    // trivially copyable, pinned).
    double warpedValue(int v, int lane, double u) const;
    double warpedValue(int v, int lane, double u, bool delegated) const;
    WarpF effectiveF(int v, int lane, bool delegated) const;
    float effectiveW(int v, int lane, bool delegated) const;

    // ── The set-time drawn-curve pool (CC_PROMPT_02 step 3) ─────────────────────────────
    // Populated ONLY at set-time (UI commit / patch load). Identical curves SHARE a slot
    // (dedup on control points — metadata is editor attribution, not math), so only
    // voices using a DISTINCT drawn curve consume capacity. Slot reclamation is
    // CC_PROMPT_04 scope (the UI owns the edit lifecycle).
    static constexpr int kNoSlot = -1;   // putCurve: pool full, distinct curve refused

    uint16_t poolUsed = 0;               // high-water mark; slots [0, poolUsed) populated

    int  poolSize() const { return (int)poolUsed; }
    bool poolFull() const { return poolUsed >= kWarpPoolCap; }

    const MonotoneCurve& poolCurve(uint16_t idx) const {
        assert(idx < kWarpPoolCap);
        return pool[idx];
    }

    // Put a curve into the pool: reuse the slot of an IDENTICAL curve, else append while
    // capacity remains, else kNoSlot (the caller decides — see poolFull()).
    int putCurve(const MonotoneCurve& c) {
        for (uint16_t i = 0; i < poolUsed; ++i)
            if (samePoints(pool[i], c)) return i;
        if (poolUsed >= kWarpPoolCap) return kNoSlot;
        pool[poolUsed] = c;
        return (int)poolUsed++;
    }

    // The set-time commit path (the UI calls this when a curve is chosen/edited for a
    // slot): parametrics lower through lowerWarp; a drawn curve allocates (or reuses) a
    // pool slot. Pool full ⟹ IdentityWarp — the safe default; the UI checks poolFull().
    WarpF fromDistribution(const Distribution& d) {
        if (std::strcmp(d.typeName(), "drawn") == 0) {
            const int slot = putCurve(static_cast<const PchipDistribution&>(d).curve);
            if (slot != kNoSlot) return DrawnWarp{(uint16_t)slot};
            return IdentityWarp{};
        }
        return lowerWarp(d);
    }

    // ── JSON round-trip (CC_PROMPT_02 step 4) ──────────────────────────────────────────
    // MacroTapBank's pattern: the bank owns the codec; MonsoonPersistenceManager calls it
    // under the "editor" prefix, the unit test drives these same functions. Keys (prefix
    // "editor"): editorWarpW (112 depths) · editorWarpF (112 slot objects/nulls) ·
    // editorWarpPool (the drawn curves, KEYED BY POOL INDEX — each a .dmcurve object) ·
    // editorGlobalWarpW (7) · editorGlobalWarpF (7). Missing keys ⟹ the defaults survive
    // (all-IdentityWarp + w=0 — old patches unchanged).
    static void serialize(const WarpBank& b, json_t* root, const char* prefix) {
        const std::string pfx(prefix);
        json_t* jw = json_array();
        for (int i = 0; i < N; ++i) json_array_append_new(jw, json_real((double)b.warpW[i]));
        json_object_set_new(root, (pfx + "WarpW").c_str(), jw);
        json_t* jf = json_array();
        for (int i = 0; i < N; ++i) json_array_append_new(jf, warpToJson(b.warpF[i]));
        json_object_set_new(root, (pfx + "WarpF").c_str(), jf);
        json_t* jp = json_array();
        for (uint16_t i = 0; i < b.poolUsed; ++i) json_array_append_new(jp, b.pool[i].toJson());
        json_object_set_new(root, (pfx + "WarpPool").c_str(), jp);
        json_t* gw = json_array();
        for (int l = 0; l < LANES; ++l) json_array_append_new(gw, json_real((double)b.globalWarpW[l]));
        json_object_set_new(root, (pfx + "GlobalWarpW").c_str(), gw);
        json_t* gf = json_array();
        for (int l = 0; l < LANES; ++l) json_array_append_new(gf, warpToJson(b.globalWarpF[l]));
        json_object_set_new(root, (pfx + "GlobalWarpF").c_str(), gf);
        // FEEL-TEST stages (additive): tilt + atom (level, mass) per voice/lane + Macro's
        // global defaults. Missing keys on load ⟹ the neutral defaults survive (same
        // contract as every key above — old patches unchanged, bit-identical).
        json_t* jt = json_array();
        for (int i = 0; i < N; ++i) json_array_append_new(jt, json_real((double)b.warpTilt[i]));
        json_object_set_new(root, (pfx + "WarpTilt").c_str(), jt);
        json_t* jl = json_array();
        for (int i = 0; i < N; ++i) json_array_append_new(jl, json_real((double)b.warpAtomLevel[i]));
        json_object_set_new(root, (pfx + "WarpAtomLevel").c_str(), jl);
        json_t* jm = json_array();
        for (int i = 0; i < N; ++i) json_array_append_new(jm, json_real((double)b.warpAtomMass[i]));
        json_object_set_new(root, (pfx + "WarpAtomMass").c_str(), jm);
        json_t* gt = json_array();
        for (int l = 0; l < LANES; ++l) json_array_append_new(gt, json_real((double)b.globalWarpTilt[l]));
        json_object_set_new(root, (pfx + "GlobalWarpTilt").c_str(), gt);
        json_t* gl = json_array();
        for (int l = 0; l < LANES; ++l) json_array_append_new(gl, json_real((double)b.globalWarpAtomLevel[l]));
        json_object_set_new(root, (pfx + "GlobalWarpAtomLevel").c_str(), gl);
        json_t* gm = json_array();
        for (int l = 0; l < LANES; ++l) json_array_append_new(gm, json_real((double)b.globalWarpAtomMass[l]));
        json_object_set_new(root, (pfx + "GlobalWarpAtomMass").c_str(), gm);
        // THE GAP STAGE (additive, the same missing-key-⟹-defaults contract): the
        // authored (location, size) per voice/lane + Macro's global defaults.
        json_t* jgl = json_array();
        for (int i = 0; i < N; ++i) json_array_append_new(jgl, json_real((double)b.warpGapLoc[i]));
        json_object_set_new(root, (pfx + "WarpGapLoc").c_str(), jgl);
        json_t* jgs = json_array();
        for (int i = 0; i < N; ++i) json_array_append_new(jgs, json_real((double)b.warpGapSize[i]));
        json_object_set_new(root, (pfx + "WarpGapSize").c_str(), jgs);
        json_t* ggl = json_array();
        for (int l = 0; l < LANES; ++l) json_array_append_new(ggl, json_real((double)b.globalWarpGapLoc[l]));
        json_object_set_new(root, (pfx + "GlobalWarpGapLoc").c_str(), ggl);
        json_t* ggs = json_array();
        for (int l = 0; l < LANES; ++l) json_array_append_new(ggs, json_real((double)b.globalWarpGapSize[l]));
        json_object_set_new(root, (pfx + "GlobalWarpGapSize").c_str(), ggs);
        // THE MACRO SEND SUBPANEL's per-voice send amounts (additive, the same
        // missing-key-⟹-defaults contract): flat VOICES×LANES×DPARS, index
        // (v*LANES + lane)*DPARS + p — the bank's own stride convention.
        json_t* js = json_array();
        for (int v = 0; v < VOICES; ++v)
            for (int l = 0; l < LANES; ++l)
                for (int p = 0; p < DPARS; ++p)
                    json_array_append_new(js, json_real((double)b.distSend[v][l][p]));
        json_object_set_new(root, (pfx + "DistSend").c_str(), js);
    }

    // Load: RESETS the bank to defaults first (deterministic — never merges into stale
    // state), then overwrites from the keys actually present. Short arrays keep the
    // default tail (identity / w=0). A pool entry that fails MonotoneCurve's validation
    // resets (a slot referencing it then evaluates the reset curve — the 2-point
    // identity: safe). Returns true when BOTH per-voice arrays were full-length (the
    // MacroTapBank::parse convention, applied to the two 112-banks).
    static bool parse(json_t* root, const char* prefix, WarpBank& out) {
        out = WarpBank{};   // deterministic base: all-identity, w=0, empty pool
        const std::string pfx(prefix);
        bool full = true;

        if (json_t* jw = json_object_get(root, (pfx + "WarpW").c_str())) {
            if (json_is_array(jw)) {
                const size_t n = json_array_size(jw);
                for (int i = 0; i < N && (size_t)i < n; ++i) {
                    json_t* jv = json_array_get(jw, (size_t)i);
                    if (json_is_number(jv)) out.warpW[i] = (float)json_number_value(jv);
                }
                full = full && n >= (size_t)N;
            } else full = false;
        } else full = false;

        if (json_t* jf = json_object_get(root, (pfx + "WarpF").c_str())) {
            if (json_is_array(jf)) {
                const size_t n = json_array_size(jf);
                for (int i = 0; i < N && (size_t)i < n; ++i)
                    out.warpF[i] = warpFromJson(json_array_get(jf, (size_t)i));
                full = full && n >= (size_t)N;
            } else full = false;
        } else full = false;

        if (json_t* jp = json_object_get(root, (pfx + "WarpPool").c_str())) {
            if (json_is_array(jp)) {
                const size_t n = json_array_size(jp);
                const size_t m = n < (size_t)kWarpPoolCap ? n : (size_t)kWarpPoolCap;
                for (size_t i = 0; i < m; ++i) {
                    json_t* jc = json_array_get(jp, i);
                    if (json_is_object(jc)) out.pool[i].fromJson(jc);   // false ⟹ stays reset
                }
                out.poolUsed = (uint16_t)m;
            }
        }

        if (json_t* gw = json_object_get(root, (pfx + "GlobalWarpW").c_str())) {
            if (json_is_array(gw)) {
                const size_t n = json_array_size(gw);
                for (int l = 0; l < LANES && (size_t)l < n; ++l) {
                    json_t* jv = json_array_get(gw, (size_t)l);
                    if (json_is_number(jv)) out.globalWarpW[l] = (float)json_number_value(jv);
                }
            }
        }
        if (json_t* gf = json_object_get(root, (pfx + "GlobalWarpF").c_str())) {
            if (json_is_array(gf)) {
                const size_t n = json_array_size(gf);
                for (int l = 0; l < LANES && (size_t)l < n; ++l)
                    out.globalWarpF[l] = warpFromJson(json_array_get(gf, (size_t)l));
            }
        }
        // FEEL-TEST stages (additive): same short-array-keeps-the-default-tail shape as
        // the keys above; absent keys leave the neutral defaults (the ctor's reset).
        // These do NOT participate in the `full` return (that stays the two original
        // 112-banks' contract — additive keys never change an old patch's verdict).
        if (json_t* jt = json_object_get(root, (pfx + "WarpTilt").c_str())) {
            if (json_is_array(jt)) {
                const size_t n = json_array_size(jt);
                for (int i = 0; i < N && (size_t)i < n; ++i) {
                    json_t* jv = json_array_get(jt, (size_t)i);
                    if (json_is_number(jv)) out.warpTilt[i] = (float)json_number_value(jv);
                }
            }
        }
        if (json_t* jl = json_object_get(root, (pfx + "WarpAtomLevel").c_str())) {
            if (json_is_array(jl)) {
                const size_t n = json_array_size(jl);
                for (int i = 0; i < N && (size_t)i < n; ++i) {
                    json_t* jv = json_array_get(jl, (size_t)i);
                    if (json_is_number(jv)) out.warpAtomLevel[i] = (float)json_number_value(jv);
                }
            }
        }
        if (json_t* jm = json_object_get(root, (pfx + "WarpAtomMass").c_str())) {
            if (json_is_array(jm)) {
                const size_t n = json_array_size(jm);
                for (int i = 0; i < N && (size_t)i < n; ++i) {
                    json_t* jv = json_array_get(jm, (size_t)i);
                    if (json_is_number(jv)) out.warpAtomMass[i] = (float)json_number_value(jv);
                }
            }
        }
        auto loadGlobalStage = [&](const char* key, float* a) {
            if (json_t* j = json_object_get(root, (pfx + key).c_str())) {
                if (json_is_array(j)) {
                    const size_t n = json_array_size(j);
                    for (int l = 0; l < LANES && (size_t)l < n; ++l) {
                        json_t* jv = json_array_get(j, (size_t)l);
                        if (json_is_number(jv)) a[l] = (float)json_number_value(jv);
                    }
                }
            }
        };
        loadGlobalStage("GlobalWarpTilt", out.globalWarpTilt);
        loadGlobalStage("GlobalWarpAtomLevel", out.globalWarpAtomLevel);
        loadGlobalStage("GlobalWarpAtomMass", out.globalWarpAtomMass);
        auto loadStage112 = [&](const char* key, float* a) {
            if (json_t* j = json_object_get(root, (pfx + key).c_str())) {
                if (json_is_array(j)) {
                    const size_t n = json_array_size(j);
                    for (int i = 0; i < N && (size_t)i < n; ++i) {
                        json_t* jv = json_array_get(j, (size_t)i);
                        if (json_is_number(jv)) a[i] = (float)json_number_value(jv);
                    }
                }
            }
        };
        loadStage112("WarpGapLoc", out.warpGapLoc);        // additive: short array keeps the
        loadStage112("WarpGapSize", out.warpGapSize);      // default tail; absent key leaves
        loadGlobalStage("GlobalWarpGapLoc", out.globalWarpGapLoc);   // the ctor's neutral state
        loadGlobalStage("GlobalWarpGapSize", out.globalWarpGapSize);
        // DistSend (additive): short array keeps the identity tail; absent key leaves
        // the ctor's 1.0 fill. Does NOT participate in `full` (the two 112-banks'
        // contract — additive keys never change an old patch's verdict).
        // THE STRIDE MIGRATION (PHASE_DISTRIBUTION_SPEC, gap params appended): a
        // pre-gap patch stores the OLD 6-param layout — flat (v*LANES + lane)*6 + p.
        // Detected by the exact old length (112 groups x 6); each group's six values
        // map onto the new group's indices 0..5 IDENTITY (the appended axis kept
        // 0..5 untouched ON PURPOSE) and the gap sends (6,7) keep the ctor's
        // identity default. Anything else reads the new layout (short keeps tail).
        if (json_t* js = json_object_get(root, (pfx + "DistSend").c_str())) {
            if (json_is_array(js)) {
                const size_t n = json_array_size(js);
                constexpr size_t kGroups = (size_t)VOICES * LANES;
                const size_t oldLen = kGroups * 6, newLen = kGroups * DPARS;
                if (n == oldLen && oldLen != newLen) {
                    for (size_t g = 0; g < kGroups; ++g)
                        for (int p = 0; p < 6; ++p) {
                            json_t* jv = json_array_get(js, g * 6 + (size_t)p);
                            if (json_is_number(jv))
                                out.distSend[(int)(g / LANES)][(int)(g % LANES)][p]
                                    = (float)json_number_value(jv);
                        }
                } else {
                    size_t i = 0;
                    for (int v = 0; v < VOICES; ++v)
                        for (int l = 0; l < LANES; ++l)
                            for (int p = 0; p < DPARS; ++p, ++i) {
                                if (i >= n) break;
                                json_t* jv = json_array_get(js, i);
                                if (json_is_number(jv)) out.distSend[v][l][p] = (float)json_number_value(jv);
                            }
                }
            }
        }
        // ── THE BIDIRECTIONAL MIGRATION (the sign owns the reading now): a pre-sign
        // patch persisted the dual as a STORED state — the {"type":"dual"} envelope
        // riding warpF/globalWarpF with its depth w ≥ 0 in warpW/globalWarpW. The
        // SAME sound under the amended law is the BARE member at depth −w: the old
        // mix (1−w)·u + w·(dual quantile) IS (1−|−w|)·u + |−w|·(member CDF) — the
        // dual's quantile IS the member's CDF, |−w| == w, both bit-exact — so the
        // peel below cannot shift a loaded patch by even one bit. Every persisted
        // dual (the 112 per-voice slots AND Macro's 7 global defaults) is peeled
        // here, so eval sees bare members and ONE reading rule; the envelope codec
        // itself stays (the legacy writer's form, and the codec round-trip pins).
        auto peelDual = [](WarpF& f, float& w) {
            if (!isDualWarp(f)) return;
            f = dualMember(f);
            if (w != 0.f) w = -w;   // a depth-zero dual IS the bare member; −0 is never written
        };
        for (int i = 0; i < N; ++i) peelDual(out.warpF[i], out.warpW[i]);
        for (int l = 0; l < LANES; ++l) peelDual(out.globalWarpF[l], out.globalWarpW[l]);
        return full;
    }

private:
    // Exact control-point equality (the dedup key): same size, same x/y bit-for-bit.
    static bool samePoints(const MonotoneCurve& a, const MonotoneCurve& b) {
        const auto& pa = a.getPoints();
        const auto& pb = b.getPoints();
        if (pa.size() != pb.size()) return false;
        for (size_t i = 0; i < pa.size(); ++i)
            if (pa[i].x != pb[i].x || pa[i].y != pb[i].y) return false;
        return true;
    }
};

} // namespace dotModular

// RESOLVER STEP 2 (SandsModifierResolver): the bank's mix/effective member
// DEFINITIONS live in SandsModifierResolver.hpp — the only place both the bank
// and the resolver skeleton are complete. This include is the LAST line of the
// file ON PURPOSE: the header pair forms a guard-safe cycle (this header → the
// skeleton at the bottom; the skeleton → this header at the top), safe in BOTH
// include orders precisely because every cross-referencing definition lives in
// the skeleton, which always completes after this file's body. No code may
// follow this line.
#include "SandsModifierResolver.hpp"
