#pragma once

#include <rack.hpp>
#include "../engines/SequencerEngine.hpp"

struct MonsoonExpanderManager;

/**
 * MonsoonSandsManager
 * 
 * Encapsulates all DNA strand scramble and reset operations.
 * This manager abstracts away the repetitive pattern of:
 *   1. Call PatternEngine rotate/reset methods
 *   2. Refresh visual cache
 * 
 * By centralizing this logic, we reduce Monsoon class bloat and make
 * DNA strand operations easier to understand and extend.
 */
class MonsoonSandsManager {
public:
    MonsoonSandsManager(SequencerEngine& engine) 
        : engine(engine), patternEngine(engine.pe) {}
    
    // ──── Scramble Operations ────────────────────────────────────────────

    /// Main update loop called at control rate
    void processDNA(const MonsoonExpanderManager& expanderManager, bool spreadInterpMono = false);

    // Rotate a single strand by a random amount (0–15 steps)
    // and refresh the visual cache
    
    void scrambleRhythm();
    void scrambleMelody();
    void scrambleVariation();
    void scrambleAccent();
    void scrambleOctave();
    void scrambleLegato();
    
    // Scramble all strands in related groups
    void scrambleRhythmGroup();      // Scramble rhythm + variation + legato
    void scrambleMelodyGroup();      // Scramble melody + octave
    void scrambleAll();              // Scramble all 6 strands
    
    // ──── Reset Operations ───────────────────────────────────────────────
    // Restore a single strand to its original (pre-scramble) values
    // and refresh the visual cache
    
    void resetRhythm();
    void resetMelody();
    void resetVariation();
    void resetAccent();
    void resetOctave();
    void resetLegato();
    
    // Reset all strands in related groups
    void resetRhythmGroup();         // Reset rhythm + variation + legato
    void resetMelodyGroup();         // Reset melody + octave
    void resetAll();                 // Reset all 6 strands
    
private:
    SequencerEngine& engine;
    PatternEngine& patternEngine;

    // DNA Action Triggers (Input Gates) - Suffixes match MeloDicerIds (ALL, R, M, etc.)
    rack::dsp::SchmittTrigger scrambleInputTrig_ALL, scrambleInputTrig_R, scrambleInputTrig_M, 
                               scrambleInputTrig_V, scrambleInputTrig_L, scrambleInputTrig_A, scrambleInputTrig_O;
    rack::dsp::SchmittTrigger resetInputTrig_ALL, resetInputTrig_R, resetInputTrig_M, 
                               resetInputTrig_V, resetInputTrig_L, resetInputTrig_A, resetInputTrig_O;

    // DNA Action Triggers (UI Buttons)
    rack::dsp::BooleanTrigger scrambleParamTrig_ALL, scrambleParamTrig_R, scrambleParamTrig_M, 
                               scrambleParamTrig_V, scrambleParamTrig_L, scrambleParamTrig_A, scrambleParamTrig_O;
    rack::dsp::BooleanTrigger resetParamTrig_ALL, resetParamTrig_R, resetParamTrig_M, 
                               resetParamTrig_V, resetParamTrig_L, resetParamTrig_A, resetParamTrig_O;
    
    // Helper to refresh visual cache after any strand change
    void refreshVisualCache_();
};
