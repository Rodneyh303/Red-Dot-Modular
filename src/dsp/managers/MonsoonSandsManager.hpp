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
};
