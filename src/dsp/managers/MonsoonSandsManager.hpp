#pragma once

#include <rack.hpp>
#include <array>   // P-B: std::array mirror members (ARRAY_CONTAINER_AUDIT)
#include <cstdint>
#include "../engines/SequencerEngine.hpp"
#include "../SandsTopology.hpp"   // cachedTopo_ — the topology-rebuild fingerprint gate

struct MonsoonExpanderManager;
struct Monsoon;

/**
 * MonsoonSandsManager
 * 
 * Owns the control-rate DNA/Sands pipeline (processDNA): the A/B mix, slew, the
 * Change Alley pre-spread pin remap, and spread application.
 *
 * (The DNA strand scramble/reset family that this class was originally built for was
 *  removed in cleanup/dna-rotation-dead-code — it was never wired to any trigger loop.)
 */
class MonsoonSandsManager {
public:
    MonsoonSandsManager(SequencerEngine& engine) 
        : engine(engine), patternEngine(engine.pe) {}
    
    /// Main update loop called at control rate
    void processDNA(const MonsoonExpanderManager& expanderManager);

    // ── RATE MANAGER (CC_PROMPT_RATE_MANAGER_START, S4) ──────────────────────────
    // publishGlobal — the Macro display/global publish, the SPEC's proto-producer/slot
    // ("publish at control rate, consumers read") — folds into the ONE producer/slot
    // mechanism: a Control-rate producer with an input+produced-cells fingerprint (the
    // S0b pattern), dispatched at the sync position. DECLARATION ORDER IS LOAD-BEARING:
    // this producer must be declared BEFORE the expander manager's sync-population
    // producer (it writes the macroBase that population reads).
    void declareRateProducers(redDot::RateManager& rm, Monsoon& ownerMon);

private:
    SequencerEngine& engine;
    PatternEngine& patternEngine;

    // RATE MANAGER (S4): publishGlobal's producer halves — D17 compare-in-place form.
    // compare_ walks the LIVE inputs field-by-field against publishSig_ (the typed
    // shadow below) and early-outs on the first difference — no image build, no copy
    // through the manager; capture_ stores the post-run image into publishSig_ after a
    // run. The compare is the capture's MIRROR — same fields, same order, adjacent
    // functions, one enumeration each (drift between the pair = stale state; the
    // RateManager R9 coverage test pins the property). The old byte-image
    // publishGlobalFingerprint_ is DELETED (deletion is the enforcement).
    bool publishGlobalCompare_(Monsoon& ownerMon);
    void publishGlobalCapture_(Monsoon& ownerMon);
    void   publishGlobalProduce_(Monsoon& ownerMon);

    // The input + produced-cells shadow (the S0b pattern, D17-typed): every input the
    // publish reads (the global LOR/spread/atten stores on the owner, the Macro CV
    // jacks, presence) plus the published cells (macroBase/CVDelta/preD/postD/
    // spreadEffective on the visual + macroOwnSpread on the engine) — self-healing
    // against foreign writers (UI store edits on the visual land in the shadow and
    // re-publish). The pointer words stay FIRST so a presence flip short-circuits the
    // walk; the null-presence case zero-fills the rest (capture), and the compare
    // skips the blocked regions — gated by the same pointer words, exactly as the
    // byte image was.
    struct PublishSig {
        uint64_t macroVis, gMon;
        std::array<std::array<float, 3>, 7>   lor = {};        // getGlobalLor(lane, 0..2) — the LOR knobs
        std::array<float, 7>                  spr = {};        // getGlobalSpread(lane)
        std::array<std::array<float, 4>, 7>   atten = {};      // getGlobalAtten(lane, item) — the CV depths
        std::array<std::array<uint8_t, 4>, 7> cvConnected = {};
        std::array<std::array<float, 4>, 7>   cvVoltage = {};  // macroCvId(lane,item): connected + voltage
        // produced cells (self-healing)
        std::array<std::array<float, 4>, 7>   macroBase = {};
        std::array<std::array<float, 4>, 7>   macroCVDelta = {};
        std::array<std::array<float, 4>, 7>   macroPreDelta = {};
        std::array<std::array<float, 4>, 7>   macroPostDelta = {};
        std::array<float, 7>                  spreadEffective = {};
        std::array<float, 7>                  macroOwnSpread = {};
    } publishSig_{};

