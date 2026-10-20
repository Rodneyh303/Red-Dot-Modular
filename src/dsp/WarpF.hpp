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
#include <memory>
#include <string>
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

    float warpW[N]          = {};   // per-voice-per-lane depth 0..1 (0 = no warp)
    WarpF warpF[N];                 // default-constructed = all IdentityWarp = uniform
    float globalWarpW[LANES] = {};  // Macro's default depth (the delegation source)
    WarpF globalWarpF[LANES];       // Macro's default curve (default = identity)
    MonotoneCurve pool[kWarpPoolCap];   // set-time drawn-curve pool (DrawnWarp.idx indexes it)

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
