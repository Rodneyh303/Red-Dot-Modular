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
    /// Calculate a bitmask where bits 0-11 represent semitones C-B in the given scale
    static uint16_t calculateMask(int root, int scaleIdx) {
        if (scaleIdx < 0 || scaleIdx >= (int)BITWIG_SCALES.size()) return 0xFFF;
        uint16_t mask = 0;
        for (int interval : BITWIG_SCALES[scaleIdx].intervals) {
            mask |= (1 << ((root + interval) % 12));
        }
        return mask;
    }

    /// Redistribute probability weights from out-of-scale notes to the nearest in-scale neighbors
    static void redistributeWeights(uint16_t mask, float* weights) {
        float originalWeights[12];
        for (int i = 0; i < 12; i++) {
            originalWeights[i] = weights[i];
            weights[i] = 0.0f;
        }

        // Pass 1: Keep original in-scale weights
        for (int i = 0; i < 12; i++) {
            if ((mask & (1 << i))) {
                weights[i] = originalWeights[i];
            }
        }

        // Pass 2: Redistribute out-of-scale weights to neighbors
        for (int i = 0; i < 12; i++) {
            if (!(mask & (1 << i)) && originalWeights[i] > 0.0001f) {
                float val = originalWeights[i];

                int dist_up = 13, dist_down = 13;

                for (int d = 1; d <= 6; d++) {
                    if (mask & (1 << ((i + d) % 12))) { dist_up = d; break; }
                }
                for (int d = 1; d <= 6; d++) {
                    if (mask & (1 << ((i - d + 12) % 12))) { dist_down = d; break; }
                }

                if (dist_up < dist_down) {
                    weights[(i + dist_up) % 12] += val;
                } else if (dist_down < dist_up) {
                    weights[(i - dist_down + 12) % 12] += val;
                } else if (dist_up <= 6) {
                    weights[(i + dist_up) % 12] += val * 0.5f;
                    weights[(i - dist_down + 12) % 12] += val * 0.5f;
                }
            }
        }
        for (int i = 0; i < 12; ++i) if (weights[i] > 1.0f) weights[i] = 1.0f;
    }
};