#pragma once
#include "SandsModifierResolver.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// SpreadResolver — API-COMPATIBLE SHELL over SandsModifierResolver<SpreadPolicy>.
//
// Spread was the ONE tested delegation authority for "effective spread amount"
// (the bipolar value in [-1,1] fed to redDot::SpreadInterp::apply); the resolver
// step-1 extraction turned its proven arithmetic into the FIRST POLICY of the
// shared per-(voice,lane) skeleton. The math, the policy and the contract
// comments now live in SandsModifierResolver.hpp (SpreadPolicy); this header
// only keeps every historical name resolving so the production call sites
// (MonsoonSandsManager.cpp :393/:659/:723) and the tests link UNCHANGED:
//   SpreadResolver, ::CvTerm (alias to the shared, RAW term), ::Inputs,
//   ::effective, ::tappedDelta, ::clampSpread, ::kSpreadLanes, ::kBipolarSpan.
// effective()/tappedDelta() are one-line delegates, so outputs are byte-
// identical by construction — pinned same-BITS by test_resolver_spread_bridge.cpp
// (seeded fuzz vs a verbatim copy of the OLD formula) and by test_spread_resolver /
// test_send_tap, which pass unchanged.
//
// The ×2 note: kBipolarSpan's single definition now lives in SpreadPolicy (it is
// spread-specific — the bipolar CV reach); this shell aliases it. CvTerm::
// contribution() returns the RAW uV·atten product; SpreadPolicy::scaleCv applies
// the ×2 inside the resolver, so effective() outputs are unchanged.
//
// Layering (mirrors the topology/LOR resolvers, unchanged by the extraction):
//   • SandsTopology resolves OWNERSHIP/PRESENCE  → who owns the lane, who's here.
//     The caller wires topology's already-resolved booleans (delegated /
//     macroPresent) into Inputs; they are never re-derived here.
//   • The RESOLVER resolves the AMOUNT given that → a pure arithmetic function.
//   • SpreadInterp resolves the INTERPOLATION of the amount into the draw.
//
// Lane index is the SPREAD/engine lane (0=REST,1=MEL,2=OCT,3=ACC), matching
// spreadEffective[], sprId(), and SpreadInterp's lane arg. There are exactly 4
// spread lanes; VAR/LEG (editor 4,5) have no spread and are never resolved here.
// ─────────────────────────────────────────────────────────────────────────────

namespace redDot {

struct SpreadResolver {
    // Spread-specific lane count (the skeleton is lane-free; this is caller-side).
    static constexpr int kSpreadLanes = 4;   // REST, MEL, OCT, ACC (engine order)
    // The ×2 bipolar CV reach — single definition in SpreadPolicy, aliased here.
    static constexpr float kBipolarSpan = SpreadPolicy::kBipolarSpan;

    // The shared CV term (contribution() is RAW uV·atten; scaleCv scales).
    using CvTerm = redDot::CvTerm;
    // The resolver's Inputs (Value=float fields + Policy::Extra, empty for spread).
    using Inputs = SandsModifierResolver<SpreadPolicy>::Inputs;

    // One-line delegates — byte-identical by construction.
    static float tappedDelta(const Inputs& in) { return SandsModifierResolver<SpreadPolicy>::tappedDelta(in); }
    static float effective(const Inputs& in)   { return SandsModifierResolver<SpreadPolicy>::effective(in); }
    static float clampSpread(float v)          { return SpreadPolicy::constrain(v, {}); }
};

} // namespace redDot
