#pragma once

/**
 * DNAEngine.hpp
 * 
 * Generates stochastic DNA windows (probability distributions) for
 * controlling Rest, Melody (pitch variation), and Octave randomness
 * in the sequencer.
 * 
 * DNA Window: A 64-step probability pattern that determines where in
 * a repeating cycle randomness is applied.
 * 
 * Each DNA dimension is independently controlled by:
 * - Length: How many steps repeat the pattern (1-64)
 * - Offset: Where in the pattern to start (0.0-1.0)
 * - Rotation: How much to shift the pattern (0.0-1.0)
 */

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>

namespace RDM {

/**
 * DNAWindow
 * 
 * A 64-step probability window used to control when a DNA dimension
 * (Rest, Melody, Octave) affects the sequencer output.
 * 
 * Each step contains a probability (0.0 - 1.0):
 * - 0.0 = DNA dimension has no effect
 * - 1.0 = DNA dimension has full effect
 * - 0.5 = 50% influence
 */
struct DNAWindow {
    static constexpr int WINDOW_SIZE = 64;
    std::array<float, WINDOW_SIZE> probabilities;
    
    DNAWindow() : probabilities{} {
        // Default: flat distribution (all steps same influence)
        for (auto& p : probabilities) {
            p = 0.5f;
        }
    }
    
    /**
     * Get probability at a specific step
     * @param step: Step index (0-63)
     */
    float getAt(int step) const {
        return probabilities[step & 0x3F];  // Wrap to 0-63
    }
    
    /**
     * Get probability with fractional step (for smooth interpolation)
     * @param step: Step index as float
     */
    float getInterpolated(float step) const {
        int idx = (int)step & 0x3F;
        int nextIdx = (idx + 1) & 0x3F;
        float frac = step - (int)step;
        
        float v1 = probabilities[idx];
        float v2 = probabilities[nextIdx];
        return v1 + (v2 - v1) * frac;
    }
};

/**
 * DNAEngine
 * 
 * Generates and manages DNA windows based on parameters:
 * - Length: Pattern cycle length
 * - Offset: Starting position
 * - Rotation: Pattern shift
 */
class DNAEngine {
public:
    enum Dimension {
        REST = 0,
        MELODY = 1,
        OCTAVE = 2
    };
    
    struct DNAParameters {
        float length = 0.5f;      // 0.0 - 1.0 (maps to 1-64 steps)
        float offset = 0.0f;      // 0.0 - 1.0 (starting position)
        float rotation = 0.0f;    // 0.0 - 1.0 (pattern shift)
    };
    
    DNAEngine() : restParams{}, melodyParams{}, octaveParams{} {}
    
    /**
     * Update DNA parameters
     * @param dimension: REST, MELODY, or OCTAVE
     * @param params: New parameter values
     */
    void setParameters(Dimension dimension, const DNAParameters& params) {
        switch (dimension) {
            case REST:    restParams = params; break;
            case MELODY:  melodyParams = params; break;
            case OCTAVE:  octaveParams = params; break;
        }
        // Mark dirty so window is regenerated
        dirtyFlags |= (1 << dimension);
    }
    
    /**
     * Get DNA window for a dimension
     * Regenerates if parameters changed
     */
    const DNAWindow& getWindow(Dimension dimension) {
        if (dirtyFlags & (1 << dimension)) {
            regenerateWindow(dimension);
            dirtyFlags &= ~(1 << dimension);
        }
        
        switch (dimension) {
            case REST:    return restWindow;
            case MELODY:  return melodyWindow;
            case OCTAVE:  return octaveWindow;
        }
        return restWindow;
    }
    
    /**
     * Get current DNA value at a step
     * @param dimension: REST, MELODY, or OCTAVE
     * @param step: Current step (0-63)
     */
    float getDNAValueAt(Dimension dimension, int step) {
        return getWindow(dimension).getAt(step);
    }
    
    /**
     * Scramble a DNA dimension
     * Randomizes all probability values while maintaining structure
     */
    void scrambleDimension(Dimension dimension) {
        DNAParameters& params = (dimension == REST) ? restParams :
                               (dimension == MELODY) ? melodyParams :
                               octaveParams;
        
        params.offset = randomFloat();
        params.rotation = randomFloat();
        
        dirtyFlags |= (1 << dimension);
    }
    
    /**
     * Reset a DNA dimension to default
     */
    void resetDimension(Dimension dimension) {
        DNAParameters& params = (dimension == REST) ? restParams :
                               (dimension == MELODY) ? melodyParams :
                               octaveParams;
        
        params.length = (dimension == REST) ? 0.5f :
                       (dimension == MELODY) ? 0.375f :
                       0.25f;  // Different defaults per dimension
        params.offset = 0.0f;
        params.rotation = 0.0f;
        
        dirtyFlags |= (1 << dimension);
    }
    
    /**
     * Reset all DNA dimensions to defaults
     */
    void resetAll() {
        resetDimension(REST);
        resetDimension(MELODY);
        resetDimension(OCTAVE);
    }
    
private:
    DNAWindow restWindow;
    DNAWindow melodyWindow;
    DNAWindow octaveWindow;
    
    DNAParameters restParams;
    DNAParameters melodyParams;
    DNAParameters octaveParams;
    
    uint8_t dirtyFlags = 0x7;  // All dimensions dirty initially
    
    /**
     * Regenerate a DNA window based on current parameters
     * 
     * Algorithm:
     * 1. Create base pattern based on length
     * 2. Apply offset shift
     * 3. Apply rotation transformation
     * 4. Smooth the result
     */
    void regenerateWindow(Dimension dimension) {
        DNAWindow& window = (dimension == REST) ? restWindow :
                           (dimension == MELODY) ? melodyWindow :
                           octaveWindow;
        
        DNAParameters& params = (dimension == REST) ? restParams :
                               (dimension == MELODY) ? melodyParams :
                               octaveParams;
        
        // Map parameter values to actual lengths
        int length = (int)(params.length * 63.0f) + 1;  // 1-64
        length = std::clamp(length, 1, DNAWindow::WINDOW_SIZE);
        
        int offset = (int)(params.offset * DNAWindow::WINDOW_SIZE);
        float rotation = params.rotation;  // 0.0 - 1.0
        
        // Generate base pattern
        for (int i = 0; i < DNAWindow::WINDOW_SIZE; i++) {
            // Repeating pattern based on length
            int patternPos = i % length;
            
            // Base: higher probability at start, fades toward end
            float base = 1.0f - ((float)patternPos / (float)length);
            
            // Apply offset
            int shiftedPos = (i + offset) % DNAWindow::WINDOW_SIZE;
            
            // Apply rotation (shift base pattern)
            float rotated = base + (rotation * 0.3f) - 0.15f;
            rotated = std::clamp(rotated, 0.0f, 1.0f);
            
            // Smooth with neighboring values
            float smoothed = rotated;
            
            window.probabilities[shiftedPos] = smoothed;
        }
    }
    
    /**
     * Random float helper (0.0 - 1.0)
     * Should be replaced with actual RNG from Rack
     */
    float randomFloat() {
        // Placeholder - will be provided by parent module
        return 0.5f;
    }
};

}  // namespace RDM
