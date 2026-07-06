#pragma once
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// SpreadResolver — the SINGLE definition of "effective spread amount" for a
// (voice, lane): the bipolar value in [-1,1] that is then fed to
// redDot::SpreadInterp::apply (the interpolation resolver). Previously this
// amount was computed inline at ≥3 sites (MonsoonSandsManager mono loop, Macro
// publishGlobal, Macro standalone) with the same arithmetic copied by hand, and
// its output (spreadEffective[]) carried a [4]-vs-[6] size split and an
// editor-vs-engine seed inconsistency. This is now the one place the amount is
// defined; callers pass raw values so it is pure and unit-testable outside Rack.
//
// Layering (mirrors the topology/LOR resolvers):
//   • SandsTopology resolves OWNERSHIP/PRESENCE  → who owns the lane, who's here.
//   • SpreadResolver resolves the AMOUNT given that → a pure arithmetic function.
//   • SpreadInterp resolves the INTERPOLATION of the amount into the draw.
// The caller wires topology's already-resolved booleans (delegated / eastPresent /
// macroPresent) into SpreadResolver::Inputs; SpreadResolver never re-derives them.
//
// Canonical arithmetic (agreed with project owner), for a NON-delegated lane. NOTE: the current
// manager clamps after EVERY additive term (applyMonoSprCV clamps, then East-add clamps, then
// Macro-add clamps), so the resolver replicates that STEP-WISE clamping to be behaviour-inert with
// the code it replaces. (A single end-clamp would be cleaner and differ only when an intermediate
// overflows then a later term pulls it back; changing that is a deliberate future decision, not part
// of this migration.)
//   eff = clamp(base + ownCv·2)          (applyMono/MacroSprCV: clamp after own CV)
//   eff = clamp(eff  + eastCv·2)         (East V1 spread add: clamp after)
//   eff = clamp(eff  + macroSendDelta·send)   (tapped Macro send: clamp after)
// For a DELEGATED lane (Mono cedes to Macro, per topology owner()==MACRO):
//   eff = clamp(macroBase + macroSendDelta)   (track Macro; ignore own base/CV)
//
// Lane index is the SPREAD/engine lane (0=REST,1=MEL,2=OCT,3=ACC), matching
// spreadEffective[], sprId(), and SpreadInterp's lane arg. There are exactly 4
// spread lanes; VAR/LEG (editor 4,5) have no spread and are never resolved here.

namespace redDot {

struct SpreadResolver {
    static constexpr int   kSpreadLanes = 4;   // REST, MEL, OCT, ACC (engine order)
    static constexpr float kBipolarSpan = 2.f; // ±1 range → ×2 so full-depth CV reaches the ends

    // One additive CV/attenuverter modulation term. `connected` gates it (an
    // unconnected jack contributes nothing). Voltage is already /10 (unit-scaled)
    // by the caller, matching the existing applyMono/MacroSprCV convention.
    struct CvTerm {
        bool  connected = false;
        float unitVoltage = 0.f;   // jack voltage / 10
        float atten       = 0.f;   // attenuverter -1..1
        float contribution() const { return connected ? unitVoltage * atten * kBipolarSpan : 0.f; }
    };

    struct Inputs {
        float base = 0.f;          // the lane's base spread param (bipolar -1..1)

        // Topology-derived (caller fills from SandsTopology — NOT re-derived here):
        bool  delegated = false;   // this lane ceded to Macro (owner()==MACRO) → track Macro
        bool  macroPresent = false;

        // Own CV (Mono's sprCv jack, or Macro's macroCv jack — same shape):
        CvTerm ownCv;
        // East V1 spread CV add (only meaningful on the mono path; leave default off elsewhere):
        CvTerm eastCv;

        // Macro contributions (used both for the delegated override and the additive send):
        float macroBase = 0.f;        // Macro's base spread for the lane  (macroBase[lane][3])
        float macroSendDelta = 0.f;   // Macro's tapped send delta         (macroSendDelta[lane][3])
        float macroSend = 0.f;        // the send amount (0..1)            (sendId param)
    };

    // The single authority. Pure: same inputs → same output, no hidden state.
    // Clamps STEP-WISE to match the manager code it replaces (see header note). A term that isn't
    // present (unconnected CV / macro absent) is skipped entirely — including its clamp — exactly as
    // the manager's `if (connected)` / `if (hasMacro)` guards do, so an absent term is a true no-op.
    static float effective(const Inputs& in) {
        if (in.delegated) {
            // Ceded lane: mirror Macro entirely, ignore own base/CV.
            return clampSpread(in.macroBase + in.macroSendDelta);
        }
        float eff = in.base;
        if (in.ownCv.connected)  eff = clampSpread(eff + in.ownCv.contribution());   // applyMono/MacroSprCV
        if (in.eastCv.connected) eff = clampSpread(eff + in.eastCv.contribution());  // East V1 spread add
        if (in.macroPresent)     eff = clampSpread(eff + in.macroSendDelta * in.macroSend);  // tapped send
        return eff;
    }

    static float clampSpread(float v) { return std::max(-1.f, std::min(1.f, v)); }
};

} // namespace redDot
