#pragma once
#include <algorithm>
#include <cmath>     // std::lround — resolveDirItem's round+cast (the moved direction algebra)
#include <concepts>

#include "GaussianCopula.hpp"   // redDot::copula::mix2 — the polarity primitive (leaf; no cycles)
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
//
// Step 3 (third rider): LorPolicy — MonsoonExpanderManager's combineLOR (the
// design doc's "near-verbatim DUPLICATE", 15 send/tap call sites) rides this
// skeleton via resolveLorItem below: the ownership ternary, the P9b tap-lerp
// send blend, the end-clamp and the (int) truncation live HERE; the manager
// lambda keeps only the port/store reads. eastLorVal does NOT move — its
// CV-span math is interleaved with module port reads, and its INTERMEDIATE
// clamp is load-bearing (see LorPolicy). Gate: test_resolver_lor_bridge.cpp
// fuzzes seeded inputs against a verbatim copy of the OLD combineLOR algebra
// and asserts int EQUALITY + same-BITS of the pre-cast float. BYTE-IDENTICAL,
// step-1 discipline (no gate flips this time).
//
// Step 4 (fourth rider — the LAST hand-rolled delegation shape): DirPolicy —
// BOTH direction pushes (MonsoonExpanderManager's poly laneDirVPending_ block
// + the mono laneDirPending_ field arm) ride the skeleton via resolveDirItem
// below: the ownership selection, the [0,3] end-clamp and the (int)lround
// live HERE; the manager keeps only the store reads. PURE SELECTION — the
// resolved value is bit-identical to the old local ternary (the dead
// `l < POLY_LANES` else-arm dies with it; NUM_STRANDS == POLY_LANES == 7).
// Step 4 also widens advancePlayhead's promotion loops to slot 0, closing
// DIR-SLOT0-ROW-INERT — the LAST manifest entry; the uniformity harness's
// knownSeams() goes EMPTY. Gate: test_resolver_dir_bridge.cpp — EXHAUSTIVE
// over the tiny selection space (delegated × ownDir × globalDir, ~242 grid
// cases) + a seeded fuzz, asserting int equality AND same-BITS of the
// pre-round float vs a verbatim copy of the old line.
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

// ── FOLLOW-CA TARGET SELECTION (the fifth rider — Rodney's final architecture,
//    CC_PROMPT_east_spread_followCA_display_bug_START) ─────────────────────────
// CA-spread target selection was the GAP in the resolver scope: it lived in
// SpreadInterp::applyMono/applyPoly (the AUDIO view) and was never taught to the
// DISPLAY accessor (effectiveProbability) — which hardcoded the V1-anchor twin and
// drifted (the East follow-CA bars bug: spread→+1 showed V1, spread-0 showed the
// CA-remapped base). ONE decision, two read-rates: BOTH views call THIS entry —
// the audio fills the endpoints from the LIVE slewed*, the display from the PUB
// snapshots (+ the pin-gated live preRemap — a µs, self-healing window; no new
// pub arrays). Convergence-at-step is STRUCTURAL: same function, same inputs at
// the step boundary → same value — the bars CANNOT show something the engine
// won't read. (This rider does NOT fit the delegation skeleton — it selects
// endpoints per voice, it does not delegate Macro↔East — so it is a sibling
// resolver entry, not a SandsModifierPolicy.)
struct FollowCaSpread {
    uint8_t mode = 0;               // pe.spreadTargetMode[lane]: 0=anchor V1, 1=follow CA
    float   ownAnchor = 0.f;        // mode 0: the voice's own (post-remap) value
    float   ownPreRemap = 0.f;      // mode 1: the voice's PRE-remap value (its TRUE own)
    float   targetV1 = 0.f;         // mode 0: V1's value (the anchor)
    float   targetPostRemap = 0.f;  // mode 1: the voice's post-remap value (the CA source's material)
    float   amount = 0.f;           // the bipolar spread amount
};

// The polarity primitive — ONE definition (was SpreadInterp::interpolate's body;
// interpolate() delegates here). spread==0 → own exactly (bit-identity at zero);
// positive self-target → a no-op (blending a value with itself in normal space
// would concentrate it); else the copula mix (negative → toward the complement).
inline float spreadMixEndpoints(float own, float target, float amount) {
    if (amount == 0.0f) return own;
    if (amount > 0.0f && target == own) return own;   // V1 self-target no-op
    return (float)redDot::copula::mix2((double)own, (double)target, (double)amount);
}

