#pragma once
#include <vector>
#include <string>

struct ScaleType {
    std::string name;
    std::vector<int> intervals; 
};

static const std::vector<ScaleType> BITWIG_SCALES = {
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

struct ScaleHelper {
    static uint16_t calculateMask(int root, int scaleIdx) {
        if (scaleIdx < 0 || scaleIdx >= (int)BITWIG_SCALES.size()) return 0xFFF;
        uint16_t mask = 0;
        for (int interval : BITWIG_SCALES[scaleIdx].intervals) {
            mask |= (1 << ((root + interval) % 12));
        }
        return mask;
    }

    static void redistributeWeights(uint16_t mask, float* weights) {
        float redistributed[12] = {0.0f};
        for (int i = 0; i < 12; i++) {
            if (mask & (1 << i)) {
                redistributed[i] += weights[i];
            } else if (weights[i] > 0.0001f) {
                float val = weights[i];
                int distUp = 13, distDown = 13;
                for (int d = 1; d <= 6; d++) {
                    if (mask & (1 << ((i + d) % 12))) { distUp = d; break; }
                }
                for (int d = 1; d <= 6; d++) {
                    if (mask & (1 << ((i - d + 12) % 12))) { distDown = d; break; }
                }

                if (distUp < distDown) {
                    redistributed[(i + distUp) % 12] += val;
                } else if (distDown < distUp) {
                    redistributed[(i - distDown + 12) % 12] += val;
                } else if (distUp <= 6) {
                    redistributed[(i + distUp) % 12] += val * 0.5f;
                    redistributed[(i - distDown + 12) % 12] += val * 0.5f;
                }
            }
        }
        for (int i = 0; i < 12; i++) {
            weights[i] = redistributed[i];
        }
    }
    
    /// Update the active scale mask based on root and scale selection
    /// Returns the computed mask (0xFFF = all notes, 0 = custom scale)
    static uint16_t updateScaleMask(int root, int scaleIdx) {
        if (scaleIdx < 0 || scaleIdx >= (int)BITWIG_SCALES.size()) return 0xFFF;
        uint16_t newMask = 0;
        for (int interval : BITWIG_SCALES[scaleIdx].intervals) {
            newMask |= (1 << ((root + interval) % 12));
        }
        return newMask;
    }
};