    // Guard for the CA pin remap (forceRecomputeSlewed + remapSlewedByPins). That pair used to run
    // EVERY control block while the pins were non-identity, rewriting the slewed buffers in place --
    // which the Sands MONO/MACRO display reads, so the UI caught partially-rewritten buffers and the
    // REST/ACCENT lanes FLICKERED continuously after a scatter (see SANDS_SCATTER_FLICKER_DIAGNOSIS).
    // The remapped result only changes when the PINS change or the underlying slewed buffers are
    // regenerated (a redraw moves the draw counters / mix / slew). So we run the pair only when this
    // signature changes; otherwise last cycle's remapped buffers are already correct.
// P-B (ARRAY_CONTAINER_AUDIT): 2-D member alias — byte-identical to T[Rows][Cols] (Finding D).
template <class T, size_t Rows, size_t Cols> using Matrix2 = std::array<std::array<T, Cols>, Rows>;
    struct RemapSig {
        std::array<uint8_t, 16> rSrc = {0}, mSrc = {0}, qSrc = {0};
        int64_t  rCtr = 0, mCtr = 0, qCtr = 0;
        // Mix + slew intentionally excluded (phrase-boundary consumption — see remapSigChanged_ comment)
        bool wasIdentity = true;
    } lastRemap_;
    bool remapSigChanged_(const uint8_t* rSrc, const uint8_t* mSrc, const uint8_t* qSrc, bool identity) const;
    void captureRemapSig_(const uint8_t* rSrc, const uint8_t* mSrc, const uint8_t* qSrc, bool identity);

    // ── Control-tick redundancy gates (perf round 2 — CC_PROMPT_cpu_section_profile) ──
    // processDNA runs at ~1500 Hz but several of its outputs change only on UI/presence
    // edits or rolls. Each gate fingerprints the ACTUAL inputs of the gated work and
    // skips the work when they are bit-unchanged. The bit-identical argument lives at
    // the gate site; all state is per-Monsoon (this manager is a Monsoon member).

    // GATE 1 — SandsTopology rebuild + warpDelegated_ mirror rescan. The fill is a pure
    // function of (presence bits, gMon-null, eastOwns[112], polyVoiceCount); build is a
    // pure function of the filled Inputs; the mirror scan only writes owner() flips.
    struct TopoSig {
        bool valid = false;
        bool eastPresent = false, macroPresent = false, polyBaseActive = false;
        bool gMonNull = true;
        int  polyVoiceCount = 0;
        std::array<bool, 112> own = {};   // raw eastOwns bytes (meaningful only when !gMonNull)
    } lastTopoSig_;
    dotModular::SandsTopology cachedTopo_{};

    // GATE 2 — the V1 spread pass (Mono-visual branch AND East-V1 branch; ONE sig because
    // it captures (inputs → row), not which branch ran). Inputs: per-lane spread values,
    // axis gates, spreadTargetMode, the mono slewed + pre-remap rows — PLUS random_[0]
    // as last written, so ANY external writer of the row (pin-remap promote, rotates,
    // resets) breaks the match and the pass re-runs, preserving the unconditional
    // version's every-tick healing property exactly.
    struct V1SpreadSig {
        bool valid = false;
        bool axR = true, axM = true, axQ = true;
        std::array<float, 7> sp = {};          // unified PL lane order (MEL,OCT,QMIX,REST,ACC,VAR,LEG)
        std::array<uint8_t, 7> mode = {};
        std::array<float, 16> swRhythm{}, swMelody{}, swOctave{}, swQmix{},
              swAccent{}, swVariation{}, swLegato{};
        std::array<float, 16> prRhythm{}, prMelody{}, prOctave{}, prQmix{},
              prAccent{}, prVariation{}, prLegato{};
        std::array<std::array<float, 16>, 7> row = {};     // random_[0] as last written by the pass
    } lastV1Spread_;
    bool v1SpreadDirty_(bool axR, bool axM, bool axQ, const float sp[7]);
    void v1SpreadCapture_(bool axR, bool axM, bool axQ, const float sp[7]);

    // GATE 3 — rebuildWarpMirrorVoice(0) after the spread passes: the mirror row is a
    // pure function of (random_[0], warpDelegated_[0], warpBank_) — warpMixCell reads
    // exactly those three, so fingerprinting them at the call site is complete.
    struct MirrorSig {
        bool valid = false;
        const void* bank = nullptr;
        std::array<bool, 7> deleg = {};
        Matrix2<float, 7, 16> row{};
    } lastMirrorSig_;
};