// THE follow-CA/anchor endpoint-selection decision. Pure; rate-agnostic.
inline float resolveFollowCaSpread(const FollowCaSpread& in) {
    const bool followCA = (in.mode == 1);
    const float own = followCA ? in.ownPreRemap    : in.ownAnchor;
    const float tgt = followCA ? in.targetPostRemap : in.targetV1;
    return spreadMixEndpoints(own, tgt, in.amount);
}

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
    // FEEL-TEST STAGES (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START): rail tilt + injected
    // atom ride the cell so they resolve from the SAME source as (f, w) — own slot vs
    // Macro's global default — through this resolver's ONE selection (no re-derived
    // delegation anywhere else). The policy ops below touch v.w only, so these ride
    // along untouched; NEUTRAL NSDMIs keep the default cell the no-warp state the
    // bridge test pins. Eval happens in WarpBank::warpedValue (the bottom of this file).
    float tilt      = 0.f;   // rail tilt p (neutral 0)
    float atomLevel = 0.5f;  // atom location ℓ (interior default; inert at mass 0)
    float atomMass  = 0.f;   // atom mass m (neutral 0)
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

// ── LOR, the third policy — ONE item's LEN/OFF/ROT window value ───────────────
// Value is a SINGLE float (one item's value), NOT a len/off/rot triple: the
// manager resolves per item — the 15 call sites each pass their own (lo,hi)
// window and jack coords, so a triple would fake a batch the call structure
// doesn't have. Extra carries that per-item window {lo, hi} to constrain.
//
// LOR's CV-span math (cv·att·(hi−lo) + the INTERMEDIATE clamp) does NOT live
// here: it is interleaved with module port reads inside the manager's
// eastLorVal (eastVisual->inputs[cvId(r,c)], getMacroAtten, getPolyVoltage),
// and port reads cannot enter a pure policy — the design doc never asked for
// a port-abstraction layer. The wiring (resolveLorItem below) therefore
// leaves ownCv/eastCv default-disconnected; scaleCv/addCv exist to satisfy
// the concept and are unused by LOR's resolution path. The intermediate clamp
// is LOAD-BEARING: poly East-owned is clamp(clamp(base+cv) + blend) —
// sum-once would differ (hi=16, base+cv=21, blend=−3 → old 13, sum-once 16)
// — so it stays verbatim inside eastLorVal while the resolver's
// end-clamp-once applies to the blend sum, exactly like the old final
// clamp(base+blend).
//
// addSend is the Macro send blend, NOT clamped (end-clamp-once law).
// applyDelegated is the ceded branch (macroBase + delta). constrain is the
// max/min form — the NaN convention, one convention for the whole resolver
// file (same as SpreadPolicy/WarpPolicy; NOT std::clamp).
struct LorPolicy {
    using Value = float;         // ONE item's window value (pre-cast)
    struct Extra {               // the per-item window: LEN {1,16}, OFF/ROT {0,15}
        float lo = 0.f;
        float hi = 0.f;
    };

    static float scaleCv(float raw)                                { return raw; }   // identity — LOR's CV math stays inside eastLorVal (port reads)
    static float addCv  (float v, float c)                         { return v + c; }  // concept-only: ownCv/eastCv stay disconnected in the wiring
    static float addSend(float v, float s)                         { return v + s; }  // the blend term, UNCLAMPED — end-clamp-once
    static float applyDelegated(float mb, float td, const Extra&)  { return mb + td; }
    static float constrain(float v, const Extra& e)                { return std::max(e.lo, std::min(e.hi, v)); }
};

// The concept must BITE here too (the located-error enforcement, same as spread/warp).
static_assert(SandsModifierPolicy<LorPolicy>);

