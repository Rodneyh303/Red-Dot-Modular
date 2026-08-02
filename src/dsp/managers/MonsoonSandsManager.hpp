#pragma once

#include <rack.hpp>
#include "../engines/SequencerEngine.hpp"

struct MonsoonExpanderManager;

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

private:
    SequencerEngine& engine;
    PatternEngine& patternEngine;

    // Guard for the CA pin remap (forceRecomputeSlewed + remapSlewedByPins). That pair used to run
    // EVERY control block while the pins were non-identity, rewriting the slewed buffers in place --
    // which the Sands MONO/MACRO display reads, so the UI caught partially-rewritten buffers and the
    // REST/ACCENT lanes FLICKERED continuously after a scatter (see SANDS_SCATTER_FLICKER_DIAGNOSIS).
    // The remapped result only changes when the PINS change or the underlying slewed buffers are
    // regenerated (a redraw moves the draw counters / mix / slew). So we run the pair only when this
    // signature changes; otherwise last cycle's remapped buffers are already correct.
    struct RemapSig {
        uint8_t  rSrc[16] = {0}, mSrc[16] = {0};
        int64_t  rCtr = 0, mCtr = 0;
        float    rMix = 1e30f, mMix = 1e30f, rSlew = 1e30f, mSlew = 1e30f;
        bool wasIdentity = true;
    } lastRemap_;
    bool remapSigChanged_(const uint8_t* rSrc, const uint8_t* mSrc, bool identity) const;
    void captureRemapSig_(const uint8_t* rSrc, const uint8_t* mSrc, bool identity);
};
