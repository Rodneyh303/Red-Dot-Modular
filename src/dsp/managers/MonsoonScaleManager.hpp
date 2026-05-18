#pragma once
#include <rack.hpp>
#include <vector>
#include <string>
#include <cstdint>

struct ScaleType {
    std::string name;
    std::vector<int> intervals; 
};

extern const std::vector<ScaleType> MONSOON_SCALES;

struct Monsoon;
class ParameterManager;

/**
 * ScaleManager
 * 
 * Encapsulates all scale-related state and logic.
 */
class ScaleManager {
public:
    int scaleRoot = 0;
    int lastSelectedScale = -1;
    bool lockScaleNotes = false;
    uint16_t activeScaleMask = 0xFFF;

    ScaleManager(Monsoon* module) : module(module) {}

    /// Recalculates the mask and applies fader locks/redistribution if enabled
    void updateScaleMask();

    /// Returns the effective semitone weight, respecting the scale mask if locked
    float getSemitoneWeight(int semIdx, const ParameterManager& pm) const;

    /// Resets scale state to defaults
    void reset() {
        scaleRoot = 0;
        lastSelectedScale = -1;
        lockScaleNotes = false;
        updateScaleMask();
    }

    /// Calculate a bitmask where bits 0-11 represent semitones C-B in the given scale
    static uint16_t calculateMask(int root, int scaleIdx);

    /// Redistribute probability weights from out-of-scale notes to the nearest in-scale neighbors
    static void redistributeWeights(uint16_t mask, float* weights);

private:
    Monsoon* module;
};