// ── DIRECTION, the fourth policy — the LAST hand-rolled delegation shape ──────
// ONE (voice,lane)'s lane direction. Value is the RAW 0..3 store direction
// FLOAT — 0=Forward, 1=Reverse, 2=Pendulum, 3=PingPong (SequencerEngine::
// LaneDir, enum class : uint8_t, SequencerEngine.hpp:214) — NOT LaneDir
// itself: this header must stay engine-include-free (layering: engines
// include dsp-root, never the reverse; a second direction enum here would be
// worse than one documented float+mapping).
//
// The old algebra selects on the RAW floats and rounds ONCE after selection:
//   (int)std::lround(math::clamp(dirVal, 0.f, 3.f))   with dirVal = own-or-global.
// lround is monotone so round-then-select ≡ select-then-round, but the old
// order is kept VERBATIM (the byte-identical law): select in the resolver,
// round+cast in the wiring helper (resolveDirItem below), exactly like the
// old line. Do NOT pre-round the sources.
//
// PURE SELECTION, no blend — direction has no delta term (no CV/send/tap
// jack exists for it). applyDelegated is EXPLICITLY `return mb;` — td is
// structurally ignored, unlike spread/warp/LOR's mb + td. A future
// direction-modulation feature would be a NEW policy member (e.g. a depth
// input in Extra), never a silent td. scaleCv/addCv/addSend exist to satisfy
// the concept and are structurally UNUSED by direction's resolution path:
// the wiring leaves ownCv/eastCv disconnected and macroSend = 0 — but the
// policy must not DEPEND on that (the bridge test's full-Inputs fuzz pins
// the concept members anyway).
//
// constrain is the max/min form — the NaN convention, one convention for the
// whole resolver file (same as SpreadPolicy/WarpPolicy/LorPolicy; NOT
// std::clamp). Identity on valid inputs: the old code clamped the same
// [0,3] AFTER selection; the resolver owns that clamp now (end-clamp-once).
struct DirPolicy {
    using Value = float;         // the RAW store direction 0..3 (the float+mapping above)
    struct Extra {};             // direction needs no extra inputs

    static float scaleCv(float raw)                                { return raw; }   // identity — concept-only: no direction CV exists; the wiring leaves ownCv/eastCv disconnected
    static float addCv  (float v, float c)                         { return v + c; }  // concept-only (same)
    static float addSend(float v, float s)                         { return v + s; }  // concept-only: the wiring leaves macroSend = 0
    // PURE SELECTION — td structurally ignored (NOT mb + td like spread/warp/LOR:
    // direction has no delta term; a future direction-modulation feature would be
    // a NEW policy member, never a silent td).
    static float applyDelegated(float mb, float /*td*/, const Extra&) { return mb; }
    static float constrain(float v, const Extra&)                  { return std::max(0.f, std::min(3.f, v)); }
};

// The concept must BITE here too (the located-error enforcement, same as spread/warp/LOR).
static_assert(SandsModifierPolicy<DirPolicy>);

// ── resolveLorItem — the ONE LOR wiring, header-side (resolveWarpCell's ────────
// counterpart for LOR; type-pure — no manager types — so it lives here beside
// its policy rather than down in the dotModular section the warp wiring needs
// for the bank). Fills SandsModifierResolver<LorPolicy>::Inputs EXACTLY like
// the old combineLOR algebra (MonsoonExpanderManager.cpp, pre-step-3),
// bit-for-bit; the port reads (eastLorVal) and the store reads (send/tap)
// stay call-site:
//
//   delegated = !ownerEast && macroPresent
//     reproduces the old ternary `(ownerEast || !macroPresent) ? eastVal
//     : macroBase + macroCVDelta` EXACTLY — the no-Macro case (ownerEast ==
//     false && macroPresent == false) takes the FREE path with base = eastVal.
//     Do NOT write `delegated = !ownerEast`: that breaks the no-Macro case.
//
//   free path: base = eastVal (the CALLER-computed eastLorVal result); when
//     macroPresent, macroSend/macroPreDelta/macroPostDelta/macroTap pass
//     through and the resolver does addSend(v, tappedDelta(in)·send). The
//     skeleton's lerp (pre + (post−pre)·tap) is BIT-IDENTICAL to
//     StraitsSandsMacroVisual::tappedDelta — same expression order — so
//     passing pre/post/tap separately reproduces the old Visual call
//     bit-exactly. The header never calls the Visual (it stays Rack-free;
//     the CALLER reads the published arrays and this voice's tap).
//
//   delegated path: macroBase passes through and the tap is NEUTRALIZED —
//     pre = post = macroCvDelta (any tap): the lerp yields macroCvDelta
//     bit-exactly ((post−pre) == +0.0f, ·tap == +0.0f, pre+0.0f == pre), so
//     the ONE shared applyDelegated(mb, td) reproduces the old
//     `macroBase + macroCVDelta` (the old delegated delta was the POST delta
//     directly — no lerp, no send) instead of a special case.
//
// resolveLorValue returns the PRE-CAST float (the bridge test pins this layer
// same-BITS); resolveLorItem adds the (int) cast — TRUNCATION toward zero
// after the clamp, NOT round (the mono path rounds; that difference is why
// the cast lives HERE, keeping the call sites one-token thin, and not inside
// constrain).
inline float resolveLorValue(bool ownerEast, bool macroPresent,
                             float eastVal,
                             float macroBase, float macroCvDelta,
                             float macroSend, float pre, float post, float tap,
                             float lo, float hi) {
    SandsModifierResolver<LorPolicy>::Inputs in;
    in.delegated    = !ownerEast && macroPresent;
    in.macroPresent = macroPresent;
    in.extra        = LorPolicy::Extra{lo, hi};
    if (in.delegated) {
        in.macroBase      = macroBase;
        in.macroPreDelta  = macroCvDelta;   // NEUTRALIZE the tap: pre == post
        in.macroPostDelta = macroCvDelta;   // → the lerp yields macroCvDelta bit-exactly
    } else {
        in.base = eastVal;
        if (macroPresent) {
            in.macroSend      = macroSend;
            in.macroPreDelta  = pre;
            in.macroPostDelta = post;
            in.macroTap       = tap;
        }
    }
    return SandsModifierResolver<LorPolicy>::effective(in);
}

