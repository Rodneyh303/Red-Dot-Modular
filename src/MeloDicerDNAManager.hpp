#pragma once

#include "PatternEngine.hpp"

/**
 * DNAStrandManager
 * 
 * Encapsulates all DNA strand scramble and reset operations.
 * This manager abstracts away the repetitive pattern of:
 *   1. Call PatternEngine rotate/reset methods
 *   2. Refresh visual cache
 * 
 * By centralizing this logic, we reduce MeloDicer class bloat and make
 * DNA strand operations easier to understand and extend.
 */
class DNAStrandManager {
public:
    DNAStrandManager(PatternEngine& patternEngine) 
        : patternEngine(patternEngine) {}
    
    // ──── Scramble Operations ────────────────────────────────────────────
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
    PatternEngine& patternEngine;
    
    // Helper to refresh visual cache after any strand change
    void refreshVisualCache_();
};
