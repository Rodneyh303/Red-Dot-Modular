#pragma once

#include <rack.hpp>
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

    // RATE MANAGER (S4): publishGlobal's producer halves.
    size_t publishGlobalFingerprint_(Monsoon& ownerMon, void* dst);
    void   publishGlobalProduce_(Monsoon& ownerMon);

    // Guard for the CA pin remap (forceRecomputeSlewed + remapSlewedByPins). That pair used to run
    // EVERY control block while the pins were non-identity, rewriting the slewed buffers in place --
    // which the Sands MONO/MACRO display reads, so the UI caught partially-rewritten buffers and the
    // REST/ACCENT lanes FLICKERED continuously after a scatter (see SANDS_SCATTER_FLICKER_DIAGNOSIS).
    // The remapped result only changes when the PINS change or the underlying slewed buffers are
    // regenerated (a redraw moves the draw counters / mix / slew). So we run the pair only when this
    // signature changes; otherwise last cycle's remapped buffers are already correct.
    struct RemapSig {
        uint8_t  rSrc[16] = {0}, mSrc[16] = {0}, qSrc[16] = {0};
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
        bool own[112] = {};   // raw eastOwns bytes (meaningful only when !gMonNull)
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
        float sp[7] = {};          // unified PL lane order (MEL,OCT,QMIX,REST,ACC,VAR,LEG)
        uint8_t mode[7] = {};
        float swRhythm[16] = {}, swMelody[16] = {}, swOctave[16] = {}, swQmix[16] = {},
              swAccent[16] = {}, swVariation[16] = {}, swLegato[16] = {};
        float prRhythm[16] = {}, prMelody[16] = {}, prOctave[16] = {}, prQmix[16] = {},
              prAccent[16] = {}, prVariation[16] = {}, prLegato[16] = {};
        float row[7][16] = {};     // random_[0] as last written by the pass
    } lastV1Spread_;
    bool v1SpreadDirty_(bool axR, bool axM, bool axQ, const float sp[7]);
    void v1SpreadCapture_(bool axR, bool axM, bool axQ, const float sp[7]);

    // GATE 3 — rebuildWarpMirrorVoice(0) after the spread passes: the mirror row is a
    // pure function of (random_[0], warpDelegated_[0], warpBank_) — warpMixCell reads
    // exactly those three, so fingerprinting them at the call site is complete.
    struct MirrorSig {
        bool valid = false;
        const void* bank = nullptr;
        bool deleg[7] = {};
        float row[7][16] = {};
    } lastMirrorSig_;
};