inline int resolveLorItem(bool ownerEast, bool macroPresent,
                          float eastVal,
                          float macroBase, float macroCvDelta,
                          float macroSend, float pre, float post, float tap,
                          float lo, float hi) {
    return (int)resolveLorValue(ownerEast, macroPresent, eastVal, macroBase,
                                macroCvDelta, macroSend, pre, post, tap, lo, hi);
}

// ── resolveDirItem — the ONE direction wiring, header-side (resolveWarpCell's /
// resolveLorItem's counterpart; type-pure — no manager/engine types — so it
// lives here beside its policy). Fills SandsModifierResolver<DirPolicy>::Inputs
// with delegated/base/macroBase only; every other Input stays at defaults
// (ownCv/eastCv disconnected, macroSend 0, pre/post/tap neutral — macroPresent
// is irrelevant: the delegated branch never reads send terms and the free path
// adds none). BOTH direction pushes call this (MonsoonExpanderManager's poly
// laneDirVPending_ block and the mono laneDirPending_ field arm); the mono
// param arm is a SOURCE-TYPE dispatch (which module owns the value), not a
// Macro/East selection, and stays call-site.
//
// resolveDirValue returns the PRE-ROUND float (the bridge test pins this layer
// same-BITS); resolveDirItem adds the round+cast — (int)std::lround AFTER the
// clamp, the old order VERBATIM (the round+cast is part of the MOVED algebra;
// keeping it here keeps both pushes one-token thin).
inline float resolveDirValue(bool delegated, float ownDir, float macroGlobalDir) {
    SandsModifierResolver<DirPolicy>::Inputs in;
    in.delegated = delegated;
    in.base      = ownDir;          // the voice's own store direction (East owns)
    in.macroBase = macroGlobalDir;  // Macro's global direction (Macro owns)
    return SandsModifierResolver<DirPolicy>::effective(in);
}

inline int resolveDirItem(bool delegated, float ownDir, float macroGlobalDir) {
    return (int)std::lround(resolveDirValue(delegated, ownDir, macroGlobalDir));
}

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

// ── the distribution send-bus fold (CC_PROMPT_SANDS_DISTRIBUTION_EXPANDER_SPEC) ─────
// One cell's share of the expander-published unit deltas: p1/p2 re-mint the member
// (window-clamped against warpParamSpec — the same table the knobs bind, so a fold
// and a lock can never disagree; kinds without the param are a no-op, the spec's
// unused-param ruling), tilt/ℓ/m ADD (the eval-side clamps in WarpStages catch
// overshoot — the same "the STORE trusts its writer" split the authored params use),
// w adds through the policy's clamp01. glob = the Macro-sent component (the
// cross-voice expander); own = the East-local component (the per-voice expander's
// row for THIS voice, nullptr on delegated cells — delegated cells mirror Macro
// entirely, the resolver's standing law). The future Macro send subpanel's per-voice
// amounts scale the glob term here (identity until that subpanel lands — the spec's
// "per-voice scaling lives in the receiving side's send amounts").
inline void foldDistDeltas(redDot::WarpCell& c, const WarpBank& b,
                           const float* glob, const float* own) {
    const int kind = (int)c.f.index();
    for (int p = 0; p < WarpBank::DPARS; ++p) {
        const float d = glob[p] + (own ? own[p] : 0.f);
        if (d == 0.f) continue;
        switch (p) {
            case 0: case 1: {
                const WarpParamSpec win = warpParamSpec(kind, p);
                if (!win.present) break;                     // unused-param: harmless no-op
                const double base = warpParamValue(c.f, p);
                c.f = warpWithParam(c.f, p,
                    std::clamp(base + (double)d, (double)win.lo, (double)win.hi));
                break;
            }
            case 2: c.tilt      += d; break;
            case 3: c.atomLevel += d; break;
            case 4: c.atomMass  += d; break;
            default: c.w = redDot::WarpPolicy::clamp01(c.w + d); break;   // 5 = WARP
        }
    }
}

