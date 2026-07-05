#pragma once

/**
 * SpreadInterpolation.hpp
 * 
 * DNA spread interpolation system for Monsoon polyphonic voices.
 * 
 * Two modes:
 * 1. POLY_AVERAGE: Blend each voice toward average of all poly voices
 * 2. MONO_SOURCE: Blend each voice toward mono sequencer's DNA
 * 
 * Called from Monsoon::process() after all voices have generated their
 * random DNA values, but before DNA windows are applied to step generation.
 */

#include <algorithm>
#include <cmath>

namespace RDM {

enum class SpreadMode : uint8_t {
    POLY_AVERAGE = 0,    // Default: blend toward average of all voices
    MONO_SOURCE = 1      // Blend toward mono sequencer
};

/**
 * SpreadInterpolationEngine
 * 
 * Manages per-voice DNA blending for 16-voice polyphonic sequencer.
 * 
 * The DNA window (Rest, Melody, Octave) for each voice can be blended
 * toward either:
 * - The average value across all connected voices (POLY_AVERAGE)
 * - The mono sequencer's value (MONO_SOURCE)
 * 
 * Spread amount is controlled per-dimension (R, M, O) from Sands Macro
 * expander, allowing independent control of how much each DNA dimension
 * is affected.
 */
class SpreadInterpolationEngine {
public:
    SpreadInterpolationEngine() = default;
    
    /**
     * Apply spread interpolation to voice DNA windows
     * 
     * @param voiceDNA: Array of 16 float values (one per voice) for a dimension
     * @param monoValue: The mono sequencer's DNA value for this dimension
     * @param numVoices: Number of connected voices (1-16)
     * @param spreadAmount: How much to blend (0.0 = no effect, 1.0 = full blend)
     * @param mode: POLY_AVERAGE or MONO_SOURCE
     * 
     * Modifies voiceDNA in-place
     */
    static void applySpread(
        float voiceDNA[16],
        float monoValue,
        int numVoices,
        float spreadAmount,
        SpreadMode mode
    ) {
        if (spreadAmount < 0.001f) {
            // No spread, skip
            return;
        }
        
        if (mode == SpreadMode::POLY_AVERAGE) {
            applyPolyAverageSpread(voiceDNA, numVoices, spreadAmount);
        } else {
            applyMonoSourceSpread(voiceDNA, monoValue, numVoices, spreadAmount);
        }
    }
    
private:
    /**
     * POLY_AVERAGE MODE
     * 
     * Blend each voice toward the average of all poly voices.
     * 
     * Musical effect:
     * - Voices pull toward each other
     * - Creates coherent ensemble feeling
     * - Less chaos, more unity
     * 
     * Use case:
     * - Building harmonies that stay together
     * - Reducing randomness while maintaining variation
     * - Creating evolving chord structures
     */
    static void applyPolyAverageSpread(
        float voiceDNA[16],
        int numVoices,
        float spreadAmount
    ) {
        // Calculate average DNA value across all voices
        float average = 0.0f;
        for (int v = 0; v < numVoices; v++) {
            average += voiceDNA[v];
        }
        average /= numVoices;
        
        // Blend each voice toward average
        for (int v = 0; v < numVoices; v++) {
            voiceDNA[v] = lerp(voiceDNA[v], average, spreadAmount);
        }
    }
    
    /**
     * MONO SOURCE MODE
     * 
     * Blend each voice toward the mono sequencer's DNA value.
     * 
     * Musical effect:
     * - All voices follow the main sequencer
     * - Creates leader-follower behavior
     * - More predictable, structured variation
     * 
     * Use case:
     * - Poly voices inheriting mono's characteristics
     * - Creating variations on a theme
     * - Building from a central idea
     * - Structured poly expansion
     */
    static void applyMonoSourceSpread(
        float voiceDNA[16],
        float monoValue,
        int numVoices,
        float spreadAmount
    ) {
        // Blend each voice toward mono
        // Voice 0 is mono, don't blend it (it IS the source)
        for (int v = 1; v < numVoices; v++) {
            voiceDNA[v] = lerp(voiceDNA[v], monoValue, spreadAmount);
        }
    }
    
    /**
     * Linear interpolation helper
     * @return a + (b - a) * t
     */
    static float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }
};

/**
 * SpreadState
 * 
 * Per-dimension spread information managed by Sands Macro expander.
 * Holds spread amount and mode selection.
 */
struct SpreadState {
    float restSpread = 0.0f;     // 0.0 - 1.0
    float melodySpread = 0.0f;   // 0.0 - 1.0
    float octaveSpread = 0.0f;   // 0.0 - 1.0
    SpreadMode mode = SpreadMode::POLY_AVERAGE;
    
    void reset() {
        restSpread = 0.0f;
        melodySpread = 0.0f;
        octaveSpread = 0.0f;
        mode = SpreadMode::POLY_AVERAGE;
    }
};

}  // namespace RDM
