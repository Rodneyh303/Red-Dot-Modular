#pragma once
// src/ui/RateManager.hpp — THE COMPUTE-RATE AUTHORITY (CC_PROMPT_RATE_MANAGER_START /
// RATE_MANAGER_AND_RESOLVER_UI_DESIGN.md / CC_PROMPT_RATE_MANAGER_SPEC.md).
// One authority for WHEN in-scope sequencing work runs: producers DECLARE
// (rate, fingerprint, work); the manager owns dispatch. There is NO public
// "run now" — the declaring site hands the work over and never keeps it callable.
//
// STAGE 0 (this header's first landing): the LEAN core — the Rate identity, tier
// tick/generation state, and the producer mechanism (declareProducer + dispatch).
// The per-mode RateChains + tier SOURCES (AudioClock/VideoClock/WallClockDiv/
// PhaseMap/GateEdge/TickCount) grow here in the later migration steps; the API is
// already the target shape so no producer is rewritten when they land.
//
// DESIGN CONTRACT (the enforced properties):
//   1. ONE producer per quantity: work is a lambda handed to the manager. Duplicate
//      computation and "call the heavy fn yourself" are un-writable.
//   2. The manager dispatches a producer ONLY on its declared tier's tick, at most
//      ONCE per tier generation (per tick).
//   3. SKIP-ON-UNCHANGED — the SweepSig pattern, now the ONE change-detection
//      mechanism (publishGlobal + SweepSig fold into it): on each tick the producer's
//      input FINGERPRINT is snapshotted and memcmp'd against the shadow of its last
//      RUN; identical inputs → the produce is SKIPPED (the published state is already
//      correct). The first tick always runs (nothing to compare against). A moving
//      input re-runs — correct: the value really changed. THE FINGERPRINT MUST COVER
//      EVERY INPUT THE PRODUCE READS; a missed input = stale state.
//   4. RATES ARE INDEPENDENT CLOCKS — the enum is an IDENTITY, not an ordering and
//      not a nested subset. Freshness comes from published state (the produce's
//      side effects), never from tick-nesting. Frame never drives another tier;
//      Control is a wall-clock division (the SR-aware controlDivider pattern this
//      generalizes).
//   5. Rack-free + header-only so the headless rig drives the REAL manager (the
//      ModuleLookupCore pattern).
//
// Stage 0 tier drive: tiers are fed EXTERNALLY by the existing edges (advance()) —
// a pure wrapper, behaviour-identical. The migration converts one site at a time and
// DELETES the raw primitives at the end; deletion is the compile-time enforcement.
//
// THREADING: single-threaded use (the owning module's process()). The cross-module
// shared-manager step (design property 3) reuses the adjacency authority's reach;
// until then this instance is Monsoon-internal.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

namespace redDot {

// Identity only. NOT comparable for "faster/slower" — that framing was superseded by
// the per-mode rate chains (see the design doc's CORRECTION section).
enum class Rate { Sample, Frame, Control, PPQN, Step, Phrase };

// ── Per-mode rate chains — the timing topology in ONE place ────────────────────
// Rates are INDEPENDENT clocks; a chain declares which tiers a MODE uses and each
// tier's SOURCE. GATE's Step source is the gate-edge detector (PPQN plays no role in
// gate-mode STEP derivation — structural absence); the PPQN grid still serves
// gate-width counting in every mode (a consumer, not the step source). Frame/Control
// are always present, never divide the others. Mode switch = the active chain.
enum class RateSource { AudioClock, VideoClock, WallClockDiv, Periodic, PhaseMap, GateEdge, TickCount };
struct RateTierDecl { Rate rate; RateSource source; };
inline constexpr RateTierDecl kChainClock[] = {
    {Rate::Sample, RateSource::AudioClock}, {Rate::PPQN, RateSource::Periodic},
    {Rate::Step,  RateSource::TickCount},   {Rate::Phrase, RateSource::TickCount},
};
inline constexpr RateTierDecl kChainPhase[] = {
    {Rate::Sample, RateSource::AudioClock}, {Rate::PPQN, RateSource::PhaseMap},
    {Rate::Step,  RateSource::TickCount},   {Rate::Phrase, RateSource::TickCount},
};
inline constexpr RateTierDecl kChainGate[] = {
    {Rate::Sample, RateSource::AudioClock}, {Rate::Step, RateSource::GateEdge},
    {Rate::Phrase, RateSource::TickCount},
    // NO PPQN tier — structural absence in gate-mode step derivation.
};

class RateManager {
public:
    // Fingerprint budget: the Stage-0 sync-population image is ≈ 2.5 KB (its inputs PLUS
    // the produced cells it self-heals — see MonsoonExpanderManager). 4 KiB headroom.
    static constexpr size_t kMaxFingerprint = 4096;

    // ── Tier state ─────────────────────────────────────────────────────────────
    // Record whether tier r fired THIS sample; bump its generation when it did.
    // Stage 0: the owner feeds the EXISTING edge (e.g. controlDivider.process()).
    // Later steps replace the feed with the tier's own source from the active chain.
    void advance(Rate r, bool tickedNow) {
        Tier& t = tiers_[index(r)];
        t.tick = tickedNow;
        if (tickedNow) ++t.generation;
    }
    bool ticked(Rate r) const { return tiers_[index(r)].tick; }
    uint64_t generation(Rate r) const { return tiers_[index(r)].generation; }