// The one wiring: this voice's own slot is the base, Macro's global default is
// the macroBase, the topology-resolved flag is the delegation. No warp
// CV/send/tap plumbing exists yet, so every other Input stays absent — the
// resolver's output is PURE SELECTION, bit-identical to the old branch. When
// warp grows the spread send/tap shape, ONLY this wiring changes.
// THE SEND-BUS EXCEPTION (the spec's decided design): the distribution deltas ride
// IN the bank (WarpBank::distGlobalDelta/distOwnDelta), so this frozen 4-arg
// signature needs no change — the fold happens HERE, gated on distDeltasActive so
// a flag-off bank (the U2 oracle, every pinned bridge, every patch without the
// expander) takes the exact old path, BIT-IDENTICAL. The Macro-sent component
// reaches BOTH branches (it is Macro-side machinery); the own component reaches
// only the voice's own cells.
inline redDot::WarpCell resolveWarpCell(const WarpBank& b, int v, int lane, bool delegated) {
    redDot::SandsModifierResolver<redDot::WarpPolicy>::Inputs in;
    const int i = WarpBank::index(v, lane);
    in.base      = redDot::WarpCell{b.warpF[i], b.warpW[i],
                                    b.warpTilt[i], b.warpAtomLevel[i], b.warpAtomMass[i]};
    in.macroBase = redDot::WarpCell{b.globalWarpF[lane], b.globalWarpW[lane],
                                    b.globalWarpTilt[lane], b.globalWarpAtomLevel[lane],
                                    b.globalWarpAtomMass[lane]};
    in.delegated = delegated;
    if (b.distDeltasActive)
        foldDistDeltas(delegated ? in.macroBase : in.base, b,
                       b.distGlobalDelta[lane], delegated ? nullptr : b.distOwnDelta[v][lane]);
    return redDot::SandsModifierResolver<redDot::WarpPolicy>::effective(in);
}

// THE MIX, branchless, on the RESOLVED pair (see the declaration site in
// WarpF.hpp for the w=0/identity bit-exactness pins). The 4-arg form is the
// delegation-resolved entry (PatternEngine::warpMixCell — the ONLY production
// caller); the 2-arg form is the same authority with delegated=false.
//
// FEEL-TEST STAGES (CC_PROMPT_TILT_AND_RAIL_FEELTEST_START): the member's invCDF is
// composed member → tilt → atom (the prompt's FEATURE 1/2 — "out = R_p(member.invCDF(u))"
// then the atom over the TILTED member's CDF G; both stages' closed forms + the verified
// algebra live in WarpStages.hpp), and the existing branchless mix runs over the
// COMPOSED quantile:
//     out = (1-w)·u + w·H⁻¹(u),    H⁻¹ = atom ∘ R_p ∘ F⁻¹
// so the stages are the member-level feature the design notes specify (the palette's
// shared tilt stage; the atom's "branch in invCDF, one compare + a rescale"), heard
// through the SAME depth blend as every curve — at w=1 the engine evaluates exactly the
// popup's displayed composition. The NEUTRAL early-out (tilt==0 AND mass==0 — every
// pinned bridge's state) runs the ORIGINAL mix line verbatim: BIT-IDENTICAL, by the
// early-outs' construction, never by trusting R_0 ≡ id arithmetic.
inline double WarpBank::warpedValue(int v, int lane, double u, bool delegated) const {
    const redDot::WarpCell c = resolveWarpCell(*this, v, lane, delegated);
    if (c.tilt == 0.f && c.atomMass == 0.f)
        return (1.0 - c.w) * u + c.w * invCDF(c.f, u);   // neutral: the pre-feel-test mix
    const double p    = warpStages::clampTilt((double)c.tilt);
    const double ell  = warpStages::clampLevel((double)c.atomLevel);
    const double m    = warpStages::clampMass((double)c.atomMass);
    const double q    = warpStages::invCDF(
        u, p, ell, m,
        [this, &c](double uu) { return invCDF(c.f, uu); },     // the member quantile F⁻¹
        [this, &c](double x)  { return cdf(c.f, x); });        // the member CDF F
    return (1.0 - c.w) * u + c.w * q;
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
