#pragma once
#include <rack.hpp>
#include <vector>
#include <string>
#include <cstdint>

struct ScaleType {
    std::string name;
    std::vector<int> intervals; 
};

static const std::vector<ScaleType> MONSOON_SCALES = {
    {"Chromatic", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {"Major", {0, 2, 4, 5, 7, 9, 11}},
    {"Natural Minor", {0, 2, 3, 5, 7, 8, 10}},
    {"Harmonic Minor", {0, 2, 3, 5, 7, 8, 11}},
    {"Melodic Minor", {0, 2, 3, 5, 7, 9, 11}},
    {"Dorian", {0, 2, 3, 5, 7, 9, 10}},
    {"Phrygian", {0, 1, 3, 5, 7, 8, 10}},
    {"Lydian", {0, 2, 4, 6, 7, 9, 11}},
    {"Mixolydian", {0, 2, 4, 5, 7, 9, 10}},
    {"Locrian", {0, 1, 3, 5, 6, 8, 10}},
    {"Pentatonic Major", {0, 2, 4, 7, 9}},
    {"Pentatonic Minor", {0, 3, 5, 7, 10}},
    {"Blues", {0, 3, 5, 6, 7, 10}},
    {"Whole Tone", {0, 2, 4, 6, 8, 10}},
    {"Diminished", {0, 2, 3, 5, 6, 8, 9, 11}},
    {"Super Locrian", {0, 1, 3, 4, 6, 8, 10}},
    {"Bhairav", {0, 1, 4, 5, 7, 8, 11}},
    {"Hungarian Minor", {0, 2, 3, 6, 7, 8, 11}},
    {"Hirojoshi", {0, 2, 3, 7, 8}},
    {"In-Sen", {0, 1, 5, 7, 10}},
    {"Iwato", {0, 1, 5, 6, 10}},
    {"Kumoi", {0, 2, 3, 7, 9}},
    {"Pelog", {0, 1, 3, 7, 8}},
    {"Spanish", {0, 1, 4, 5, 7, 8, 10}}
};

struct MeloDicer;
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

    ScaleManager(MeloDicer* module) : module(module) {}

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
    MeloDicer* module;
};