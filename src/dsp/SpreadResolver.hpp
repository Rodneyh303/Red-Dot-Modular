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
// Canonical arithmetic (agreed with project owner), for a NON-delegated lane. Contributions are
// SUMMED, then clamped ONCE at the end (end-clamp). This is order-independent and preserves the net of
// a large-then-opposite pair — an intermediate that overflows ±1 is not pinned before a later term can
// pull it back (which per-term clamping would do). Absent terms (unconnected CV / no macro) add 0.
//   eff = base + ownCv·2 + eastCv·2 + lerp(pre,post,tap_v)·send
//   eff = clamp(eff, -1, 1)                (single clamp at the end)
// For a DELEGATED lane (Mono cedes to Macro, per topology owner()==MACRO):
//   eff = clamp(macroBase + tappedDelta)      (track Macro; ignore own base/CV)
//
// Lane index is the SPREAD/engine lane (0=REST,1=MEL,2=OCT,3=ACC), matching
// spreadEffective[], sprId(), and SpreadInterp's lane arg. There are exactly 4
// spread lanes; VAR/LEG (editor 4,5) have no spread and are never resolved here.

namespace redDot {

struct SpreadResolver {
    static constexpr int   kSpreadLanes = 4;   // REST, MEL, OCT, ACC (engine order)
    static constexpr float kBipolarSpan = 2.f; // ±1 range → ×2 so full-depth CV reaches the ends

    // One additive CV/attenuverter modulation term. connected gates it (an
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
        float macroSend = 0.f;        // the send amount (0..1)            (sendId param)

        // PER-VOICE SEND-TAP (the P9b fix): the publish now carries BOTH endpoints and each
        // receiving VOICE selects its own tap — the pre/post choice moves downstream of
        // Macro's publish (which can no longer bake one globally-tapped delta).
        //   macroPreDelta  = raw cv·(hi-lo)          (tap=0 endpoint, PRE the attenuverter)
        //   macroPostDelta = cv·att·(hi-lo)          (tap=1 endpoint, POST — the old tappedDelta)
        //   macroTap      = THIS voice's tap (0..1; default 1.0 = POST, the old behaviour)
        // NOTE (test-first): these fields are the seam test_send_tap.cpp drives. effective()
        // does not read them YET — wiring the lerp in is what turns the bug assert green.
        float macroPreDelta  = 0.f;
        float macroPostDelta = 0.f;
        float macroTap       = 1.f;
    };

    // The per-voice tap selection: lerp between the two published endpoints. Pure; the
    // reader-side half of the publish-both-endpoints contract (voiceBlend = tappedDelta·send).
    static float tappedDelta(const Inputs& in) {
        return in.macroPreDelta + (in.macroPostDelta - in.macroPreDelta) * in.macroTap;
    }

    // The single authority. Pure: same inputs → same output, no hidden state.
    // Sums the present contributions and clamps ONCE at the end (end-clamp; see header note) — order-
    // independent, so a large-then-opposite pair keeps its net (no intermediate pinning). A term that
    // isn't present (unconnected CV / macro absent) is skipped entirely, so an absent term is a true
    // no-op — exactly as the manager's if (connected) / if (hasMacro) guards intend.
    static float effective(const Inputs& in) {
        if (in.delegated) {
            // Ceded lane: mirror Macro entirely, ignore own base/CV. The ceding VOICE's
            // tap selects the delta (P9b) — the caller fills macroTap with that voice's.
            return clampSpread(in.macroBase + tappedDelta(in));
        }
        float eff = in.base;
        if (in.ownCv.connected)  eff += in.ownCv.contribution();   // applyMono/MacroSprCV
        if (in.eastCv.connected) eff += in.eastCv.contribution();  // East V1 spread add
        if (in.macroPresent)     eff += tappedDelta(in) * in.macroSend;  // P9b per-voice tapped send
        return clampSpread(eff);
    }

    static float clampSpread(float v) { return std::max(-1.f, std::min(1.f, v)); }
};

} // namespace redDot
