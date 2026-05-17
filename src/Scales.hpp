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
        float original_weights[12];
        for (int i = 0; i < 12; i++) {
            original_weights[i] = weights[i];
            weights[i] = 0.0f; // Clear current weights, we'll rebuild them
        }

        // Pass 1: Place original in-scale non-zero faders
        for (int i = 0; i < 12; i++) {
            if ((mask & (1 << i)) && original_weights[i] > 0.0001f) {
                weights[i] = original_weights[i];
            }
        }

        // Pass 2: Nudge out-of-scale non-zero faders to the nearest available in-scale slot.
        for (int i = 0; i < 12; i++) {
            if (!(mask & (1 << i)) && original_weights[i] > 0.0001f) {
                float val = original_weights[i];

                int nearest_in_scale_note = -1;
                int dist_up = 13, dist_down = 13;
                int target_up = -1, target_down = -1;

                // Find nearest in-scale note "up"
                for (int d = 1; d <= 6; d++) {
                    int current_up = (i + d) % 12;
                    if (mask & (1 << current_up)) {
                        dist_up = d;
                        target_up = current_up;
                        break;
                    }
                }
                // Find nearest in-scale note "down"
                for (int d = 1; d <= 6; d++) {
                    int current_down = (i - d + 12) % 12;
                    if (mask & (1 << current_down)) {
                        dist_down = d;
                        target_down = current_down;
                        break;
                    }
                }

                // Determine the single nearest target
                if (target_up == -1 && target_down == -1) {
                    // No in-scale notes found, value is effectively lost.
                    continue;
                } else if (target_up == -1) {
                    nearest_in_scale_note = target_down;
                } else if (target_down == -1) {
                    nearest_in_scale_note = target_up;
                } else if (dist_up < dist_down) {
                    nearest_in_scale_note = target_up;
                } else if (dist_down < dist_up) {
                    nearest_in_scale_note = target_down;
                } else { // Equidistant, prefer higher index (up)
                    nearest_in_scale_note = target_up;
                }

                if (nearest_in_scale_note != -1) {
                    // If the target slot is currently empty (0.0f), move the fader there.
                    // Otherwise, the nudged fader's value is discarded to preserve existing heights.
                    if (weights[nearest_in_scale_note] < 0.0001f) { // Check if slot is effectively empty
                        weights[nearest_in_scale_note] = val;
                    }
                }
            }
        }
    }
};