    // ── Producers (manager-dispatched work) ────────────────────────────────────
    // fingerprint: write the producer's INPUT snapshot into dst (a plain byte image —
    //   memcpy-able struct or field-by-field copies; beware padding) and return the
    //   byte count. Must cover EVERY input the produce reads.
    // produce: the work. The manager runs it only on this tier's tick, at most once
    //   per tick, and only when the fingerprint changed since its last run (or on
    //   its first run ever).
    using SnapshotFn = std::function<size_t(void* dst)>;
    using ProduceFn  = std::function<void()>;
    int declareProducer(Rate at, SnapshotFn fingerprint, ProduceFn produce) {
        producers_.push_back(Producer{at, std::move(fingerprint), std::move(produce),
                                      /*lastGen*/ 0, /*everRan*/ false, /*runs*/ 0, /*shadow*/ {}});
        return (int)producers_.size() - 1;
    }

    // Dispatch one tier's due producers. The owner calls this on the tier's tick
    // (after advance()). NOT a "run now": it touches only producers DECLARED at this
    // tier, at most once per generation, and only when their fingerprint changed.
    // The shadow is the POST-RUN image: after a produce the fingerprint is
    // re-snapshotted, so an input change costs exactly ONE run (the post-run state
    // matches what the produce wrote) and any later difference is a genuine foreign
    // change (an input moved, or — when the image includes the produced cells — an
    // external writer touched them).
    void dispatch(Rate r) {
        const uint64_t gen = generation(r);
        for (Producer& p : producers_) {
            if (p.at != r || p.lastGen == gen) continue;   // other tier / already dispatched this tick
            p.lastGen = gen;
            const size_t n = p.fingerprint ? p.fingerprint(scratch_.data()) : 0;
            if (!p.everRan || n != p.shadow.size() ||
                std::memcmp(scratch_.data(), p.shadow.data(), n) != 0) {
                p.produce();
                p.everRan = true;
                ++p.runs;
                // Re-snapshot AFTER the produce: the shadow holds the post-run image.
                const size_t n2 = p.fingerprint ? p.fingerprint(scratch_.data()) : 0;
                p.shadow.assign(scratch_.data(), scratch_.data() + n2);
            }
        }
    }

    // ── Internal time-anchored tier sources (the S7 deletion) ────────────────────
    // Control (WallClockDiv, ~1500 Hz) and Frame (VideoClock cadence, ~90 Hz) are
    // TIME-anchored: fixed Hz regardless of sample rate — the SR-aware controlDivider
    // pattern (division = round(SR/Hz), min 1) generalized INTO the manager, so the
    // raw ClockDivider members could be deleted (deletion = the enforcement: a deleted
    // member cannot be .process()'d). Call setSampleRate on SR change (and once at
    // construction), then tickClocks() ONCE per sample; the tiers' ticks come from
    // these internal counters — the owner no longer feeds them.
    void setSampleRate(float sampleRate) {
        controlDivision_ = sampleRate > 0.f ? (int)(sampleRate * (1.f / 1500.f) + 0.5f) : 1;
        frameDivision_   = sampleRate > 0.f ? (int)(sampleRate * (1.f /   90.f) + 0.5f) : 1;
        if (controlDivision_ < 1) controlDivision_ = 1;
        if (frameDivision_   < 1) frameDivision_   = 1;
    }
    void tickClocks() {
        advance(Rate::Control, ++controlCount_ >= controlDivision_);
        if (controlCount_ >= controlDivision_) controlCount_ = 0;
        advance(Rate::Frame,   ++frameCount_   >= frameDivision_);
        if (frameCount_   >= frameDivision_)   frameCount_   = 0;
    }

    // ── Observation (harness/diagnostics only) ─────────────────────────────────
    uint64_t runs(int id) const { return producers_[(size_t)id].runs; }
    bool everRan(int id) const { return producers_[(size_t)id].everRan; }
    int producerCount() const { return (int)producers_.size(); }

private:
    struct Tier { bool tick = false; uint64_t generation = 0; };
    struct Producer {
        Rate at = Rate::Control;
        SnapshotFn fingerprint;
        ProduceFn produce;
        uint64_t lastGen = 0;                 // generation this producer last dispatched in
        bool everRan = false;
        uint64_t runs = 0;
        std::vector<uint8_t> shadow;          // fingerprint image of the last RUN
    };
    static size_t index(Rate r) { return (size_t)r; }

    Tier tiers_[6] = {};
    std::vector<Producer> producers_;
    std::vector<uint8_t> scratch_ = std::vector<uint8_t>(kMaxFingerprint);
    // The internal time-anchored counters (see setSampleRate/tickClocks).
    int controlDivision_ = 1, controlCount_ = 0;
    int frameDivision_ = 1, frameCount_ = 0;
};

}  // namespace redDot
