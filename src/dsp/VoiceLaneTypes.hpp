#pragma once
// ── VoiceSlot / LaneId — the STRONG per-voice/lane index types ─────────────────
// docs/design/CC_PROMPT_STRONG_TYPES_START.md (SEAM_ERADICATION_PLAN Stage 2):
// a WRONG per-voice/lane index must be a COMPILE ERROR, not a Rack-later bug.
//
//   VoiceSlot — the 16-wide per-voice STORAGE index (0 = V1/mono anchor … 15 = V16;
//               the ONE convention VoiceResolver documents). MINT-ONLY: the
//               constructor is private and VoiceResolver is the friend, so there is
//               no implicit int→VoiceSlot and no call-site VoiceSlot(0). The mono
//               ROLE (V1 = the anchor voice) is PRESERVED — but TYPED and searchable
//               (VoiceResolver::mono()), never an anonymous magic 0.
//   LaneId    — the canonical lane descriptor, 0..6 in PL_* order
//               (MEL0 OCT1 QMIX2 REST3 ACC4 VAR5 LEG6; == STRAND_* == editor lane,
//               the lane-index unification). Explicit construction only
//               (LaneId{PL_MELODY}); allLanes() iterates ALL 7 so a lane loop
//               cannot stop at 4 and silently drop VAR/LEG.
//
// Self-contained (no engine includes) so PatternEngine.hpp, SequencerEngine.hpp and
// manager/UI code can all include it. This is the ADDRESSING twin of the resolver
// (values) and the rate manager (timing): one authority, structure-enforced —
// "same thing decided in multiple places drifts" no longer compiles here.

#include <array>

namespace dotModular {

struct VoiceResolver;   // the minter (VoiceResolver.hpp) — sole friend below

// ── VoiceSlot ─────────────────────────────────────────────────────────────────
struct VoiceSlot {
    int v;   // 0 = V1 (mono anchor) … 15 = V16. THE per-voice storage convention.
private:
    // MINT-ONLY (the enforcement): construction is internal. VoiceResolver (the
    // friend) hands these out via mono() / slot(V) / allSlots() — a call site can
    // neither write VoiceSlot(0) nor pass a bare 0 where a VoiceSlot is required.
    explicit constexpr VoiceSlot(int raw) : v(raw) {}
    friend struct VoiceResolver;
    friend constexpr VoiceSlot monoVoiceSlot_();   // the kMonoVoiceSlot mint (below)

public:
    constexpr bool operator==(VoiceSlot o) const { return v == o.v; }
    constexpr bool operator!=(VoiceSlot o) const { return v != o.v; }
};

// The named default-anchor constant. Member declarations that default to the mono
// slot init through this (engine headers cannot include VoiceResolver.hpp — the
// resolver includes the engine, so the ONE default mint lives here instead). Every
// OTHER slot value still mints via VoiceResolver::slot()/allSlots(); this constant
// is only ever the mono default — the same value mono() returns.
inline constexpr VoiceSlot monoVoiceSlot_() { return VoiceSlot(0); }
inline constexpr VoiceSlot kMonoVoiceSlot = monoVoiceSlot_();

// ── LaneId ────────────────────────────────────────────────────────────────────
struct LaneId {
    int l;   // 0..6, PL_* order (MEL0 OCT1 QMIX2 REST3 ACC4 VAR5 LEG6)
    explicit constexpr LaneId(int lane) : l(lane) {}
    constexpr bool operator==(LaneId o) const { return l == o.l; }
    constexpr bool operator!=(LaneId o) const { return l != o.l; }
};

// Lane count the types cover. Identity with PL_LANES / EDITOR_LANE_COUNT /
// NUM_STRANDS is pinned where those enums are visible (PatternEngine.hpp).
inline constexpr int kLaneIdCount = 7;

// ALL lanes — the supported "loop over lanes" range. Iterating allLanes() cannot
// stop at 4 and silently drop VAR/LEG (the lane-drop bug class).
inline constexpr std::array<LaneId, (size_t)kLaneIdCount> kAllLanes = {
    LaneId(0), LaneId(1), LaneId(2), LaneId(3), LaneId(4), LaneId(5), LaneId(6),
};
inline constexpr const std::array<LaneId, (size_t)kLaneIdCount>& allLanes() { return kAllLanes; }

// ── Compile-time self-checks (the voiceResolverIndexSelfTest_ pattern) ─────────
static_assert(kLaneIdCount == 7, "7 lanes: MEL OCT QMIX REST ACC VAR LEG");
static_assert(allLanes().size() == 7, "allLanes covers exactly the 7 lanes");
static_assert(allLanes()[0] == LaneId(0) && allLanes()[6] == LaneId(6), "allLanes spans 0..6");
static_assert(allLanes()[2] == LaneId(2) && allLanes()[5] == LaneId(5), "QMIX 2 / VAR 5 in range");
static_assert(LaneId(0) == LaneId(0) && LaneId(0) != LaneId(6), "LaneId compares by value");
static_assert(kMonoVoiceSlot.v == 0, "kMonoVoiceSlot is the mono anchor (slot 0)");
// (VoiceSlot minting math is asserted in VoiceResolver.hpp, where the friend lives.)

} // namespace dotModular
